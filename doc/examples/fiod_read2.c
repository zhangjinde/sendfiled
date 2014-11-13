if (state == AWAITING_FILE_INFO) {
    struct fiod_file_info file_info;

    ssize_t nread = read(file_fd, buf, sizeof(file_info));

    if (nread <= 0 ||
        fiod_get_cmd(buf) != FIOD_FILE_INFO ||
        fiod_get_stat(buf) != FIOD_STAT_OK) {
        goto fail;
    }

    if (!fiod_unmarshal_file_info(&file_info, buf))
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
