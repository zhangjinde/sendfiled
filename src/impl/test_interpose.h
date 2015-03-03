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

#ifndef SFD_TEST_INTERPOSE_H
#define SFD_TEST_INTERPOSE_H

#include <sys/types.h>

/* Special return value which causes the real function to be invoked */
#define MOCK_REALRV ~(ssize_t)0

#define DECL_MOCK_FUNCS(name)                                           \
    void mock_##name##_set_retval(ssize_t);                             \
    void mock_##name##_set_retval_n(const ssize_t* retvals, int nvals); \
    void mock_##name##_set_retval_except_fd(ssize_t, int fd);           \
    void mock_##name##_reset(void)

#ifdef __cplusplus
extern "C" {
#endif

    DECL_MOCK_FUNCS(read);

    DECL_MOCK_FUNCS(write);

    DECL_MOCK_FUNCS(splice);

    DECL_MOCK_FUNCS(sendfile);

#ifdef __cplusplus
}
#endif

#endif
