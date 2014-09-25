#ifndef FIOD_EVENTLOOP_H
#define FIOD_EVENTLOOP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    bool srv_run(const int listenfd, int maxfds);

#ifdef __cplusplus
}
#endif

#endif
