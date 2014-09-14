#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../attributes.h"
#include "file_io.h"
#include "protocol.h"
#include "server.h"
#include "syspoll.h"
#include "unix_sockets.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct xfer {
    /* The syspoll-registered fd *must* come first! */
    int dest_fd;
    int stat_fd;
    enum prot_cmd cmd;
    struct file file;
    int idx;
};

struct context
{
    struct syspoll* poller;
    int listenfd;
    const char* sockname;
    struct xfer** xfers;
    int nxfers;
    int max_xfers;
};

#pragma GCC diagnostic pop

static void print_xfer(const struct xfer* x)
{
    printf("stat_fd: %d; dest_fd: %d; cmd: %d;"
           " file->size: %ld; file->blksize: %d\n",
           x->stat_fd, x->dest_fd, x->cmd, x->file.size, x->file.blksize);
}

#define PRINT_XFER(x) printf("%s: ", __func__); print_xfer(x)

static bool errno_is_fatal(const int err);

static bool process_file_op(struct context* ctx, struct xfer* xfer);

static void context_destruct(struct context* ctx);

static void delete_xfer(struct xfer* x)
{
    close(x->dest_fd);
    if (x->stat_fd != x->dest_fd)
        close(x->stat_fd);
    file_close(&x->file);
    free(x);
}

static bool context_construct(struct context* ctx,
                              const int poll_timeout,
                              const int listenfd,
                              const char* sockname,
                              const int maxfds)
{
    *ctx = (struct context) {
        .poller = syspoll_new(poll_timeout, maxfds),
        .listenfd = listenfd,
        .sockname = sockname,
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

static bool add_xfer(struct context* ctx,
                     const enum prot_cmd cmd,
                     struct file* file,
                     const int stat_fd,
                     const int dest_fd)
{
    assert (ctx->nxfers < ctx->max_xfers);

    struct xfer* xfer = malloc(sizeof(*xfer));
    if (!xfer)
        return false;

    *xfer = (struct xfer) {
        .cmd = cmd,
        .dest_fd = dest_fd,
        .stat_fd = stat_fd,
        .file = *file
    };

    if (!syspoll_register(ctx->poller, xfer->dest_fd, xfer, SYSPOLL_WRITE))
        goto fail;

    PRINT_XFER(xfer);

    ctx->xfers[ctx->nxfers] = xfer;
    xfer->idx = ctx->nxfers;
    ctx->nxfers++;

    return true;

 fail:
    free(xfer);
    return false;
}

static bool add_read(struct context* ctx,
                     const char* filename,
                     const int dest_fd)
{
    struct file file;
    if (!file_open_read(&file, filename))
        return false;

    return add_xfer(ctx, PROT_CMD_READ, &file, dest_fd, dest_fd);
}

static bool add_send(struct context* ctx,
                     const char* filename,
                     const int stat_fd,
                     const int dest_fd)
{
    struct file file;
    if (!file_open_read(&file, filename))
        return false;

    return add_xfer(ctx, PROT_CMD_SEND, &file, stat_fd, dest_fd);
}

static enum prot_stat handle_request(struct context* ctx,
                                     const void* buf, const size_t size,
                                     int* fds, const size_t nfds UNUSED)
{
    struct prot_pdu pdu;
    prot_unmarshal(buf, size, &pdu);
    char fname[PROT_FILENAME_MAX + 1] = {0};
    memcpy(fname, pdu.filename, pdu.filename_len);
    printf("XXX nrecvd: %lu; cmd: %d; stat: %d;"
           " fnlen: %d; fname: %s;"
           " recvd_fds[0]: %d; recvd_fds[1]: %d\n",
           size, pdu.cmd, pdu.stat, pdu.filename_len, fname,
           fds[0], fds[1]);

    switch (pdu.cmd) {
    case PROT_CMD_READ:
        if (ctx->nxfers == ctx->max_xfers)
            return PROT_STAT_XXX;

        return (add_read(ctx, fname, fds[0]) ?
                PROT_STAT_OK :
                PROT_STAT_XXX);

    case PROT_CMD_SEND:
        if (ctx->nxfers == ctx->max_xfers)
            return PROT_STAT_XXX;

        return (add_send(ctx, fname, fds[0], fds[1]) ?
                PROT_STAT_OK :
                PROT_STAT_XXX);

    default:
        return PROT_STAT_XXX;
    }
}

static bool send_error(const int fd, const enum prot_stat err)
{
    prot_stat_buf buf;
    prot_marshal_stat(buf, sizeof(buf), err, 0, 0);

    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    return (us_sendv(fd, &iov, 1, NULL, 0) > 0);
}

static bool read_and_process_reqs(struct context* ctx,
                                  uint8_t* buf, const size_t buf_size)
{
    int recvd_fds[2] = {-1, -1};
    size_t nfds = 2;

    ssize_t nread;

    for (;;) {
        nread = us_recv(ctx->listenfd, buf, buf_size, recvd_fds, &nfds);

        if (nread < 0) {
            if (errno_is_fatal(errno)) {
                /* Fatal error */
                return false;
            }
            /* Non-fatal error */
            break;

        } else {
            /* Recv of zero makes no sense on a UDP (connectionless) socket */
            assert (nread > 0);

            const enum prot_stat stat = handle_request(ctx,
                                                       buf, (size_t)nread,
                                                       recvd_fds, nfds);
            if (stat != PROT_STAT_OK) {
                puts("REQUEST FAILED");
                send_error(ctx->listenfd, stat);
            }
        }
    }

    return true;
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

    delete_xfer(xfer);
}

bool srv_run(const int listenfd, const char* sockname, const int maxfds)
{
    struct context ctx;

    if (!context_construct(&ctx, 1000, listenfd, sockname, maxfds))
        return false;

    if (!syspoll_register(ctx.poller,
                          ctx.listenfd,
                          &ctx.listenfd,
                          SYSPOLL_READ | SYSPOLL_WRITE))
        goto fail;

    uint8_t recvbuf[PROT_PDU_MAXSIZE] = {0};

    /* for (;;) { */
    for (int i = 0; i < 3; i++) {
        const int n = syspoll_poll(ctx.poller);

        for (int j = 0; j < n; j++) {
            struct syspoll_resrc resrc = syspoll_get(ctx.poller, j);

            if (*(int*)resrc.udata == ctx.listenfd) {
                /* The request socket */
                if (resrc.events & SYSPOLL_READ) {
                    if (!read_and_process_reqs(&ctx, recvbuf, sizeof(recvbuf))) {
                        /* TODO handle fatal error */
                    }
                }

            } else {
                /* One of the responses (file transfers) */
                assert (resrc.events & SYSPOLL_WRITE);
                struct xfer* xfer = (struct xfer*)resrc.udata;

                printf("XXX xfer (stat: %d; dest: %d) ready\n",
                       xfer->stat_fd, xfer->dest_fd);

                if (!process_file_op(&ctx, xfer)) {
                    /* Fatal error */
                    remove_xfer(&ctx, xfer);
                }
            }
        }
    }

    context_destruct(&ctx);

    return true;

 fail:
    context_destruct(&ctx);
    return false;
}

/* --------------- Internal implementations ------------- */

static bool xfer_complete(const struct xfer* x)
{
    return ((size_t)file_offset(&x->file) == x->file.size);
}

static bool errno_is_fatal(const int err)
{
    return (err == EWOULDBLOCK || err == EAGAIN);
}

static bool process_file_op(struct context* ctx, struct xfer* xfer)
{
    switch (xfer->cmd) {
    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        const ssize_t n = file_splice(&xfer->file, xfer->dest_fd);

        PRINT_XFER(xfer);

        if (n < 0) {
            if (errno_is_fatal(errno)) {
                send_error(xfer->stat_fd, PROT_STAT_XXX);
                return false;
            }
        } else if (n == 0) {
            return false;
        } else {
            /* Report the transfer via the status descriptor */
            prot_stat_buf buf;

            prot_marshal_stat(buf, sizeof(buf), PROT_STAT_XFER,
                              xfer->file.size,
                              (uint64_t)file_offset(&xfer->file));

            if (write(xfer->stat_fd, buf, sizeof(buf)) != sizeof(buf) == -1) {
                if (errno_is_fatal(errno)) {
                    /* TODO Handle fatal error */
                }
            }

            if (xfer_complete(xfer)) {
                remove_xfer(ctx, xfer);
            }
        }

        return (n > 0);
    }

    case PROT_CMD_STAT:
        /* Not a file operation */
        abort();

    case PROT_CMD_CANCEL:
        break;
    }

    return false;
}
