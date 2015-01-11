/*
  Copyright (c) 2015, Francois Kritzinger
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

#ifndef SFD_SENDFILED_H
#define SFD_SENDFILED_H

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

       The process will @a chroot(2) to @a root_dir and will accept file
       operation requests on a UNIX UDP socket located at @a /root_dir/sockdir,
       with a name derived from @a root_dir and @a server_name.

       @param server_name The server instance name. Used to identify the server
       instance to sfd_connect().

       @param root_dir The server process' new root directory.

       @param sockdir The directory, relative to @a root_dir, in which to place
       the server's UNIX socket file. Must begin with a '/'.

       @param maxfiles The maximum number of concurrent file transfers. When
       this limit is reached the fact will be logged and new requests will be
       rejected with a status code of <em>EMFILE (too many open files)</em>.

       @param open_fd_timeout_ms The number of milliseconds after which files
       opened by sfd_open() and then abandoned will be closed.

       @retval >0 The server's process id.

       @retval 0 Server instance of the same name was already running

       @retval -1 An error occurred--check @c errno(3). If the error occurred in
       the server process, @c errno(3) will contain the @e server process's @a
       errno value.

       @sa sfd_shutdown()
    */
    pid_t sfd_spawn(const char* server_name,
                    const char* root_dir,
                    const char* sockdir,
                    int maxfiles,
                    int open_fd_timeout_ms) SFD_API;

    /**
       Connects to a server process.

       @param server_sockdir The full path to the directory in which the
       server's UNIX socket is located. Must begin with a '/'.

       @note The server socket directory is relative to the @e client's root
       directory (cf. sfd_spawn()).

       @param server_name The server instance name

       @retval >=0 A socket connected to the server instance

       @retval -1 An error occurred--check @c errno(3)

       @sa sfd_spawn()
    */
    int sfd_connect(const char* server_sockdir,
                    const char* server_name) SFD_API;

    /**
       Shuts down a server process.

       Has the same semantics as @a waitpid(2). I.e., the caller will block
       until the server process's state changes.

       @param pid The server's process ID

       @return -1 on error; otherwise the status value as per @a waitpid(2)
       which can be inspected using the associated macros such as @c WIFEXITED,
       @c WEXITSTATUS, etc.

       @sa sfd_spawn()
    */
    int sfd_shutdown(pid_t pid) SFD_API;

    /**
       @}
       @name File transfer operations (client requests)
       @{
    */

    /**
       Requests the server to write the contents of a file to the returned file
       descriptor.

       This operation is useful in cases where the file data or metadata needs
       to be inspected or processed by the client.

       The server will respond with a message of type sfd_file_info.

       @param srv_sockfd A socket connected to the server

       @param path Path to the file

       @param offset The starting file offset. May be @a zero, in which case the
       file will be read from the beginning.

       @param len The number of bytes, starting from @a offset, to be read from
       the file. May be @a zero, in which case the file will be read all the way
       to its end.

       @param dest_fd_nonblock Whether or not the returned file descriptor (a
       combined status and data channel) should be in non-blocking mode

       @retval >0 A new file descriptor from which the file metadata and file
       data is to be read

       @retval -1 An error occurred--check @c errno(3)
    */
    int sfd_read(int srv_sockfd,
                 const char* path,
                 loff_t offset, size_t len,
                 bool dest_fd_nonblock) SFD_API;

    /**
       Requests the server to write the contents of a file to an open file
       descriptor.

       This operation is appropriate if the file data should be written to the
       destination without delay or further involvement of the client.

       The server will respond with a message of type sfd_file_info and
       one or more of type sfd_xfer_stat.

       @param srv_sockfd A socket connected to the server

       @param path Path to the file

       @param destination_fd The descriptor to which the file data is to be
       written

       @param offset The starting file offset. May be @a zero, in which case the
       file will be read from the beginning.

       @param len The number of bytes, starting from @a offset, to be read from
       the file. May be @a zero, in which case the file will be read all the way
       to its end.

       @param stat_fd_nonblock Whether or not the returned file descriptor (the
       status channel) should be in non-blocking mode

       @retval >0 A new file descriptor from which the file metadata and
       transfer status updates are to be read (the status channel)

       @retval -1 An error occurred--check @c errno(3)
    */
    int sfd_send(int srv_sockfd,
                 const char* path,
                 int destination_fd,
                 loff_t offset, size_t len,
                 bool stat_fd_nonblock) SFD_API;

    /**
       Requests the server to open and return metadata about a file (leaving it
       open for a configurable period).

       This operation is useful in cases where the file data or metadata needs
       to be inspected or processed by the client. This caters to the same use
       case as sfd_read() but is more efficient on systems without a facility
       equivalent to Linux's @a splice(2).

       The server will respond with a message of type sfd_open_file_info.

       @param srv_sockfd A socket connected to the server

       @param path Path to the file

       @param offset The starting file offset. May be @a zero, in which case the
       file will be read from the beginning.

       @param len The number of bytes, starting from @a offset, to be read from
       the file. May be @a zero, in which case the file will be read all the way
       to its end.

       @param stat_fd_nonblock Whether or not the returned file descriptor (the
       status channel) should be in non-blocking mode

       @retval >0 A new file descriptor from which the file metadata and
       transfer status updates are to be read (the status channel)

       @retval -1 An error occurred--check @c errno(3)

       @sa sfd_send_open()
    */
    int sfd_open(int srv_sockfd,
                 const char* path,
                 loff_t offset, size_t len,
                 bool stat_fd_nonblock) SFD_API;

    /**
       Request the server to send a previously-opened file to an open file
       descriptor.

       If the provided file identifier is invalid, the server will assume that
       it is due to the expiration of the timer associated with the open file
       and will silently ignore the request.

       @param srv_sockfd A socket connected to the server

       @param txnid Open file identifier (read from the file descriptor returned
       by sfd_open()).

       @param destination_fd The descriptor to which the file data is to be
       written

       @retval true The request was sent
       @retval false An error occurred--check @c errno(3)

       @sa sfd_open()
    */
    bool sfd_send_open(int srv_sockfd,
                       size_t txnid,
                       int destination_fd) SFD_API;

    /**@}*/

#ifdef __cplusplus
}
#endif

#endif
