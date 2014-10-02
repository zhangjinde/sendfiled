#ifndef FIOD_SYSPOLL_H
#define FIOD_SYSPOLL_H

#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

enum {
    SYSPOLL_ERROR = 1 << 0,
    SYSPOLL_READ = 1 << 1,
    SYSPOLL_WRITE = 1 << 2,
    /* Shutdown event (e.g., a signal such as SIGTERM or SIGINT) */
    SYSPOLL_TERM = 1 << 3,
    SYSPOLL_ONESHOT = 1 << 4
};

struct syspoll;

struct syspoll* syspoll_new(int timeout_ms, int maxevents);

void syspoll_delete(struct syspoll*);

/**
   @param data User data. The first item at this address must be the file
   descriptor (i.e., of type 'int').
 */
bool syspoll_register(struct syspoll*, int fd, void* data, unsigned events);

int syspoll_timer(struct syspoll* this, void* data, unsigned millis);

bool syspoll_deregister(struct syspoll*, int fd);

int syspoll_poll(struct syspoll*);

struct syspoll_resrc {
    int events;
    void* udata;
};

struct syspoll_resrc syspoll_get(struct syspoll*, int eventnum);

#pragma GCC diagnostic pop

#endif
