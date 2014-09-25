#include <fcntl.h>
#include <unistd.h>

#include "util.h"

bool set_nonblock(int fd, const bool enabled)
{
    int val = fcntl(fd, F_GETFL, 0);
    if (val == -1)
        return false;

    if (enabled)
        val |= O_NONBLOCK;
    else
        val &= ~O_NONBLOCK;

    return (fcntl(fd, F_SETFL, val) == 0);
}
