void on_server_readable(struct xfer_context* ctx)
{
    if (ctx->state == READING_METADATA) {
        /* Omitted for the sake of brevity; see earlier example */
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

        ctx->state = TRANSFERRING;
    }

    if (ctx->state == TRANSFERRING) {
        /* Splice file content from the data channel pipe to destination socket
           in sensibly-sized chunks */

        for (;;) {
            ssize_t nspliced = splice(data_fd, NULL,
                                      destination_fd, NULL,
                                      file_blksize);

            ctx->total_nsent += nspliced;

            if (ctx->total_nsent == ctx->file_size) {
                ctx->state = COMPLETE;
                break;

            } else if (nspliced < file_blksize) {
                log(INFO, "Transfer progress: %ld/%ld bytes\n",
                    ctx->total_nsent, ctx->file_size);
                break;
            }
        }
    }
}
