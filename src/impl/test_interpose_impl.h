#ifndef FIOD_TEST_INTERPOSE_IMPL_H
#define FIOD_TEST_INTERPOSE_IMPL_H

#include <sys/types.h>

#include <errno.h>

#include "test_interpose.h"

#define MOCK_NVALS 32

#define DEFINE_MOCK_DATA(name)                  \
    static struct {                             \
        ssize_t retvals [MOCK_NVALS];           \
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
        assert (nvals <= MOCK_NVALS);                                   \
        memcpy(mock_data_##name.retvals, vals, sizeof(ssize_t) * (size_t)nvals); \
        mock_data_##name.i = 0;                                         \
            mock_data_##name.size = (int)nvals;                         \
    }

#define DEFINE_MOCK_CONSTRUCTS(name)            \
    DEFINE_MOCK_DATA(name);                     \
    DEFINE_MOCK_FUNCS(name)

#define MOCK_RETURN(name)                                               \
    if (mock_data_##name.i == mock_data_##name.size || mock_data_##name.i == MOCK_NVALS) \
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

#endif
