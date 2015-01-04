int data_fd = sfd_read(srv_fd,
                       "/mnt/disk0/abc.tar.gz",
                       offset, nbytes,
                       stat_fd_nonblock);
