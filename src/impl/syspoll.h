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

struct syspoll_resrc {
    int ident;
};

struct syspoll;

struct syspoll* syspoll_new(int maxevents);

void syspoll_delete(struct syspoll*);

/**
   @param data User data. The first item at this address must be the file
   descriptor (i.e., of type 'int').
 */
bool syspoll_register(struct syspoll*, struct syspoll_resrc*, unsigned events);

bool syspoll_timer(struct syspoll* this, struct syspoll_resrc*, unsigned millis);

bool syspoll_deregister(struct syspoll*, int fd);

int syspoll_poll(struct syspoll*);

struct syspoll_events {
    int events;
    void* udata;
};

struct syspoll_events syspoll_get(struct syspoll*, int eventnum);

#pragma GCC diagnostic pop

#endif
