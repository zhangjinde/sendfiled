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

#define PROT_SIZEOF(s, m)                       \
    sizeof(((struct s*)NULL)->m)

#define PROT_FILENAME_MAX 512   /* Excludes the terminating '\0' */

#define INSERT_FIELD(p, f)                      \
    memcpy(p, &f, sizeof(f));                   \
    p += sizeof(f)

#define EXTRACT_FIELD(p, f)                     \
    memcpy(&f, p, sizeof(f));                   \
    p += sizeof(f)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/* ------------------ PDU Headers ------------- */

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat;                               \
    size_t body_len

struct prot_hdr {
    PROT_HDR_FIELDS;
};

#define PROT_HDR_SIZE                           \
    (PROT_SIZEOF(prot_hdr, cmd) +               \
     PROT_SIZEOF(prot_hdr, stat) +              \
     PROT_SIZEOF(prot_hdr, body_len))

typedef uint8_t prot_hdr_buf [PROT_HDR_SIZE];

/* ------------- File Operation Request PDU ------------ */

struct prot_request {
    PROT_HDR_FIELDS;
    loff_t offset;
    size_t len;
    const char* filename;
};

/* A marshaled request PDU */
struct prot_request_m {
    uint8_t hdr [PROT_HDR_SIZE +
                 PROT_SIZEOF(prot_request, offset) +
                 PROT_SIZEOF(prot_request, len)];
    const char* filename;
    struct iovec iovs[2];
};

/* Maximum size of a file operation request PDU */
#define PROT_REQ_MAXSIZE (PROT_SIZEOF(prot_request_m, hdr) +  \
                          PROT_FILENAME_MAX + 1)

/* -------------- 'Send Open File' PDU */

struct prot_send_open {
    PROT_HDR_FIELDS;
    uint32_t txnid;
};

#define PROT_SEND_OPEN_SIZE                     \
    (PROT_HDR_SIZE +                            \
     PROT_SIZEOF(prot_send_open, txnid))

typedef uint8_t prot_send_open_buf [PROT_SEND_OPEN_SIZE];

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

#define PROT_FILE_INFO_SIZE                     \
    (PROT_HDR_SIZE +                            \
     PROT_SIZEOF(prot_file_info, size) +        \
     PROT_SIZEOF(prot_file_info, atime) +       \
     PROT_SIZEOF(prot_file_info, mtime) +       \
     PROT_SIZEOF(prot_file_info, ctime))

typedef uint8_t prot_file_info_buf [PROT_FILE_INFO_SIZE];

/* ------------- Open File Information PDU ------------ */

struct prot_open_file_info {
    PROT_FILE_INFO_FIELDS;
    uint32_t txnid;
};

#define PROT_OPEN_FILE_INFO_SIZE                \
    (PROT_FILE_INFO_SIZE +                      \
     PROT_SIZEOF(prot_open_file_info, txnid))

typedef uint8_t prot_open_file_info_buf [PROT_OPEN_FILE_INFO_SIZE];

/* -------------- Transfer Status PDU -------------- */

struct prot_xfer_stat {
    PROT_HDR_FIELDS;
    size_t size;
};

#define PROT_XFER_STAT_SIZE                     \
    (PROT_HDR_SIZE +                            \
     PROT_SIZEOF(prot_xfer_stat, size))         \

typedef uint8_t prot_xfer_stat_buf [PROT_XFER_STAT_SIZE];

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    uint8_t* prot_marshal_hdr(void* buf,
                              enum prot_cmd cmd,
                              int stat,
                              size_t len);

    const uint8_t* prot_unmarshal_hdr(struct prot_hdr* hdr, const void* buf);

#ifdef __cplusplus
}
#endif

#endif
