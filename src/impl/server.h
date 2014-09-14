#ifndef FIOD_EVENTLOOP_H
#define FIOD_EVENTLOOP_H

#include <stdbool.h>

bool srv_run(const int listenfd, int maxfds);

#endif
