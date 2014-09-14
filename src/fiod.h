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
       @retval -1 An error occurred (see @a errno)
     */
    pid_t fiod_spawn(const char* name,
                     const char* root,
                     int maxfiles) DSO_EXPORT;

    /**
       Connects to a running instance of the FIOD process.

       @param name The server instance name

       @retval >0 The request channel socket file descriptor
       @retval <0 An error occurred
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
       Sends a file over a socket.

       The result of each I/O operation will be reported on the returned pipe
       file descriptor.

       @param[in] sockfd The socket over which to send the file.

       @return The read end of the pipe on which status will be reported.
     */
    int fiod_send(int srv_sockfd,
                  const char* filename,
                  int dest_sockfd,
                  off_t offset, size_t count) DSO_EXPORT;

    /**
       Reads a file into a pipe.

       On Linux, this will be achieved by calling @a splice(2), while on other
       systems this call equates to a read into a userspace buffer followed by a
       write from the userspace buffer to the pipe, so it is advised to use
       fiod_send() on such systems.

       @param sockfd A socket connected to the fiod.

       @param filename The name of the file to be read.

       @retval >0 The read end of the pipe to which the file will be written.

       @retval <0 An error occurred
     */
    int fiod_read(int sockfd,
                  const char* filename,
                  off_t offset, size_t count) DSO_EXPORT;

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
