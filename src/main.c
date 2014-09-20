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
    if (argc < 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* root_dir = argv[1];
    const char* name = argv[2];

    errno = 0;
    const long maxfiles = strtol(argv[3], NULL, 10);
    if (errno != 0 || maxfiles == 0 ||
        maxfiles == LONG_MIN || maxfiles == LONG_MAX) {
        const int tmp = errno;
        fprintf(stderr, "Invalid value '%s' for max files\n", argv[3]);
        errno = tmp;
        if (errno != 0)
            LOGERRNO("\n");
        return EXIT_FAILURE;
    }

    printf("root dir: %s; name: %s\n", root_dir, name);

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
    printf("Usage: %s <root_directory> <server_name>\n", progname);
}
