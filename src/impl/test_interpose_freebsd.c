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

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "test_interpose_impl.h"

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

int sendfile(int fd, int s, off_t offset, size_t nbytes,
             struct sf_hdtr *hdtr, off_t *sbytes, int flags)
{
    MOCK_RETURN_INT(sendfile, s);

    typedef int (*fptr) (int fd, int s, off_t offset, size_t nbytes,
                         struct sf_hdtr *hdtr, off_t *sbytes, int flags);

    fptr real_sendfile = (fptr)dlsym(RTLD_NEXT, "sendfile");
    assert (real_sendfile);

    return real_sendfile(fd, s, offset, nbytes, hdtr, sbytes, flags);
}
