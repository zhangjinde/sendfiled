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

#ifndef SFD_PROCESS_H
#define SFD_PROCESS_H

#include <sys/types.h>

#include <stdbool.h>

/**
   Number of the file descriptor (opened in parent process by sfd_spawn()) to
   which the server process' startup success (0) or error code is to be written
   in order to sync with parent and to facilitate error reporting in the parent
   process.
*/
static const int PROC_SYNCFD = 3;

/**
   Performs setup common to child server processes.

   Redirects stdin, stdout, and stderr to /dev/null, and closes all other file
   descriptors, excluding those in @a excluded_fds.

   'Child processes' in this context means ones forked by sfd_spawn() and ones
   going into daemon mode.
*/
bool proc_init_child(const int* excluded_fds, size_t nfds);

/** @todo Will probably need to be platform-specific because the glibc and
    FreeBSD implementations apparently differ significantly. */
bool proc_daemonise(const int* noclose_fds, const size_t nfds);

#endif
