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

/**
   A marshaled file info PDU
 */
struct prot_file_info_m {
    uint8_t data [PROT_FILE_INFO_SIZE];
};

/* A marshaled tranfer status PDU */
struct prot_xfer_stat_m {
    uint8_t data [PROT_XFER_STAT_SIZE];
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

    void prot_marshal_file_info(struct prot_file_info_m* pdu,
                                size_t size,
                                const time_t atime,
                                const time_t mtime,
                                const time_t ctime);

    void prot_marshal_xfer_stat(struct prot_xfer_stat_m* pdu, size_t val);

#ifdef __cplusplus
}
#endif

#endif
