if (state == AWAITING_FILE_INFO) {
    struct fiod_file_info file_info;

    ssize_t nread = read(stat_fd, buf, sizeof(file_info));

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
    struct fiod_xfer_stat xfer;

    ssize_t nread = read(stat_fd, buf, sizeof(xfer));

    if (nread <= 0 ||
        fiod_get_cmd(buf) != FIOD_XFER_STAT ||
        fiod_get_stat(buf) != FIOD_STAT_OK) {
        goto fail;
    }

    if (!fiod_unmarshal_xfer_stat(&xfer, buf))
        goto fail;

    if (fiod_xfer_complete(&xfer)) {
        log(INFO, "Transfer of %lu bytes complete\n", ctx->size);
    } else {
        transfer_nsent += xfer.size;

        log(INFO, "Transfer xfer: %lu/%lu bytes\n",
            transfer_nsent, file_size);
    }
 }
