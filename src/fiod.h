/*
  Copyright (c) 2014, Francois Kritzinger
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

/**
   @defgroup mod_client Client API

   Constructs which facilitate interaction with the server.
 */

/**
   @file

   @ingroup mod_client
 */

#ifndef FIOD_FIOD_H
#define FIOD_FIOD_H

#include <sys/types.h>

#include <stdbool.h>

#include "attr.h"
#include "responses.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @addtogroup mod_client
       @{

       @name Server interaction
       @{
     */

    /**
       Spawns a server process.

       @param server_name The server instance name. (The name of the server's
       'listening' UNIX socket will be based on this value.)

       @param sockdir The directory in which to place the server's UNIX socket
       file.

       @param root_dir The server's root directory. The process will @a
       chroot(2) to it if it's anything but "/".

       @param bindir The directory in which to look for the server application
       binary.

       @param maxfiles The maximum number of concurrent file transfers

       @param open_fd_timeout_ms The number of milliseconds after which open
       file descriptors should be closed

       @retval >0 The process id

       @retval 0 Server instance of the same name was already running

       @retval -1 An error occurred (see @a errno). If the error occurred in the
       server process, @a errno will contain the @e server process's @a errno
       value.
     */
    pid_t fiod_spawn(const char* server_name,
                     const char* root_dir,
                     const char* sockdir,
                     const char* bindir,
                     int maxfiles,
                     int open_fd_timeout_ms) FIOD_API;

    /**
       Connects to a running server instance.

       @param server_sockdir The directory in which the server's UNIX socket is
       located

       @param server_name The server instance name

       @retval >0 A socket connected to the server instance

       @retval -1 An error occurred
     */
    int fiod_connect(const char* server_sockdir,
                     const char* server_name) FIOD_API;

    /**
       Shuts down a server instance.

       Signals the process to shut down and waits for it to terminate.

       @param pid The server instance's process ID.

       @return -1 on error; otherwise the status value as per @a waitpid(2)
       which can be inspected using the associated macros such as @c WIFEXITED,
       @c WEXITSTATUS, etc.
     */
    int fiod_shutdown(pid_t pid) FIOD_API;

    /**
       @}
       @name File transfer operations (client requests)
       @{
    */

    /**
       Requests the server to write the contents of a file to a pipe.

       The server will first respond with a message of type struct
       fiod_file_info to confirm the transfer, after which it will write the
       data read from the file into the returned pipe.

       @note This operation is much more efficient on Linux than on other
       systems due to the use of the splice system call, which minimises the
       amount of copying and does not cross the kernel/userspace boundary. On
       non-Linux systems this operation degrades to the usual read/write with a
       userspace buffer inbetween, and therefore fiod_send() or fiod_send_open()
       should be preferred.

       @param srv_sockfd A socket connected to the server

       @param path Path to the file

       @param offset The starting file offset

       @param len The number of bytes, starting from @a offset, to read from the
       file. May be @a zero, in which case the read will be to the end of the
       file.

       @param dest_fd_nonblock Whether or not the destination pipe file
       descriptor should be non-blocking

       @retval >0 The read end of the pipe to which the file will be written

       @retval -1 An error occurred
     */
    int fiod_read(int srv_sockfd,
                  const char* path,
                  loff_t offset, size_t len,
                  bool dest_fd_nonblock) FIOD_API;

    /**
       Requests the server to write the contents of a file to an open file
       descriptor.

       The server will first respond with a message of type struct
       fiod_file_info to confirm the transfer, after which it will write the
       data read from the file to the destination file descriptor, reporting the
       progress of the transfer in messages of type struct fiod_xfer_stat on the
       status channel.

       @note This operation is implemented in terms of @a sendfile(2), which
       places certain constraints on @a destination_fd on some platforms. E.g.,
       on OS X and FreeBSD, the destination descriptor must be a socket,
       otherwise the operation degrades to the usual read/write with a userspace
       buffer inbetween.

       @param srv_sockfd A socket connected to the server

       @param path Path to the file

       @param destination_fd The descriptor to which the file will be written
       (the data channel)

       @param offset The starting file offset

       @param len The number of bytes, starting from @a offset, to read from the
       file. May be @a zero, in which case the read will be to the end of the
       file.

       @param stat_fd_nonblock Whether or not the status channel file descriptor
       should be non-blocking.

       @retval >0 The status channel descriptor
       @retval -1 An error occurred
     */
    int fiod_send(int srv_sockfd,
                  const char* path,
                  int destination_fd,
                  loff_t offset, size_t len,
                  bool stat_fd_nonblock) FIOD_API;

    /**
       Requests the server to open and return information about a file.

       This operation is useful in cases where the client needs access to the
       file metadata before the transfer begins. E.g., many protocols require
       the file size to precede the file contents.

       The server will confirm the request on the status channel with a message
       of type struct fiod_open_file_info. Besides the file size and file system
       timestamps, the message will also include a unique transfer identifier to
       be passed to the subsequent call to fiod_send_open().

       A timer will be set on the open file, after the expiraton of which the
       file will be closed and its associated state purged.

       @param offset The starting file offset

       @param len The number of bytes, starting from @a offset, to read from the
       file. May be @a zero, in which case the read will be to the end of the
       file.

       @retval >0 The status channel file descriptor
       @retval -1 An error occurred
     */
    int fiod_open(int srv_sockfd,
                  const char* path,
                  loff_t offset, size_t len,
                  bool stat_fd_nonblock) FIOD_API;

    /**
       Request that a file previously opened with fiod_open() be written to an
       open file descriptor.

       The server will immediately commence the transfer, and write transfer
       progress messages of type struct fiod_xfer_stat to the status channel, as
       usual.

       If the provided file identifier is invalid, the server will assume that
       it is due to the expiration of the timer associated with the open file
       and will silently ignore the request. (The client will, however, receive
       EOF when it tries to read from the status channel returned previously by
       fiod_open().)

       @param srv_sockfd A socket connected to the server

       @param txnid Open file identifier

       @param destination_fd The file descriptor to which the file will be
       written
     */
    bool fiod_send_open(int srv_sockfd,
                        size_t txnid,
                        int destination_fd) FIOD_API;

    /**@}*/

#ifdef __cplusplus
}
#endif

#endif
