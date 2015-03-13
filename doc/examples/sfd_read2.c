void on_server_readable(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Omitted for the sake of brevity; see earlier example */
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
