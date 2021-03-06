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

#include <sys/socket.h>
#include <sys/un.h>

#include <sys/stat.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "errors.h"
#include "log.h"
#include "unix_socket_server.h"
#include "unix_sockets.h"
#include "util.h"

/**
   The invalid process ID. Negative values are dangerous because I don't want to
   make assumptions about the signedness of the pid_t type. Some OSes put system
   processes at PID 0, but that's fine as long as no user process ever gets PID
   0.
 */
const pid_t US_INVALID_PID = 0;

/* Defined in unix_sockets_<platform>.c */
int us_socket(int, int, int);

int us_serve(const char* sockdir,
             const char* srvname,
             const uid_t socket_uid, const uid_t socket_gid)
{
    const int fd = us_socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;

    if (!us_set_passcred_option(fd))
        goto fail1;

    /* FIXME: this could reside in the DATA segment as there are no other
       threads at this time. */
    const char* const sockpath = us_make_sockpath(sockdir, srvname);
    if (!sockpath)
        goto fail1;

    const size_t sockpath_len = strlen(sockpath);

    struct sockaddr_un un = {
        .sun_family = AF_UNIX
    };
    memcpy(un.sun_path, sockpath, sockpath_len + 1);
    const socklen_t addrlen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                          sockpath_len + 1);
    if (bind(fd, (struct sockaddr*)&un, addrlen) == -1) {
        if (errno != EADDRINUSE)
            LOGERRNO("bind");
        goto fail2;
    }

    if (chown(sockpath, socket_uid, socket_gid) == -1) {
        LOGERRNO("chown");
        goto fail2;
    }

    if (chmod(sockpath, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
        LOGERRNO("chmod");
        goto fail2;
    }

    free((void*)sockpath);

    return fd;

 fail2:
    PRESERVE_ERRNO(free((void*)sockpath));
 fail1:
    PRESERVE_ERRNO(close(fd));

    return -1;
}

void us_stop_serving(const char* sockdir,
                     const char* srv_name,
                     const int listenfd)
{
    close(listenfd);

    const char* sockpath = us_make_sockpath(sockdir, srv_name);
    if (!sockpath) {
        sfd_log(LOG_ALERT,
                "Unable to generate UNIX socket pathname [errno: %m]\n");
    } else {
        unlink(sockpath);
        free((void*)sockpath);
    }
}

bool us_get_fds_and_creds(struct msghdr* msg,
                          int* fds, size_t* nfds,
                          const int cred_cmsg_type,
                          void* creds)
{
    bool got_fds = false;
    bool got_creds = false;

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
         cmsg != NULL;
         cmsg = CMSG_NXTHDR(msg, cmsg)) {

        assert (cmsg->cmsg_level == SOL_SOCKET);

        const size_t data_off = (size_t)((char*)CMSG_DATA(cmsg) - (char*)cmsg);
        const size_t payload_size = (cmsg->cmsg_len - data_off);

        if (cmsg->cmsg_type == SCM_RIGHTS) {
            /* File descriptors */
            got_fds = true;

            assert (cmsg->cmsg_len >= CMSG_LEN(sizeof(int)));

            memset(fds, 0, sizeof(*fds) * *nfds);
            memcpy(fds, CMSG_DATA(cmsg), payload_size);

            *nfds = (payload_size / sizeof(int));

        } else if (cmsg->cmsg_type == cred_cmsg_type) {
            /* Client process credentials */
            got_creds = true;

            memcpy(creds, CMSG_DATA(cmsg), payload_size);
        }
    }

    if (!got_fds)
        *nfds = 0;

    return got_creds;
}
