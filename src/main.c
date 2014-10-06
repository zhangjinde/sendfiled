#define _POSIX_C_SOURCE 200809L

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

    int opt;
    while ((opt = getopt(argc, argv, "+r:s:n:t:p")) != -1) {
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

    printf("root dir: %s; name: %s; maxfiles: %ld; fd_timeout_ms: %ld\n",
           root_dir, name, maxfiles, fd_timeout_ms);

    if (!proc_common_init(root_dir, &syncfd, 1)) {
        perror("proc_common_init");
        return EXIT_FAILURE;
    }

    /* proc_daemonise(); */

    const int listenfd = us_serve(name);
    if (listenfd == -1) {
        if (do_sync && !sync_parent(errno))
            perror("Failed to write errno to sync fd");
        exit(EXIT_FAILURE);
    }

    if (do_sync) {
        if (!sync_parent(0))
            perror("Failed to sync with parent");
        close (syncfd);
    }

    const bool success = srv_run(listenfd, (int)maxfiles, fd_timeout_ms);

    puts("\nGot SIGTERM or SIGINT; exiting\n");

    us_stop_serving(name, listenfd);

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
