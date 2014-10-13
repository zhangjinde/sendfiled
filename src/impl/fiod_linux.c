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

#define _GNU_SOURCE 1

#include <fcntl.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fiod.h"

int fiod_pipe(int fds[2], const int flags)
{
    return pipe2(fds, flags);
}

bool fiod_exec_server(const char* path,
                      const char* srvname,
                      const char* root_dir,
                      const int maxfiles,
                      const int open_fd_timeout_ms)
{
    const long line_max = sysconf(_SC_LINE_MAX);

    assert (line_max > 0);

    const size_t srvname_len = strnlen(srvname, (size_t)line_max + 1);
    const size_t root_dir_len = strnlen(root_dir, (size_t)line_max + 1);

    if (srvname_len == (size_t)line_max + 1 ||
        root_dir_len == (size_t)line_max + 1) {
        errno = ENAMETOOLONG;
        return false;
    }

    char maxfiles_str [10];

    int ndigits = snprintf(maxfiles_str,
                           sizeof(maxfiles_str),
                           "%d", maxfiles);

    if (ndigits >= (int)sizeof(maxfiles_str))
        return false;

    char open_fd_timeout_ms_str [10];

    ndigits = snprintf(open_fd_timeout_ms_str,
                       sizeof(open_fd_timeout_ms_str),
                       "%d", open_fd_timeout_ms);

    if (ndigits >= (int)sizeof(open_fd_timeout_ms_str))
        return false;

    const char* args[] = {
        "fiod",
        "-s", srvname,
        "-r", root_dir,
        "-n", maxfiles_str,
        "-t", open_fd_timeout_ms_str,
        "-p",
        NULL
    };

    execve(path, (char**)args, NULL);

    /* execve does not return on success */
    return false;
}
