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

#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "unix_sockets.h"

/** @todo Directory not to be hard-coded */
#define SOCKDIR "/tmp/"
#define FIOD_PREFIX "fiod."
#define SOCKEXT ".socket"

const char* us_make_sockpath(const char* srvname)
{
    struct sockaddr_un un;

    const size_t nonname_len = strlen(SOCKDIR FIOD_PREFIX SOCKEXT);
    const size_t namelen_max = (sizeof(un.sun_path) - nonname_len);
    const size_t namelen = strnlen(srvname, namelen_max);
    const size_t pathlen = (nonname_len + namelen);

    if (namelen >= namelen_max || pathlen >= sizeof(un.sun_path)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char* path = malloc(pathlen + 1);
    if (!path)
        return NULL;

    char* p = path;

    memcpy(p, SOCKDIR, strlen(SOCKDIR));
    p += strlen(SOCKDIR);

    memcpy(p, FIOD_PREFIX, strlen(FIOD_PREFIX));
    p += strlen(FIOD_PREFIX);

    memcpy(p, srvname, namelen);
    p += namelen;

    memcpy(p, SOCKEXT, strlen(SOCKEXT));
    p += strlen(SOCKEXT);

    *p = '\0';

    return path;
}
