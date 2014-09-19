#ifndef FIOD_PROTOCOL_H
#define FIOD_PROTOCOL_H

#include <sys/types.h>
#include <sys/uio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum prot_cmd {
    /* Read file contents */
    PROT_CMD_READ,
    /* Send file contents to destination file descriptor */
    PROT_CMD_SEND,
    /* Cancel file transfer */
    PROT_CMD_CANCEL,
    /* File transfer request/operation status */
    PROT_CMD_STAT
};

enum {
    /* No error */
    PROT_STAT_OK
};

#define PROT_HDR_SIZE 10        /* cmd{1} + stat{1} + len{8} */

#define PROT_FILENAME_MAX 512   /* Excludes the terminating '\0' */

/* Size of file operation request PDU minus the filename */
#define PROT_REQ_BASE_SIZE (PROT_HDR_SIZE + 8 + 8)

/* Maximum size of a file operation request PDU */
#define PROT_REQ_MAXSIZE (PROT_REQ_BASE_SIZE + PROT_FILENAME_MAX + 1)

/* Size of a status PDU  */
#define PROT_STAT_SIZE (PROT_HDR_SIZE + 8)

/* Maximum PDU size */
#define PROT_PDU_MAXSIZE PROT_REQ_MAXSIZE

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/* ---------------- Unmarshaled PDUs --------------- */

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat;                               \
    size_t body_len

struct prot_request {
    PROT_HDR_FIELDS;
    loff_t offset;
    size_t len;
    const char* filename;
};

struct prot_file_stat {
    PROT_HDR_FIELDS;
    size_t size;
};

/* ---------------- Marshaled PDUs --------------- */

struct prot_hdr_m {
    uint8_t data [PROT_HDR_SIZE];
};

/* A marshaled request PDU */
struct prot_request_m {
    uint8_t hdr [PROT_REQ_BASE_SIZE];
    const char* filename;
    struct iovec iovs[2];
};

/* A marshaled file status PDU */
struct prot_file_stat_m {
    uint8_t data [PROT_STAT_SIZE];
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    void prot_marshal_hdr(struct prot_hdr_m* hdr,
                          enum prot_cmd cmd,
                          int stat,
                          size_t len);

    bool prot_marshal_send(struct prot_request_m* req, const char* filename,
                           loff_t offset, size_t len);

    bool prot_marshal_read(struct prot_request_m* req, const char* filename,
                           loff_t offset, size_t len);

    void prot_marshal_stat(struct prot_file_stat_m* pdu, size_t val);

    /**
       @retval 0 Success
       @retval -1 Malformed PDU
       @retval >0 Error code from PDU header
    */
    int prot_unmarshal_request(struct prot_request*, const void* buf);

    /**
       @retval 0 Success
       @retval -1 Malformed PDU
       @retval >0 Error code from PDU header
    */
    int prot_unmarshal_stat(struct prot_file_stat* pdu, const void* buf);

#ifdef __cplusplus
}
#endif

#endif
