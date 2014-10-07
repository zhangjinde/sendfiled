#define _POSIX_C_SOURCE 200809L

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

#include "process.h"

bool proc_daemonise(void)
{
    /*
     * Clear file creation mask.
     */
    umask(0);

    /*
     * Get maximum number of file descriptors.
     */
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        return false;

    /*
     * Become a session leader to lose controlling TTY.
     */
    pid_t pid = fork();
    if (pid < 0)
        return false;
    else if (pid != 0) /* parent */
        exit(EXIT_SUCCESS);
    setsid();

    /*
     * Ensure future opens won’t allocate controlling TTYs.
     */
    struct sigaction sa = {
        .sa_flags = 0,
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
        .sa_handler = SIG_IGN
#pragma GCC diagnostic pop
    };

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return false;
    if ((pid = fork()) < 0)
        return false;
    else if (pid != 0) /* parent */
        exit(EXIT_SUCCESS);

    /*
     * Change the current working directory to the root so
     * we won’t prevent file systems from being unmounted.
     */
    /* if (chdir("/") < 0) */
    /*     exit (EXIT_FAILURE); */

    /* Close stdin, stdout, stderr and attach them to /dev/null */
    close(0);
    close(1);
    close(2);
    int fd0 = open("/dev/null", O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);

    /* /\* openlog(cmd, LOG_CONS, LOG_DAEMON); *\/ */

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        /* syslog(LOG_ERR, "unexpected file descriptors %d %d %d", */
        /*        fd0, fd1, fd2); */
        return false;
    }

    return true;
}
