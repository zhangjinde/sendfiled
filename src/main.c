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

#include <syslog.h>
#include <unistd.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "fiod.h"

#include "impl/errors.h"
#include "impl/process.h"
#include "impl/server.h"
#include "impl/unix_socket_server.h"

static const long OPEN_FD_TIMEOUT_MS_MAX = 60 * 60 * 1000;

/*
  File descriptor (opened in parent process by fiod_spawn()) to which server
  startup success (0) or error code is to be written in order to sync with
  parent and to facilitate error reporting in the parent process.
 */
static const int syncfd = 3;

static void print_usage(const char* progname, long fd_timeout_ms);

static bool sync_parent(int stat);

static long opt_strtol(const char* s)
{
    errno = 0;
    const long l = strtol(s, NULL, 10);

    if (errno != 0 || l == 0 || l == LONG_MIN || l == LONG_MAX) {
        if (errno != 0)
            LOGERRNO("\n");
        return -1;
    } else {
        return l;
    }
}

int main(const int argc, char** argv)
{
    const char* root_dir = NULL;
    const char* name = NULL;
    long maxfiles = 0;
    bool do_sync = false;
    long fd_timeout_ms = 30000;
    bool daemonise = false;

    int opt;
    while ((opt = getopt(argc, argv, "+r:s:n:t:pd")) != -1) {
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

    if (!proc_common_init(root_dir, &syncfd, 1)) {
        perror("proc_common_init");
        return EXIT_FAILURE;
    }

    const int requestfd = us_serve(name);
    if (requestfd == -1) {
        if (do_sync && !sync_parent(errno))
            perror("Failed to write errno to sync fd");
        return EXIT_FAILURE;
    }

    if (do_sync) {
        if (!sync_parent(0))
            perror("Failed to sync with parent");
        close (syncfd);
    }

    if (daemonise && !proc_daemonise())
        return EXIT_FAILURE;

    syslog(LOG_INFO,
           "Starting; root dir: %s; name: %s;"
           " maxfiles: %ld; fd_timeout_ms: %ld\n",
           root_dir, name, maxfiles, fd_timeout_ms);

    const bool success = srv_run(requestfd, (int)maxfiles, fd_timeout_ms);

    syslog(LOG_INFO, "Stopping\n");

    us_stop_serving(name, requestfd);

    closelog();

    return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void print_usage(const char* progname, const long fd_timeout_ms)
{
    printf("Usage %s -r <root_directory> -s <server_name> -n <maxfiles>"
           " [-p (sync with parent process)]"
           " [-t <open_fd_timeout_ms> (default: %ld)]\n",
           progname, fd_timeout_ms);
}

static bool sync_parent(const int status)
{
    return (write(syncfd, &status, sizeof(status)) == sizeof(status));
}
