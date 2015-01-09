int data_fd = sfd_read(srv_fd,
                       "/www/abc.html",
                       offset, nbytes,
                       stat_fd_nonblock);
