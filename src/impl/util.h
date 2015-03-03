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

#ifndef SFD_UTIL_H
#define SFD_UTIL_H

#include <errno.h>
#include <stdbool.h>

#define MAX_(a, b) ((a) > (b) ? (a) : (b))

#define MIN_(a, b) ((a) < (b) ? (a) : (b))

#define PRESERVE_ERRNO(statement)               \
    {                                           \
        const int errno_saved_ = errno;         \
        statement;                              \
        errno = errno_saved_;                   \
    }

#ifdef __cplusplus
extern "C" {
#endif

    bool set_nonblock(int fd, bool enabled);

    bool set_cloexec(int fd, bool enabled);

    /**
       Creates a pipe.

       @note This function exists because Linux has an overload of @a pipe(2)
       with a flags parameter (i.e., creates the pipe and sets the flags in one
       operation), whereas FreeBSD requires two calls to @c fcntl(2) to set
       flags on the pipe descriptors.

       @param[out] fds The pipe file descriptors

       @param[in] flags The flags to set on the pipe's file descriptors. E.g.,
       @c O_NONBLOCK, @c O_CLOEXEC (@c FD_CLOEXEC not accepted).
     */
    int sfd_pipe(int fds[2], int flags);

#ifdef __cplusplus
}
#endif

#endif
