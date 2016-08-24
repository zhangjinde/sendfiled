/*
  Copyright (c) 2016, Francois Kritzinger
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

#ifndef SFD_TEST_INTERPOSE_IMPL_H
#define SFD_TEST_INTERPOSE_IMPL_H

#include <errno.h>

#include "test_interpose.h"

#define MOCK_NVALS 32

#define DEFINE_MOCK_DATA(name)                  \
    static struct {                             \
        ssize_t retvals [MOCK_NVALS];           \
        int fds [MOCK_NVALS];                   \
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
    }                                                                   \
                                                                        \
    void mock_##name##_set_retval_except_fd(ssize_t r, const int fd)    \
    {                                                                   \
        mock_data_##name.retvals[0] = r;                                \
            mock_data_##name.fds[0] = fd;                               \
                mock_data_##name.i = 0;                                 \
                    mock_data_##name.size = 1;                          \
    }                                                                   \

#define DEFINE_MOCK_CONSTRUCTS(name)            \
    DEFINE_MOCK_DATA(name);                     \
    DEFINE_MOCK_FUNCS(name)

/* Emacs completely destroys the alignment of this macro :( */
#define MOCK_RETURN_IMPL(name, return_type, fd_out)                     \
    {                                                                   \
        if (mock_data_##name.i == mock_data_##name.size ||              \
            mock_data_##name.i == MOCK_NVALS) {                         \
            mock_data_##name.i = -1;                                    \
        }                                                               \
                                                                        \
        if (mock_data_##name.i != -1) {                                 \
            const int except_fd = mock_data_##name.fds[mock_data_##name.i]; \
            if (except_fd == 0 || except_fd != fd_out) {            \
                mock_data_##name.fds[mock_data_##name.i] = 0;       \
                const return_type rv =                                  \
                    (return_type)mock_data_##name.retvals[mock_data_##name.i++]; \
                if (rv != MOCK_REALRV) {                    \
                    if (rv < 0) {                           \
                        errno = (int)-rv;                   \
                        return -1;                          \
                    } else {                                \
                        return rv;                          \
                    }                                       \
                }                                           \
            }                                                           \
        }                                                               \
    }

#define MOCK_RETURN_INT(name, fd_out) MOCK_RETURN_IMPL(name, int, fd_out)

#define MOCK_RETURN_SSIZE_T(name, fd_out) MOCK_RETURN_IMPL(name, ssize_t, fd_out)

#endif
