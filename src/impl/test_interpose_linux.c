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

#define _GNU_SOURCE 1

#include <dlfcn.h>
#include <sys/sendfile.h>

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "test_interpose_impl.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas" /* Clang doesn't know
                                                      -pedantic */
#pragma GCC diagnostic ignored "-pedantic" /* For casting from dlsym() to
                                              function pointer */

DEFINE_MOCK_CONSTRUCTS(read)
DEFINE_MOCK_CONSTRUCTS(write)
DEFINE_MOCK_CONSTRUCTS(splice)
DEFINE_MOCK_CONSTRUCTS(sendfile)

ssize_t read(int fd, void* buf, size_t len)
{
    MOCK_RETURN_SSIZE_T(read, fd);

    typedef ssize_t (*fptr) (int, void*, size_t);

    fptr real_read = (fptr)dlsym(RTLD_NEXT, "read");
    assert (real_read);

    return real_read(fd, buf, len);
}

ssize_t write(int fd, const void* buf, size_t len)
{
    MOCK_RETURN_SSIZE_T(write, fd);

    typedef ssize_t (*fptr) (int, const void*, size_t);

    fptr real_write = (fptr)dlsym(RTLD_NEXT, "write");
    assert (real_write);

    return real_write(fd, buf, len);
}

ssize_t splice(int fd_in, loff_t *off_in, int fd_out,
               loff_t *off_out, size_t len, unsigned int flags)
{
    MOCK_RETURN_SSIZE_T(splice, fd_out);

    typedef ssize_t (*fptr) (int, loff_t*, int, loff_t*, size_t, unsigned int);

    fptr real_splice = (fptr)dlsym(RTLD_NEXT, "splice");
    assert (real_splice);

    return real_splice(fd_in, off_in, fd_out, off_out, len, flags);
}

ssize_t sendfile(int s, int fd, off_t* offset, size_t count)
{
    MOCK_RETURN_SSIZE_T(sendfile, s);

    typedef ssize_t (*fptr) (int, int, off_t*, size_t);

    fptr real_sendfile = (fptr)dlsym(RTLD_NEXT, "sendfile");
    assert (real_sendfile);

    return real_sendfile(s, fd, offset, count);
}

#pragma GCC diagnostic pop
