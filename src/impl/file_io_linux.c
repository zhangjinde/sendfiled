#define _GNU_SOURCE 1

#include <fcntl.h>

#include "file_io.h"

/* ssize_t file_splice(struct file* file, int fd) */
/* { */
/*     loff_t file_off = file_offset(file); */

/*     printf("CCC file_off: %ld; blksize: %d\n", file_off, file->blksize); */

/*     const ssize_t n = splice(fileno(file->ptr), &file_off, */
/*                              fd, NULL, */
/*                              (size_t)file->blksize, */
/*                              0); */

/*     printf("DDD file_off: %ld; blksize: %d\n", file_off, file->blksize); */

/*     return n; */
/* } */

ssize_t file_splice(struct file* file, int fd)
{
    const ssize_t n = splice(fileno(file->ptr), NULL,
                             fd, NULL,
                             (size_t)file->blksize,
                             0);

   printf("XXX %s: nspliced: %ld; file_off: %ld; blksize: %d\n",
          __func__, n, ftello(file->ptr), file->blksize);

    return n;
}
