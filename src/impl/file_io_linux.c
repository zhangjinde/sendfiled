#define _GNU_SOURCE 1

#include <fcntl.h>
#include <sys/sendfile.h>

#include "file_io.h"

struct fio_ctx* fio_ctx_new(size_t capacity __attribute__((unused)))
{
    return NULL;
}

void fio_ctx_delete(struct fio_ctx* this __attribute__((unused)))
{
}

bool fio_ctx_valid(const struct fio_ctx* this)
{
    return (this == NULL);
}

ssize_t file_splice(const int fd_in, const int fd_out,
                    struct fio_ctx* ctx __attribute__((unused)),
                    const size_t nbytes)
{
    return splice(fd_in, NULL,
                  fd_out, NULL,
                  nbytes,
                  SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
}

ssize_t file_sendfile(const int fd_in, const int fd_out,
                      struct fio_ctx* ctx __attribute__((unused)),
                      const size_t nbytes)
{
    return sendfile(fd_out, fd_in, NULL, nbytes);
}
