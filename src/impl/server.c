/*
  Copyright (c) 2016, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "errors.h"
#include "log.h"
#include "server.h"
#include "server_resources.h"
#include "server_responses.h"
#include "server_xfer_table.h"
#include "syspoll.h"
#include "unix_socket_server.h"
#include "util.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
   Server context.
*/
struct server
{
    /** The poller (epoll, kqueue, etc.) */
    struct syspoll* poller;
    /** The table of running file transfers */
    struct xfer_table* xfers;
    /** Table of open file timers */
    struct xfer_table* xfer_timers;
    /** Transfers which are to be processed in the secondary event-processing
        loop. E.g., ones that have been cancelled or ones with unexhausted I/O
        spaces. */
    struct resrc_xfer** deferred_xfers;
    /** Size of @a deferred_xfers */
    size_t ndeferred_xfers;
    /** The next transfer ID to be assigned. Starts at 1 and is incremented by 1
        for each new transaction. */
    size_t next_txnid;
    /** The file descriptor upon which client requests are received */
    int reqfd;
    /** The number of milliseconds after which open files are closed */
    unsigned open_file_timeout_ms;
    /* The user ID of this, the server process */
    uid_t uid;
};

#pragma GCC diagnostic pop

static struct server* srv_new(long open_file_timeout_ms, int reqfd, int maxfds);

static void srv_delete(struct server* srv);

static bool errno_is_fatal(int err);

/**
   Adds a new transfer.

   Opens the file, allocates memory for the data structure, and inserts it into
   the running transfers table. Does @e not register the destination file
   descriptor with the poller.

   @post Status and data channel file descriptors are still open, regardless of
   whether the call succeeded or not.
*/
static struct resrc_xfer* add_xfer(struct server* srv,
                                   const struct prot_request* req,
                                   pid_t client_pid,
                                   int stat_fd, int dest_fd,
                                   struct fio_stat* info);

static void delete_xfer_and_close_file_fd(void* p);

static void delete_xfer_and_close_all_fds(void* p);

/**
   Deletes a transfer which has not been registered.

   These transfers have been added to internal structures but have not been
   registered for I/O events with the system poller. None of the file data has
   been transferred.
*/
static void delete_unregistered_xfer(struct server*, struct resrc_xfer*);

/**
   Deletes a transfer which has been completely registered.

   These transfers have been added to internal data structures and have been
   registered for I/O events with the system poller. Zero, some, or all of the
   file data may have been transferred already.
 */
static void delete_registered_xfer(struct server* srv, struct resrc_xfer* xfer);

/**
   Designates a transfer to be processed during the secondary event-processing
   loop.
*/
static void defer_xfer(struct server* srv,
                       struct resrc_xfer* xfer,
                       enum deferral how);

/**
   Designates a transfer to be processed during the primary event-processing
   loop again.
*/
static size_t undefer_xfer(struct server* srv, size_t idx);

/* ----------------- ----------------- */

static bool process_events(struct server* srv,
                           int nevents,
                           void* buf, size_t buf_size);

static void process_deferred(struct server* const ctx);

static bool handle_reqfd(struct server* srv,
                         int events,
                         void* buf,
                         size_t buf_size);

static bool process_request(struct server* srv,
                            const void* buf, size_t size,
                            pid_t client_pid, const int* fds);

static bool transfer_file(struct server* srv, struct resrc_xfer* xfer);

bool srv_run(const int reqfd,
             const int maxfds,
             const long open_file_timeout_ms)
{
    struct server* const srv = srv_new(open_file_timeout_ms, reqfd, maxfds);
    if (!srv)
        return false;

    if (!syspoll_register(srv->poller,
                          (struct syspoll_resrc*)&srv->reqfd,
                          SYSPOLL_READ)) {
        goto fail;
    }

    void* const recvbuf = calloc(PROT_REQ_MAXSIZE, 1);
    if (!recvbuf)
        goto fail;

    for (;;) {
        /* If there are deferred transfers, don't block on waiting for events
           otherwise deferred transfers will be starved */
        const int nready = (srv->ndeferred_xfers == 0 ?
                            syspoll_wait(srv->poller) :
                            syspoll_poll(srv->poller));

        if (nready == -1) {
            if (errno != EINTR && errno_is_fatal(errno)) {
                sfd_log(LOG_ERR, "Fatal error in syspoll_wait/poll(): [%m]\n");
                break;
            }
        } else {
            if (!process_events(srv, nready, recvbuf, PROT_REQ_MAXSIZE))
                break;
        }

        process_deferred(srv);
    }

    free(recvbuf);
    srv_delete(srv);

    return true;

 fail:
    PRESERVE_ERRNO(srv_delete(srv));

    return false;
}

static bool process_events(struct server* srv,
                           const int nevents,
                           void* buf, const size_t buf_size)
{
    for (int i = 0; i < nevents; i++) {
        struct syspoll_events events = syspoll_get(srv->poller, i);

        if (events.events & SYSPOLL_TERM)
            return false;

        if (*(int*)events.udata == srv->reqfd) {
            if (events.events & SYSPOLL_ERROR ||
                !handle_reqfd(srv, events.events, buf, buf_size)) {
                sfd_log(LOG_ERR,
                        "Fatal error on request socket (%m); shutting down\n");
                return false;
            }

        } else {
            const bool error_event = (events.events & SYSPOLL_ERROR);

            if (error_event) {
                sfd_log(LOG_ERR,
                        "Fatal error on resource (from system poller)");
            }

            if (is_timer(events.udata)) {
                struct resrc_timer* const timer = events.udata;
                struct resrc_xfer* const xfer = xfer_table_find(srv->xfers,
                                                                timer->txnid);

                if (xfer) {
                    /* Timer has elapsed and a transfer with the same txnid
                       exists */
                    if (xfer == timer->xfer_addr) {
                        if (xfer->nbytes_left == xfer->file.size) {
                            /* Transfer has expired before first byte was
                               transferred */
                            send_xfer_err(xfer->stat_fd, ETIMEDOUT);
                            defer_xfer(srv, xfer, CANCEL);
                        }
                    } else {
                        /* Transfer has same txnid but different address ->
                           wrapped transaction ID (!) */
                        sfd_log(LOG_EMERG, "Expired timer has invalid txnid\n");
                    }
                }

                xfer_table_erase(srv->xfer_timers, timer->txnid);
                timer_delete(timer);

            } else if (is_response(events.udata)) {
                struct resrc_resp* r = (struct resrc_resp*)events.udata;

                if (error_event || send_pdu(r->stat_fd, &r->pdu, r->pdu_size) ||
                    errno_is_fatal(errno)) {
                    close(r->stat_fd);
                    free(r);
                }

            } else {
                assert (is_xfer(events.udata));

                struct resrc_xfer* const xfer = events.udata;

                if (xfer->defer != CANCEL &&
                    (error_event ||
                     (xfer->defer != READY && !transfer_file(srv, xfer)))) {
                    PRESERVE_ERRNO(delete_registered_xfer(srv, xfer));
                }
            }
        }
    }

    return true;
}

static void process_deferred(struct server* const ctx)
{
    for (size_t i = 0; i < ctx->ndeferred_xfers; /* noop */) {
        struct resrc_xfer* const x = ctx->deferred_xfers[i];

        assert (is_xfer(x));

        switch (x->defer) {
        case CANCEL:
            i = undefer_xfer(ctx, i);
            delete_registered_xfer(ctx, x);
            break;

        case READY:
            if (!transfer_file(ctx, x)) {
                i = undefer_xfer(ctx, i);
                delete_registered_xfer(ctx, x);

            } else {
                if (x->defer == NONE) {
                    /* I/O space was filled during transfer -> back to primary
                       processing */
                    i = undefer_xfer(ctx, i);
                } else {
                    i++;
                }
            }

            break;

        case NONE:
            assert (false);
            sfd_log(LOG_EMERG,
                    "Non-deferred transfer (defer state %d) in deferred list",
                    x->defer);
            break;
        }
    }
}

static void close_fds(int* fds, size_t nfds);

static bool handle_reqfd(struct server* srv,
                         const int events,
                         void* buf,
                         const size_t buf_size)
{
    assert (events == SYSPOLL_READ);

    int recvd_fds [PROT_MAXFDS];
    size_t nfds;
    uid_t uid;
    gid_t gid;
    pid_t pid;

    for (;;) {
        nfds = PROT_MAXFDS;

        const ssize_t nread = us_recv(srv->reqfd,
                                      buf, buf_size,
                                      recvd_fds, &nfds,
                                      &uid, &gid, &pid);

        /* Recv of zero makes no sense on a UDP (connectionless) socket */
        assert (nread != 0);

        if (nread < 0) {
            return !errno_is_fatal(errno);

        } else {
            if (sfd_get_cmd(buf) != PROT_CMD_CANCEL &&
                (nfds < 1 || nfds > PROT_MAXFDS)) {
                sfd_log(LOG_ERR,
                        "Received unexpected number of file descriptors (%lu)"
                        " from client; ignoring request\n",
                        nfds);

            } else if (uid != srv->uid) {
                sfd_log(LOG_ERR, "Invalid UID: expected %d; got %d\n",
                        srv->uid, uid);
                send_xfer_err(recvd_fds[0], EACCES);
                close_fds(recvd_fds, nfds);

            } else {
                if (!process_request(srv, buf, (size_t)nread, pid, recvd_fds))
                    close_fds(recvd_fds, nfds);
            }
        }
    }

    return true;
}

static void close_fds(int* fds, const size_t nfds)
{
    if (nfds > 0) {
        close(fds[0]);
        if (nfds == 2)
            close(fds[1]);
    }
}

static struct resrc_timer* add_open_file(struct server* srv,
                                         const struct prot_request* req,
                                         pid_t client_pid, int stat_fd,
                                         struct fio_stat* info);

static struct resrc_xfer* get_open_file(struct server* srv,
                                        const pid_t client_pid,
                                        const size_t txnid);

static bool register_xfer(struct server* srv, struct resrc_xfer* xfer);

static bool deregister_xfer(struct server* srv, struct resrc_xfer* xfer);

#define MALFORMED_REQ_MSG "Received malformed request\n"
#define INVALID_CMD_MSG "Received invalid command ID (%d) in request\n"

static bool process_request(struct server* srv,
                            const void* buf, const size_t size,
                            const pid_t client_pid, const int* fds)
{
    if (sfd_get_stat(buf) != SFD_STAT_OK) {
        sfd_log(LOG_NOTICE, "Received error status (%x) in request\n",
                sfd_get_stat(buf));
        return false;
    }

    if (!PROT_IS_REQUEST(sfd_get_cmd(buf))) {
        sfd_log(LOG_NOTICE, INVALID_CMD_MSG, sfd_get_cmd(buf));
        return false;
    }

    const int cmd_id = sfd_get_cmd(buf);

    switch ((const enum prot_cmd_req)cmd_id) {
    case PROT_CMD_FILE_OPEN: {
        struct prot_request pdu;
        if (!prot_unmarshal_request(&pdu, buf, size)) {
            sfd_log(LOG_NOTICE, MALFORMED_REQ_MSG);
            /* TODO: send NACK */
            return false;
        }

        struct fio_stat finfo;
        const struct resrc_timer* const timer = add_open_file(srv,
                                                              &pdu,
                                                              client_pid,
                                                              fds[0],
                                                              &finfo);

        if (!timer) {
            send_req_err(fds[0], errno);
            return false;
        }

        send_file_info(fds[0], timer->txnid, &finfo);

    } break;

    case PROT_CMD_SEND_OPEN: {
        struct prot_send_open pdu;
        if (!prot_unmarshal_send_open(&pdu, buf)) {
            sfd_log(LOG_NOTICE, MALFORMED_REQ_MSG);
            return false;
        }

        struct resrc_xfer* const xfer = get_open_file(srv,
                                                      client_pid,
                                                      pdu.txnid);
        if (!xfer || xfer->defer == CANCEL) {
            close(fds[0]);
            return false;
        }

        xfer->cmd = PROT_CMD_SEND;
        xfer->dest_fd = fds[0];

        if (!register_xfer(srv, xfer)) {
            send_xfer_err(xfer->stat_fd, errno);
            delete_unregistered_xfer(srv, xfer);
            return false;
        }

    } break;

    case PROT_CMD_CANCEL: {
        struct prot_cancel pdu;
        if (!prot_unmarshal_cancel(&pdu, buf)) {
            sfd_log(LOG_NOTICE, MALFORMED_REQ_MSG);
            return false;
        }

        struct resrc_xfer* const xfer = get_open_file(srv,
                                                      client_pid,
                                                      pdu.txnid);
        if (!xfer)
            return false;

        defer_xfer(srv, xfer, CANCEL);

    } break;

    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        struct prot_request pdu;
        if (!prot_unmarshal_request(&pdu, buf, size)) {
            sfd_log(LOG_NOTICE, MALFORMED_REQ_MSG);
            return false;
        }

        struct fio_stat finfo;
        struct resrc_xfer* const xfer =
            add_xfer(srv,
                     &pdu,
                     client_pid,
                     fds[0], (pdu.cmd == PROT_CMD_SEND ? fds[1] : fds[0]),
                     &finfo);

        if (!xfer) {
            send_req_err(fds[0], errno);
            return false;
        }

        if (!register_xfer(srv, xfer)) {
            send_req_err(xfer->stat_fd, errno);
            delete_unregistered_xfer(srv, xfer);
            return false;
        }

        send_file_info(xfer->stat_fd, xfer->txnid, &finfo);

    } break;

    default:
        sfd_log(LOG_NOTICE, INVALID_CMD_MSG, sfd_get_cmd(buf));
        return false;
    }

    return true;
}

static struct resrc_xfer* get_open_file(struct server* srv,
                                        const pid_t client_pid,
                                        const size_t txnid)
{
    struct resrc_xfer* const xfer = xfer_table_find(srv->xfers, txnid);
    if (!xfer) {
        /* Timer probably expired; can't send any errors because the status
           channel would've been closed when the timer expired */
        return NULL;
    }

    /* If the transfer's client PID is US_INVALID_PID it could not be
       determined. This would be the case on FreeBSD, on which the recommended
       way of transferring process credentials (see recvmsg(2) on FreeBSD) does
       not transfer the PID. */
    if (xfer->client_pid != US_INVALID_PID && xfer->client_pid != client_pid) {
        /* Client trying to send a file or cancel a transfer it did not open or
           initiate itself */
        sfd_log(LOG_ALERT,
                "Client with PID %d tried to access transaction with"
                " mismatching PID %d (txnid %lu)\n",
                client_pid, xfer->client_pid, xfer->txnid);
        return NULL;
    }

    return xfer;
}

static bool register_xfer(struct server* srv, struct resrc_xfer* xfer)
{
    return syspoll_register(srv->poller,
                            (struct syspoll_resrc*)xfer,
                            SYSPOLL_WRITE);
}

static bool deregister_xfer(struct server* srv, struct resrc_xfer* xfer)
{
    return syspoll_deregister(srv->poller, xfer->dest_fd);
}

static bool has_stat_channel(const struct resrc_xfer* x);

/**
   Sends a terminal response to the client.
*/
static void send_terminal_resp(struct server* srv,
                               struct resrc_xfer* x,
                               const void* pdu, const size_t size);

static bool transfer_file(struct server* srv, struct resrc_xfer* xfer)
{
    switch (xfer->cmd) {
    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        size_t total_nwritten = 0;

        for (;;) {
            const size_t write_size =
                SFD_MIN(xfer->file.blksize,
                    SFD_MIN(xfer->nbytes_left,
                        pipe_capacity() - total_nwritten));

            assert (write_size > 0);

            const ssize_t nwritten = (xfer->cmd == PROT_CMD_READ ?
                                      file_splice(xfer->file.fd,
                                                  xfer->dest_fd,
                                                  xfer->fio_ctx,
                                                  write_size) :
                                      file_sendfile(xfer->file.fd,
                                                    xfer->dest_fd,
                                                    xfer->fio_ctx,
                                                    write_size));

            if (nwritten == -1) {
                if (errno_is_fatal(errno)) {
                    if (!has_stat_channel(xfer))
                        return false;

                    struct prot_hdr pdu = {
                        .cmd = SFD_XFER_STAT,
                        .stat = (uint8_t)errno
                    };

                    send_terminal_resp(srv, xfer, &pdu, sizeof(pdu));

                    return false;
                }

            } else {
                /* (write_size > 0) ==> (xfer->nbytes_left > 0) ==> EOF could
                   not have been seen by the read */
                assert (nwritten > 0);

                xfer->nbytes_left -= (size_t)nwritten;
                total_nwritten += (size_t)nwritten;
            }

            assert (nwritten > 0 || (nwritten == -1 && !errno_is_fatal(errno)));

            if (xfer->nbytes_left == 0) {
                if (has_stat_channel(xfer)) {
                    /* Terminal notification; delivery is critical */
                    struct sfd_xfer_stat pdu;
                    prot_marshal_xfer_stat(&pdu, PROT_XFER_COMPLETE);
                    send_terminal_resp(srv, xfer, &pdu, sizeof(pdu));

                    return false;
                }

                return false;

            } else if (nwritten == -1) {
                /* Nonterminal notification; delivery not critical */
                if (has_stat_channel(xfer)) {
                    if (!send_xfer_stat(xfer->stat_fd, total_nwritten) &&
                        errno_is_fatal(errno)) {
                        return false;
                    }
                }
            }

            if (nwritten == -1) {
                xfer->defer = NONE;
                return true;
            }

            if (total_nwritten >= pipe_capacity()) {
                if (xfer->defer == NONE)
                    defer_xfer(srv, xfer, READY);
                return true;
            }
        }
    }

    default:
        sfd_log(LOG_NOTICE, "Invalid state for command ID %d\n", xfer->cmd);
        break;
    }

    return false;
}

static bool has_stat_channel(const struct resrc_xfer* x)
{
    assert ((x->stat_fd == x->dest_fd) || x->cmd == PROT_CMD_SEND);
    return (x->stat_fd != x->dest_fd);
}

static struct resrc_resp* new_resrc_resp(int fd,
                                         const void* pdu,
                                         size_t pdu_size);

static void send_terminal_resp(struct server* srv,
                               struct resrc_xfer* x,
                               const void* pdu, const size_t pdu_size)
{
    if (send_pdu(x->stat_fd, pdu, pdu_size) || errno_is_fatal(errno))
        return;

    /* Temporary send error--retry it later.

       Any failure past this point is a failure in the retry mechanism, and
       therefore the client will never see the response. There is no remedy, but
       at least log it loudly. (The client will be aware that *something* has
       happened, because all the file descriptors will have been closed.)
    */

    /* Dupe stat fd because it will be closed when the transfer is deleted */
    const int stat_fd = dup(x->stat_fd);
    if (stat_fd == -1) {
        sfd_log(LOG_EMERG,
                "Unable to dup(2) status fd; aborting send"
                " of terminal response message");
        return;
    }

    struct resrc_resp* const resp = new_resrc_resp(stat_fd, pdu, pdu_size);
    if (!resp) {
        sfd_log(LOG_EMERG,
                "Couldn't allocate memory for response retry [%m]\n");
        close(stat_fd);
    }

    if (!syspoll_register(srv->poller,
                          (struct syspoll_resrc*)resp,
                          SYSPOLL_WRITE)) {
        sfd_log(LOG_EMERG, "Unable to register transfer's stat fd [%m]\n");
        free(resp);
        close(stat_fd);
    }
}

static struct resrc_resp* new_resrc_resp(int fd,
                                         const void* pdu,
                                         size_t pdu_size)
{
    assert (pdu_size <= sizeof(struct sfd_xfer_stat));

    struct resrc_resp* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    *this = (struct resrc_resp) {
        .stat_fd = fd,
        .tag = PENDING_RESP_TAG,
        .pdu_size = pdu_size,
    };

    memcpy(&this->pdu, pdu, pdu_size);

    return this;
}

/* --------------- (Uninteresting) Internal implementations ------------- */

static struct server* srv_new(const long open_file_timeout_ms,
                              const int reqfd,
                              const int maxfds)
{
    assert (maxfds > 0);

    struct server* const this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    *this = (struct server) {
        .poller = syspoll_new(maxfds),
        .xfers = xfer_table_new(resrc_xfer_txnid, (size_t)maxfds),
        .xfer_timers = xfer_table_new(resrc_timer_txnid, (size_t)maxfds),
        .deferred_xfers = malloc(sizeof(struct resrc_xfer) * (size_t)maxfds),
        .ndeferred_xfers = 0,
        .open_file_timeout_ms = (unsigned)open_file_timeout_ms,
        .reqfd = reqfd,
        .next_txnid = 1,
        .uid = geteuid()
    };

    if (!this->poller ||
        !this->xfers ||
        !this->xfer_timers ||
        !this->deferred_xfers) {
        PRESERVE_ERRNO(srv_delete(this));
        return NULL;
    }

    return this;
}

static void srv_delete(struct server* this)
{
    syspoll_delete(this->poller);

    close(this->reqfd);

    xfer_table_delete(this->xfers, delete_xfer_and_close_all_fds);
    xfer_table_delete(this->xfer_timers, timer_delete);

    /* Deferred xfers were also in this->xfers (the running transfer table) */
    free(this->deferred_xfers);

    free(this);
}

static bool errno_is_fatal(const int err)
{
    switch (err) {
    case EWOULDBLOCK:
#if (EWOULDBLOCK != EAGAIN)
    case EAGAIN:
#endif
    case ENFILE:
    case ENOBUFS:
    case ENOLCK:
    case ENOSPC:
        return false;
    default:
        return true;
    }
}

static struct resrc_xfer* add_xfer(struct server* srv,
                                   const struct prot_request* req,
                                   const pid_t client_pid,
                                   const int stat_fd, const int dest_fd,
                                   struct fio_stat* finfo)
{
    assert (req->cmd == PROT_CMD_READ ||
            req->cmd == PROT_CMD_SEND ||
            req->cmd == PROT_CMD_FILE_OPEN);

    if (srv->xfers->size == srv->xfers->capacity) {
        sfd_log(LOG_CRIT, "Transfer table is full (%lu/%lu items)\n",
                srv->xfers->size, srv->xfers->capacity);
        errno = EMFILE;
        return NULL;
    }

    const int fd = file_open_read(req->filename, req->offset, req->len, finfo);
    if (fd == -1)
        return NULL;

    if (finfo->size == 0) {
        close(fd);
        errno = EINVAL;
        return NULL;
    }

    if (((size_t)req->offset + req->len) > finfo->size) {
        close(fd);
        errno = ERANGE;
        return NULL;
    }

    const size_t xfer_nbytes = (req->len > 0 ?
                                req->len :
                                (finfo->size - (size_t)req->offset));

    finfo->size = xfer_nbytes;

    struct resrc_xfer_file file = {
        .size = xfer_nbytes,
        .fd = fd,
        .blksize = finfo->blksize
    };

    struct resrc_xfer* const xfer = xfer_new(req->cmd,
                                             &file,
                                             xfer_nbytes,
                                             client_pid,
                                             stat_fd, dest_fd,
                                             srv->next_txnid);
    if (!xfer) {
        PRESERVE_ERRNO(close(fd));
        return NULL;
    }

    srv->next_txnid++;

    if (!xfer_table_insert(srv->xfers, xfer)) {
        sfd_log(LOG_CRIT,
                "Couldn't insert item into transfer table"
                " (slot for txnid %lu probably already taken)\n",
                xfer->txnid);
        PRESERVE_ERRNO(delete_xfer_and_close_file_fd(xfer));
        return NULL;
    }

    return xfer;
}

static struct resrc_timer* add_open_file(struct server* srv,
                                         const struct prot_request* req,
                                         const pid_t client_pid,
                                         const int stat_fd,
                                         struct fio_stat* finfo)
{
    struct resrc_xfer* const xfer = add_xfer(srv,
                                             req,
                                             client_pid,
                                             stat_fd, -1,
                                             finfo);
    if (!xfer)
        return NULL;

    struct resrc_timer* timer = malloc(sizeof(*timer));
    if (!timer)
        goto fail1;

    *timer = (struct resrc_timer) {
        .ident = -1,
        .tag = TIMER_RESRC_TAG,
        .txnid = xfer->txnid,
        .xfer_addr = xfer
    };

    if (!xfer_table_insert(srv->xfer_timers, timer))
        goto fail2;

    if (!syspoll_timer(srv->poller,
                       (struct syspoll_resrc*)timer,
                       srv->open_file_timeout_ms)) {
        goto fail3;
    }

    return timer;

 fail3:
    PRESERVE_ERRNO(xfer_table_erase(srv->xfer_timers, timer->txnid));
 fail2:
    PRESERVE_ERRNO(timer_delete(timer));
 fail1:
    PRESERVE_ERRNO(delete_unregistered_xfer(srv, xfer));

    return NULL;
}

static void delete_xfer_and_close_file_fd(void* p)
{
    if (p) {
        struct resrc_xfer* const this = p;
        assert (this->tag == XFER_RESRC_TAG);

        close(this->file.fd);
        xfer_delete(this);
    }
}

static void delete_xfer_and_close_all_fds(void* p)
{
    if (p) {
        struct resrc_xfer* const this = p;
        assert (this->tag == XFER_RESRC_TAG);

        close(this->stat_fd);
        if (this->dest_fd != this->stat_fd && this->dest_fd >= 0)
            close(this->dest_fd);

        delete_xfer_and_close_file_fd(p);
    }
}

static void delete_unregistered_xfer(struct server* srv, struct resrc_xfer* x)
{
    xfer_table_erase(srv->xfers, x->txnid);
    delete_xfer_and_close_file_fd(x);
}

static void delete_registered_xfer(struct server* srv, struct resrc_xfer* xfer)
{
    xfer_table_erase(srv->xfers, xfer->txnid);

    /* The client and server processes share the dest fd's file table entry (it
       was sent over a UNIX socket), so closing it here will not cause it to be
       automatically removed from the system poller if the client process has
       not yet closed *its* copy, and therefore it may be returned again by the
       next call to syspoll_wait(), *after* the memory associated with it has
       been freed here, unless it is removed explicitly.
    */
    deregister_xfer(srv, xfer);

    delete_xfer_and_close_all_fds(xfer);
}

static void defer_xfer(struct server* const srv,
                       struct resrc_xfer* const xfer,
                       enum deferral how)
{
    switch (how) {
    case CANCEL:
        assert (xfer->defer == READY || srv->ndeferred_xfers < srv->xfers->size);

        if (xfer->defer == NONE) {    /* Not in the list yet */
            srv->deferred_xfers[srv->ndeferred_xfers] = xfer;
            srv->ndeferred_xfers++;
        }

        xfer->defer = CANCEL;
        break;

    case READY:
        assert (xfer->defer != CANCEL);
        assert (srv->ndeferred_xfers < srv->xfers->size);

        srv->deferred_xfers[srv->ndeferred_xfers] = xfer;
        srv->ndeferred_xfers++;

        xfer->defer = READY;
        break;

    default:
        assert (false);
        break;
    }
}

static size_t undefer_xfer(struct server* const srv, const size_t i)
{
    srv->deferred_xfers[i]->defer = NONE;

    if (i < srv->ndeferred_xfers - 1) {
        srv->deferred_xfers[i] =
            srv->deferred_xfers[srv->ndeferred_xfers - 1];
    }

    srv->ndeferred_xfers--;

    return i;
}
