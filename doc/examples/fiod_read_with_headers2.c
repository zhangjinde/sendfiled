if (ctx->state == AWAITING_ACK) {
    struct fiod_open_file_info file_info;

    ssize_t nread = read(data_fd, buf, sizeof(file_info));

    if (nread <= 0 ||
        fiod_get_cmd(buf) != FIOD_FILE_INFO ||
        fiod_get_stat(buf) != FIOD_STAT_OK) {
        goto fail;
    }

    if (!fiod_unmarshal_file_info(&file_info, buf))
        goto fail;

    /* Store file size, to be sent with headers */
    ctx->file_size = file_info.size;
    ctx->nsent = 0;

    ctx->state = SENDING_HEADERS;
 }

if (ctx->state == SENDING_HEADERS) {
    /* Compose headers and write them to the ultimate destination file
       descriptor */

    int header_size = snprintf(buf,
                               "Content-Length: %d\r\n\r\n",
                               ctx->file_size);

    if (write(sockfd, buf, header_size) != header_size)
        goto fail;

    /* Headers have been written */

    ctx->state = TRANSFERRING;
 }

if (ctx->state == TRANSFERRING) {
    /* Splice file content from the data channel pipe to destination socket */

    ssize_t nspliced = splice(data_fd, NULL, sockfd, NULL, ctx->blksize);

    if (nspliced <= 0)
        goto fail;

    ctx->nsent += nspliced;

    if (ctx->nsent == ctx->file_size) {
        log(INFO, "Transfer complete\n");
    } else {
        log(INFO, "Transfer progress: %ld/%ld bytes\n",
            ctx->nsent, ctx->file_size);
    }
 }
