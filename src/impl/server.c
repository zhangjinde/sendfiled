#include <sys/types.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../attributes.h"
#include "errors.h"
#include "file_io.h"
#include "protocol_server.h"
#include "server.h"
#include "syspoll.h"
#include "unix_sockets.h"
#include "util.h"

struct xfer_file {
    size_t size;
    int fd;
    unsigned blksize;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct xfer {
    /* The syspoll-registered fd (must be the first field of this struct) */
    int dest_fd;
    int stat_fd;
    struct xfer_file file;
    enum prot_cmd cmd;
    size_t len;
    int idx;
};

struct context
{
    struct syspoll* poller;
    int listenfd;
    struct xfer** xfers;
    int nxfers;
    int max_xfers;
};

#pragma GCC diagnostic pop

static void print_xfer(const struct xfer* x);

#define PRINT_XFER(x)                           \
    printf("%s: ", __func__);                   \
    print_xfer(x)

static bool errno_is_fatal(const int err);

static bool process_file_op(struct xfer* xfer);

static void context_destruct(struct context* ctx);

static bool add_read_xfer(struct context* ctx,
                          const char* filename,
                          loff_t offset,
                          size_t len,
                          int dest_fd,
                          struct file_info* info);

static bool add_send_xfer(struct context* ctx,
                          const char* filename,
                          loff_t offset,
                          size_t len,
                          int stat_fd,
                          int dest_fd,
                          struct file_info* info);

static bool send_file_info(int fd, const struct file_info* info);

static bool send_xfer_stat(int fd, size_t file_size);

static bool send_err(int fd, int err);

/*
  Removing, e.g., xfer 'b':

  |a|b|c|d| -> |a|d|c|

  (i.e., the last element replaces the removed one and its index updated.)
*/
static void remove_xfer(struct context* ctx, struct xfer* xfer);

static void delete_xfer(struct xfer* x);

static bool process_events(struct context* ctx,
                           const int nevents,
                           uint8_t* buf, const size_t buf_size);

static bool context_construct(struct context* ctx,
                              int poll_timeout,
                              int listenfd,
                              int maxfds);

static void context_destruct(struct context* ctx);

bool srv_run(const int listenfd, const int maxfds)
{
    struct context ctx;

    if (!context_construct(&ctx, 1000, listenfd, maxfds))
        return false;

    if (!syspoll_register(ctx.poller,
                          ctx.listenfd,
                          &ctx.listenfd,
                          SYSPOLL_READ)) {
        goto fail;
    }

    uint8_t* const recvbuf = calloc(PROT_REQ_MAXSIZE, 1);
    if (!recvbuf)
        goto fail;

    /* The processing loop */
    for (;;) {
        const int n = syspoll_poll(ctx.poller);

        if (n == -1) {
            LOGERRNO("syspoll_poll() failed");
            break;
        }

        if (!process_events(&ctx, n, recvbuf, PROT_REQ_MAXSIZE))
            break;
    }

    free(recvbuf);
    context_destruct(&ctx);

    return true;

 fail:
    context_destruct(&ctx);
    return false;
}

static bool handle_listenfd(struct context* ctx,
                            int events,
                            uint8_t* buf,
                            size_t buf_size);

static bool xfer_complete(const struct xfer* x);

static bool process_events(struct context* ctx,
                           const int nevents,
                           uint8_t* buf, const size_t buf_size)
{
    for (int i = 0; i < nevents; i++) {
        struct syspoll_resrc resrc = syspoll_get(ctx->poller, i);

        if (*(int*)resrc.udata == ctx->listenfd) {
            if (resrc.events & SYSPOLL_ERROR) {
                fprintf(stderr,
                        "%s: fatal error on request socket; aborting\n",
                        __func__);
                return false;
            }

            if (!handle_listenfd(ctx, resrc.events, buf, buf_size)) {
                fprintf(stderr, "%s: fatal error on listen socket\n", __func__);
                return false;
            }

        } else {
            if (resrc.events & SYSPOLL_TERM)
                return false;

            struct xfer* const xfer = (struct xfer*)resrc.udata;

            if (resrc.events & SYSPOLL_ERROR) {
                fprintf(stderr, "System poller got error on "
                        " xfer {statfd: %d; destfd: %d}; terminating it\n",
                        xfer->stat_fd, xfer->dest_fd);
                remove_xfer(ctx, xfer);

            } else if (!process_file_op(xfer) || xfer_complete(xfer)) {
                remove_xfer(ctx, xfer);
            }
        }
    }

    return true;
}

static bool process_request(struct context* ctx,
                            const void* buf, const size_t size,
                            int* fds, const size_t nfds);

static bool handle_listenfd(struct context* ctx,
                            const int events,
                            uint8_t* buf,
                            const size_t buf_size)
{
    assert (events == SYSPOLL_READ);

    int recvd_fds[2];
    size_t nfds;

    for (;;) {
        nfds = 2;

        const ssize_t nread = us_recv(ctx->listenfd,
                                      buf, buf_size,
                                      recvd_fds, &nfds);

        if (nread < 0) {
            return !errno_is_fatal(errno);
        } else {
            /* Recv of zero makes no sense on a UDP (connectionless)
               socket */
            assert (nread > 0);

            if (!process_request(ctx,
                                 buf, (size_t)nread,
                                 recvd_fds, nfds)) {
                if (nfds > 0) {
                    close(recvd_fds[0]);
                    if (nfds == 2)
                        close(recvd_fds[1]);
                }
            }
        }
    }

    return true;
}

static bool process_request(struct context* ctx,
                            const void* buf, const size_t size,
                            int* fds, const size_t nfds UNUSED)
{
    struct prot_request pdu;
    int err = prot_unmarshal_request(&pdu, buf);

    if (err == -1) {
        fprintf(stderr, "%s: received malformed request PDU\n", __func__);
        /* TODO: send NACK */
        return false;

    } else if (err > 0) {
        fprintf(stderr, "%s: client sent error code %d\n", __func__, pdu.stat);
        return false;
    }

    printf("XXX nrecvd: %lu; cmd: %d; stat: %d;"
           " fnlen: %lu; fname: %s;"
           " recvd_fds[0]: %d; recvd_fds[1]: %d\n",
           size, pdu.cmd, pdu.stat, pdu.body_len, pdu.filename,
           fds[0], fds[1]);

    if ((pdu.cmd == PROT_CMD_READ || pdu.cmd == PROT_CMD_SEND) &&
        ctx->nxfers == ctx->max_xfers) {
        return false;
    }

    switch (pdu.cmd) {
    case PROT_CMD_READ: {
        struct file_info finfo;
        if (!add_read_xfer(ctx,
                           pdu.filename,
                           (loff_t)pdu.offset, pdu.len,
                           fds[0],
                           &finfo)) {
            send_err(fds[0], errno);
            return false;
        }

        send_file_info(fds[0], &finfo);
    } break;

    case PROT_CMD_SEND: {
        struct file_info finfo;
        if (!add_send_xfer(ctx,
                           pdu.filename,
                           (loff_t)pdu.offset, pdu.len,
                           fds[0], fds[1],
                           &finfo)) {
            send_err(fds[0], errno);
            return false;
        }

        send_file_info(fds[0], &finfo);
    } break;

    default:
        /* TODO report "unknown command" */
        return false;
    }

    return true;
}

static bool needs_stat(const struct xfer* x)
{
    return (x->stat_fd != x->dest_fd);
}

static bool process_file_op(struct xfer* xfer)
{
    switch (xfer->cmd) {
    case PROT_CMD_FILE_INFO:
        break;
    case PROT_CMD_READ:
    case PROT_CMD_SEND:
        for (;;) {
            const ssize_t nwritten = file_splice(xfer->file.fd,
                                                 xfer->dest_fd,
                                                 MIN_(xfer->file.blksize,
                                                      xfer->len));

            if (nwritten < 0) {
                if (errno_is_fatal(errno)) {
                    if (needs_stat(xfer))
                        send_err(xfer->stat_fd, errno);
                    return false;
                }
                return true;

            } else if (nwritten == 0) {
                /* FIXME Not sure how to deal with this properly */
                return true;

            }

            xfer->len -= (size_t)nwritten;

            if (xfer->len == 0) {
                return (!needs_stat(xfer) ||
                        send_xfer_stat(xfer->stat_fd, (size_t)nwritten));
            }
        }

    case PROT_CMD_CANCEL:
        break;

    case PROT_CMD_XFER_STAT:
        LOGERRNOV("invalid client command: %d\n", xfer->cmd);
        return false;
    }

    return false;
}

/* --------------- (Uninteresting) Internal implementations ------------- */

static bool context_construct(struct context* ctx,
                              const int poll_timeout,
                              const int listenfd,
                              const int maxfds)
{
    *ctx = (struct context) {
        .poller = syspoll_new(poll_timeout, maxfds),
        .listenfd = listenfd,
        .xfers = calloc((size_t)maxfds, sizeof(struct xfer*)),
        .max_xfers = maxfds
    };

    if (!ctx->poller || !ctx->xfers) {
        context_destruct(ctx);
        return false;
    }

    return true;
}

static void context_destruct(struct context* ctx)
{
    syspoll_delete(ctx->poller);

    close(ctx->listenfd);

    for (int i = 0; i < ctx->nxfers; i++) {
        struct xfer* const x = ctx->xfers[i];
        delete_xfer(x);
    }

    free(ctx->xfers);
}

static bool xfer_complete(const struct xfer* x)
{
    return (x->len == 0);
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

static struct xfer* add_xfer(struct context* ctx,
                             const enum prot_cmd cmd,
                             const struct xfer_file* file,
                             const loff_t offset,
                             const size_t len,
                             const int stat_fd,
                             const int dest_fd)
{
    assert (ctx->nxfers < ctx->max_xfers);

    if (((size_t)offset + len) > file->size) {
        /* Requested range is invalid */
        errno = ERANGE;
        return NULL;
    }

    struct xfer* xfer = malloc(sizeof(*xfer));
    if (!xfer)
        return NULL;

    *xfer = (struct xfer) {
        .cmd = cmd,
        .dest_fd = dest_fd,
        .stat_fd = stat_fd,
        .file = *file,
        .len = (len > 0 ? len : (file->size - (size_t)offset))
    };

    if (!syspoll_register(ctx->poller, xfer->dest_fd, xfer, SYSPOLL_WRITE))
        goto fail;

    PRINT_XFER(xfer);

    ctx->xfers[ctx->nxfers] = xfer;
    xfer->idx = ctx->nxfers;
    ctx->nxfers++;

    return xfer;

 fail:
    free(xfer);
    return NULL;
}

static bool add_read_xfer(struct context* ctx,
                          const char* filename,
                          const loff_t offset,
                          const size_t len,
                          const int dest_fd,
                          struct file_info* finfo)
{
    const int fd = file_open_read(filename, offset, len, finfo);
    if (fd == -1)
        return false;

    struct xfer_file file = {
        .size = finfo->size,
        .fd = fd,
        .blksize = finfo->blksize
    };

    const struct xfer* const x = add_xfer(ctx,
                                          PROT_CMD_READ,
                                          &file,
                                          offset, len,
                                          dest_fd, dest_fd);

    finfo->size = x->len;

    return (bool)x;
}

static bool add_send_xfer(struct context* ctx,
                          const char* filename,
                          const loff_t offset,
                          const size_t len,
                          const int stat_fd,
                          const int dest_fd,
                          struct file_info* finfo)
{
    const int fd = file_open_read(filename, offset, len, finfo);
    if (fd == -1)
        return false;

    struct xfer_file file = {
        .size = finfo->size,
        .fd = fd,
        .blksize = finfo->blksize
    };

    const struct xfer* const x = add_xfer(ctx,
                                          PROT_CMD_SEND,
                                          &file,
                                          offset, len,
                                          stat_fd, dest_fd);

    finfo->size = x->len;

    return (bool)x;
}

static void delete_xfer(struct xfer* x)
{
    close(x->dest_fd);
    if (x->stat_fd != x->dest_fd)
        close(x->stat_fd);
    close(x->file.fd);
    free(x);
}

static bool send_pdu(const int fd, void* pdu, const size_t size)
{
    const ssize_t n = write(fd, pdu, size);
    assert (n == -1 || (size_t)n == size);
    return (n != -1);
}

static bool send_file_info(int fd, const struct file_info* info)
{
    struct prot_file_info_m pdu;
    prot_marshal_file_info(&pdu,
                           info->size, info->atime, info->mtime, info->ctime);
    return send_pdu(fd, pdu.data, sizeof(pdu.data));
}

static bool send_xfer_stat(int fd, size_t file_size)
{
    struct prot_xfer_stat_m pdu;
    prot_marshal_xfer_stat(&pdu, file_size);
    return send_pdu(fd, pdu.data, sizeof(pdu.data));
}

static bool send_err(int fd, const int stat)
{
    struct prot_hdr_m pdu;
    prot_marshal_hdr(pdu.data, PROT_CMD_XFER_STAT, (int)stat, 0);
    return send_pdu(fd, pdu.data, sizeof(pdu.data));
}

/*
  Removing, e.g., xfer 'b':

  |a|b|c|d| -> |a|d|c|

  (i.e., the last element replaces the removed one and its index updated.)
*/
static void remove_xfer(struct context* ctx, struct xfer* xfer)
{
    const int i = xfer->idx;

    if (i < ctx->nxfers - 1) {
        ctx->xfers[i] = ctx->xfers[ctx->nxfers - 1];
        ctx->xfers[i]->idx = i;
        ctx->xfers[ctx->nxfers - 1] = NULL;
    }

    ctx->nxfers--;

    /* The client and server processes share the dest fd's file table entry (it
       was sent over a UNIX socket), so closing it here will not cause it to be
       automatically removed from the system poller if the client process has
       not yet closed *its* copy, and therefore it may be returned again by the
       next call to syspoll_poll(), *after* the memory associated with it has
       been freed here.
    */
    syspoll_deregister(ctx->poller, xfer->dest_fd);

    delete_xfer(xfer);
}

static void print_xfer(const struct xfer* x)
{
    printf("stat_fd: %d; dest_fd: %d; cmd: %d;"
           " file->size: %ld; file->blksize: %d\n",
           x->stat_fd, x->dest_fd, x->cmd, x->file.size, x->file.blksize);
}
