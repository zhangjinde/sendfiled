#ifndef FIOD_PROTOCOL_H
#define FIOD_PROTOCOL_H

#include <sys/uio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum prot_cmd {
    PROT_CMD_STAT,
    PROT_CMD_READ,
    PROT_CMD_SEND,
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

#define PROT_HDR_SIZE 4
#define PROT_FILENAME_MAX 512
#define PROT_PDU_MAXSIZE (PROT_HDR_SIZE + PROT_FILENAME_MAX)

#define PROT_PDU_NIOVS 2

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct prot_pdu {
    uint8_t cmd;
    uint8_t stat;
    uint16_t filename_len;
    const char* filename;
    struct iovec iovs[PROT_PDU_NIOVS];
};

typedef uint8_t prot_stat_buf[18];

#pragma GCC diagnostic pop

bool prot_marshal_send(struct prot_pdu* pdu, const char* filename);

bool prot_marshal_read(struct prot_pdu* pdu, const char* filename);

ssize_t prot_marshal_stat(void* buf, size_t bufsize, enum prot_stat stat,
                          size_t file_size, size_t new_file_offset);

ssize_t prot_unmarshal(const void* buf, size_t size, struct prot_pdu* pdu);

#endif
