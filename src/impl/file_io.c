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

static bool set_fstat(int fd, struct file_info*);

int file_open_read(const char* name,
                   const off_t offset, const size_t len,
                   struct file_info* info)
{
    const int fd = open(name, O_RDONLY);

    if (fd == -1)
        return -1;

    if (!lock_file(fd, offset, (off_t)len) ||
        !set_fstat(fd, info)) {
        goto fail;
    }

    if (offset > 0 && lseek(fd, offset, SEEK_SET) == -1)
        goto fail;

    return fd;

 fail:
    close(fd);

    return -1;
}

off_t file_offset(const int fd)
{
    return lseek(fd, 0, SEEK_CUR);
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

static bool set_fstat(const int fd, struct file_info* info)
{
    struct stat st;

    if (fstat(fd, &st) == -1)
        return false;

    *info = (struct file_info) {
        .size = (size_t)st.st_size,
        .atime = st.st_atime,
        .mtime = st.st_mtime,
        .ctime = st.st_ctime,
        .blksize = (unsigned)st.st_blksize,
    };

    return true;
}
