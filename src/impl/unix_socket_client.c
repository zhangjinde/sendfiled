/*
  Copyright (c) 2015, Francois Kritzinger
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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "unix_socket_client.h"
#include "unix_sockets.h"
#include "util.h"

/* Defined in unix_sockets_<platform>.c */
int us_socket(int, int, int);

int us_connect(const char* sockdir, const char* srvname)
{
    struct sockaddr_un srv_addr = {
        .sun_family = AF_UNIX
    };

    const int fd = us_socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;

    const char* sockpath = us_make_sockpath(sockdir, srvname);
    if (!sockpath)
        goto fail;

    const size_t sockpath_len = strlen(sockpath);

    memcpy(srv_addr.sun_path, sockpath, sockpath_len + 1);

    free((void*)sockpath);

    socklen_t addrlen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                    sockpath_len + 1);
    if (connect(fd, (struct sockaddr*)&srv_addr, addrlen) == -1)
        goto fail;

    return fd;

 fail:
    PRESERVE_ERRNO(close(fd));

    return -1;
}

void us_attach_fds_and_creds(struct msghdr* msg,
                             uint8_t* cmsg_buf,
                             const int* fds, const size_t nfds,
                             const int cred_type,
                             const void* const creds, const size_t creds_size)
{
    struct cmsghdr* cmsg = NULL;

    if (fds && nfds > 0) {
        msg->msg_control = cmsg_buf;
        const size_t rightslen = sizeof(int) * nfds;
        msg->msg_controllen += (socklen_t)us_cmsg_space(rightslen);

        cmsg = CMSG_FIRSTHDR(msg);

        *cmsg = (struct cmsghdr) {
            .cmsg_level = SOL_SOCKET,
            .cmsg_type = SCM_RIGHTS,
            .cmsg_len = (socklen_t)us_cmsg_len(rightslen)
        };
        memcpy(CMSG_DATA(cmsg), fds, rightslen);
    }

    if (creds && creds_size > 0) {
        msg->msg_control = cmsg_buf;
        msg->msg_controllen += (socklen_t)us_cmsg_space(creds_size);

        cmsg = (cmsg ? CMSG_NXTHDR(msg, cmsg) : CMSG_FIRSTHDR(msg));

        *cmsg = (struct cmsghdr) {
            .cmsg_level = SOL_SOCKET,
            .cmsg_type = cred_type,
            .cmsg_len = (socklen_t)us_cmsg_len(creds_size)
        };
        memcpy(CMSG_DATA(cmsg), creds, creds_size);
    }
}
