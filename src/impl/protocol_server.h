#ifndef FIOD_PROTOCOL_SERVER_H
#define FIOD_PROTOCOL_SERVER_H

#include <sys/types.h>

#include <stdint.h>

#include "protocol.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct prot_hdr_m {
    uint8_t data [PROT_HDR_SIZE];
};

struct prot_request {
    PROT_HDR_FIELDS;
    loff_t offset;
    size_t len;
    const char* filename;
};

/* A marshaled file status PDU */
struct prot_file_stat_m {
    uint8_t data [PROT_STAT_SIZE];
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @retval 0 Success
       @retval -1 Malformed PDU
       @retval >0 Error code from PDU header
    */
    int prot_unmarshal_request(struct prot_request*, const void* buf);

    void prot_marshal_stat(struct prot_file_stat_m* pdu, size_t val);

#ifdef __cplusplus
}
#endif

#endif
