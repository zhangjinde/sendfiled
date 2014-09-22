#ifndef FILE_IO_H
#define FILE_IO_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdio.h>

struct file {
    size_t size;
    int fd;
    int blksize;
};

bool file_open_read(struct file* file, const char* name, off_t offset);

void file_close(struct file*);

off_t file_offset(const struct file*);

ssize_t file_splice(struct file* file, int fd, size_t len);

#endif
