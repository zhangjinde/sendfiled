#define _GNU_SOURCE 1

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "test_interpose_impl.h"

DEFINE_MOCK_CONSTRUCTS(write)
DEFINE_MOCK_CONSTRUCTS(splice)
DEFINE_MOCK_CONSTRUCTS(sendfile)

ssize_t write(int fd, const void* buf, size_t len)
{
    MOCK_RETURN(write);

    typedef ssize_t (*fptr) (int, const void*, size_t);

    fptr real_write = (fptr)dlsym(RTLD_NEXT, "write");
    assert (real_write);

    return real_write(fd, buf, len);
}

ssize_t splice(int fd_in, loff_t *off_in, int fd_out,
               loff_t *off_out, size_t len, unsigned int flags)
{
    MOCK_RETURN(splice);

    typedef ssize_t (*fptr) (int, loff_t*, int, loff_t*, size_t, unsigned int);

    fptr real_splice = (fptr)dlsym(RTLD_NEXT, "splice");
    assert (real_splice);

    return real_splice(fd_in, off_in, fd_out, off_out, len, flags);
}

ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count)
{
    MOCK_RETURN(sendfile);

    typedef ssize_t (*fptr) (int, int, off_t*, size_t);

    fptr real_sendfile = (fptr)dlsym(RTLD_NEXT, "sendfile");
    assert (real_sendfile);

    return real_sendfile(out_fd, in_fd, offset, count);
}
