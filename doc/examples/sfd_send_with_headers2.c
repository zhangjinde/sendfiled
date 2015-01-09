struct xfer_context {
    enum {
        READING_METADATA,
        SENDING_HEADERS,
        TRANSFERRING,
        COMPLETE
    } state;

    size_t txnid;               /* Unique open file identifier */
    size_t file_size;           /* File size on disk */
    time_t file_mtime;          /* File's last modification time */
    size_t total_nsent;         /* Total number of bytes transferred */
};

void handle_file_server(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Read file metadata sent from server */

        struct sfd_open_file_info file_info;

        read(stat_fd, buf, sizeof(file_info));
        sfd_unmarshal_open_file_info(&file_info, buf);

        /* Store the open file identifier */
        ctx->txnid = file_info.txnid;

        /* Store metadata to be sent with the headers */
        ctx->file_size = file_info.size;
        ctx->file_mtime = file_info.mtime;

        ctx->state = SENDING_HEADERS;
    }

    if (ctx->state == SENDING_HEADERS) {
        /* Compose headers and write them to the destination file descriptor */

        char last_modified[50];
        format_mtime(last_modified, ctx->file_mtime);

        int header_size = snprintf(buf, sizeof(buf),
                                   "HTTP/1.1 200 OK\r\n"
                                   "Content-Length: %lu\r\n"
                                   "Last-Modified: %s\r\n"
                                   "\r\n",
                                   ctx->file_size, last_modified);

        write(destination_fd, buf, header_size);

        /* Now that the headers have been written, get the server to start
           sending the file data */
        sfd_send_open(srv_sockfd, ctx->txnid, destination_fd);

        ctx->state = TRANSFERRING;
    }

    if (ctx->state == TRANSFERRING) {
        /* Read the server's transfer status updates as it sends the file to the
           destination file descriptor */

        struct sfd_xfer_stat stat;

        read(stat_fd, buf, sizeof(stat));
        sfd_unmarshal_xfer_stat(&stat, buf);

        if (sfd_xfer_complete(&stat)) {
            ctx->state = COMPLETE;
            close(stat_fd);

        } else {
            file_nsent += stat.size;

            log(INFO, "Transfer progress: %lu/%lu bytes\n",
                ctx->file_nsent, ctx->file_size);
        }
    }
}
