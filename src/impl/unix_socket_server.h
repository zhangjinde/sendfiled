/*
  Copyright (c) 2016, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef UNIX_SOCKET_SERVER_H
#define UNIX_SOCKET_SERVER_H

#include <sys/types.h>

#include <stdbool.h>

struct msghdr;

/** Value denoting an invalid PID */
extern const pid_t US_INVALID_PID;

#ifdef __cplusplus
extern "C" {
#endif

    int us_serve(const char* sockdir,
                 const char* srv_instance_name,
                 uid_t socket_uid, uid_t socket_gid);

    void us_stop_serving(const char* sockdir,
                         const char* srv_instance_name,
                         int request_fd);

    /**
       Receives a message from a client.

       @param fd The socket file descriptor

       @param buf The destination buffer

       @param len The size of the buffer

       @param recvd_fds File descriptors received from the client. There will
       always be at least one: the status channel descriptor. 'Send' requests
       will also have a second one: the destination file descriptor.

       @param nfds The number of file descriptors in @a recvd_fds

       @param[out] uid The user ID of the client process

       @param[out] gid The group ID of the client process

       @param[out] pid The client process ID

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
                    uid_t* uid, gid_t* gid, pid_t* pid);

    /**
        Turns on the credential-passing option on a socket.

        @note Platform-specific differences include the option name and option
        level (socket on Linux, protocol on FreeBSD).
    */
    bool us_set_passcred_option(int fd);

    bool us_get_fds_and_creds(struct msghdr* msg,
                              int* fds, size_t* nfds,
                              int cred_cmsg_type,
                              void* creds);

#ifdef __cplusplus
}
#endif

#endif
