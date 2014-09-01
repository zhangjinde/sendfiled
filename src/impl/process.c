#include <sys/resource.h>
#include <unistd.h>

#include <stdio.h>

#include "../attributes.h"
#include "process.h"

void proc_common_init(int pid UNUSED, const char* root)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        fprintf(stderr, "%s: can’t get file limit", __func__);

    if (chdir(root) < 0)
        fprintf(stderr, "%s: can’t change directory to %s", __func__, root);

    /* Close all open file descriptors */
    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;
    for (int i = 3; i < (int)rl.rlim_max; i++)
        close(i);
}
