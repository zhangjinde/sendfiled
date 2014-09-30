/**
   @defgroup mod_client Client Interface
 */

/**
   @file

   @ingroup mod_client
 */

#ifndef FIOD_FIOD_H
#define FIOD_FIOD_H

#include <sys/types.h>

#include <stdbool.h>

#include "attributes.h"

struct fiod_context;

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

       @retval >0 The process id
       @retval 0 Process of same name was already running
       @retval -1 An error occurred (see @a errno)
     */
    pid_t fiod_spawn(const char* name,
                     const char* root,
                     int maxfiles) DSO_EXPORT;

    /**
       Connects to a running instance of the FIOD process.

       @param name The server instance name

       @retval >0 The request channel socket file descriptor
       @retval -1 An error occurred
     */
    int fiod_connect(const char* name) DSO_EXPORT;

    /**
       Shuts down a file I/O process.
     */
    int fiod_shutdown(pid_t pid) DSO_EXPORT;

    /**
       @}
       @name Reading & Sending
       @{
    */

    /**
       Requests the server to open and return information about a file.

       The server will write the file information (size and timestamps) and
       server-internal file number to the status channel (the returned file
       descriptor).

       @attention The file will remain open in the server; it is the client's
       responsibility to initiate the close on the server.

       @retval >0 The status channel file descriptor
       @retval -1 An error occurred
     */
    int fiod_open(int srv_sockfd,
                  const char* filename,
                  loff_t offset, size_t len,
                  bool stat_fd_nonblock) DSO_EXPORT;

    /**
       Sends a file over a user-supplied file descriptor.

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
                  bool stat_fd_nonblock) DSO_EXPORT;

    /**
       Splices a file into a pipe.

       The server will write data read from the file into a pipe of which the
       read end is returned to the caller.

       On Linux, this will be achieved by calling @a splice(2), while on other
       systems this call equates to a @a read(2) into a userspace buffer
       followed by a @a write(2) from the userspace buffer to the pipe, so it
       would be better to use fiod_send() on such systems.

       @param srv_sockfd A socket connected to the server (see fiod_connect())

       @param filename The name of the file to be read

       @param offset The start file offset

       @param len The number of bytes from the file to be sent

       @param dest_fd_nonblock Whether or not the destination file descriptor
       should be non-blocking.

       @retval >0 The destination file descriptor
       @retval -1 An error occurred
     */
    int fiod_read(int srv_sockfd,
                  const char* filename,
                  loff_t offset, size_t len,
                  bool dest_fd_nonblock) DSO_EXPORT;

    /**
       @}
       @name Receiving & Writing
       @{
     */

    /**
       Creates and writes to a new file.

       @param filename The name of the new file.

       @param fd_in The file descriptor from which the file contents will be
       read.
     */
    int fiod_create(struct fiod_context*,const char* filename, int fd_in);

    /**
       Appends to a file created with fiod_create().
     */
    int fiod_append(struct fiod_context*, int fd_in, int fd_out);

    /**
       Closes a file created with fiod_create().
     */
    int fiod_close(struct fiod_context*, int fd);

    /**@}*/
    /**@}*/

#ifdef __cplusplus
}
#endif

#endif
