#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "impl/errors.h"
#include "impl/process.h"
#include "impl/protocol.h"
#include "impl/server.h"
#include "impl/unix_sockets.h"

#include "attributes.h"
#include "fiod.h"

int fiod_pipe(int fds[2], int flags);

static int wait_child(pid_t pid);

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
        /* Parent process */

        close(pfd[1]);

        int child_err = 0;
        if (read(pfd[0], &child_err, sizeof(child_err)) != sizeof(child_err)) {
            LOGERRNO("Read error synching with child\n");
            close(pfd[0]);
            return -1;
        }

        close(pfd[0]);

        if (child_err != 0) {
            if (child_err == EADDRINUSE) {
                printf("%s: daemon named '%s' already running"
                       " (UNIX socket exists)\n",
                       __func__, name);
                wait_child(pid);
                return 0;

            } else {
                fprintf(stderr, "%s: child failed with errno %d [%s]\n",
                        __func__, child_err, strerror(child_err));
                wait_child(pid);
                return -1;
            }
        }

        return pid;
    }

    /* Child process */

    close(pfd[0]);

    proc_common_init(root, pfd[1]);

    int err = 0;

    const int listenfd = us_serve(name);
    if (listenfd == -1)
        err = errno;

    if (write(pfd[1], &err, sizeof(int)) != sizeof(int)) {
        LOGERRNO("Write error synching with parent\n");
        exit(EXIT_FAILURE);
    }

    close(pfd[1]);

    if (err != 0)
        exit(EXIT_FAILURE);

    const bool success = srv_run(listenfd, maxfiles);

    us_stop_serving(name, listenfd);

    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

int fiod_connect(const char* name)
{
    const int fd = us_connect(name);
    if (fd != -1)
        shutdown(fd, SHUT_RD);
    return fd;
}

int fiod_shutdown(const pid_t pid)
{
    if (kill(pid, SIGTERM) == -1) {
        LOGERRNOV("kill(%d, SIGTERM) failed\n", pid);
        return -1;
    }

    return wait_child(pid);
}

int fiod_send(int srv_sockfd,
              const char* filename,
              int dest_sockfd,
              const loff_t offset,
              const size_t len)
{
    int fds[3];

    if (pipe(fds) == -1)
        return -1;

    fds[2] = dest_sockfd;

    struct prot_request_m req;
    if (!prot_marshal_send(&req, filename, offset, len))
        goto fail1;

    if (us_sendv(srv_sockfd, req.iovs, 2, &fds[1], 2) == -1)
        goto fail1;

    /* No use for the write end of the pipe in this process */
    close (fds[1]);

    return fds[0];

 fail1:
    close(fds[0]);
    close(fds[1]);

    return -1;
}

int fiod_read(const int sockfd,
              const char* filename,
              const loff_t offset,
              const size_t len)
{
    int fds[2];

    if (pipe(fds) == -1)
        return -1;

    struct prot_request_m req;
    if (!prot_marshal_read(&req, filename, offset, len))
        goto fail1;

    if (us_sendv(sockfd, req.iovs, 2, &fds[1], 1) == -1)
        goto fail1;

    /* No use for the write end of the pipe in this process */
    close (fds[1]);

    return fds[0];

 fail1:
    close(fds[0]);
    close(fds[1]);

    return -1;
}

/* -------------- Internal implementations ------------ */

static int wait_child(pid_t pid)
{
    int stat;

    if (waitpid(pid, &stat, 0) == -1) {
        LOGERRNOV("waitpid(%d) failed\n", pid);
        return -1;
    }

    return stat;
}
