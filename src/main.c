#define _POSIX_C_SOURCE 200809L

#include <unistd.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "fiod.h"

#include "impl/errors.h"
#include "impl/process.h"
#include "impl/server.h"
#include "impl/unix_sockets.h"

static void print_usage(const char* progname);

int main(const int argc, char** argv)
{
    const char* root_dir = NULL;
    const char* name = NULL;
    long maxfiles = 0;

    int opt;
    while ((opt = getopt(argc, argv, "+d:s:n:")) != -1) {
        switch (opt) {
        case 'd':
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

    proc_common_init(root_dir, -1);

    /* proc_daemonise(); */

    const int listenfd = us_serve(name);
    if (listenfd == -1)
        exit(EXIT_FAILURE);

    const bool success = srv_run(listenfd, (int)maxfiles);

    puts("\nGot SIGTERM or SIGINT; exiting\n");

    us_stop_serving(name, listenfd);

    return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void print_usage(const char* progname)
{
    printf("Usage: %s -r <root_directory> -s <server_name> -n <maxfiles>\n",
           progname);
}
