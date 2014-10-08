#ifndef UNIX_SOCKET_CLIENT_H
#define UNIX_SOCKET_CLIENT_H

#include <sys/types.h>

#include <stdint.h>

struct iovec;
struct msghdr;

int us_connect(const char* server_name);

ssize_t us_sendv(int srv_fd,
                 const struct iovec* iovs, size_t niovs,
                 const int* fds_to_send, size_t nfds);

void us_attach_fds_and_creds(struct msghdr* msg,
                             uint8_t* cmsg_buf,
                             const int* fds, size_t nfds,
                             int cred_type,
                             void* creds, size_t creds_size);

#endif
