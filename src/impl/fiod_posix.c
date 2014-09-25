#include <fcntl.h>
#include <unistd.h>

#include "fiod.h"

int fiod_pipe(int fds[2], const int flags)
{
    if (pipe(fds) == -1)
        return -1;

    /* Set CLOEXEC */
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1)
        goto fail;
    if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1)
        goto fail;

    /* Set NONBLOCK */
    if (!set_nonblock(fds[0], true) ||
        !set_nonblock(fds[1], true)) {
        goto fail;
    }

    return 0;

 fail:
    const int tmp = errno;
    close(fds[0]);
    close(fds[1]);
    errno = tmp;
    return -1;
}
