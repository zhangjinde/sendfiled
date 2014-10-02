#ifndef FIOD_PROTOCOL_SERVER_H
#define FIOD_PROTOCOL_SERVER_H

#include <sys/types.h>

#include <stdint.h>

#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

    enum prot_cmd prot_get_cmd(const void*);

    int prot_get_stat(const void*);

    /**
       @retval 0 Success
       @retval -1 Malformed/invalid PDU
       @retval >0 Error code from PDU header
    */
    int prot_unmarshal_request(struct prot_request*, const void* buf);

    int prot_unmarshal_send_open(struct prot_send_open*, const void* buf);

    void prot_marshal_file_info(prot_file_info_buf pdu,
                                size_t size,
                                const time_t atime,
                                const time_t mtime,
                                const time_t ctime);

    void prot_marshal_open_file_info(prot_open_file_info_buf pdu,
                                     size_t size,
                                     const time_t atime,
                                     const time_t mtime,
                                     const time_t ctime,
                                     uint32_t txnid);

    void prot_marshal_xfer_stat(prot_xfer_stat_buf pdu, size_t val);

#ifdef __cplusplus
}
#endif

#endif
