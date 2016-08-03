/*
  Copyright (c) 2015, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_io.h"
#include "util.h"

/**
   Read-locks a file.

   @param offset The offset from the beginning of the file

   @param len The number of bytes to lock; a value of zero will lock from the
   offset to the end of the file
*/
static bool lock_file(int fd, off_t offset, off_t len);

/**
   Sets @a errno to EINVAL if the descriptor does not refer to a regular file.
*/
static int stat_file(int fd, struct fio_stat*);

int file_open_read(const char* name,
                   const off_t offset, const size_t len,
                   struct fio_stat* info)
{
    const int fd = open(name, O_RDONLY);

    if (fd == -1)
        return -1;

    if (stat_file(fd, info) == -1 ||
        !lock_file(fd, offset, (off_t)len)) {
        goto fail;
    }

    if (offset > 0 && lseek(fd, offset, SEEK_SET) == -1)
        goto fail;

    return fd;

 fail:
    PRESERVE_ERRNO(close(fd));

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

static int stat_file(const int fd, struct fio_stat* info)
{
    struct stat st;

    if (fstat(fd, &st) == -1)
        return -1;

    if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    *info = (struct fio_stat) {
        .size = (size_t)st.st_size,
        .atime = st.st_atime,
        .mtime = st.st_mtime,
        .ctime = st.st_ctime,
        .blksize = (unsigned)st.st_blksize
    };

    return 0;
}
