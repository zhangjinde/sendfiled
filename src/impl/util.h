#ifndef FIOD_UTIL_H
#define FIOD_UTIL_H

#include <stdbool.h>

#define MAX_(a, b) ((a) > (b) ? (a) : (b))

#define MIN_(a, b) ((a) < (b) ? (a) : (b))

#ifdef __cplusplus
extern "C" {
#endif

bool set_nonblock(int fd, bool enabled);

#ifdef __cplusplus
}
#endif

#endif
