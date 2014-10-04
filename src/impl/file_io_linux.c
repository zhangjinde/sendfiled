#define _GNU_SOURCE 1

#include <fcntl.h>
#include <sys/sendfile.h>

#include "file_io.h"

ssize_t file_splice(const int fd_in, const int fd_out, const size_t nbytes)
{
    return splice(fd_in, NULL,
                  fd_out, NULL,
                  nbytes,
                  SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
}

ssize_t file_sendfile(const int fd_in, const int fd_out, const size_t nbytes)
{
    return sendfile(fd_out, fd_in, NULL, nbytes);
}
