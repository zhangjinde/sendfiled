#ifndef FIOD_TEST_INTERPOSE_H
#define FIOD_TEST_INTERPOSE_H

#include <sys/types.h>

#define DECL_MOCK_FUNCS(name)                                           \
    void mock_##name##_set_retval(ssize_t);                             \
    void mock_##name##_set_retval_n(const ssize_t* retvals, int nvals); \
    void mock_##name##_reset(void)

#ifdef __cplusplus
extern "C" {
#endif

    DECL_MOCK_FUNCS(splice);

    DECL_MOCK_FUNCS(sendfile);

#ifdef __cplusplus
}
#endif

#endif
