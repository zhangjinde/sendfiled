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

#define _GNU_SOURCE 1

#include <sys/sendfile.h>

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>

#include "file_io.h"

struct fio_ctx* fio_ctx_new(size_t capacity __attribute__((unused)))
{
    return NULL;
}

void fio_ctx_delete(struct fio_ctx* this __attribute__((unused)))
{
}

bool fio_ctx_valid(const struct fio_ctx* this)
{
    return (this == NULL);
}

ssize_t file_splice(const int fd_in, const int fd_out,
                    struct fio_ctx* ctx __attribute__((unused)),
                    const size_t nbytes)
{
    assert (nbytes > 0);

    return splice(fd_in, NULL,
                  fd_out, NULL,
                  nbytes,
                  SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
}

ssize_t file_sendfile(const int fd_in, const int fd_out,
                      struct fio_ctx* ctx __attribute__((unused)),
                      const size_t nbytes)
{
    assert (nbytes > 0);

    return sendfile(fd_out, fd_in, NULL, nbytes);
}
