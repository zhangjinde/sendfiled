#ifndef FIOD_PROTOCOL_H
#define FIOD_PROTOCOL_H

#include <sys/uio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum prot_cmd {
    PROT_CMD_READ,
    PROT_CMD_SEND,
    PROT_CMD_ACK,
    PROT_CMD_STAT,
    /* Cancel transfer */
    PROT_CMD_CANCEL
};

enum prot_stat {
    /* Used in undecided cases */
    PROT_STAT_XXX,
    /* Success */
    PROT_STAT_OK,
    PROT_STAT_UNKNOWN_CMD,
    /* File operation capacity reached */
    PROT_STAT_CAPACITY,
    /* Transfer status report */
    PROT_STAT_XFER,
    /* File not found */
    PROT_STAT_NOTFOUND
};

#define PROT_FILENAME_MAX 512

#define PROT_HDR_SIZE 10        /* cmd{1} + stat{1} + len{8} */
#define PROT_REQ_MAXSIZE (PROT_HDR_SIZE + PROT_FILENAME_MAX)
#define PROT_ACK_SIZE (PROT_HDR_SIZE + 8)
#define PROT_XFER_STAT_SIZE (PROT_HDR_SIZE + 8 + 8)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat;                               \
    uint64_t body_len

struct prot_hdr {
    PROT_HDR_FIELDS;
};

struct prot_pdu {
    PROT_HDR_FIELDS;
    const char* filename;
};

struct prot_ack {
    PROT_HDR_FIELDS;
    uint64_t file_size;
};

struct prot_xfer_stat {
    PROT_HDR_FIELDS;
    uint64_t file_size;
    uint64_t new_file_offset;
};

/* A marshaled request PDU */
struct prot_request_m {
    uint8_t hdr [PROT_HDR_SIZE];
    const char* filename;
    struct iovec iovs[2];
};

/* A marshaled transfer status PDU */
struct prot_xfer_stat_m {
    uint8_t data [PROT_XFER_STAT_SIZE];
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    bool prot_marshal_send(struct prot_request_m* req, const char* filename);

    ssize_t prot_marshal_stat(struct prot_xfer_stat_m* pdu,
                              enum prot_stat stat,
                              size_t file_size, size_t new_file_offset);

    bool prot_unmarshal_request(struct prot_pdu*, const void* buf);

    bool prot_unmarshal_ack(struct prot_ack* pdu, const void* buf);

    bool prot_unmarshal_xfer_stat(struct prot_xfer_stat* pdu, const void* buf);

#ifdef __cplusplus
}
#endif

#endif
