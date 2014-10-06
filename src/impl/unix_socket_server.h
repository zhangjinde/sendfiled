#ifndef UNIX_SOCKET_SERVER_H
#define UNIX_SOCKET_SERVER_H

#include <sys/types.h>

#include <stdbool.h>

#include "unix_sockets.h"

struct msghdr;

#ifdef __cplusplus
extern "C" {
#endif

    int us_serve(const char* name);

    void us_stop_serving(const char* name, int listenfd);

    ssize_t us_recv(int srv_fd,
                    void* buf, size_t len,
                    int* recvd_fds, size_t* nfds,
                    uid_t* uid, gid_t* gid);

    int us_passcred_option(void);

    bool us_get_fds_and_creds(struct msghdr* msg,
                              int* fds, size_t* nfds,
                              int cred_cmsg_type,
                              void* creds, const size_t creds_size);

#ifdef __cplusplus
}
#endif

#endif
