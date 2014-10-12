/**
   @defgroup mod_client Client Interface

   Interface through which client applications interact with the file I/O
   server.
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

       @name Process Control
       @{
     */

    /**
       Spawns a file I/O process.

       @param name The instance name. (The name of the server's UNIX socket.)

       @param root The root directory

       @param maxfiles The maximum number of open files

       @param open_fd_timeout_ms The number of milliseconds after which unused,
       open file descriptors should be closed

       @retval >0 The process id
       @retval 0 Process of same name was already running
       @retval -1 An error occurred (see @a errno)
     */
    pid_t fiod_spawn(const char* name,
                     const char* root,
                     int maxfiles,
                     int open_fd_timeout_ms) FIOD_API;

    /**
       Connects to a running instance of the FIOD process.

       @param name The server instance name

       @retval >0 The request channel socket file descriptor
       @retval -1 An error occurred
     */
    int fiod_connect(const char* name) FIOD_API;

    /**
       Shuts down a file I/O process.
     */
    int fiod_shutdown(pid_t pid) FIOD_API;

    /**
       @}
       @name Client Requests
       @{
    */

    /**
       Reads a file into a pipe.

       The server will write data read from the file into a pipe of which the
       read end is returned to the caller.

       @note This operation is only zero-copy on Linux. On other systems a
       userspace buffer is used between the file read and pipe write.

       @param srv_sockfd A socket connected to the server (see fiod_connect())

       @param filename The name of the file to be read

       @param offset The beginning file offset

       @param len The number of bytes, starting from @a offset, to read from the
       file

       @param dest_fd_nonblock Whether or not the destination pipe file
       descriptor should be non-blocking

       @retval >0 The read end of the pipe to which the file will be written
       @retval -1 An error occurred
     */
    int fiod_read(int srv_sockfd,
                  const char* filename,
                  loff_t offset, size_t len,
                  bool dest_fd_nonblock) FIOD_API;

    /**
       Writes a file to a user-supplied file descriptor.

       The result of each I/O operation will be reported on the status channel
       (the returned file descriptor).

       @param srv_sockfd A socket connected to the server (see fiod_connect())

       @param filename The name of the file to be sent

       @param dest_fd The descriptor to which the file will be written (the
       'data channel')

       @param offset The start file offset

       @param len The number of bytes from the file to be sent

       @param stat_fd_nonblock Whether or not the status channel file descriptor
       should be non-blocking.

       @retval >0 The status channel file descriptor
       @retval -1 An error occurred

       @note This operation is implemented in terms of @a sendfile(2), which
       places certain constraints on @a dest_fd on some platforms. E.g., on OS X
       and FreeBSD, the destination descriptor must refer to a socket.
     */
    int fiod_send(int srv_sockfd,
                  const char* filename,
                  int dest_fd,
                  loff_t offset, size_t len,
                  bool stat_fd_nonblock) FIOD_API;

    /**
       Requests the server to open and return information about a file.

       The server will write the file information (size and timestamps) and
       server-internal file number to the status channel (the returned file
       descriptor).

       @retval >0 The status channel file descriptor
       @retval -1 An error occurred
     */
    int fiod_open(int srv_sockfd,
                  const char* filename,
                  loff_t offset, size_t len,
                  bool stat_fd_nonblock) FIOD_API;

    /**
       Sends a file previously opened with fiod_open().

       @param srv_sockfd A socket connected to the server (see fiod_connect())

       @param txnid Transaction ID/open file ID

       @param dest_fd The file descriptor to which to write the file
     */
    bool fiod_send_open(int srv_sockfd,
                        size_t txnid,
                        int dest_fd) FIOD_API;

    /**@}*/

#ifdef __cplusplus
}
#endif

#endif
