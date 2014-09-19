#define _GNU_SOURCE 1

#include <fcntl.h>

#include "file_io.h"
#include "util.h"

ssize_t file_splice(struct file* file, const int fd,
                    const loff_t offset, const size_t count)
{
    loff_t o = (loff_t)offset;

    printf("XXX %s: offset: %lu; len: %lu; nsplice: %lu\n",
           __func__, offset, count, MIN_((size_t)file->blksize, count));

    const ssize_t n =  splice(fileno(file->ptr), &o,
                              fd, NULL,
                              MIN_((size_t)file->blksize, count),
                              0);

    printf("XXX n: %ld\n", n);

    return n;
}
