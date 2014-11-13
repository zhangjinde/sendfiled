int stat_fd = fiod_open(srv_fd,
                        "/mnt/disk0/abc.tar.gz",
                        destination_fd,
                        offset, nbytes,
                        stat_fd_nonblock);
