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
    /* Header preceding a chunk of file data */
    PROT_CMD_DATA,
    /* Cancel transfer */
    PROT_CMD_CANCEL
};

enum prot_stat {
    /* No error */
    PROT_STAT_OK
};

#define PROT_FILENAME_MAX 512

#define PROT_HDR_SIZE 10        /* cmd{1} + stat{1} + len{8} */
#define PROT_REQ_MAXSIZE (PROT_HDR_SIZE + PROT_FILENAME_MAX)
#define PROT_ACK_SIZE (PROT_HDR_SIZE + 8)
#define PROT_DATA_FRAME_SIZE (PROT_HDR_SIZE + 8)
#define PROT_PDU_MAXSIZE PROT_REQ_MAXSIZE

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/* ---------------- Unmarshaled PDUs --------------- */

#define PROT_HDR_FIELDS                         \
    uint8_t cmd;                                \
    uint8_t stat;                               \
    uint64_t body_len

struct prot_request {
    PROT_HDR_FIELDS;
    const char* filename;
};

struct prot_ack {
    PROT_HDR_FIELDS;
    uint64_t file_size;
};

struct prot_chunk_hdr {
    PROT_HDR_FIELDS;
    uint64_t chunk_size;
};

/* ---------------- Marshaled PDUs --------------- */

struct prot_hdr_m {
    uint8_t data [PROT_HDR_SIZE];
};

/* A marshaled request PDU */
struct prot_request_m {
    uint8_t hdr [PROT_HDR_SIZE];
    const char* filename;
    struct iovec iovs[2];
};

/* A marshaled ACK PDU */
struct prot_ack_m {
    uint8_t data [PROT_ACK_SIZE];
};

/* A marshaled file data chunk */
struct prot_chunk_hdr_m {
    uint8_t data [PROT_DATA_FRAME_SIZE];
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    void prot_marshal_hdr(struct prot_hdr_m* hdr,
                          enum prot_cmd cmd,
                          enum prot_stat stat,
                          uint64_t len);

    bool prot_marshal_send(struct prot_request_m* req, const char* filename);

    void prot_marshal_ack(struct prot_ack_m* pdu, uint64_t file_size);

    void prot_marshal_nack(struct prot_ack_m* pdu, enum prot_stat stat);

    void prot_marshal_chunk_hdr(struct prot_chunk_hdr_m* pdu,
                                uint64_t file_size);

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
    int prot_unmarshal_ack(struct prot_ack* pdu, const void* buf);

    /**
       @retval 0 Success
       @retval -1 Malformed PDU
       @retval >0 Error code from PDU header
     */
    int prot_unmarshal_chunk_hdr(struct prot_chunk_hdr* pdu, const void* buf);

#ifdef __cplusplus
}
#endif

#endif
