#define _GNU_SOURCE 1

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.h>

#include "unix_socket_client.h"
#include "unix_sockets.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

int us_socket(int domain, int type, int protocol)
{
    return socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
}

#pragma GCC diagnostic pop
