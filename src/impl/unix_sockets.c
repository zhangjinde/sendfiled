#define _POSIX_C_SOURCE 200809L /* To enable S_ISSOCK on Linux */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../attributes.h"
#include "unix_sockets.h"

#define MAXFDS 2

/* Defined in unix_sockets_<platform>.c */
int us_socket(int, int, int);

static const char* const TMPDIR = "/tmp/";

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

    unlink(name);

    const int fd = us_socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;

    memcpy(un.sun_path, name, namelen);

    const socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                      namelen);

    if (bind(fd, (struct sockaddr*)&un, len) == -1)
        goto fail;

    return fd;

 fail:                          /* socket() */
    close(fd);
    return -1;
}

void us_stop_serving(const char* name, const int listenfd)
{
    close(listenfd);
    unlink(name);
}

static bool cli_sockfile_ok(const char* name, uid_t* uid)
{
    struct stat st;

    if (stat(name, &st) == -1)
        return false;

    if (S_ISSOCK(st.st_mode) == 0 ||
        (st.st_mode & (S_IRWXG | S_IRWXO)) ||
        (st.st_mode & S_IRWXU) != S_IRWXU) {
        return false;
    }

    const time_t staletime = time(NULL) - 30;
    if (st.st_atime < staletime ||
        st.st_ctime < staletime ||
        st.st_mtime < staletime) {
        return false;
    }

    if (uid)
        *uid = st.st_uid;

    return true;
}

int us_accept(int listenfd, uid_t* uid)
{
    struct sockaddr_un un;

    socklen_t len = sizeof(un);

    const int fd = accept(listenfd, (struct sockaddr*)&un, &len);
    if (fd == -1)
        return -1;

    char* name = malloc(sizeof(un.sun_path) + 1);
    if (!name)
        goto fail1;

    len -= offsetof(struct sockaddr_un, sun_path);
    memcpy(name, un.sun_path, len);
    name[len] = '\0';

    if (!cli_sockfile_ok(name, uid))
        goto fail2;

    unlink(name);
    free(name);

    return fd;

 fail2:                         /* malloc() */
    free(name);
 fail1:                         /* accept() */
    close(fd);
    return -1;
}

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

    char xxx[256] = {0};
    memcpy(xxx, srv_addr.sun_path, strlen(TMPDIR) + namelen);

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

    char cmsg_buf[CMSG_SPACE(sizeof(int) * MAXFDS)] = {0};

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

    char cmsg_buf[CMSG_SPACE(sizeof(int) * MAXFDS)];

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

            memcpy(recvd_fds, CMSG_DATA(cmsg), sizeof(int) * *nfds);

            *nfds = (payload_size / sizeof(int));
        }
    }

    return nrecvd;
}
