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

#ifdef __linux__
#define _XOPEN_SOURCE 500 /* For sigemptyset et al and chroot */
#endif

#include <sys/resource.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "errors.h"
#include "process.h"
#include "util.h"

static bool dup_to_open_fd(int srcfd, int dstfd);

bool proc_init_child(const int* excluded_fds, size_t nfds)
{
    /* Redirect stdin, stdout, and stderr to /dev/null */
    const int nullfd = open("/dev/null", O_RDWR);
    if (nullfd == -1) {
        LOGERRNO("open()");
        return false;
    }

    if (!dup_to_open_fd(nullfd, STDIN_FILENO) ||
        !dup_to_open_fd(nullfd, STDOUT_FILENO) ||
        !dup_to_open_fd(nullfd, STDERR_FILENO)) {
        PRESERVE_ERRNO(close(nullfd));
        return false;
    }

    close(nullfd);

    /* Close all other file descriptors except the specified ones */
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        LOGERRNO("getrlimit");
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

static bool dup_to_open_fd(const int oldfd, const int newfd)
{
    /* As per Linux's dup(2) manpage, closing newfd manually catches errors that
       leaving it to dup2 would not report */
    if (close(newfd) == -1) {
        LOGERRNOV("Couldn't close new file descriptor %d\n", newfd);
        return false;
    }

    return (dup2(oldfd, newfd) == newfd);
}

int proc_chroot(const char* path)
{
    return chroot(path);
}

bool proc_daemonise(const int* noclose_fds, const size_t nfds)
{
    /*
      Clear file creation mask.
    */
    umask(0);

    /*
      Get maximum number of file descriptors.
    */
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        return false;

    /*
      Become a session leader to lose controlling TTY.
    */
    pid_t pid = fork();
    if (pid < 0)
        return false;
    else if (pid != 0) /* parent */
        exit(EXIT_SUCCESS);
    setsid();

    /*
      Ensure future opens won’t allocate controlling TTYs.

      (Not sure what this does, to be honest.)
    */
    struct sigaction sa = {
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#endif
        .sa_handler = SIG_IGN
#ifdef __clang__
#pragma GCC diagnostic pop
#endif
    };
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return false;
    if ((pid = fork()) < 0)
        return false;
    else if (pid != 0) /* parent */
        exit(EXIT_SUCCESS);

    /*
      Change the current working directory to the root so we won’t prevent file
      systems from being unmounted.
    */
    if (chdir("/") == -1)
        return false;

    if (!proc_init_child(noclose_fds, nfds))
        return false;

    return true;
}
