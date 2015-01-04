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

#define _POSIX_C_SOURCE 200809L

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>
#include <stdlib.h>

#include "process.h"

/*
  This function will need to be revised. Portable sections will probably need to
  be moved to a central location.

  Most of this code is based on 'Advanced Programming in the UNIX Environment'
  by Stevens and Rago, section 13.3.
 */
bool proc_daemonise(const int* noclose_fds, const size_t nfds)
{
    /*
      Clear file creation mask.
     */
    umask(0);

    /*
      Get maximum number of file descriptors.
     */
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        return false;

    /*
      Become a session leader to lose controlling TTY.
     */
    pid_t pid = fork();
    if (pid < 0)
        return false;
    else if (pid != 0) /* parent */
        exit(EXIT_SUCCESS);
    setsid();

    /*
      Ensure future opens won’t allocate controlling TTYs.

      (Not sure what this does, to be honest.)
     */
    struct sigaction sa = {
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#endif
        .sa_handler = SIG_IGN
#ifdef __clang__
#pragma GCC diagnostic pop
#endif
    };
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return false;
    if ((pid = fork()) < 0)
        return false;
    else if (pid != 0) /* parent */
        exit(EXIT_SUCCESS);

    /*
      Change the current working directory to the root so we won’t prevent file
      systems from being unmounted.
     */
    if (chdir("/") == -1)
        return false;

    if (!proc_init_child(noclose_fds, nfds))
        return false;

    return true;
}
