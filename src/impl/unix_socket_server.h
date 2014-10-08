#ifndef UNIX_SOCKET_SERVER_H
#define UNIX_SOCKET_SERVER_H

#include <sys/types.h>

#include <stdbool.h>

struct msghdr;

#ifdef __cplusplus
extern "C" {
#endif

    int us_serve(const char* name);

    void us_stop_serving(const char* name, int listenfd);

    /**
       Receives a message from a client.

       @param fd The socket file descriptor

       @param buf The destination buffer

       @param len The size of the buffer

       @param recvd_fds File descriptors received from the client. There will
       always be at least one: the status channel descriptor. 'Send' requests
       will also have a second one: the destination file descriptor.

       @param nfds The number of file descriptors in @a recvd_fds

       @param uid The user ID of the client process

       @param gid The group ID of the client process

       @return The number of bytes received, or -1 on error, in which case errno
       will have been set.

       Errno values:

       @li ERANGE The datagram was too large for the provided buffer and was
       truncated. This is a fatal programmer error and recovery would be
       pointless, so fail outright.
     */
    ssize_t us_recv(int fd,
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
