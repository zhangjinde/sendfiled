/*
  Copyright (c) 2016, Francois Kritzinger
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

#define _GNU_SOURCE 1

#include <sys/socket.h>
#include <sys/uio.h>

#include <assert.h>

#include "protocol.h"
#include "unix_socket_server.h"
#include "util.h"

bool us_set_passcred_option(const int fd)
{
    const int on = 1;
    return (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) == 0);
}

ssize_t us_recv(int srv_fd,
                void* buf, size_t len,
                int* recvd_fds, size_t* nfds,
                uid_t* uid, gid_t* gid, pid_t* pid)
{
    assert (recvd_fds && nfds && *nfds > 0);

    struct iovec iov = {
        .iov_base = buf,
        .iov_len = len
    };

    struct ucred creds;

    char cmsg_buf [CMSG_SPACE(sizeof(int) * PROT_MAXFDS) +
                   CMSG_SPACE(sizeof(creds))] = {0};

    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = cmsg_buf,
        .msg_controllen = sizeof(cmsg_buf)
    };

    const ssize_t nrecvd = recvmsg(srv_fd, &msg, 0);
    if (nrecvd == -1)
        return -1;

    if (msg.msg_flags == MSG_TRUNC ||
        msg.msg_flags == MSG_CTRUNC) {
        /* Datagram or ancilliary data was truncated */
        errno = ERANGE;
        return -1;
    }

    if (!us_get_fds_and_creds(&msg,
                              recvd_fds, nfds,
                              SCM_CREDENTIALS, &creds)) {
        errno = EBADF;
        return -1;
    }

    if (*nfds > 0) {
        set_nonblock(recvd_fds[0], true);
        if (*nfds == 2)
            set_nonblock(recvd_fds[1], true);
    }

    *uid = creds.uid;
    *gid = creds.gid;
    *pid = creds.pid;

    return nrecvd;
}
