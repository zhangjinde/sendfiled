int stat_fd = sfd_open(srv_fd,
                       "/mnt/disk0/abc.tar.gz",
                       destination_fd,
                       offset, nbytes,
                       stat_fd_nonblock);
