#define _GNU_SOURCE 1

#include <fcntl.h>

#include "file_io.h"
#include "util.h"

ssize_t file_splice(struct file* file, const int fd, const size_t len)
{
    return splice(fileno(file->ptr), NULL,
                  fd, NULL,
                  MIN_((size_t)file->blksize, len),
                  0);
}
