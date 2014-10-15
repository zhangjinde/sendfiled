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

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <assert.h>
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

static bool exec_server(const char* filename,
                        const char* srvname,
                        int maxfiles,
                        int open_fd_timeout_ms);

pid_t fiod_spawn(const char* name,
                 const int maxfiles,
                 const int open_fd_timeout_ms)
{
    /* Pipe used to sync with child */
    int pfd[2];

    if (fiod_pipe(pfd, O_CLOEXEC) == -1) {
        LOGERRNO("fiod_pipe()");
        return -1;
    }

    const pid_t pid = fork();

    if (pid == -1) {
        LOGERRNO("fork");
        PRESERVE_ERRNO(close(pfd[0]));
        PRESERVE_ERRNO(close(pfd[1]));
        return -1;

    } else if (pid > 0) {
        /* In the parent process */

        close(pfd[1]);

        int child_err = 0;
        if (read(pfd[0], &child_err, sizeof(child_err)) != sizeof(child_err)) {
            LOGERRNO("Read error synching with child");
            PRESERVE_ERRNO(close(pfd[0]));
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
                errno = child_err;
                return -1;
            }
        }

        return pid;
    }

    /* In the child process */

    close(pfd[0]);

    /* Descriptor to which the status code (0 or errno) is written (the write
       end of the pipe shared with the parent); the server only writes the
       status/error code after it has bound to its request socket and is
       therefore ready to accept requests.
    */
    int syncfd = pfd[1];

    /* If the write end of the pipe does not have the value expected be the
       server (PROC_SYNCFD), dupe it to PROC_SYNCFD and close it */
    if (syncfd != PROC_SYNCFD) {
        if (dup2(syncfd, PROC_SYNCFD) == -1)
            goto fail;
        close(syncfd);
        syncfd = PROC_SYNCFD;
    }

    if (!proc_init_child(&PROC_SYNCFD, 1))
        goto fail;

    if (!exec_server("build/fiod", name, maxfiles, open_fd_timeout_ms))
        LOGERRNO("Couldn't exec server process");

 fail:
    write(syncfd, &errno, sizeof(errno));
    exit(EXIT_FAILURE);
}

static bool exec_server(const char* path,
                        const char* srvname,
                        const int maxfiles,
                        const int open_fd_timeout_ms)
{
    const long line_max = sysconf(_SC_LINE_MAX);

    assert (line_max > 0);

    const size_t srvname_len = strnlen(srvname, (size_t)line_max + 1);

    if (srvname_len == (size_t)line_max + 1) {
        errno = ENAMETOOLONG;
        return false;
    }

    char maxfiles_str [10];

    int ndigits = snprintf(maxfiles_str,
                           sizeof(maxfiles_str),
                           "%d", maxfiles);

    if (ndigits >= (int)sizeof(maxfiles_str))
        return false;

    char open_fd_timeout_ms_str [10];

    ndigits = snprintf(open_fd_timeout_ms_str,
                       sizeof(open_fd_timeout_ms_str),
                       "%d", open_fd_timeout_ms);

    if (ndigits >= (int)sizeof(open_fd_timeout_ms_str))
        return false;

    const char* args[] = {
        "fiod",
        "-s", srvname,
        "-n", maxfiles_str,
        "-t", open_fd_timeout_ms_str,
        "-p",
        NULL
    };

    execve(path, (char**)args, NULL);

    /* execve does not return on success */
    return false;
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
    PRESERVE_ERRNO(close(fds[0]));
    PRESERVE_ERRNO(close(fds[1]));

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
    PRESERVE_ERRNO(close(fds[0]));
    PRESERVE_ERRNO(close(fds[1]));

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
    PRESERVE_ERRNO(close(fds[0]));
    PRESERVE_ERRNO(close(fds[1]));

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
