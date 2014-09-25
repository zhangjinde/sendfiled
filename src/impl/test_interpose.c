#define _GNU_SOURCE 1

#include <dlfcn.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "test_interpose.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#define NVALS 32

#define DEFINE_MOCK_DATA(name)                  \
    static struct {                             \
        ssize_t retvals [NVALS];                \
        int i;                                  \
        int size;                               \
    } mock_data_##name = {                      \
        .i = -1                                 \
    }

#define DEFINE_MOCK_FUNCS(name)                                         \
    void mock_##name##_reset(void)                                      \
    {                                                                   \
        mock_data_##name.i = -1;                                        \
    }                                                                   \
                                                                        \
    void mock_##name##_set_retval(ssize_t r)                            \
    {                                                                   \
        mock_data_##name.retvals[0] = r;                                \
            mock_data_##name.i = 0;                                     \
                mock_data_##name.size = 1;                              \
    }                                                                   \
                                                                        \
    void mock_##name##_set_retval_n(const ssize_t* vals, int nvals)     \
    {                                                                   \
        assert (nvals <= NVALS);                                        \
        memcpy(mock_data_##name.retvals, vals, sizeof(ssize_t) * (size_t)nvals); \
        mock_data_##name.i = 0;                                         \
            mock_data_##name.size = (int)nvals;                         \
    }

#define MOCK_RETURN(name)                                               \
    if (mock_data_##name.i == mock_data_##name.size || mock_data_##name.i == NVALS) \
        mock_data_##name.i = -1;                                        \
                                                                        \
        if (mock_data_##name.i != -1) {                                 \
            const ssize_t rv = mock_data_##name.retvals[mock_data_##name.i++]; \
                if (rv < 0) {                                           \
                    errno = (int)-rv;                                   \
                    return -1;                                          \
                } else {                                                \
                    return rv;                                          \
                }                                                       \
        }

DEFINE_MOCK_DATA(splice);
DEFINE_MOCK_DATA(sendfile);

DEFINE_MOCK_FUNCS(splice)
DEFINE_MOCK_FUNCS(sendfile)

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

#pragma GCC diagnostic pop
