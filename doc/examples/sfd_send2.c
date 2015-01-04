if (state == AWAITING_FILE_INFO) {
    struct sfd_file_info file_info;

    ssize_t nread = read(stat_fd, buf, sizeof(file_info));

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
    struct sfd_xfer_stat xfer;

    ssize_t nread = read(stat_fd, buf, sizeof(xfer));

    if (nread <= 0 ||
        sfd_get_cmd(buf) != SFD_XFER_STAT ||
        sfd_get_stat(buf) != SFD_STAT_OK) {
        goto fail;
    }

    if (!sfd_unmarshal_xfer_stat(&xfer, buf))
        goto fail;

    if (sfd_5xfer_complete(&xfer)) {
        log(INFO, "Transfer of %lu bytes complete\n", ctx->size);
    } else {
        transfer_nsent += xfer.size;

        log(INFO, "Transfer xfer: %lu/%lu bytes\n",
            transfer_nsent, file_size);
    }
 }
