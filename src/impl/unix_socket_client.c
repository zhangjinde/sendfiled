#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "errors.h"
#include "unix_socket_client.h"

/** FIXME: not to be hard-coded */
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

 fail1: {                         /* socket() */
        const int tmp = errno;
        close(fd);
        errno = tmp;
        return -1;
    }
}

void us_attach_fds_and_creds(struct msghdr* msg,
                             uint8_t* cmsg_buf,
                             const int* fds, const size_t nfds,
                             const int cred_type,
                             void* creds, size_t creds_size)
{
    msg->msg_control = cmsg_buf;
    msg->msg_controllen = (CMSG_SPACE(sizeof(int) * nfds) +
                           CMSG_SPACE(creds_size));

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);

    *cmsg = (struct cmsghdr) {
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
        .cmsg_len = CMSG_LEN(sizeof(int) * nfds)
    };
    memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * nfds);

    cmsg = CMSG_NXTHDR(msg, cmsg);

    *cmsg = (struct cmsghdr) {
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = cred_type,
        .cmsg_len = CMSG_LEN(creds_size)
    };
    memcpy(CMSG_DATA(cmsg), creds, creds_size);
}
