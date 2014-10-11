#ifndef FIOD_PROTOCOL_SERVER_H
#define FIOD_PROTOCOL_SERVER_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

    bool prot_unmarshal_request(struct prot_request*,
                                const void* buf, size_t size);

    bool prot_unmarshal_send_open(struct prot_send_open*, const void* buf);

    void prot_marshal_file_info(struct prot_file_info* pdu,
                                size_t size,
                                const time_t atime,
                                const time_t mtime,
                                const time_t ctime);

    void prot_marshal_open_file_info(struct prot_open_file_info* pdu,
                                     size_t size,
                                     const time_t atime,
                                     const time_t mtime,
                                     const time_t ctime,
                                     size_t txnid);

    void prot_marshal_xfer_stat(struct prot_xfer_stat* pdu, size_t val);

#ifdef __cplusplus
}
#endif

#endif
