#define _GNU_SOURCE 1

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "unix_socket_server.h"

#include <stdio.h>

int us_passcred_option(void)
{
    return SO_PASSCRED;
}

ssize_t us_recv(int srv_fd,
                void* buf, size_t len,
                int* recvd_fds, size_t* nfds,
                uid_t* uid, gid_t* gid)
{
    assert (recvd_fds && nfds && *nfds > 0);

    struct iovec iov = {
        .iov_base = buf,
        .iov_len = len
    };

    struct ucred creds;

    char cmsg_buf [CMSG_SPACE(sizeof(int) * US_MAXFDS) +
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

    if (!us_get_fds_and_creds(&msg,
                              recvd_fds, nfds,
                              SCM_CREDENTIALS, &creds, sizeof(creds))) {
        return -1;
    }

    *uid = creds.uid;
    *gid = creds.gid;

    return nrecvd;
}
