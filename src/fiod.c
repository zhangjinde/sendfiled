#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "impl/process.h"
#include "impl/protocol.h"
#include "impl/server.h"
#include "impl/unix_sockets.h"

#include "attributes.h"
#include "fiod.h"

int fiod_pipe(int fds[2], int flags);

pid_t fiod_spawn(const char* name, const char* root, const int maxfiles)
{
    /* Pipe used to sync with child */
    int pfd[2];

    if (fiod_pipe(pfd, 0) == -1)
        return -1;

    const pid_t pid = fork();

    if (pid == -1) {
        fprintf(stderr, "%s: fork failed\n", __func__);
        close(pfd[0]);
        close(pfd[1]);
        return -1;

    } else if (pid > 0) {
        close(pfd[1]);

        int b = 0;
        if (read(pfd[0], &b, 1) != 1) {
            fprintf(stderr, "%s: read error synching with child\n", __func__);
            close(pfd[0]);
            return -1;
        }

        close(pfd[0]);
        return pid;
    }

    close(pfd[0]);

    proc_common_init(root, pfd[1]);

    const int listenfd = us_serve(name);
    if (listenfd == -1)
        exit(EXIT_FAILURE);

    int b = 1;
    if (write(pfd[1], &b, 1) != 1) {
        fprintf(stderr, "%s: write error synching with parent\n", __func__);
        return false;
    }

    close(pfd[1]);

    printf("%s: starting event loop\n", __func__);

    const bool success = srv_run(listenfd, maxfiles);

    printf("%s: event loop done\n", __func__);

    us_stop_serving(name, listenfd);

    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

int fiod_connect(const char* name)
{
    return us_connect(name);
}

int fiod_shutdown(const pid_t pid)
{
    int status;

    /* XXX TODO Send shutdown signal to child */

    if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "%s: waitpid failed\n", __func__);
        status = -1;
    }

    return status;
}

int fiod_send(int srv_sockfd,
              const char* filename,
              int dest_sockfd,
              off_t offset UNUSED, size_t count UNUSED)
{
    int fds[3];

    if (pipe(fds) == -1)
        return -1;

    fds[2] = dest_sockfd;

    struct prot_request_m req;
    if (!prot_marshal_send(&req, filename))
        goto fail1;

    if (us_sendv(srv_sockfd,
                 req.iovs, 2,
                 &fds[1], 2) == -1) {
        goto fail1;
    }

    /* No use for the write end of the pipe in this process */
    close (fds[1]);

    return fds[0];

 fail1:
    close(fds[0]);
    close(fds[1]);

    return -1;
}

/* int fiod_read(const int sockfd, */
/*               const char* filename, */
/*               off_t offset UNUSED, size_t count UNUSED) */
/* { */
/*     int fds[3]; */

/*     if (pipe(fds) == -1) */
/*         return -1; */

/*     struct prot_pdu req; */

/*     if (!prot_marshal_read(&req, filename)) */
/*         goto fail1; */

/*     if (us_sendv(sockfd, req.iovs, PROT_PDU_NIOVS, &fds[1], 1) == -1) */
/*         goto fail1; */

/*     /\* No use for the write end of the pipe in this process *\/ */
/*     close (fds[1]); */

/*     return fds[0]; */

/*  fail1: */
/*     close(fds[0]); */
/*     close(fds[1]); */

/*     return -1; */
/* } */

/* -------------- Internal implementations ------------ */
