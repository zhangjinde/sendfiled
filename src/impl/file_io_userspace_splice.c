/**
   @file

   Non-Linux platforms don't have the splice(2) system call (copies data from
   one fd to another using a kernel buffer inbetween; supposedly zero-copy), and
   they will all need to use this userspace, non-zero-copy implementation,
   e.g. when asked to read a file into a pipe or to send a file to a non-socket
   file descriptor.
 */

#include <stdint.h>
#include <stdlib.h>

#include "file_io.h"
#include "util.h"

struct fio_ctx {
    uint8_t* data;
    size_t capacity;
    uint8_t* rp;
    uint8_t* wp;
};

struct fio_ctx* fio_ctx_new(size_t capacity)
{
    struct fio_ctx* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    this->data = malloc(capacity);
    if (!this->data) {
        free(this);
        return NULL;
    }

    this->capacity = capacity;
    this->rp = this->wp = this->data;

    return this;
}

void fio_ctx_delete(struct fio_ctx* this)
{
    free(this->data);
    free(this);
}

bool fio_ctx_valid(const struct fio_ctx* this)
{
    return (this && this->data);
}

ssize_t file_splice(const int fd_in, const int fd_out,
                    struct fio_ctx* ctx,
                    const size_t nbytes)
{
    const size_t nunwritten = (ctx->capacity - (size_t)(ctx->wp - ctx->data));

    if (nunwritten > 0) {
        const ssize_t nread = read(fd_in, ctx->wp, MIN_(nbytes, nunwritten));

        if (nread == -1 || nread == 0)
            return nread;

        ctx->wp += nread;
    }

    if (ctx->rp < ctx->wp) {
        const ssize_t nwritten = write(fd_out, ctx->rp,
                                       (size_t)(ctx->wp - ctx->rp));

        if (nwritten > 0) {
            ctx->rp += (size_t)nwritten;
            if (ctx->rp == ctx->wp)
                ctx->rp = ctx->wp = ctx->data;
            return nwritten;
        }

        assert (nwritten == 0 || nwritten == -1);
        return nwritten;
    }

    return 0;
}
