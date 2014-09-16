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
    struct xfer** xfers;
    int nxfers;
    int max_xfers;
};

#pragma GCC diagnostic pop

static void print_xfer(const struct xfer* x);

#define MIN_(a, b) ((a) < (b) ? (a) : (b))

#define MAX_(a, b) ((a) > (b) ? (a) : (b))

#define PRINT_XFER(x)                           \
    printf("%s: ", __func__);                   \
    print_xfer(x)

static bool errno_is_fatal(const int err);

static bool process_file_op(struct xfer* xfer);

static void context_destruct(struct context* ctx);

static size_t add_read_xfer(struct context* ctx,
                            const char* filename,
                            int dest_fd);

static size_t add_send_xfer(struct context* ctx,
                            const char* filename,
                            int stat_fd,
                            int dest_fd);

static bool send_chunk_hdr(int stat_fd, const size_t file_size);

static bool send_ack(int fd, size_t file_size);

static bool send_nack(int fd, enum prot_stat stat);

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
    /* for (;;) { */
    for (int i = 0; i < 3; i++) {
        const int n = syspoll_poll(ctx.poller);

        if (!process_events(&ctx, n, recvbuf, PROT_REQ_MAXSIZE)) {
            fprintf(stderr,
                    "%s: fatal error during event processing; exiting\n",
                    __func__);
            break;
        }
    }

    printf("%s: terminating\n", __func__);

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
            struct xfer* const xfer = (struct xfer*)resrc.udata;

            if (resrc.events & SYSPOLL_ERROR) {
                fprintf(stderr,
                        "%s: fatal error on"
                        " xfer {statfd: %d; destfd: %d}; terminating it\n",
                        __func__, xfer->stat_fd, xfer->dest_fd);
                remove_xfer(ctx, xfer);

            } else if (!process_file_op(xfer) || xfer_complete(xfer)) {
                printf("%s: file operation complete; removing\n", __func__);
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
                puts("BAD REQUEST");
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
    if (!prot_unmarshal_request(&pdu, buf)) {
        fprintf(stderr, "%s: received malformed request PDU\n", __func__);
        /* TODO: send NACK */
        return false;
    }

    char fname[PROT_FILENAME_MAX + 1] = {0};
    memcpy(fname, pdu.filename, pdu.body_len);
    printf("XXX nrecvd: %lu; cmd: %d; stat: %d;"
           " fnlen: %lu; fname: %s;"
           " recvd_fds[0]: %d; recvd_fds[1]: %d\n",
           size, pdu.cmd, pdu.stat, pdu.body_len, fname,
           fds[0], fds[1]);

    if ((pdu.cmd == PROT_CMD_READ || pdu.cmd == PROT_CMD_SEND) &&
        ctx->nxfers == ctx->max_xfers) {
        return false;
    }

    switch (pdu.cmd) {
    case PROT_CMD_READ: {
        const size_t fsize = add_read_xfer(ctx, fname, fds[0]);
        if (fsize == 0)
            return false;
        send_ack(fds[0], fsize);
    } break;

    case PROT_CMD_SEND: {
        const size_t fsize = add_send_xfer(ctx, fname, fds[0], fds[1]);
        if (fsize == 0)
            return false;
        send_ack(fds[0], fsize);
    } break;

    default:
        return false;
    }

    return true;
}

static bool process_file_op(struct xfer* xfer)
{
    switch (xfer->cmd) {
    case PROT_CMD_READ:
    case PROT_CMD_SEND: {
        const size_t nbytes = MIN_(xfer->file.size, (size_t)xfer->file.blksize);

        if (!send_chunk_hdr(xfer->stat_fd, nbytes))
            return false;

        const ssize_t nwritten = file_splice(&xfer->file, xfer->dest_fd);

        if (nwritten < 0) {
            if (errno_is_fatal(errno)) {
                send_err(xfer->stat_fd, errno);
                return false;
            }
            return true;
        } else if (nwritten == 0) {
            /* FIXME Not sure how to deal with this properly */
            return false;
        } else {
            return true;
        }
    }

    case PROT_CMD_CANCEL:
        break;
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
    return ((size_t)file_offset(&x->file) == x->file.size);
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

static size_t add_read_xfer(struct context* ctx,
                            const char* filename,
                            const int dest_fd)
{
    struct file file;
    if (!file_open_read(&file, filename))
        return false;

    return (add_xfer(ctx, PROT_CMD_READ, &file, dest_fd, dest_fd) ?
            file.size :
            0);
}

static size_t add_send_xfer(struct context* ctx,
                            const char* filename,
                            const int stat_fd,
                            const int dest_fd)
{
    struct file file;
    if (!file_open_read(&file, filename))
        return false;

    return (add_xfer(ctx, PROT_CMD_SEND, &file, stat_fd, dest_fd) ?
            file.size :
            0);
}

static void delete_xfer(struct xfer* x)
{
    close(x->dest_fd);
    if (x->stat_fd != x->dest_fd)
        close(x->stat_fd);
    file_close(&x->file);
    free(x);
}

static bool send_pdu(const int fd, void* pdu, const size_t size)
{
    const ssize_t n = write(fd, pdu, size);
    assert (n == -1 || (size_t)n == size);
    return (n != -1);
}

static bool send_chunk_hdr(const int stat_fd, const size_t file_size)
{
    struct prot_chunk_hdr_m pdu;
    prot_marshal_chunk_hdr(&pdu, file_size);
    return send_pdu(stat_fd, pdu.data, sizeof(pdu.data));
}

static bool send_ack(int fd, size_t file_size)
{
    struct prot_ack_m pdu;
    prot_marshal_ack(&pdu, file_size);
    return send_pdu(fd, pdu.data, sizeof(pdu.data));
}

static bool send_nack(int fd, enum prot_stat stat)
{
    struct prot_ack_m pdu;
    prot_marshal_nack(&pdu, stat);
    return send_pdu(fd, pdu.data, sizeof(pdu.data));
}

static bool send_err(int fd, const int stat)
{
    struct prot_hdr_m pdu;
    prot_marshal_hdr(&pdu, PROT_CMD_CANCEL, (uint8_t)stat, 0);
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

    delete_xfer(xfer);
}

static void print_xfer(const struct xfer* x)
{
    printf("stat_fd: %d; dest_fd: %d; cmd: %d;"
           " file->size: %ld; file->blksize: %d\n",
           x->stat_fd, x->dest_fd, x->cmd, x->file.size, x->file.blksize);
}
