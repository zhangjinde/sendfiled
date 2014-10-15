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

#define _GNU_SOURCE 1

#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "protocol.h"
#include "unix_socket_client.h"

ssize_t us_sendv(const int fd,
                 const struct iovec* iovs, size_t niovs,
                 const int* fds_to_send, const size_t nfds)
{
    struct msghdr msg = {
        .msg_iov = (struct iovec*)iovs,
        .msg_iovlen = niovs
    };

    struct ucred cred = {
        .uid = geteuid(),
        .gid = getegid(),
        .pid = getpid()
    };

    uint8_t cmsg_buf [CMSG_SPACE(sizeof(int) * PROT_MAXFDS) +
                      CMSG_SPACE(sizeof(cred))] = {0};

    us_attach_fds_and_creds(&msg, cmsg_buf, fds_to_send, nfds,
                            SCM_CREDENTIALS, &cred, sizeof(cred));

    return sendmsg(fd, &msg, 0);
}
