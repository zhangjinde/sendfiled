/*
  Copyright (c) 2015, Francois Kritzinger
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

/**
   @file

   Non-Linux platforms don't have the splice(2) system call (copies data from
   one fd to another using a kernel buffer inbetween; supposedly zero-copy), and
   they will all need to use this userspace, non-zero-copy implementation,
   e.g. when asked to read a file into a pipe or to send a file to a non-socket
   file descriptor.
*/

#include <sys/socket.h>

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

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
    assert (nbytes > 0);

    const size_t nunwritten = (ctx->capacity - (size_t)(ctx->wp - ctx->data));

    if (nunwritten > 0) {
        const ssize_t nread = read(fd_in, ctx->wp, SFD_MIN(nbytes, nunwritten));

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
