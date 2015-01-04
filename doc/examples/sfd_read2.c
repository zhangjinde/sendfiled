if (state == AWAITING_FILE_INFO) {
    struct sfd_file_info file_info;

    ssize_t nread = read(file_fd, buf, sizeof(file_info));

    if (nread <= 0 ||
        sfd_get_cmd(buf) != SFD_FILE_INFO ||
        sfd_get_stat(buf) != SFD_STAT_OK) {
        goto fail;
    }

    if (!sfd_unmarshal_file_info(&file_info, buf))
        goto fail;

    file_size = file_info.size;
    state = TRANSFERRING;
 }

if (state == TRANSFERRING) {
    ssize_t nread = read(file_fd, buf, buf_size);

    file_nread += nread;

    if (file_nread == file_size) {
        log(INFO, "File read complete\n");
    } else {
        log(INFO, "Read %lu/%lu bytes from file\n", file_nread, file_size);
    }
 }
