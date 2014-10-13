/*
  Copyright (c) 2014, Francois Kritzinger
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
