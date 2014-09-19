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

    printf("XXX %s: size: %ld; blksize: %d\n", __func__, file->size, file->blksize);

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
        goto fail1;

    if (offset > 0) {
        if (fseeko(file->ptr, offset, SEEK_SET) == -1)
            goto fail1;
    }

    return true;

 fail1:
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
    return ftello(file->ptr);
}
