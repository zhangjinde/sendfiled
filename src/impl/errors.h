#ifndef FIOD_ERRORS_H
#define FIOD_ERRORS_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define LOGERRNO(msg) {                                 \
        const int tmp__ = errno;                        \
        fprintf(stderr, "%s [errno %d %s] %s",          \
                __func__, errno, strerror(errno), msg); \
        errno = tmp__;                                  \
    }

#define LOGERRNOV(fmt, ...) {                                   \
        const int tmp__ = errno;                                \
        fprintf(stderr, "%s [errno %d %s] "fmt,                 \
                __func__, errno, strerror(errno), __VA_ARGS__); \
        errno = tmp__;                                          \
    }                                                           \

#endif
