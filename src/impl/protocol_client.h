#ifndef FIOD_PROTOCOL_CLIENT_H
#define FIOD_PROTOCOL_CLIENT_H

#include <sys/types.h>
#include <sys/uio.h>

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

    bool prot_marshal_file_open(struct prot_request_m* req,
                                const char* filename,
                                loff_t offset, size_t len);

    bool prot_marshal_send(struct prot_request_m* req,
                           const char* filename,
                           loff_t offset, size_t len);

    void prot_marshal_send_open(prot_send_open_buf,
                                uint32_t txnid);

    bool prot_marshal_read(struct prot_request_m* req,
                           const char* filename,
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
    int prot_unmarshal_open_file_info(struct prot_open_file_info* pdu,
                                      const void* buf);

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
