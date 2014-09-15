#ifndef FIOD_SYSPOLL_H
#define FIOD_SYSPOLL_H

#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

enum {
    SYSPOLL_ERROR = 1,
    SYSPOLL_READ = 2,
    SYSPOLL_WRITE = 4
};

struct syspoll;

struct syspoll* syspoll_new(int timeout_ms, int maxevents);

void syspoll_delete(struct syspoll*);

bool syspoll_register(struct syspoll*, int fd, void* data, unsigned events);

bool syspoll_deregister(struct syspoll*, int fd);

int syspoll_register_signal();

int syspoll_poll(struct syspoll*);

struct syspoll_resrc {
    int events;
    void* udata;
};

struct syspoll_resrc syspoll_get(struct syspoll*, int eventnum);

#pragma GCC diagnostic pop

#endif
