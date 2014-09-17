#define _GNU_SOURCE 1

#include <fcntl.h>

#include "file_io.h"

ssize_t file_splice(struct file* file, int fd)
{
    return splice(fileno(file->ptr), NULL,
                  fd, NULL,
                  (size_t)file->blksize,
                  0);
}
