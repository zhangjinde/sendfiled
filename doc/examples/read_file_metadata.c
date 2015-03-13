struct xfer_context {
    enum {
        READING_METADATA,
        TRANSFERRING,
        COMPLETE
    } state;

    size_t file_size;           /* File size on disk */
    time_t file_mtime;          /* File's last modification time */
    size_t xfer_id;             /* Transfer's unique identifier */

    size_t total_nread;         /* Total number of bytes read */
};

void on_server_readable(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Read file metadata sent from server */

        struct sfd_file_info pdu;

        read(stat_fd, buf, sizeof(pdu)); /* or read(data_fd, ...) */
        sfd_unmarshal_file_info(&pdu, buf);

        *ctx = (struct xfer_context) {
            /* Store file size in order to recognise transfer completion */
            .file_size = pdu.size,
            /* Store file's last modification time */
            .file_mtime = pdu.mtime,
            /* Store transfer's unique identifier */
            .xfer_id = pdu.txnid,
            .total_nread = 0,
            .state = TRANSFERRING
        };
    }

    if (ctx->state == TRANSFERRING) {
        /* Handle file data transfer: see subsequent examples */
    }
}
