#ifndef FIOD_PROTOCOL_H
#define FIOD_PROTOCOL_H

#include <sys/types.h>
#include <sys/uio.h>

#include <stdint.h>

enum prot_cmd {
    /* Open and send file information  */
    PROT_CMD_FILE_OPEN,
    /* Read file contents */
    PROT_CMD_READ,
    /* Send file contents to destination file descriptor */
    PROT_CMD_SEND,
    /* Send a previously-opened file */
    PROT_CMD_SEND_OPEN,
    /* Cancel file transfer */
    PROT_CMD_CANCEL,
    /* File information as per fstat(2) */
    PROT_CMD_OPEN_FILE_INFO,
    PROT_CMD_FILE_INFO,
    /* File transfer request/operation status */
    PROT_CMD_XFER_STAT
};

enum {
    /* No error */
    PROT_STAT_OK
};

#define PROT_FILENAME_MAX 512   /* Excludes the terminating '\0' */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/* ------------------ PDU Headers ------------- */

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat

struct prot_hdr {
    PROT_HDR_FIELDS;
};

/* ------------- File Operation Request PDU ------------ */

struct prot_request {
    PROT_HDR_FIELDS;
    loff_t offset;
    size_t len;

    /* The remaining fields are not sent as-is */
    const char* filename;
    size_t filename_len;
};

#define PROT_REQ_BASE_SIZE (offsetof(struct prot_request, len) +        \
                            sizeof(((struct prot_request*)NULL)->len))

/* 1 for a non-empty filename; 1 for the terminating NUL */
#define PROT_REQ_MINSIZE PROT_REQ_BASE_SIZE + 1 + 1

/* Maximum size of a file operation request PDU */
#define PROT_REQ_MAXSIZE (sizeof(struct prot_request) + \
                          PROT_FILENAME_MAX + 1)

/* -------------- 'Send Open File' PDU */

struct prot_send_open {
    PROT_HDR_FIELDS;
    uint32_t txnid;
};

/* --------------- File Information PDU ------------- */

#define PROT_FILE_INFO_FIELDS                   \
    PROT_HDR_FIELDS;                            \
    size_t size;                                \
    time_t atime;                               \
    time_t mtime;                               \
    time_t ctime

struct prot_file_info {
    PROT_FILE_INFO_FIELDS;
};

/* ------------- Open File Information PDU ------------ */

struct prot_open_file_info {
    PROT_FILE_INFO_FIELDS;
    uint32_t txnid;
};

/* -------------- Transfer Status PDU -------------- */

struct prot_xfer_stat {
    PROT_HDR_FIELDS;
    size_t size;
};

#pragma GCC diagnostic pop

#endif
