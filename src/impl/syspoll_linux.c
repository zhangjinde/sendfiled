#define _POSIX_C_SOURCE 200809L

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syspoll.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct syspoll
{
    int epollfd;
    int sigfd;
    sigset_t orig_sigmask;
    struct epoll_event* events;
    int nevents;
};

#pragma GCC diagnostic pop

struct syspoll* syspoll_new(const int maxevents)
{
    assert (maxevents > 0);

    struct syspoll* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    *this = (struct syspoll) {
        .epollfd = epoll_create1(EPOLL_CLOEXEC),
        .nevents = maxevents
    };

    if (this->epollfd == -1) {
        fprintf(stderr, "%s: epoll_create1() failed\n", __func__);
        goto fail;
    }

    this->events = malloc(sizeof(*this->events) * (unsigned long)this->nevents);
    if (!this->events)
        goto fail;

    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGINT);

    if (sigprocmask(SIG_BLOCK, &sigmask, &this->orig_sigmask) == -1)
        goto fail;

    this->sigfd = signalfd(-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (this->sigfd == -1)
        goto fail;

    if (!syspoll_register(this, (struct syspoll_resrc*)&this->sigfd, SYSPOLL_READ))
        goto fail;

    return this;

 fail:
    syspoll_delete(this);
    return NULL;
}

void syspoll_delete(struct syspoll* this)
{
    if (this) {
        close(this->epollfd);
        close(this->sigfd);
        sigprocmask(SIG_SETMASK, &this->orig_sigmask, NULL);

        free(this->events);
        free(this);
    }
}

bool syspoll_register(struct syspoll* this,
                      struct syspoll_resrc* resrc,
                      unsigned events)
{
    unsigned epoll_events = EPOLLET;

    if (events & SYSPOLL_READ)
        epoll_events |= EPOLLIN;

    if (events & SYSPOLL_WRITE)
        epoll_events |= EPOLLOUT;

    if (events & SYSPOLL_ONESHOT)
        epoll_events |= EPOLLONESHOT;

    struct epoll_event event = {
        .events = epoll_events,
        .data = {.ptr = resrc}
    };

    return (epoll_ctl(this->epollfd, EPOLL_CTL_ADD, resrc->ident, &event) == 0);
}

bool syspoll_timer(struct syspoll* this,
                   struct syspoll_resrc* resrc,
                   unsigned millis)
{
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd == -1)
        return -1;

    struct itimerspec time;
    time.it_value.tv_sec = millis / 1000;
    millis -= time.it_value.tv_sec * 1000;
    time.it_value.tv_nsec = millis * 1000000;
    time.it_interval.tv_sec = 0;
    time.it_interval.tv_nsec = 0;

    if (timerfd_settime(fd, 0, &time, 0) == -1)
        goto fail;

    if(!syspoll_register(this,
                         resrc,
                         SYSPOLL_READ | SYSPOLL_ONESHOT)) {
        goto fail;
    }

    resrc->ident = fd;

    return true;

 fail:
    close(fd);

    return false;
}

bool syspoll_deregister(struct syspoll* this, int fd)
{
    struct epoll_event event;
    return (epoll_ctl(this->epollfd, EPOLL_CTL_DEL, fd, &event) == 0);
}

int syspoll_poll(struct syspoll* this)
{
    return epoll_wait(this->epollfd, this->events, this->nevents, -1);
}

static bool recvd_term_signal(struct syspoll* this);

struct syspoll_events syspoll_get(struct syspoll* this, int eventnum)
{
    const struct epoll_event* const e = &this->events[eventnum];

    struct syspoll_events info = {
        .events = 0,
        .udata = e->data.ptr
    };

    if (e->events & EPOLLERR) {
        info.events = SYSPOLL_ERROR;

    } else {
        if (e->events & EPOLLOUT)
            info.events |= SYSPOLL_WRITE;

        if (e->events & EPOLLIN) {
            const struct syspoll_resrc* r = (struct syspoll_resrc*)e->data.ptr;

            if (r->ident == this->sigfd) {
                info.events = (recvd_term_signal(this) ?
                               SYSPOLL_TERM :
                               SYSPOLL_ERROR);
                return info;
            }

            info.events |= SYSPOLL_READ;
        }
    }

    info.udata = e->data.ptr;

    return info;
}

// ------------------ (Uninteresting) Internal implementations ---------------

static bool recvd_term_signal(struct syspoll* this)
{
    struct signalfd_siginfo info;

    const ssize_t s = read(this->sigfd, &info, sizeof(struct signalfd_siginfo));
    return (s == sizeof(struct signalfd_siginfo) &&
            (info.ssi_signo == SIGTERM || info.ssi_signo == SIGINT));
}
