#define _POSIX_C_SOURCE 200809L /* for fileno */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>

#include "file_io.h"

static bool set_info(struct file* file)
{
    struct stat st;

    if (fstat(fileno(file->ptr), &st) == -1)
        return false;

    file->size = (size_t)st.st_size;
    file->blksize = (int)st.st_blksize;

    return true;
}

bool file_open_read(struct file* file, const char* name, const loff_t offset)
{
    *file = (struct file) {
        .ptr = fopen(name, "rb")
    };

    if (!file->ptr)
        return false;

    if (!set_info(file))
        goto fail;

    if (offset > 0 && fseeko(file->ptr, offset, SEEK_SET) == -1)
        goto fail;

    return true;

 fail:
    fclose(file->ptr);
    file->ptr = NULL;

    return false;
}

void file_close(struct file* f)
{
    fclose(f->ptr);
}

off_t file_offset(const struct file* file)
{
    return lseek(fileno(file->ptr), 0, SEEK_CUR);
}
