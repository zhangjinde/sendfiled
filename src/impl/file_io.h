#ifndef FILE_IO_H
#define FILE_IO_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct file_info {
    /* Size of file on disk, as returned by stat(2); will be >= the total
       number of bytes transferred */
    size_t size;
    time_t atime;
    time_t mtime;
    time_t ctime;
    unsigned blksize;
};

#pragma GCC diagnostic pop

/**
   @retval >0 The file descriptor
   @retval <0 An error occurred
 */
int file_open_read(const char* name,
                   off_t offset, size_t len,
                   struct file_info*);

off_t file_offset(int fd);

ssize_t file_splice(int fd_in, int fd_out, size_t nbytes);

ssize_t file_sendfile(int fd_in, int fd_out, size_t nbytes);

#endif
