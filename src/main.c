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

#define _XOPEN_SOURCE 500       /* For chroot, primarily */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <fiod_config.h>

#include "fiod.h"

#include "impl/errors.h"
#include "impl/process.h"
#include "impl/server.h"
#include "impl/unix_socket_server.h"

static const long OPEN_FD_TIMEOUT_MS_MAX = 60 * 60 * 1000;

static void print_usage(const char* progname, long fd_timeout_ms);
static bool sync_parent(int status_code);
static long opt_strtol(const char*);
static bool chroot_and_drop_privs(const char* root_dir);

int main(const int argc, char** argv)
{
    const char* root_dir = NULL;
    const char* name = NULL;
    long maxfiles = 0;
    bool do_sync = false;
    long fd_timeout_ms = 30000;
    bool daemonise = false;

    int opt;
    while ((opt = getopt(argc, argv, "+s:n:t:r:pd")) != -1) {
        switch (opt) {
        case 'r':
            root_dir = optarg;
            break;
        case 's':
            name = optarg;
            break;

        case 'n': {
            maxfiles = opt_strtol(optarg);
            if (maxfiles == -1) {
                const int tmp = errno;
                fprintf(stderr, "Invalid value '%s' for max files\n", optarg);
                errno = tmp;
                return EXIT_FAILURE;
            }
        } break;

        case 't':
            fd_timeout_ms = opt_strtol(optarg);
            if (fd_timeout_ms == -1 || fd_timeout_ms > OPEN_FD_TIMEOUT_MS_MAX) {
                const int tmp = errno;
                fprintf(stderr,
                        "Invalid value '%s' for open file descriptor timeout\n",
                        optarg);
                errno = tmp;
                return EXIT_FAILURE;
            }
            break;

        case 'p':
            do_sync = true;
            break;

        case 'd':
            daemonise = true;
            break;

        case '?':
            return EXIT_FAILURE;

        default:
            print_usage(argv[0], fd_timeout_ms);
            return EXIT_FAILURE;
        }
    }

    if (!root_dir || !name || (maxfiles == 0)) {
        print_usage(argv[0], fd_timeout_ms);
        return EXIT_FAILURE;
    }

    /* Ignore SIGPIPE */
    sigset_t sigmask;
    if (sigemptyset(&sigmask) == -1 ||
        sigaddset(&sigmask, SIGPIPE) == -1 ||
        sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        LOGERRNO("Couldn't ignore SIGPIPE");
        return EXIT_FAILURE;;
    }

    const int requestfd = us_serve(name, getuid(), getgid());
    if (requestfd == -1) {
        if (do_sync && !sync_parent(errno))
            LOGERRNO("Failed to write errno to sync fd");
        return EXIT_FAILURE;
    }

    if (do_sync) {
        if (!sync_parent(0)) {
            LOGERRNO("Failed to sync with parent");
            return EXIT_FAILURE;
        }
        close(PROC_SYNCFD);
    }

    if (daemonise && !proc_daemonise(&requestfd, 1))
        return EXIT_FAILURE;

    openlog(FIOD_PROGNAME, LOG_NDELAY | LOG_CONS | LOG_PID, LOG_DAEMON);

    if (!chroot_and_drop_privs(root_dir)) {
        us_stop_serving(name, requestfd);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO,
           "Starting; name: %s; root_dir: %s;"
           " maxfiles: %ld; fd_timeout_ms: %ld\n",
           name, root_dir, maxfiles, fd_timeout_ms);

    const bool success = srv_run(requestfd, (int)maxfiles, fd_timeout_ms);

    if (!success) {
        syslog(LOG_EMERG, "srv_run() failed [%s]; server shutting down\n",
               strerror(errno));
    } else {
        syslog(LOG_INFO, "Shutting down\n");
    }

    us_stop_serving(name, requestfd);

    closelog();

    return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}

static bool chroot_and_drop_privs(const char* root_dir)
{
    const uid_t ruid = getuid();
    const uid_t euid = geteuid();

    if (ruid == 0) {
        syslog(LOG_ERR, "Refusing to run as root\n");
        return false;
    }

    if (euid != 0) {
        syslog(LOG_ERR, "Executable doesn't appear to be setuid\n");
        return false;
    }

    if (chroot(root_dir) == -1) {
        syslog(LOG_ERR, "Couldn't chroot to %s\n", root_dir);
        return false;
    }

    if (chdir("/") == -1) {
        syslog(LOG_ERR, "Couldn't chdir to '/'\n");
        return false;
    }

    if (setuid(ruid) == -1) {
        syslog(LOG_ERR, "Couldn't setuid to UID %d: %s\n",
               ruid, strerror(errno));
        return false;
    }

    return true;
}

static long opt_strtol(const char* s)
{
    errno = 0;
    const long l = strtol(s, NULL, 10);

    if (errno != 0 || l == 0 || l == LONG_MIN || l == LONG_MAX) {
        if (errno != 0)
            LOGERRNO("strtol");
        return -1;
    } else {
        return l;
    }
}

static void print_usage(const char* progname, const long fd_timeout_ms)
{
    printf("Usage:\n"
           "%s\n"
           "-r <root_dir>\n"
           "-s <server_name>\n"
           "-n <maxfiles>\n"
           "[-p (sync with parent process)]\n"
           "[-t <open_fd_timeout_ms> (default: %ld)]\n",
           progname, fd_timeout_ms);
}

static bool sync_parent(const int status)
{
    return (write(PROC_SYNCFD, &status, sizeof(status)) == sizeof(status));
}
