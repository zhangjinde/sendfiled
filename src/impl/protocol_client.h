#ifndef FIOD_PROTOCOL_CLIENT_H
#define FIOD_PROTOCOL_CLIENT_H

#include <sys/types.h>
#include <sys/uio.h>

#include <stdbool.h>

#include "protocol.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/* A marshaled request PDU */
struct prot_request_m {
    uint8_t hdr [PROT_REQ_BASE_SIZE];
    const char* filename;
    struct iovec iovs[2];
};

struct prot_file_info {
    PROT_HDR_FIELDS;
    size_t size;
    time_t atime;
    time_t mtime;
    time_t ctime;
};

struct prot_xfer_stat {
    PROT_HDR_FIELDS;
    size_t size;
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    bool prot_marshal_send(struct prot_request_m* req, const char* filename,
                           loff_t offset, size_t len);

    bool prot_marshal_read(struct prot_request_m* req, const char* filename,
                           loff_t offset, size_t len);

    /**
       @retval 0 Success
       @retval -1 Malformed PDU
       @retval >0 Error code from PDU header
    */
    int prot_unmarshal_file_info(struct prot_file_info* pdu, const void* buf);

    /**
       @retval 0 Success
       @retval -1 Malformed PDU
       @retval >0 Error code from PDU header
    */
    int prot_unmarshal_xfer_stat(struct prot_xfer_stat* pdu, const void* buf);

#ifdef __cplusplus
}
#endif

#endif
