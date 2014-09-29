#define _POSIX_C_SOURCE 200809L /* for fileno */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_io.h"

/**
   Read-locks a file.

   @param offset The offset from the beginning of the file

   @param len The number of bytes to lock; a value of zero will lock from the
   offset to the end of the file
 */
static bool lock_file(int fd, off_t offset, off_t len);

static bool set_fstat(struct file* file);

bool file_open_read(struct file* file,
                    const char* name,
                    const off_t offset, const size_t len)
{
    *file = (struct file) {
        .fd = open(name, O_RDONLY)
    };

    if (file->fd == -1)
        return false;

    if (!lock_file(file->fd, offset, (off_t)len) ||
        !set_fstat(file)) {
        goto fail;
    }

    if (offset > 0 && lseek(file->fd, offset, SEEK_SET) == -1)
        goto fail;

    return true;

 fail:
    close(file->fd);
    file->fd = -1;

    return false;
}

void file_close(const struct file* f)
{
    close(f->fd);
}

off_t file_offset(const struct file* file)
{
    return lseek(file->fd, 0, SEEK_CUR);
}

/* ------------------ Internal implementations ---------------- */

static bool lock_file(const int fd, const off_t offset, const off_t len)
{
    struct flock lock = {
        .l_type = F_RDLCK,
        .l_whence = SEEK_SET,
        .l_start = offset,
        .l_len = len
    };

    return (fcntl(fd, F_SETLK, &lock) != -1);
}

static bool set_fstat(struct file* file)
{
    struct stat st;

    if (fstat(file->fd, &st) == -1)
        return false;

    file->size = (size_t)st.st_size;
    file->blksize = (int)st.st_blksize;

    return true;
}
