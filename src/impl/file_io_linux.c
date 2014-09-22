#define _GNU_SOURCE 1

#include <fcntl.h>

#include "file_io.h"
#include "util.h"

ssize_t file_splice(struct file* file, const int fd, const size_t len)
{
    return splice(file->fd, NULL,
                  fd, NULL,
                  MIN_((size_t)file->blksize, len),
                  SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
}
