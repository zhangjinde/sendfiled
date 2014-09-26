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
#include "impl/unix_sockets.h"

/*
  File descriptor (opened in parent process by fiod_spawn()) to which server
  startup success (0) or error code is to be written in order to sync with
  parent and to facilitate error reporting in the parent process.
 */
static const int syncfd = 3;

static void print_usage(const char* progname);

static bool sync_parent(int stat);

int main(const int argc, char** argv)
{
    const char* root_dir = NULL;
    const char* name = NULL;
    long maxfiles = 0;
    bool do_sync = false;

    int opt;
    while ((opt = getopt(argc, argv, "+r:s:n:p")) != -1) {
        switch (opt) {
        case 'r':
            root_dir = optarg;
            break;

        case 's':
            name = optarg;
            break;

        case 'n': {
            errno = 0;
            maxfiles = strtol(optarg, NULL, 10);
            if (errno != 0 || maxfiles == 0 ||
                maxfiles == LONG_MIN || maxfiles == LONG_MAX) {
                const int tmp = errno;
                fprintf(stderr, "Invalid value '%s' for max files\n", optarg);
                errno = tmp;
                if (errno != 0)
                    LOGERRNO("\n");
                return EXIT_FAILURE;
            }
        } break;

        case 'p':
            do_sync = true;
            break;

        case '?':
            return EXIT_FAILURE;

        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!root_dir || !name || (maxfiles == 0)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("root dir: %s; name: %s; maxfiles: %ld\n", root_dir, name, maxfiles);

    proc_common_init(root_dir, &syncfd, 1);

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

    const bool success = srv_run(listenfd, (int)maxfiles);

    puts("\nGot SIGTERM or SIGINT; exiting\n");

    us_stop_serving(name, listenfd);

    return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void print_usage(const char* progname)
{
    printf("Usage %s -r <root_directory> -s <server_name> -n <maxfiles>"
           " [-p (sync with parent process)]\n",
           progname);
}

static bool sync_parent(const int status)
{
    return (write(syncfd, &status, sizeof(status)) == sizeof(status));
}
