#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "errors.h"
#include "unix_sockets.h"

/* Defined in unix_sockets_<platform>.c */
int us_socket(int, int, int);

int us_serve(const char* name)
{
    struct sockaddr_un un = {
        .sun_family = AF_UNIX
    };

    const size_t namelen = strnlen(name, sizeof(un.sun_path));

    if (namelen == sizeof(un.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    const int fd = us_socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;

    memcpy(un.sun_path, name, namelen);

    const socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                      namelen);

    if (bind(fd, (struct sockaddr*)&un, len) == -1)
        goto fail;

    return fd;

 fail: {
        const int tmp = errno;
        close(fd);
        errno = tmp;
        return -1;
    }
}

void us_stop_serving(const char* name, const int listenfd)
{
    close(listenfd);
    unlink(name);
}

ssize_t us_recv(int srv_fd,
                void* buf, size_t len,
                int* recvd_fds, size_t* nfds)
{
    assert (nfds);

    struct iovec iov = {
        .iov_base = buf,
        .iov_len = len
    };

    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1
    };

    const bool recv_fds = (recvd_fds && nfds && *nfds > 0);

    char cmsg_buf[CMSG_SPACE(sizeof(int) * US_MAXFDS)];

    if (recv_fds) {
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);
    }

    const ssize_t nrecvd = recvmsg(srv_fd, &msg, 0);

    if (nrecvd == -1)
        return -1;

    if (recv_fds) {
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg) {
            assert (cmsg->cmsg_level == SOL_SOCKET);
            /* There must be at least one fd */
            assert (cmsg->cmsg_len >= CMSG_LEN(sizeof(int)));

            const size_t data_off = (size_t)((char*)CMSG_DATA(cmsg) -
                                             (char*)cmsg);
            const size_t payload_size = (cmsg->cmsg_len - data_off);

            memset(recvd_fds, 0, sizeof(*recvd_fds) * *nfds);
            memcpy(recvd_fds, CMSG_DATA(cmsg), payload_size);

            *nfds = (payload_size / sizeof(int));
        }
    }

    return nrecvd;
}
