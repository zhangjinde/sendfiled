#include <sys/socket.h>

int us_socket(int domain, int type, int protocol);

int us_socket(int domain, int type, int protocol)
{
    return socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
}
