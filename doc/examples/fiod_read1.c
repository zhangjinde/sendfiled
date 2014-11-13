int file_fd = fiod_read(srv_fd,
                        "/mnt/disk0/abc.tar.gz",
                        offset, nbytes,
                        file_fd_nonblock);
