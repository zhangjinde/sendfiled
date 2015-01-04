if (state == AWAITING_ACK) {
    struct sfd_open_file_info file_info;

    ssize_t nread = read(data_fd, buf, sizeof(file_info));

    if (nread <= 0 ||
        sfd_get_cmd(buf) != SFD_FILE_INFO ||
        sfd_get_stat(buf) != SFD_STAT_OK) {
        goto fail;
    }

    if (!sfd_unmarshal_file_info(&file_info, buf))
        goto fail;

    /* Store file size, to be sent with headers */
    file_size = file_info.size;
    file_nsent = 0;

    state = SENDING_HEADERS;
 }

if (state == SENDING_HEADERS) {
    /* Compose headers and write them to the ultimate destination file
       descriptor */

    int header_size = snprintf(buf, "Content-Length: %d\r\n\r\n", file_size);

    write(sockfd, buf, header_size);

    state = TRANSFERRING;
 }

if (state == TRANSFERRING) {
    /* Splice file content from the data channel pipe to destination socket */

    ssize_t nspliced = splice(data_fd, NULL, sockfd, NULL, file_blksize);

    file_nsent += nspliced;

    if (file_nsent == file_size) {
        log(INFO, "Transfer complete\n");
    } else {
        log(INFO, "Transfer progress: %ld/%ld bytes\n",
            file_nsent, file_size);
    }
 }
