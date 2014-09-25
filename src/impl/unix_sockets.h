#ifndef FIOD_UNIX_SOCKETS_H
#define FIOD_UNIX_SOCKETS_H

#include <sys/types.h>

#include <stddef.h>

#include <stdbool.h>

struct iovec;

#ifdef __cplusplus
extern "C" {
#endif

    int us_serve(const char* name);

    void us_stop_serving(const char* name, int listenfd);

    int us_connect(const char* server_name);

    ssize_t us_sendv(int srv_fd,
                     const struct iovec* iovs, size_t niovs,
                     const int* fds_to_send, size_t nfds);

    ssize_t us_recv(int srv_fd,
                    void* buf, size_t len,
                    int* recvd_fds, size_t* nfds);

#ifdef __cplusplus
}
#endif

#endif
