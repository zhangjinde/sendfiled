#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "errors.h"
#include "unix_socket_server.h"

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

    /* Turn on the 'pass credentials' socket option */
    const int on = 1;
    if (setsockopt(fd,
                   SOL_SOCKET, us_passcred_option(),
                   &on, sizeof(on)) < 0) {
        goto fail;
    }

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

bool us_get_fds_and_creds(struct msghdr* msg,
                          int* fds, size_t* nfds,
                          const int cred_cmsg_type,
                          void* creds, const size_t creds_size)
{
    bool got_fds = false;
    bool got_creds = false;

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
         cmsg != NULL;
         cmsg = CMSG_NXTHDR(msg, cmsg)) {

        assert (cmsg->cmsg_level == SOL_SOCKET);

        if (cmsg->cmsg_type == SCM_RIGHTS) {
            /* File descriptors */
            /* There must be at least one fd */
            assert (cmsg->cmsg_len >= CMSG_LEN(sizeof(int)));

            got_fds = true;

            const size_t data_off = (size_t)((char*)CMSG_DATA(cmsg) -
                                             (char*)cmsg);
            const size_t payload_size = (cmsg->cmsg_len - data_off);

            memset(fds, 0, sizeof(*fds) * *nfds);
            memcpy(fds, CMSG_DATA(cmsg), payload_size);

            *nfds = (payload_size / sizeof(int));

        } else if (cmsg->cmsg_type == cred_cmsg_type) {
            /* Client process credentials */
            assert (cmsg->cmsg_len == CMSG_LEN(creds_size));

            got_creds = true;

            memcpy(creds, CMSG_DATA(cmsg), creds_size);
        }
    }

    return (got_fds && got_creds);
}
