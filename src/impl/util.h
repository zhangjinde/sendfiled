#ifndef FIOD_UTIL_H
#define FIOD_UTIL_H

#include <stdbool.h>

#define MAX_(a, b) ((a) > (b) ? (a) : (b))

#define MIN_(a, b) ((a) < (b) ? (a) : (b))

#define PRESERVE_ERRNO(expr)                    \
    {                                           \
        const int errno_saved_ = errno;         \
        expr;                                   \
        errno = errno_saved_;                   \
    }

#ifdef __cplusplus
extern "C" {
#endif

    bool set_nonblock(int fd, bool enabled);

#ifdef __cplusplus
}
#endif

#endif
