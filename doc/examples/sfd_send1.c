int stat_fd = sfd_send(srv_fd,
                       "/www/abc.html",
                       destination_fd,
                       offset, len,
                       stat_fd_nonblock);
