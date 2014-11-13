int data_fd = fiod_read(srv_fd,
                        "/mnt/disk0/abc.tar.gz",
                        offset, nbytes,
                        stat_fd_nonblock);
