#ifndef FIOD_ERRORS_H
#define FIOD_ERRORS_H

#include <errno.h>
#include <stdio.h>

#define LOGERRNO(msg) {                                 \
        const int tmp = errno;                          \
        fprintf(stderr, "%s [errno %d %s]: %s\n",       \
                __func__, errno, strerror(errno), msg); \
        errno = tmp;                                    \
    }

#define LOGERRNOV(fmt, ...) {                                   \
        const int tmp = errno;                                  \
        fprintf(stderr, "%s [errno %d %s] "fmt,                 \
                __func__, errno, strerror(errno), __VA_ARGS__); \
        errno = tmp;                                            \
    }                                                           \

#endif
