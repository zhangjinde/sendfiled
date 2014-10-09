#ifndef FIOD_TEST_INTERPOSE_H
#define FIOD_TEST_INTERPOSE_H

#include <sys/types.h>

/* Special return value which causes the real function to be invoked */
#define MOCK_REALRV ~(ssize_t)0

#define DECL_MOCK_FUNCS(name)                                           \
    void mock_##name##_set_retval(ssize_t);                             \
    void mock_##name##_set_retval_n(const ssize_t* retvals, int nvals); \
    void mock_##name##_reset(void)

#ifdef __cplusplus
extern "C" {
#endif

    DECL_MOCK_FUNCS(write);

    DECL_MOCK_FUNCS(splice);

    DECL_MOCK_FUNCS(sendfile);

#ifdef __cplusplus
}
#endif

#endif
