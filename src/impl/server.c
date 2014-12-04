/*
  Copyright (c) 2014, Francois Kritzinger
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

#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errors.h"
#include "file_io.h"
#include "protocol_server.h"
#include "../responses.h"
#include "server.h"
#include "server_xfer_table.h"
#include "syspoll.h"
#include "unix_socket_server.h"
#include "util.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
   Information about a file, as it is on disk.

   These values do not change through the course of a transfer.
*/
struct xfer_file {
    /** File size on disk */
    size_t size;
    /** File descriptor */
    int fd;
    /** Optimal block size for I/O */
    unsigned blksize;
};

/*
  Resources are event sources such as the socket on which client requests are
  received (the server's 'listen' socket), pipes to which transfer status and/or
  file data is written, timers for open files, and which are registered with the
  poller for event notification.

  The poller requires all resources to have their registered file descriptors as
  the first field (they need to be type-punnable to struct syspoll_resrc).
*/

/*
  Resource type tags.

  All of these resource types will all be returned by the poller, so there needs
  to be a way to tell them apart from each other.
*/
enum {
    /* Identifies a resource as a file transfer */
    XFER_RESRC_TAG,
    /** Tag which identifies a resource as a timer. */
    TIMER_RESRC_TAG,
    /** Identifies a response pending delivery */
    PENDING_RESP_TAG
};

/**
   A file transfer resource.

   @note The first few fields must be identical to the other resource structures
   due to the use of type punning.
*/
struct resrc_xfer {
    /** The data channel file descriptor (registered with poller) */
    int dest_fd;
    /* The type tag */
    int tag;
    /** The status channel file descriptor. Hijacked by other resource
        structures for use as a type tag (shameful!). */
    int stat_fd;
    /** The command ID */
    enum prot_cmd_req cmd;
    /** The unique identifier for this transfer */
    size_t txnid;
    /** Static information about the file being transferred, as it is on disk */
    struct xfer_file file;
    /** Context used by data-transfer functions on some platforms; NULL on
        others */
    struct fio_ctx* fio_ctx;
    /** Number of bytes left to transfer */
    size_t nbytes_left;
    /** The client process ID */
    pid_t client_pid;
};

/**
   A response waiting to be delivered.

   Instances are created when the first attempt to send a transfer error or
   completion notification fails temporarily.
*/
struct resrc_resp {
    /* Destination file descriptor (registered with poller) */
    int stat_fd;
    /* The type tag */
    int tag;
    /** The size of the PDU. Error notifications are headers only, but transfer
        completion notifications have a size_t field in the body. */
    size_t pdu_size;
    /** The PDU to be sent */
    struct fiod_xfer_stat pdu;
};

/**
   A timer set on an open file associated with a nascent file transfer.
*/
struct resrc_timer {
    /** Identifies the timer (registered with poller) */
    int ident;
    /** The type tag */
    int tag;
    /** The associated transfer ID */
    size_t txnid;
};

/**
   Server context.
*/
struct server
{
    /** The poller (epoll, kqueue, etc.) */
    struct syspoll* poller;
    /** The table of running file transfers */
    struct xfer_table* xfers;
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

/** Checks whether an @a errno value is fatal or not. */
static bool errno_is_fatal(int err);

/**
   Adds a new transfer.

   Opens the file, allocates memory for the data structure, and inserts it into
   the running transfers table.

   @post Status and data channel file descriptors are still open, regardless of
   wether the call succeeded or not.
*/
static struct resrc_xfer* add_xfer(struct server* ctx,
                                   const struct prot_request* req,
                                   pid_t client_pid,
                                   int stat_fd, int dest_fd,
                                   struct file_info* info);

/** Reverses the effects of add_xfer() */
static void undo_add_xfer(struct server*, struct resrc_xfer*);

/**
   Releases resources allocated to a transfer.
*/
static void delete_xfer(void* p);

/** Sends an error in response to a request to the client (over the status
    channel) */
static bool send_req_err(int fd, int err);

/** Sends an error which occurred during a transfer to the client (over the
    status channel) */
static bool send_xfer_err(int fd, int err);

static void close_channel_fds(struct resrc_xfer*);

static bool srv_construct(struct server* ctx,
                          long open_file_timeout_ms,
                          int reqfd,
                          int maxfds);
static void srv_destruct(struct server* ctx);
static bool process_events(struct server* ctx,
                           int nevents,
                           void* buf, size_t buf_size);
static bool send_xfer_stat(int fd, size_t file_size);

bool srv_run(const int reqfd,
             const int maxfds,
             const long open_file_timeout_ms)
{
    struct server ctx;
    if (!srv_construct(&ctx, open_file_timeout_ms, reqfd, maxfds))
        return false;

    if (!syspoll_register(ctx.poller,
                          (struct syspoll_resrc*)&ctx.reqfd,
                          SYSPOLL_READ)) {
        goto fail;
    }

    void* const recvbuf = calloc(PROT_REQ_MAXSIZE, 1);
    if (!recvbuf)
        goto fail;

    for (;;) {
        const int nready = syspoll_poll(ctx.poller);

        if (nready == -1) {
            if (errno != EINTR && errno_is_fatal(errno)) {
                syslog(LOG_ERR, "Fatal error in syspoll_poll(): [%s]\n",
                       strerror(errno));
                break;
            }
        } else if (!process_events(&ctx, nready, recvbuf, PROT_REQ_MAXSIZE)) {
            break;
        }
    }

    free(recvbuf);
    srv_destruct(&ctx);

    return true;

 fail:
    PRESERVE_ERRNO(srv_destruct(&ctx));
    return false;
}

static bool handle_reqfd(struct server* ctx,
                         int events,
                         void* buf,
                         size_t buf_size);

/** Removes a transfer from all data structures and releases its resources
    (including data and status channel file descriptors) */
static void purge_xfer(struct server* ctx, struct resrc_xfer* xfer);

static bool process_file_op(struct server* ctx, struct resrc_xfer* xfer);

static bool send_pdu(const int fd, const void* pdu, const size_t size);

static bool is_xfer(const void*);

static bool is_timer(const void*);

static bool is_response(const void*);

static void delete_resrc_resp(struct resrc_resp*);

static bool process_events(struct server* ctx,
                           const int nevents,
                           void* buf, const size_t buf_size)
{
    for (int i = 0; i < nevents; i++) {
        struct syspoll_events events = syspoll_get(ctx->poller, i);

        if (*(int*)events.udata == ctx->reqfd) {
            if (events.events & SYSPOLL_ERROR ||
                !handle_reqfd(ctx, events.events, buf, buf_size)) {
                syslog(LOG_ERR,
                       "Fatal error on request socket; shutting down\n");
                return false;
            }

        } else {
            if (events.events & SYSPOLL_TERM)
                return false;

            const bool error_event = (events.events & SYSPOLL_ERROR);

            if (error_event) {
                syslog(LOG_INFO, "Fatal error on resource fd %d\n",
                       *(int*)events.udata);
            }

            if (is_timer(events.udata)) {
                struct resrc_timer* const timer = events.udata;

                if (!error_event) {
                    struct resrc_xfer* const xfer =
                        xfer_table_find(ctx->xfers, timer->txnid);

                    if (xfer &&
                        xfer->cmd != PROT_CMD_SEND &&
                        xfer->cmd != PROT_CMD_READ) {
                        send_xfer_err(xfer->stat_fd, ETIME);
                        purge_xfer(ctx, xfer);
                    }
                }

                close(timer->ident);
                free(timer);

            } else if (is_response(events.udata)) {
                struct resrc_resp* r = (struct resrc_resp*)events.udata;

                if (error_event || send_pdu(r->stat_fd, &r->pdu, r->pdu_size) ||
                    errno_is_fatal(errno)) {
                    delete_resrc_resp(r);
                }

            } else {
                struct resrc_xfer* const xfer = events.udata;

                assert (is_xfer(events.udata));

                if (error_event || !process_file_op(ctx, xfer))
                    PRESERVE_ERRNO(purge_xfer(ctx, xfer));
            }
        }
    }

    return true;
}

static bool is_xfer(const void* p)
{
    return (((const struct resrc_xfer*)p)->tag == XFER_RESRC_TAG);
}

static bool is_timer(const void* p)
{
    return (((const struct resrc_timer*)p)->tag == TIMER_RESRC_TAG);
}

static bool is_response(const void* p)
{
    return (((const struct resrc_resp*)p)->tag == PENDING_RESP_TAG);
}

static void delete_resrc_resp(struct resrc_resp* r)
{
    close(r->stat_fd);
    free(r);
}

static bool process_request(struct server* ctx,
                            const void* buf, const size_t size,
                            pid_t client_pid, const int* fds);

static void close_fds(int* fds, const size_t nfds)
{
    close(fds[0]);
    if (nfds == 2)
        close(fds[1]);
}

static bool handle_reqfd(struct server* ctx,
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

        const ssize_t nread = us_recv(ctx->reqfd,
                                      buf, buf_size,
                                      recvd_fds, &nfds,
                                      &uid, &gid, &pid);

        /* Recv of zero makes no sense on a UDP (connectionless) socket */
        assert (nread != 0);

        if (nread < 0) {
            return !errno_is_fatal(errno);
        } else {
            if (nfds == 0 || nfds > PROT_MAXFDS) {
                syslog(LOG_ERR,
                       "Received unexpected number of"
                       " file descriptors (%d) from client; ignoring request\n",
                       (int)nfds);

            } else if (uid != ctx->uid) {
                send_xfer_err(recvd_fds[0], EACCES);
                close_fds(recvd_fds, nfds);

            } else if (!process_request(ctx,
                                        buf,
                                        (size_t)nread,
                                        pid, recvd_fds)) {
                close_fds(recvd_fds, nfds);
            }
        }
    }

    return true;
}

static struct resrc_timer* add_open_file(struct server* ctx,
                                         const struct prot_request* req,
                                         pid_t client_pid, int stat_fd,
                                         struct file_info* info);

static bool register_xfer(struct server* ctx, struct resrc_xfer* xfer);

static bool send_file_info(int fd, const struct file_info* info);

static bool send_open_file_info(int cli_fd,
                                size_t txnid,
                                const struct file_info* info);

#define MALFORMED_REQ_MSG "Received malformed request\n"
#define INVALID_CMD_MSG "Received invalid command ID (%d) in request\n"

static bool process_request(struct server* ctx,
                            const void* buf, const size_t size,
                            const pid_t client_pid, const int* fds)
{
    if (fiod_get_stat(buf) != FIOD_STAT_OK) {
        syslog(LOG_NOTICE,
               "Received error status (%x) in request\n",
               fiod_get_stat(buf));
        return false;
    }

    if (!PROT_IS_REQUEST(fiod_get_cmd(buf))) {
        syslog(LOG_NOTICE, INVALID_CMD_MSG, fiod_get_cmd(buf));
        return false;
    }

    switch (fiod_get_cmd(buf)) {
    case PROT_CMD_FILE_OPEN: {
        struct prot_request pdu;
        if (!prot_unmarshal_request(&pdu, buf, size)) {
            syslog(LOG_NOTICE, MALFORMED_REQ_MSG);
            /* TODO: send NACK */
            return false;
        }

        struct file_info finfo;
        const struct resrc_timer* const timer = add_open_file(ctx,
                                                              &pdu,
                                                              client_pid,
                                                              fds[0],
                                                              &finfo);

        if (!timer) {
            send_req_err(fds[0], errno);
            return false;
        }

        send_open_file_info(fds[0], timer->txnid, &finfo);
    } break;

    case PROT_CMD_SEND_OPEN: {
        struct prot_send_open pdu;
        if (!prot_unmarshal_send_open(&pdu, buf)) {
            syslog(LOG_NOTICE, MALFORMED_REQ_MSG);
            return false;
        }

        struct resrc_xfer* const xfer = xfer_table_find(ctx->xfers, pdu.txnid);
        if (!xfer) {
            /* Timer probably expired; can't send any errors because the status
               channel would've been closed when the timer expired */
            return false;
        }

        if (xfer->client_pid != client_pid) {
            /* Client is asking to send a file it did not itself open */
            syslog(LOG_ALERT,
                   "Client with PID %d asked to send open file with"
                   " mismatching PID %d (txnid %lu)\n",
                   client_pid, xfer->client_pid, xfer->txnid);
            undo_add_xfer(ctx, xfer);
            return false;
        }

        xfer->cmd = PROT_CMD_SEND;
        xfer->dest_fd = fds[0];

        if (!register_xfer(ctx, xfer)) {
            send_xfer_err(xfer->stat_fd, errno);
            undo_add_xfer(ctx, xfer);
            return false;
        }
    } break;

    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        struct prot_request pdu;
        if (!prot_unmarshal_request(&pdu, buf, size)) {
            syslog(LOG_NOTICE, MALFORMED_REQ_MSG);
            return false;
        }

        struct file_info finfo;
        struct resrc_xfer* const xfer =
            add_xfer(ctx,
                     &pdu,
                     client_pid,
                     fds[0], (pdu.cmd == PROT_CMD_SEND ? fds[1] : fds[0]),
                     &finfo);

        if (!xfer) {
            send_req_err(fds[0], errno);
            return false;
        }

        if (!register_xfer(ctx, xfer)) {
            send_req_err(xfer->stat_fd, errno);
            undo_add_xfer(ctx, xfer);
            return false;
        }

        send_file_info(xfer->stat_fd, &finfo);

    } break;

    default:
        syslog(LOG_NOTICE, INVALID_CMD_MSG, fiod_get_cmd(buf));
        return false;
    }

    return true;
}

static bool register_xfer(struct server* ctx, struct resrc_xfer* xfer)
{
    return syspoll_register(ctx->poller,
                            (struct syspoll_resrc*)xfer,
                            SYSPOLL_WRITE);
}

static bool has_stat_channel(const struct resrc_xfer* x);

/**
   Sends a terminal response to the client.

   @retval @a true The response will be retried later; no cleanup necessary

   @retval @a false The response has been sent, or an unrecoverable error has
   occured in the process, so purge the transfer.
*/
static bool send_term_resp(struct server* ctx,
                           struct resrc_xfer* x,
                           const void* pdu, const size_t size);

static bool process_file_op(struct server* ctx, struct resrc_xfer* xfer)
{
    switch (xfer->cmd) {
    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        size_t ntotal_written = 0;

        for (;;) {
            const size_t writemax = MIN_(xfer->file.blksize, xfer->nbytes_left);

            const ssize_t nwritten = (xfer->cmd == PROT_CMD_READ ?
                                      file_splice(xfer->file.fd,
                                                  xfer->dest_fd,
                                                  xfer->fio_ctx,
                                                  writemax) :
                                      file_sendfile(xfer->file.fd,
                                                    xfer->dest_fd,
                                                    xfer->fio_ctx,
                                                    writemax));

            if (nwritten == -1) {
                const int write_errno = errno;

                if (!errno_is_fatal(write_errno))
                    return true;

                if (!has_stat_channel(xfer))
                    return false;

                struct prot_hdr pdu = {
                    .cmd = FIOD_XFER_STAT,
                    .stat = (uint8_t)write_errno
                };
                return send_term_resp(ctx, xfer, &pdu, sizeof(pdu));

            } else if (nwritten == 0) {
                /* FIXME Not sure how to deal with this properly */
                return true;

            } else {
                assert (nwritten > 0);

                xfer->nbytes_left -= (size_t)nwritten;
                ntotal_written += (size_t)nwritten;
            }

            /* Sent as much as possible for the time being */
            if (has_stat_channel(xfer)) {
                if (xfer->nbytes_left == 0) {
                    /* Terminal notification; delivery is critical */
                    struct fiod_xfer_stat pdu;
                    prot_marshal_xfer_stat(&pdu, PROT_XFER_COMPLETE);
                    return send_term_resp(ctx, xfer, &pdu, sizeof(pdu));

                } else if (nwritten == -1 && ntotal_written > 0) {
                    /* Nonterminal notification; delivery not critical */
                    if (!send_xfer_stat(xfer->stat_fd, ntotal_written) &&
                        errno_is_fatal(errno)) {
                        return false;
                    }
                }
            } else if (xfer->nbytes_left == 0) {
                /* Indicate that the caller should purge the transfer. FIXME:
                   this is rather counter-intuitive. */
                return false;
            }

            return true;
        }
    }

    case PROT_CMD_SEND_OPEN:
    case PROT_CMD_FILE_OPEN:
        syslog(LOG_NOTICE, "Invalid state for command ID %d\n", xfer->cmd);
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

static bool send_term_resp(struct server* ctx,
                           struct resrc_xfer* x,
                           const void* pdu, const size_t pdu_size)
{
    /*
      return false -> x is purged;
      return true -> nothing is done
    */

    if (send_pdu(x->stat_fd, pdu, pdu_size) || errno_is_fatal(errno))
        return false;

    /* Temporary send error--retry it later.

       Any failure past this point is a failure in the retry mechanism, and
       therefore the client will never see the response. There is no remedy, but
       at least log it loudly. (The client will be aware that *something* has
       happened, because all the file descriptors will be closed.)
    */

    struct resrc_resp* resp = new_resrc_resp(x->stat_fd, pdu, pdu_size);
    if (!resp) {
        syslog(LOG_EMERG, "Couldn't allocate memory for response retry [%s]\n",
               strerror(errno));
        return false;
    }

    if (!syspoll_register(ctx->poller,
                          (struct syspoll_resrc*)resp,
                          SYSPOLL_WRITE)) {
        syslog(LOG_EMERG, "Unable to register transfer's stat fd [%s]\n",
               strerror(errno));
        free(resp);
        return false;
    }

    close(x->dest_fd);
    syspoll_deregister(ctx->poller, x->dest_fd);
    xfer_table_erase(ctx->xfers, x->txnid);

    return true;
}

static struct resrc_resp* new_resrc_resp(int fd,
                                         const void* pdu,
                                         size_t pdu_size) {
    assert (pdu_size <= sizeof(struct fiod_xfer_stat));
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

static size_t get_txnid(void* p)
{
    return ((struct resrc_xfer*)p)->txnid;
}

static bool srv_construct(struct server* ctx,
                          const long open_file_timeout_ms,
                          const int reqfd,
                          const int maxfds)
{
    *ctx = (struct server) {
        .poller = syspoll_new(maxfds),
        .xfers = xfer_table_new(get_txnid, (size_t)maxfds),
        .open_file_timeout_ms = (unsigned)open_file_timeout_ms,
        .reqfd = reqfd,
        .next_txnid = 1,
        .uid = geteuid()
    };

    if (!ctx->poller || !ctx->xfers) {
        PRESERVE_ERRNO(srv_destruct(ctx));
        return false;
    }

    return true;
}

static void delete_xfer_and_close_channel_fds(void* p)
{
    close_channel_fds((struct resrc_xfer*)p);
    delete_xfer(p);
}

static void srv_destruct(struct server* ctx)
{
    syspoll_delete(ctx->poller);

    close(ctx->reqfd);

    xfer_table_delete(ctx->xfers, delete_xfer_and_close_channel_fds);
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

static struct resrc_xfer* new_xfer(struct server* ctx,
                                   const struct xfer_file* file,
                                   const struct prot_request* req,
                                   pid_t client_pid,
                                   int stat_fd,
                                   int dest_fd);

static struct resrc_xfer* add_xfer(struct server* ctx,
                                   const struct prot_request* req,
                                   const pid_t client_pid,
                                   const int stat_fd, const int dest_fd,
                                   struct file_info* finfo)
{
    assert (req->cmd == PROT_CMD_READ ||
            req->cmd == PROT_CMD_SEND ||
            req->cmd == PROT_CMD_FILE_OPEN);

    if (ctx->xfers->size == ctx->xfers->capacity) {
        syslog(LOG_CRIT, "Transfer table is full (%lu/%lu items)\n",
               ctx->xfers->size, ctx->xfers->capacity);
        return false;
    }

    const int fd = file_open_read(req->filename, req->offset, req->len, finfo);
    if (fd == -1)
        return NULL;

    struct xfer_file file = {
        .size = finfo->size,
        .fd = fd,
        .blksize = finfo->blksize
    };

    struct resrc_xfer* const xfer = new_xfer(ctx,
                                             &file,
                                             req,
                                             client_pid,
                                             stat_fd, dest_fd);
    if (!xfer) {
        PRESERVE_ERRNO(close(fd));
        return NULL;
    }

    if (!xfer_table_insert(ctx->xfers, xfer)) {
        syslog(LOG_CRIT,
               "Couldn't insert item into transfer table"
               " (slot for txnid %lu probably already taken)\n",
               xfer->txnid);
        PRESERVE_ERRNO(delete_xfer(xfer));
        return NULL;
    }

    finfo->size = xfer->nbytes_left;

    return xfer;
}

static struct resrc_xfer* new_xfer(struct server* ctx,
                                   const struct xfer_file* file,
                                   const struct prot_request* req,
                                   const pid_t client_pid,
                                   const int stat_fd,
                                   const int dest_fd)
{
    if (((size_t)req->offset + req->len) > file->size) {
        /* Requested range is invalid */
        errno = ERANGE;
        return NULL;
    }

    struct resrc_xfer* xfer = malloc(sizeof(*xfer));
    if (!xfer)
        return NULL;

    *xfer = (struct resrc_xfer) {
        .dest_fd = dest_fd,
        .tag = XFER_RESRC_TAG,
        .stat_fd = stat_fd,
        .txnid = ctx->next_txnid++,
        .file = *file,
        .fio_ctx = fio_ctx_new(file->blksize),
        .nbytes_left = (req->len > 0 ?
                        req->len :
                        (file->size - (size_t)req->offset)),
        .cmd = req->cmd,
        .client_pid = client_pid
    };

    if (!fio_ctx_valid(xfer->fio_ctx)) {
        free(xfer);
        return NULL;
    }

    return xfer;
}

static void undo_add_xfer(struct server* ctx, struct resrc_xfer* x)
{
    xfer_table_erase(ctx->xfers, x->txnid);
    delete_xfer(x);
}

static struct resrc_timer* add_open_file(struct server* ctx,
                                         const struct prot_request* req,
                                         const pid_t client_pid,
                                         const int stat_fd,
                                         struct file_info* finfo)
{
    struct resrc_xfer* const xfer = add_xfer(ctx,
                                             req,
                                             client_pid,
                                             stat_fd, -1,
                                             finfo);
    if (!xfer)
        return false;

    struct resrc_timer* timer = malloc(sizeof(*timer));
    if (!timer)
        goto fail1;

    *timer = (struct resrc_timer) {
        .ident = -1,
        .tag = TIMER_RESRC_TAG,
        .txnid = xfer->txnid
    };

    if (!syspoll_timer(ctx->poller,
                       (struct syspoll_resrc*)timer,
                       ctx->open_file_timeout_ms)) {
        goto fail2;
    }

    return timer;

 fail2:
    free(timer);
 fail1:
    PRESERVE_ERRNO(undo_add_xfer(ctx, xfer));

    return NULL;
}

static void delete_xfer(void* p)
{
    struct resrc_xfer* const x = p;

    close(x->file.fd);
    fio_ctx_delete(x->fio_ctx);

    free(x);
}

static void close_channel_fds(struct resrc_xfer* x)
{
    close(x->stat_fd);
    if (x->dest_fd != x->stat_fd && x->dest_fd >= 0)
        close(x->dest_fd);
}

static void purge_xfer(struct server* ctx, struct resrc_xfer* xfer)
{
    xfer_table_erase(ctx->xfers, xfer->txnid);

    /* The client and server processes share the dest fd's file table entry (it
       was sent over a UNIX socket), so closing it here will not cause it to be
       automatically removed from the system poller if the client process has
       not yet closed *its* copy, and therefore it may be returned again by the
       next call to syspoll_poll(), *after* the memory associated with it has
       been freed here, unless it is removed explicitly.
    */
    syspoll_deregister(ctx->poller, xfer->dest_fd);

    close_channel_fds(xfer);
    delete_xfer(xfer);
}

static bool send_pdu(const int fd, const void* pdu, const size_t size)
{
    const ssize_t n = write(fd, pdu, size);
    assert (n == -1 || (size_t)n == size);
    return ((size_t)n == size);
}

static bool send_file_info(int fd, const struct file_info* info)
{
    struct fiod_file_info pdu;
    prot_marshal_file_info(&pdu,
                           info->size, info->atime, info->mtime, info->ctime);
    return send_pdu(fd, &pdu, sizeof(pdu));
}

static bool send_open_file_info(int cli_fd,
                                const size_t txnid,
                                const struct file_info* info)
{
    struct fiod_open_file_info pdu;
    prot_marshal_open_file_info(&pdu,
                                info->size,
                                info->atime, info->mtime, info->ctime,
                                txnid);
    return send_pdu(cli_fd, &pdu, sizeof(pdu));
}

static bool send_xfer_stat(const int fd, const size_t file_size)
{
    struct fiod_xfer_stat pdu;
    prot_marshal_xfer_stat(&pdu, file_size);
    return send_pdu(fd, &pdu, sizeof(pdu));
}

static bool send_req_err(const int fd, const int stat)
{
    const struct prot_hdr pdu = {
        .cmd = FIOD_FILE_INFO,
        .stat = (uint8_t)stat
    };
    return send_pdu(fd, &pdu, sizeof(pdu));
}


static bool send_xfer_err(const int fd, const int stat)
{
    const struct prot_hdr pdu = {
        .cmd = FIOD_XFER_STAT,
        .stat = (uint8_t)stat
    };
    return send_pdu(fd, &pdu, sizeof(pdu));
}
