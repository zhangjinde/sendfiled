#ifndef FIOD_PROCESS_H
#define FIOD_PROCESS_H

#include <stdbool.h>

/**
   Closes all file descriptors except 0, 1, 2, and those in the supplied list.
 */
bool proc_close_all_fds_except(const int* excluded_fds, size_t nfds);

bool proc_common_init(const char* root, const int* excluded_fds, size_t nfds);

/** @todo Will probably need to be platform-specific because the glibc and
    FreeBSD implementations apparently differ significantly. */
int proc_daemonise(void);

#endif
