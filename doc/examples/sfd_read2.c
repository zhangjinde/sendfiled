struct xfer_context {
    enum {
        READING_METADATA,
        TRANSFERRING,
        COMPLETE
    } state;

    size_t file_size;           /* File size on disk */
    size_t total_nread;         /* Total number of bytes read */
};

void handle_file_server(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Read file metadata sent from server */
        struct sfd_file_info file_info;

        read(data_fd, buf, sizeof(file_info));
        sfd_unmarshal_file_info(&file_info, buf);

        /* Store file size in order to recognise transfer completion */
        ctx->file_size = file_info.size;

        ctx->state = TRANSFERRING;
    }

    if (ctx->state == TRANSFERRING) {
        /* Read file data from server (and then do something with it
           off-stage) */
        ctx->total_nread += read(data_fd, buf, buf_size);

        if (ctx->total_nread == ctx->file_size) {
            ctx->state = COMPLETE;
            close(data_fd);
        }
    }
}
