/*
  Copyright (c) 2015, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <assert.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "errors.h"
#include "syspoll.h"
#include "util.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct syspoll
{
    int kqfd;
    struct kevent* events;
    size_t capacity;
    size_t size;
};

#pragma GCC diagnostic pop

static void kq_add(struct syspoll* this,
                   struct syspoll_resrc* resrc,
                   const int16_t filter,
                   const uint16_t flags,
                   const int64_t data,
                   const uint32_t fflags)
{
    assert (this->size < this->capacity);

    EV_SET(&this->events[this->size],
           resrc->ident,
           filter,
           /* EV_CLEAR for edge-triggered mode */
           EV_ADD | EV_CLEAR | flags,
           fflags,
           data,
           resrc);

    this->size++;
}

struct syspoll* syspoll_new(const int maxevents)
{
    assert (maxevents > 0);

    struct syspoll* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    *this = (struct syspoll) {
        .kqfd = kqueue(),
        /* Kqueue requires a separate 'event' structure for each filter (e.g.,
           readability, writability), so more slots are needed */
        .capacity = (size_t)maxevents * 2,
        .size = 0
    };

    if (this->kqfd == -1)
        goto fail;

    this->events = malloc(sizeof(*this->events) * this->capacity);
    if (!this->events)
        goto fail;

    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGINT);

    if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1)
        goto fail;

    struct syspoll_resrc sigresrc = {
        .ident = SIGTERM
    };
    kq_add(this, &sigresrc, EVFILT_SIGNAL, 0, 0, 0);
    sigresrc.ident = SIGTERM;
    kq_add(this, &sigresrc, EVFILT_SIGNAL, 0, 0, 0);

    return this;

 fail:
    syspoll_delete(this);

    return NULL;
}

void syspoll_delete(struct syspoll* this)
{
    close(this->kqfd);
    free(this->events);
    free(this);
}

bool syspoll_register(struct syspoll* this,
                      struct syspoll_resrc* resrc,
                      const int events)
{
    for (int ev = SYSPOLL_READ; ev <= SYSPOLL_WRITE; ev <<= 1) {
        assert (this->size < this->capacity);

        if ((events & ev) == 0)
            continue;

        const uint16_t flags = (events & SYSPOLL_ONESHOT ? EV_ONESHOT : 0);
        int16_t filter = 0;

        if (ev == SYSPOLL_READ)
            filter = EVFILT_READ;
        else if (ev == SYSPOLL_WRITE)
            filter = EVFILT_WRITE;

        assert (filter != 0);

        kq_add(this, resrc, filter, flags, 0, 0);
    }

    return true;
}

bool syspoll_timer(struct syspoll* this,
                   struct syspoll_resrc* resrc,
                   unsigned millis)
{
    /* fflags unset (0) -> defaults to milliseconds */
    kq_add(this, resrc, EVFILT_TIMER, EV_ONESHOT, millis, 0);

    return true;
}

bool syspoll_deregister(struct syspoll* this, int fd)
{
    assert (this->size < this->capacity);

    /* Sidebar: the final close of the descriptor will automatically remove the
       event from the kqueue, but in the case of sendfiled the file descriptor
       is still used after having been removed from the poller. */

    EV_SET(&this->events[this->size],
           fd,
           0, EV_DELETE,
           0, 0,
           0);                 /* user data */

    this->size++;

    return true;
}

static int syspoll_kevent(struct syspoll* this,
                          const struct timespec* const timeout);

int syspoll_wait(struct syspoll* this)
{
    /* NULL timeout -> wait indefinitely */
    return syspoll_kevent(this, NULL);
}

int syspoll_poll(struct syspoll* this)
{
    struct timespec timeout = {0, 0};

    return syspoll_kevent(this, &timeout);
}

static int syspoll_kevent(struct syspoll* this,
                          const struct timespec* const timeout)
{
    const int nevents = kevent(this->kqfd,
                               this->events, (int)this->size,
                               this->events, (int)this->capacity,
                               timeout);

    /* Reset size so that changes made while handling events are inserted from
       the beginning of the event array */
    this->size = 0;

    if (nevents == -1)
        return -1;

    assert (nevents > 0);

    return nevents;
}

struct syspoll_events syspoll_get(struct syspoll* this, const int eventnum)
{
    assert (eventnum >= 0);
    assert ((size_t)eventnum < this->capacity);

    struct syspoll_events info;

    struct kevent* const ev = &this->events[eventnum];

    if (ev->filter == EVFILT_SIGNAL) {
        assert (ev->ident == SIGTERM || ev->ident == SIGINT);
        info.events = SYSPOLL_TERM;
        info.udata = NULL;

    } else {
        assert (ev->flags == EV_ERROR ||
                ev->filter == EVFILT_READ ||
                ev->filter == EVFILT_WRITE ||
                ev->filter == EVFILT_TIMER);

        info.udata = (void*)ev->udata;

        if (ev->flags == EV_ERROR) {
            info.events = SYSPOLL_ERROR;
            printf("%s fd: %d; error: %d %s\n", __func__,
                   (int)ev->ident, (int)ev->data, strerror((int)ev->data));

        } else {
            switch (ev->filter) {
            case EVFILT_READ:
                info.events = SYSPOLL_READ;
                break;

            case EVFILT_WRITE:
                info.events = SYSPOLL_WRITE;
                break;

            case EVFILT_TIMER:
                info.events = SYSPOLL_READ;
                break;

            default:
                /* Unexpected filter (event) type */
                assert (false);
                break;
            }
        }
    }

    return info;
}
