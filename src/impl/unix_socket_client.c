#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../attributes.h"
#include "errors.h"
#include "unix_sockets.h"

static const char* const TMPDIR = "/tmp/";

/* Defined in unix_sockets_<platform>.c */
int us_socket(int, int, int);

int us_connect(const char* srvname)
{
    struct sockaddr_un srv_addr = {
        .sun_family = AF_UNIX
    };

    const size_t namelen = strnlen(srvname, sizeof(srv_addr.sun_path));

    if (namelen == sizeof(srv_addr.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    const int fd = us_socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;

    strcpy(srv_addr.sun_path, TMPDIR);
    memcpy(srv_addr.sun_path + strlen(TMPDIR), srvname, namelen);

    socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                strlen(TMPDIR) + namelen);

    if (connect(fd, (struct sockaddr*)&srv_addr, len) == -1)
        goto fail1;

    return fd;

 fail1:                         /* socket() */
    close(fd);
    return -1;
}

ssize_t us_sendv(int fd,
                 const struct iovec* iovs, size_t niovs,
                 const int* fds_to_send, const size_t nfds)
{
    struct msghdr msg = {
        .msg_iov = (struct iovec*)iovs,
        .msg_iovlen = niovs
    };

    char cmsg_buf[CMSG_SPACE(sizeof(int) * US_MAXFDS)] = {0};

    if (fds_to_send && nfds > 0) {
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        *cmsg = (struct cmsghdr) {
            .cmsg_level = SOL_SOCKET,
            .cmsg_type = SCM_RIGHTS,
            .cmsg_len = CMSG_LEN(sizeof(int) * nfds)
        };

        memcpy(CMSG_DATA(cmsg), fds_to_send, sizeof(int) * nfds);
    }

    return sendmsg(fd, &msg, 0);
}
