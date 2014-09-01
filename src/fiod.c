#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "impl/eventloop.h"
#include "impl/process.h"

#include "attributes.h"
#include "fiod.h"

int fiod_spawn(const char* root)
{
    const int pid = fork();

    if (pid < 0) {
        /* error */
        fprintf(stderr, "%s: fork failed\n", __func__);
        return pid;
    } else if (pid > 0) {
        /* parent */
        return pid;
    }

    proc_common_init(pid, root);

    printf("%s: starting event loop\n", __func__);

    run_eventloop();

    printf("%s: event loop done; process exiting\n", __func__);

    exit(0);
}

int fiod_shutdown(int pid)
{
    int status;

    const pid_t err = waitpid(pid, &status, 0);

    if (err == -1) {
        fprintf(stderr, "%s: waitpid failed\n", __func__);
        return err;
    }

    return status;
}

/* -------------- Internal implementations ------------ */
