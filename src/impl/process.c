#include <sys/resource.h>
#include <unistd.h>

#include <stdio.h>

#include "../attributes.h"
#include "errors.h"
#include "process.h"

bool proc_close_all_fds_except(const int* excluded_fds, const size_t nfds)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        LOGERRNO("Can’t get file limit\n");
        return false;
    }

    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;

    for (int fd = 3; fd < (int)rl.rlim_max; fd++) {
        bool excluded = false;

        /* Check whether fd is in exclusion list */
        for (size_t i = 0; i < nfds; i++) {
            if (fd == excluded_fds[i]) {
                excluded = true;
                break;
            }
        }

        if (!excluded)
            close(fd);
    }

    return true;
}

void proc_common_init(const char* root,
                      const int* excluded_fds,
                      const size_t nfds)
{
    proc_close_all_fds_except(excluded_fds, nfds);

    if (chdir(root) < 0)
        fprintf(stderr, "%s: can’t change directory to %s", __func__, root);
}
