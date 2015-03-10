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

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

bool set_nonblock(int fd, const bool enabled)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl == -1)
        return false;

    if (enabled)
        fl |= O_NONBLOCK;
    else
        fl &= ~O_NONBLOCK;

    return (fcntl(fd, F_SETFL, fl) == 0);
}

bool set_cloexec(int fd, const bool enabled)
{
    /* NOTE: For FD_CLOEXEC: 'F_GETF*D*' instead of 'F_GETF*L*' */

    int fl = fcntl(fd, F_GETFD, 0);
    if (fl == -1)
        return false;

    if (enabled)
        fl |= FD_CLOEXEC;
    else
        fl &= ~FD_CLOEXEC;

    return (fcntl(fd, F_SETFD, fl) == 0);
}

size_t pipe_capacity(void)
{
    static size_t c;

    if (c == 0) {
        const long page_size = sysconf(_SC_PAGE_SIZE);
        if (page_size == -1)
            return 0;
        assert (page_size > 0);

        void* const page = calloc((size_t)page_size, 1);
        if (!page)
            return 0;

        int fds[2] = {-1, -1};

        if (sfd_pipe(fds, O_NONBLOCK) == -1) {
            free(page);
            return 0;
        }

        ssize_t n;
        while ((n = write(fds[1], page, (size_t)page_size)) != -1) {
            assert (n > 0);
            c += (size_t)n;
        }

        if ((errno != EWOULDBLOCK && errno != EAGAIN) || c == 0) {
            /* FIXME: This happens when the
               SfdThreadSmallFileFix.send_error_send_fails test is run on its
               own. The first write to the pipe fails with EWOULDBLOCK. */
            fprintf(stderr,
                    "Couldn't write to pipe to determine pipe capacity;"
                    " errno %d [%s]\n",
                    errno, strerror(errno));
            abort();
        }

        free(page);
        PRESERVE_ERRNO(close(fds[0]));
        PRESERVE_ERRNO(close(fds[1]));
    }

    return c;
}
