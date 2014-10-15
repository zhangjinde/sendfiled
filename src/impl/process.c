/*
  Copyright (c) 2014, Francois Kritzinger
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

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <signal.h>

#include "errors.h"
#include "process.h"
#include "util.h"

static bool dup_open_fd(int srcfd, int dstfd);

bool proc_init_child(const int* excluded_fds, size_t nfds)
{
    /* Redirect stdin, stdout, and stderr to /dev/null */
    const int nullfd = open("/dev/null", O_RDWR);
    if (nullfd == -1) {
        LOGERRNO("open()\n");
        return false;
    }

    if (!dup_open_fd(nullfd, STDIN_FILENO) ||
        !dup_open_fd(nullfd, STDOUT_FILENO) ||
        !dup_open_fd(nullfd, STDERR_FILENO)) {
        PRESERVE_ERRNO(close(nullfd));
        return false;
    }

    close(nullfd);

    /* Close all other file descriptors except the specified ones */
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        LOGERRNO("getrlimit\n");
        return false;
    }

    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;

    for (int fd = STDERR_FILENO + 1; fd < (int)rl.rlim_max; fd++) {
        bool excluded = false;

        /* Check whether fd is in exclusion list */
        for (size_t i = 0; i < nfds; i++) {
            if (fd == excluded_fds[i]) {
                excluded = true;
                break;
            }
        }

        if (!excluded)
            close(fd);
    }

    return true;
}

static bool dup_open_fd(const int srcfd, const int dstfd)
{
    if (close(dstfd) == -1 && errno == EBADF) {
        LOGERRNOV("Destination file descriptor %d was not open\n", dstfd);
        return false;
    }

    return (dup2(srcfd, STDIN_FILENO) == STDIN_FILENO);
}
