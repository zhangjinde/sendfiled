#define _GNU_SOURCE 1

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <string.h>

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

    uint8_t cmsg_buf [CMSG_SPACE(sizeof(int) * US_MAXFDS) +
                      CMSG_SPACE(sizeof(cred))] = {0};

    us_attach_fds_and_creds(&msg, cmsg_buf, fds_to_send, nfds,
                            SCM_CREDENTIALS, &cred, sizeof(cred));

    return sendmsg(fd, &msg, 0);
}
