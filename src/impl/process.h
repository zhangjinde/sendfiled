#ifndef FIOD_PROCESS_H
#define FIOD_PROCESS_H

void proc_common_init(int pid, const char* root);

int daemonise(void);

#endif
