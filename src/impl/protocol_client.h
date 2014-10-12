#ifndef FIOD_PROTOCOL_CLIENT_H
#define FIOD_PROTOCOL_CLIENT_H

#include <sys/types.h>
#include <sys/uio.h>

#include <stdbool.h>

#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

    bool prot_marshal_file_open(struct prot_request* req,
                                const char* filename,
                                loff_t offset, size_t len);

    bool prot_marshal_send(struct prot_request* req,
                           const char* filename,
                           loff_t offset, size_t len);

    void prot_marshal_send_open(struct prot_send_open*,
                                size_t txnid);

    bool prot_marshal_read(struct prot_request* req,
                           const char* filename,
                           loff_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif
