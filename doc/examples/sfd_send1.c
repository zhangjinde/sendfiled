size_t offset = 0;  // from the beginning of the file
size_t nbytes = 0;  // to the end of the file
bool stat_fd_nonblock = true;

int stat_fd = sfd_send(srv_fd,
                       "/mnt/disk0/abc.tar.gz",
                       destination_fd,
                       offset, nbytes,
                       stat_fd_nonblock);
