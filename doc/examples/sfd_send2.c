void on_server_readable(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Omitted for the sake of brevity; see earlier example */
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
