#include <sys/epoll.h>

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syspoll.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct syspoll
{
    int epollfd;
    int timeout;
    struct epoll_event* events;
    int nevents;
};

#pragma GCC diagnostic pop

struct syspoll* syspoll_new(int timeout_ms, const int maxevents)
{
    struct syspoll* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    *this = (struct syspoll) {
        .timeout = timeout_ms,
        .epollfd = epoll_create1(EPOLL_CLOEXEC),
        .nevents = maxevents
    };

    if (this->epollfd == -1) {
        fprintf(stderr, "%s: epoll_create1() failed\n", __func__);
        syspoll_delete(this);
        return NULL;
    }

    this->events = malloc(sizeof(*this->events) * (unsigned long)this->nevents);
    if (!this->events) {
        syspoll_delete(this);
        return NULL;
    }

    return this;
}

void syspoll_delete(struct syspoll* this)
{
    if (this) {
        free(this->events);
        close(this->epollfd);
        free(this);
    }
}

bool syspoll_register(struct syspoll* this, int fd, void* data, unsigned events)
{
    unsigned epoll_events = EPOLLET;

    if (events & SYSPOLL_READ)
        epoll_events |= EPOLLIN;
    if (events & SYSPOLL_WRITE)
        epoll_events |= EPOLLOUT;

    struct epoll_event event = {
        .events = epoll_events,
        .data = {.ptr = data}
    };

    if (epoll_ctl(this->epollfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        fprintf(stderr, "%s: epoll_ctl() on fd %d failed (%s)\n",
                __func__, fd, strerror(errno));
        return false;
    }

    return true;
}

bool syspoll_deregister(struct syspoll* this, int fd)
{
    struct epoll_event event;

    if (epoll_ctl(this->epollfd, EPOLL_CTL_DEL, fd, &event) == -1) {
        fprintf(stderr, "%s: epoll_ctl() on fd %d failed (%s)\n",
                __func__, fd, strerror(errno));
        return false;
    }

    return true;
}

int syspoll_poll(struct syspoll* this)
{
    const int n = epoll_wait(this->epollfd,
                             this->events, this->nevents,
                             this->timeout);
    if (n < 0) {
        fprintf(stderr, "%s: epoll_wait() failed (%s)\n",
                __func__, strerror(errno));
        return -1;
    }

    return n;
}

struct syspoll_resrc syspoll_get(struct syspoll* this, int eventnum)
{
    int events = 0;

    if (this->events[eventnum].events & EPOLLERR)
        events |= SYSPOLL_ERROR;
    if (this->events[eventnum].events & EPOLLIN)
        events |= SYSPOLL_READ;
    if (this->events[eventnum].events & EPOLLOUT)
        events |= SYSPOLL_WRITE;

    return (struct syspoll_resrc) {
        .events = events,
        .udata = this->events[eventnum].data.ptr
    };
}
