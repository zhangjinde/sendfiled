#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errors.h"
#include "file_io.h"
#include "protocol_server.h"
#include "server.h"
#include "syspoll.h"
#include "unix_socket_server.h"
#include "util.h"
#include "xfer_table.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
   Information about a file being transferred.
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
  received, pipes to which transfer status and/or file data is written, and
  open file timers.

  The registered file descriptor MUST be the first field in the structure.
*/

struct resrc_xfer {
    /** The data channel file descriptor */
    int dest_fd;
    /** The status channel file descriptor */
    int stat_fd;
    /** The unique identifier for this transfer */
    size_t id;
    /** Information about the file being transferred */
    struct xfer_file file;
    /** Context used by data-transfer functions on some platforms */
    struct fio_ctx* fio_ctx;
    /** Number of bytes left to transfer */
    size_t nbytes_left;
    /* Command ID */
    enum prot_cmd_req cmd;
};

/**
   A timer set on an open file associated with a nascent file transfer.
*/
struct resrc_timer {
    /** Identifies the timer with the poller */
    int ident;
    /** A value of TIMER_TAG identifies this resource as a timer */
    int tag;
    /** The associated transfer ID */
    size_t txnid;
};

struct err_retry {
    int stat_fd;
    int tag;
    size_t txnid;
    int err_code;
};

struct term_notif_retry {
    int stat_fd;
    int tag;
    size_t txnid;
};

enum {
    /** Tag which identifies a resource as a timer */
    TIMER_TAG = INT_MIN,

    /**
       Tag which identifies a transfer as being in its 'retrying send of
       terminal status notification' state.
    */
    RETRYING_TERM_NOTIF_DELIV_TAG,
    RETRYING_ERR_DELIV_TAG
};

/**
   Convenience macro for initialising a timer resource.

   @param ident_ The timer identifier. E.g., the value returned by @a timerfd(2)
   on Linux, or a user-supplied value in the case of @a kqueue.

   @param txnid_ The unique transfer/transaction identifier
*/
#define TIMER(ident_, txnid_)                                       \
    (struct resrc_timer)                                            \
    {.ident = ident_, .tag = TIMER_TAG, .txnid = txnid_ }

/**
   Checks whether a resource is a timer.
*/
static bool is_timer(const void* p)
{
    return (((const struct resrc_timer*)p)->tag == TIMER_TAG);
}

/**
   Server context.
*/
struct srv_ctx
{
    /** The poller (epoll, kqueue, etc.) */
    struct syspoll* poller;
    /** The table of running file transfers */
    struct xfer_table* xfers;
    /** The next transfer ID to be assigned */
    size_t next_id;
    /** The file descriptor upon which client requests are received */
    int reqfd;
    /** The number of milliseconds after which open files are closed */
    unsigned open_file_timeout_ms;
    /* The user ID */
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
static struct resrc_xfer* add_xfer(struct srv_ctx* ctx,
                                   const char* filename,
                                   enum prot_cmd_req cmd,
                                   loff_t offset,
                                   size_t len,
                                   int stat_fd, int dest_fd,
                                   struct file_info* info);

/** Reverses the effects of add_xfer() */
static void undo_add_xfer(struct srv_ctx*, struct resrc_xfer*);

/** Sends an error to the client (over the status channel) */
static bool send_err(int fd, int err);

/**
   Releases resources allocated to a transfer.
*/
static void delete_xfer(void* p);

static void close_channel_fds(struct resrc_xfer*);

static bool srv_ctx_construct(struct srv_ctx* ctx,
                              long open_file_timeout_ms,
                              int reqfd,
                              int maxfds);
static void srv_ctx_destruct(struct srv_ctx* ctx);
static bool process_events(struct srv_ctx* ctx,
                           int nevents,
                           void* buf, size_t buf_size);
static bool send_xfer_stat(int fd, size_t file_size);

bool srv_run(const int reqfd,
             const int maxfds,
             const long open_file_timeout_ms)
{
    struct srv_ctx ctx;
    if (!srv_ctx_construct(&ctx, open_file_timeout_ms, reqfd, maxfds))
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
                syslog(LOG_ERR, "Fatal error in syspoll_poll(): [%m]\n");
                break;
            }
        } else if (!process_events(&ctx, nready, recvbuf, PROT_REQ_MAXSIZE)) {
            break;
        }
    }

    free(recvbuf);
    srv_ctx_destruct(&ctx);

    return true;

 fail:
    PRESERVE_ERRNO(srv_ctx_destruct(&ctx));
    return false;
}

static bool handle_reqfd(struct srv_ctx* ctx,
                         int events,
                         void* buf,
                         size_t buf_size);

/** Removes a transfer from all data structures and releases its resources
    (including data and status channel file descriptors) */
static void purge_xfer(struct srv_ctx* ctx, struct resrc_xfer* xfer);
static bool process_file_op(struct srv_ctx* ctx, struct resrc_xfer* xfer);

static bool process_events(struct srv_ctx* ctx,
                           const int nevents,
                           void* buf, const size_t buf_size)
{
    for (int i = 0; i < nevents; i++) {
        struct syspoll_events events = syspoll_get(ctx->poller, i);

        if (*(int*)events.udata == ctx->reqfd) {
            if (events.events & SYSPOLL_ERROR ||
                !handle_reqfd(ctx, events.events, buf, buf_size)) {
                syslog(LOG_ERR, "Fatal error on request socket\n");
                return false;
            }

        } else {
            if (events.events & SYSPOLL_TERM) {
                return false;

            } else if (is_timer(events.udata)) {
                struct resrc_timer* const timer = events.udata;
                struct resrc_xfer* const xfer = xfer_table_find(ctx->xfers,
                                                                timer->txnid);

                if (xfer &&
                    xfer->cmd != PROT_CMD_SEND &&
                    xfer->cmd != PROT_CMD_READ) {
                    purge_xfer(ctx, xfer);
                }

                close(timer->ident);
                free(timer);

            } else {
                struct resrc_xfer* const xfer = events.udata;

                if (events.events & SYSPOLL_ERROR) {
                    syslog(LOG_INFO,
                           "Fatal error on transfer {txnid: %lu; statfd: %d}\n",
                           xfer->id, xfer->stat_fd);
                    PRESERVE_ERRNO(purge_xfer(ctx, xfer));

                } else if (!process_file_op(ctx, xfer) || xfer->nbytes_left == 0) {
                    PRESERVE_ERRNO(purge_xfer(ctx, xfer));
                }
            }
        }
    }

    return true;
}

static bool process_request(struct srv_ctx* ctx,
                            const void* buf, const size_t size,
                            int* fds, const size_t nfds);

static void close_fds(int* fds, const size_t nfds)
{
    close(fds[0]);
    if (nfds == 2)
        close(fds[1]);
}

static bool handle_reqfd(struct srv_ctx* ctx,
                         const int events,
                         void* buf,
                         const size_t buf_size)
{
    assert (events == SYSPOLL_READ);

    int recvd_fds [PROT_MAXFDS];
    size_t nfds;
    uid_t uid;
    gid_t gid;

    for (;;) {
        nfds = PROT_MAXFDS;

        const ssize_t nread = us_recv(ctx->reqfd,
                                      buf, buf_size,
                                      recvd_fds, &nfds,
                                      &uid, &gid);

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
                send_err(recvd_fds[0], EACCES);
                close_fds(recvd_fds, nfds);

            } else if (!process_request(ctx, buf, (size_t)nread,
                                        recvd_fds, nfds)) {
                close_fds(recvd_fds, nfds);
            }
        }
    }

    return true;
}

static struct resrc_timer* add_open_file(struct srv_ctx* ctx,
                                         const char* filename,
                                         loff_t offset,
                                         size_t len,
                                         int stat_fd,
                                         struct file_info* info);

static bool register_xfer(struct srv_ctx* ctx, struct resrc_xfer* xfer);

static bool send_file_info(int fd, const struct file_info* info);

static bool send_open_file_info(int cli_fd,
                                size_t txnid,
                                const struct file_info* info);

static bool process_request(struct srv_ctx* ctx,
                            const void* buf, const size_t size,
                            int* fds, const size_t nfds __attribute__((unused)))
{
    static const char* const malformed_req_msg = "Received malformed request\n";
    static const char* const invalid_cmd_msg =
        "Received invalid command ID (%d) in request\n";

    if (prot_get_stat(buf) != PROT_STAT_OK) {
        syslog(LOG_NOTICE,
               "Received error status (%x) in request\n",
               prot_get_stat(buf));
        return false;
    }

    if (!PROT_IS_REQUEST(prot_get_cmd(buf))) {
        syslog(LOG_NOTICE, invalid_cmd_msg, prot_get_cmd(buf));
        return false;
    }

    switch (prot_get_cmd(buf)) {
    case PROT_CMD_FILE_OPEN: {
        struct prot_request pdu;
        if (prot_unmarshal_request(&pdu, buf, size) == -1) {
            syslog(LOG_NOTICE, malformed_req_msg);
            /* TODO: send NACK */
            return false;
        }

        struct file_info finfo;
        const struct resrc_timer* const timer = add_open_file(ctx,
                                                              pdu.filename,
                                                              pdu.offset,
                                                              pdu.len,
                                                              fds[0],
                                                              &finfo);

        if (!timer) {
            send_err(fds[0], errno);
            return false;
        }

        send_open_file_info(fds[0], timer->txnid, &finfo);
    } break;

    case PROT_CMD_SEND_OPEN: {
        struct prot_send_open pdu;
        if (prot_unmarshal_send_open(&pdu, buf) == -1) {
            syslog(LOG_NOTICE, malformed_req_msg);
            return false;
        }

        struct resrc_xfer* const xfer = xfer_table_find(ctx->xfers, pdu.txnid);
        if (!xfer) {
            /* Timer probably expired */
            send_err(fds[0], ENOENT);
            return false;
        }

        xfer->cmd = PROT_CMD_SEND;
        xfer->dest_fd = fds[0];

        if (!register_xfer(ctx, xfer)) {
            send_err(xfer->stat_fd, errno);
            undo_add_xfer(ctx, xfer);
            return false;
        }
    } break;

    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        struct prot_request pdu;
        if (prot_unmarshal_request(&pdu, buf, size) == -1) {
            syslog(LOG_NOTICE, malformed_req_msg);
            return false;
        }

        struct file_info finfo;
        struct resrc_xfer* const xfer =
            add_xfer(ctx,
                     pdu.filename,
                     pdu.cmd,
                     (loff_t)pdu.offset, pdu.len,
                     fds[0], (pdu.cmd == PROT_CMD_SEND ? fds[1] : fds[0]),
                     &finfo);

        if (!xfer) {
            send_err(fds[0], errno);
            return false;
        }

        if (!register_xfer(ctx, xfer)) {
            send_err(xfer->stat_fd, errno);
            undo_add_xfer(ctx, xfer);
            return false;
        }

        send_file_info(xfer->stat_fd, &finfo);

    } break;

    default:
        syslog(LOG_NOTICE, invalid_cmd_msg, prot_get_cmd(buf));
        return false;
    }

    return true;
}

static bool has_stat_channel(const struct resrc_xfer* x);

static bool retry_send_err_later(struct srv_ctx* ctx,
                                 struct resrc_xfer* x,
                                 const int err);

static bool retry_terminal_xfer_stat_later(struct srv_ctx* ctx,
                                           struct resrc_xfer* x);

static bool process_file_op(struct srv_ctx* ctx, struct resrc_xfer* xfer)
{
    switch (xfer->cmd) {
    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        size_t ntotal_written = 0;

        if (xfer->stat_fd == RETRYING_TERM_NOTIF_DELIV_TAG) {
            const struct term_notif_retry* r = (struct term_notif_retry*)xfer;
            return (send_xfer_stat(r->stat_fd, PROT_XFER_COMPLETE) ||
                    !errno_is_fatal(errno));

        } else if (xfer->stat_fd == RETRYING_ERR_DELIV_TAG) {
            const struct err_retry* e = (struct err_retry*)xfer;
            if (!send_err(e->stat_fd, e->err_code))
                return !errno_is_fatal(errno);
            return false;
        }

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

                if (send_err(xfer->stat_fd, write_errno) ||
                    errno_is_fatal(errno)) {
                    return false;
                }

                return retry_send_err_later(ctx, xfer, write_errno);

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
                    if (!send_xfer_stat(xfer->stat_fd, PROT_XFER_COMPLETE)) {
                        return (!errno_is_fatal(errno) &&
                                retry_terminal_xfer_stat_later(ctx, xfer));
                    }

                } else if (nwritten == -1 && ntotal_written > 0) {
                    /* Nonterminal notification; delivery not critical */
                    if (!send_xfer_stat(xfer->stat_fd, ntotal_written) &&
                        errno_is_fatal(errno)) {
                        return false;
                    }
                }
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

static bool retry_send_err_later(struct srv_ctx* ctx,
                                 struct resrc_xfer* x,
                                 const int err)
{
    if (!syspoll_deregister(ctx->poller, x->dest_fd)) {
        syslog(LOG_EMERG, "Unable to de-register transfer's dest fd [%m]\n");
        return false;
    }

    /* Dest fd has had a fatal error so closing would be redundant */

    struct err_retry* const e = (struct err_retry*)x;

    e->stat_fd = x->stat_fd;
    e->tag = RETRYING_ERR_DELIV_TAG;
    e->err_code = err;

    if (!syspoll_register(ctx->poller, (struct syspoll_resrc*)e, SYSPOLL_WRITE)) {
        syslog(LOG_EMERG, "Unable to register transfer's stat fd [%m]\n");
        return false;
    }

    return true;
}

static bool retry_terminal_xfer_stat_later(struct srv_ctx* ctx,
                                           struct resrc_xfer* x)
{
    if (!syspoll_deregister(ctx->poller, x->dest_fd)) {
        syslog(LOG_EMERG, "Unable to de-register transfer's dest fd [%m]\n");
        return false;
    }

    close(x->dest_fd);
    /* Prevent xfer from being removed by caller */
    x->nbytes_left = 1;

    struct term_notif_retry* const n = (struct term_notif_retry*)x;

    n->stat_fd = x->stat_fd;
    n->tag = RETRYING_TERM_NOTIF_DELIV_TAG;

    if (!syspoll_register(ctx->poller, (struct syspoll_resrc*)n, SYSPOLL_WRITE)) {
        syslog(LOG_EMERG, "Unable to register transfer's stat fd [%m]\n");
        return false;
    }

    return true;
}

/* --------------- (Uninteresting) Internal implementations ------------- */

static size_t get_txnid(void* p)
{
    return ((struct resrc_xfer*)p)->id;
}

static bool srv_ctx_construct(struct srv_ctx* ctx,
                              const long open_file_timeout_ms,
                              const int reqfd,
                              const int maxfds)
{
    *ctx = (struct srv_ctx) {
        .poller = syspoll_new(maxfds),
        .xfers = xfer_table_new(get_txnid, (size_t)maxfds),
        .open_file_timeout_ms = (unsigned)open_file_timeout_ms,
        .reqfd = reqfd,
        .next_id = 1,
        .uid = geteuid()
    };

    if (!ctx->poller || !ctx->xfers) {
        srv_ctx_destruct(ctx);
        return false;
    }

    return true;
}

static void delete_xfer_and_close_channel_fds(void* p)
{
    close_channel_fds((struct resrc_xfer*)p);
    delete_xfer(p);
}

static void srv_ctx_destruct(struct srv_ctx* ctx)
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

static struct resrc_xfer* create_xfer(struct srv_ctx* ctx,
                                      const enum prot_cmd_req cmd,
                                      const struct xfer_file* file,
                                      const loff_t offset,
                                      const size_t len,
                                      const int stat_fd,
                                      const int dest_fd)
{
    if (((size_t)offset + len) > file->size) {
        /* Requested range is invalid */
        errno = ERANGE;
        return NULL;
    }

    struct resrc_xfer* xfer = malloc(sizeof(*xfer));
    if (!xfer)
        return NULL;

    *xfer = (struct resrc_xfer) {
        .dest_fd = dest_fd,
        .stat_fd = stat_fd,
        .id = ctx->next_id++,
        .file = *file,
        .fio_ctx = fio_ctx_new(file->blksize),
        .nbytes_left = (len > 0 ? len : (file->size - (size_t)offset)),
        .cmd = cmd,
    };

    if (!fio_ctx_valid(xfer->fio_ctx)) {
        free(xfer);
        return NULL;
    }

    return xfer;
}

static bool register_xfer(struct srv_ctx* ctx, struct resrc_xfer* xfer)
{
    return syspoll_register(ctx->poller, (struct syspoll_resrc*)xfer, SYSPOLL_WRITE);
}

static struct resrc_xfer* add_xfer(struct srv_ctx* ctx,
                                   const char* filename,
                                   const enum prot_cmd_req cmd,
                                   const loff_t offset,
                                   const size_t len,
                                   const int stat_fd, const int dest_fd,
                                   struct file_info* finfo)
{
    assert (cmd == PROT_CMD_READ ||
            cmd == PROT_CMD_SEND ||
            cmd == PROT_CMD_FILE_OPEN);

    const int fd = file_open_read(filename, offset, len, finfo);
    if (fd == -1)
        return NULL;

    struct xfer_file file = {
        .size = finfo->size,
        .fd = fd,
        .blksize = finfo->blksize
    };

    struct resrc_xfer* const xfer = create_xfer(ctx,
                                                cmd,
                                                &file,
                                                offset, len,
                                                stat_fd, dest_fd);
    if (!xfer) {
        close(fd);
        return NULL;
    }

    if (!xfer_table_insert(ctx->xfers, xfer)) {
        PRESERVE_ERRNO(delete_xfer(xfer));
        return NULL;
    }

    finfo->size = xfer->nbytes_left;

    return xfer;
}

static void undo_add_xfer(struct srv_ctx* ctx, struct resrc_xfer* x)
{
    xfer_table_erase(ctx->xfers, x->id);
    delete_xfer(x);
}

static struct resrc_timer* add_open_file(struct srv_ctx* ctx,
                                         const char* filename,
                                         loff_t offset,
                                         size_t len,
                                         const int stat_fd,
                                         struct file_info* finfo)
{
    struct resrc_xfer* const xfer = add_xfer(ctx,
                                             filename,
                                             PROT_CMD_FILE_OPEN,
                                             offset, len,
                                             stat_fd, 0,
                                             finfo);
    if (!xfer)
        return false;

    struct resrc_timer* timer = malloc(sizeof(*timer));
    if (!timer)
        goto fail1;

    const int timerid = syspoll_timer(ctx->poller,
                                      (struct syspoll_resrc*)timer,
                                      ctx->open_file_timeout_ms);

    if (timerid == -1)
        goto fail2;

    *timer = TIMER(timerid, xfer->id);

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
    if (x->dest_fd != x->stat_fd)
        close(x->dest_fd);
}

static void purge_xfer(struct srv_ctx* ctx, struct resrc_xfer* xfer)
{
    xfer_table_erase(ctx->xfers, xfer->id);

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

static ssize_t send_pdu(const int fd, const void* pdu, const size_t size)
{
    const ssize_t n = write(fd, pdu, size);
    assert (n == -1 || (size_t)n == size);
    return n;
}

static bool send_file_info(int fd, const struct file_info* info)
{
    struct prot_file_info pdu;
    prot_marshal_file_info(&pdu,
                           info->size, info->atime, info->mtime, info->ctime);
    return (send_pdu(fd, &pdu, sizeof(pdu)) == sizeof(pdu));
}

static bool send_open_file_info(int cli_fd,
                                const size_t txnid,
                                const struct file_info* info)
{
    struct prot_open_file_info pdu;
    prot_marshal_open_file_info(&pdu,
                                info->size,
                                info->atime, info->mtime, info->ctime,
                                txnid);
    return (send_pdu(cli_fd, &pdu, sizeof(pdu)) == sizeof(pdu));
}

static bool send_xfer_stat(const int fd, const size_t file_size)
{
    struct prot_xfer_stat pdu;
    prot_marshal_xfer_stat(&pdu, file_size);
    return (send_pdu(fd, &pdu, sizeof(pdu)) == sizeof(pdu));
}

static bool send_err(const int fd, const int stat)
{
    const struct prot_hdr pdu = {
        .cmd = PROT_CMD_XFER_STAT,
        .stat = (uint8_t)stat
    };
    return (send_pdu(fd, &pdu, sizeof(pdu)) == sizeof(pdu));
}
