if (ctx->state == AWAITING_FILE_INFO) {
    struct fiod_open_file_info file_info;

    ssize_t nread = read(stat_fd, buf, sizeof(file_info));

    if (nread <= 0 ||
        fiod_get_cmd(buf) != FIOD_OPEN_FILE_INFO ||
        fiod_get_stat(buf) != FIOD_STAT_OK) {
        goto fail;
    }

    if (!fiod_unmarshal_open_file_info(&file_info, buf))
        goto fail;

    /* Store file size, to be sent with headers */
    ctx->size = file_info.size;

    /* Store open file's unique identifier */
    ctx->txnid = file_info.txnid;

    ctx->state = SENDING_HEADERS;
 }

if (ctx->state == SENDING_HEADERS) {
    /* Compose headers and write them to DATAFD (preceding file contents) */

    int header_size = snprintf(buf,
                               "Content-Length: %d\r\n\r\n",
                               ctx->size);

    if (write(destination_fd, buf, header_size) == -1)
        goto fail;

    /* Headers have been written; ask the server to send the previously-opened
       file, identified by ctx->txnid, to the provided destination file
       descriptor. */

    if (!fiod_send_open_file(srv_sockfd,
                             ctx->txnid,
                             destination_fd)) {
        goto fail;
    }

    ctx->state = TRANSFERRING;
 }

if (ctx->state == TRANSFERRING) {
    struct fiod_xfer_stat stat;

    ssize_t nread = read(stat_fd, buf, sizeof(stat));

    if (nread <= 0 ||
        fiod_get_cmd(buf) != FIOD_FILE_INFO ||
        fiod_get_stat(buf) != FIOD_STAT_OK) {
        goto fail;
    }

    if (!fiod_unmarshal_xfer_stat(&stat, buf))
        goto fail;

    if (fiod_xfer_complete(&stat)) {
        log(INFO, "Transfer of %lu bytes complete\n", ctx->size);
    } else {
        ctx->nsent += stat.size;

        log(INFO, "Transfer progress: %lu/%lu bytes\n", ctx->nsent, ctx->size);
    }
 }
