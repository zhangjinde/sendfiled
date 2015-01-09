int stat_fd = sfd_open(srv_fd,
                       "/www/abc.html",
                       destination_fd,
                       offset, nbytes,
                       stat_fd_nonblock);
