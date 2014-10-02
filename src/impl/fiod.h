#ifndef FIOD_IMPL_FIOD_H
#define FIOD_IMPL_FIOD_H

#include <stdbool.h>

int fiod_pipe(int fds[2], int flags);

bool fiod_exec_server(const char* filename,
                      const char* srvname,
                      const char* root_dir,
                      int maxfiles,
                      int open_fd_timeout_ms);

#endif
