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

#ifndef SFD_FILE_IO_H
#define SFD_FILE_IO_H

#include <sys/types.h>

#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct file_info {
    /* Size of file on disk, as returned by stat(2); will be >= the total
       number of bytes transferred */
    size_t size;
    time_t atime;
    time_t mtime;
    time_t ctime;
    unsigned blksize;
};

struct fio_ctx;

#pragma GCC diagnostic pop

struct fio_ctx* fio_ctx_new(size_t capacity);

void fio_ctx_delete(struct fio_ctx*);

bool fio_ctx_valid(const struct fio_ctx*);

/**
   @retval >0 The file descriptor
   @retval <0 An error occurred
*/
int file_open_read(const char* name,
                   off_t offset, size_t len,
                   struct file_info*);

off_t file_offset(int fd);

ssize_t file_splice(int fd_in, int fd_out,
                    struct fio_ctx*,
                    size_t nbytes);

ssize_t file_sendfile(int fd_in, int fd_out,
                      struct fio_ctx*,
                      size_t nbytes);

#endif
