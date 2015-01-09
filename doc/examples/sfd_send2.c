struct xfer_context {
    enum {
        READING_METADATA,
        TRANSFERRING,
        COMPLETE
    } state;
};

void handle_file_server(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Read file metadata sent from the server */

        struct sfd_file_info file_info;

        read(stat_fd, buf, sizeof(file_info));
        sfd_unmarshal_file_info(&file_info, buf);

        ctx->state = TRANSFERRING;
    }

    if (ctx->state == TRANSFERRING) {
        /* Read transfer status updates from the server */

        struct sfd_xfer_stat xfer;

        read(stat_fd, buf, sizeof(xfer));
        sfd_unmarshal_xfer_stat(&xfer, buf);

        if (sfd_xfer_complete(&xfer)) {
            ctx->state = COMPLETE;
            close(stat_fd);
        }
    }
}
