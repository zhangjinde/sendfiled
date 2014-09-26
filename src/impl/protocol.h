#ifndef FIOD_PROTOCOL_H
#define FIOD_PROTOCOL_H

#include <sys/types.h>

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

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat;                               \
    size_t body_len

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct prot_hdr {
    PROT_HDR_FIELDS;
};

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
