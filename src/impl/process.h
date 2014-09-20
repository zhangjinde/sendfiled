#ifndef FIOD_PROCESS_H
#define FIOD_PROCESS_H

void proc_common_init(const char* root, int highest_fd);

int proc_daemonise(void);

#endif
