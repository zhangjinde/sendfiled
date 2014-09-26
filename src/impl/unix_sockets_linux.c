#include <sys/socket.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

int us_socket(int domain, int type, int protocol)
{
    return socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
}

#pragma GCC diagnostic pop
