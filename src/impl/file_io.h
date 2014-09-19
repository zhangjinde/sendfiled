#ifndef FILE_IO_H
#define FILE_IO_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct file {
    FILE* ptr;
    size_t size;
    int blksize;
};

#pragma GCC diagnostic pop

bool file_open_read(struct file* file, const char* name);

void file_close(struct file*);

off_t file_offset(const struct file*);

ssize_t file_splice(struct file* file, int fd, loff_t offset, size_t count);

#endif
