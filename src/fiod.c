#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "impl/errors.h"
#include "impl/fiod.h"
#include "impl/process.h"
#include "impl/protocol_client.h"
#include "impl/server.h"
#include "impl/unix_socket_client.h"
#include "impl/util.h"

#include "fiod.h"

static int wait_child(pid_t pid);

pid_t fiod_spawn(const char* name,
                 const char* root,
                 const int maxfiles,
                 const int open_fd_timeout_ms)
{
    /* Pipe used to sync with child */
    int pfd[2];

    if (fiod_pipe(pfd, O_CLOEXEC) == -1)
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

    /* Descriptor to which status codes (0 or errno) is written (the write end
       of the pipe shared with the parent) */
    int statfd = pfd[1];

    /* Dupe the status fd to fd 3 and then close it */
    if (statfd != 3) {
        if (dup2(statfd, 3) == -1)
            goto fail;
        close(statfd);
        statfd = 3;
    }

    if (!fiod_exec_server("build/fiod", name, root, maxfiles, open_fd_timeout_ms))
        LOGERRNO("Couldn't exec server process\n");

 fail:
    write(statfd, &errno, sizeof(errno));
    exit(EXIT_FAILURE);
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

#define REQ_IOVS(req) {                                       \
        (struct iovec) { .iov_base = &req,                    \
                .iov_len = PROT_REQ_BASE_SIZE },              \
            (struct iovec) { .iov_base = (void*)req.filename, \
                    .iov_len = req.filename_len + 1 }         \
    }

int fiod_read(const int sockfd,
              const char* filename,
              const loff_t offset,
              const size_t len,
              const bool dest_fd_nonblock)
{
    int fds[2];

    if (fiod_pipe(fds, O_NONBLOCK | O_CLOEXEC) == -1)
        return -1;

    if (!dest_fd_nonblock && !set_nonblock(fds[0], false))
        goto fail;

    struct prot_request req;
    if (!prot_marshal_read(&req, filename, offset, len))
        goto fail;

    struct iovec iovs[] = REQ_IOVS(req);

    const ssize_t nsent = us_sendv(sockfd, iovs, 2, &fds[1], 1);
    if (nsent == -1)
        goto fail;

    /* No use for the write end of the pipe in this process */
    close (fds[1]);

    return fds[0];

 fail:
    close(fds[0]);
    close(fds[1]);

    return -1;
}

int fiod_open(int srv_sockfd,
              const char* filename,
              loff_t offset, size_t len,
              bool stat_fd_nonblock)
{
    int fds[2];

    if (fiod_pipe(fds, O_NONBLOCK | O_CLOEXEC) == -1)
        return -1;

    if (!stat_fd_nonblock && !set_nonblock(fds[0], false))
        goto fail;

    struct prot_request req;
    if (!prot_marshal_file_open(&req, filename, offset, len))
        goto fail;

    struct iovec iovs[] = REQ_IOVS(req);

    if (us_sendv(srv_sockfd, iovs, 2, &fds[1], 1) == -1)
        goto fail;

    close(fds[1]);

    return fds[0];

 fail:
    close(fds[0]);
    close(fds[1]);

    return -1;
}

int fiod_send(const int srv_sockfd,
              const char* filename,
              const int dest_fd,
              const loff_t offset,
              const size_t len,
              const bool stat_fd_nonblock)
{
    int fds[3];

    if (fiod_pipe(fds, O_NONBLOCK | O_CLOEXEC) == -1)
        return -1;

    if (!stat_fd_nonblock && !set_nonblock(fds[0], false))
        goto fail;

    fds[2] = dest_fd;

    struct prot_request req;
    if (!prot_marshal_send(&req, filename, offset, len))
        goto fail;

    struct iovec iovs[] = REQ_IOVS(req);

    if (us_sendv(srv_sockfd, iovs, 2, &fds[1], 2) == -1)
        goto fail;

    /* No use for the write end of the pipe in this process */
    close (fds[1]);

    return fds[0];

 fail:
    close(fds[0]);
    close(fds[1]);

    return -1;
}

bool fiod_send_open(const int srv_sockfd,
                    const size_t txnid,
                    const int dest_fd)
{
    struct prot_send_open pdu;
    prot_marshal_send_open(&pdu, txnid);

    struct iovec iov = {
        .iov_base = &pdu,
        .iov_len = sizeof(pdu)
    };

    if (us_sendv(srv_sockfd, &iov, 1, &dest_fd, 1) == -1)
        return false;

    return true;
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
