#define _GNU_SOURCE 1

#include <fcntl.h>
#include <unistd.h>

int fiod_pipe(int fds[2], int flags);

int fiod_pipe(int fds[2], const int flags)
{
    return pipe2(fds, flags);
}
