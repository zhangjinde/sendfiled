#define _POSIX_C_SOURCE 200809L /* For strnlen */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "protocol.h"

static uint8_t* marshal_hdr(void* buf,
                            const enum prot_cmd cmd,
                            const enum prot_stat stat,
                            const uint64_t len)
{
    uint8_t* p = buf;

    *p++ = cmd;
    *p++ = stat;
    memcpy(p, &len, 8);

    return (p + 8);
}

bool prot_marshal_send(struct prot_request_m* req,
                       const char* filename)
{
    const uint64_t namelen = (filename ?
                              strnlen(filename, PROT_FILENAME_MAX + 1) :
                              0);

    if (namelen == PROT_FILENAME_MAX + 1) {
        errno = ENAMETOOLONG;
        return false;
    }

    marshal_hdr(req->hdr, PROT_CMD_SEND, PROT_STAT_OK, namelen);

    req->filename = filename;

    req->iovs[0].iov_base = req->hdr;
    req->iovs[0].iov_len = PROT_HDR_SIZE;

    req->iovs[1].iov_base = (void*)req->filename;
    req->iovs[1].iov_len = namelen;

    return true;
}

ssize_t prot_marshal_stat(struct prot_xfer_stat_m* pdu, enum prot_stat stat,
                          const size_t file_size, const size_t new_file_offset)
{
    uint8_t* p = marshal_hdr(pdu->data, PROT_CMD_STAT, stat, 16);

    memcpy(p, &file_size, 8);
    p += 8;
    memcpy(p, &new_file_offset, 8);
    p += 8;

    return (p - pdu->data);
}

static const uint8_t* unmarshal_hdr(struct prot_hdr* hdr, const void* buf)
{
    const uint8_t* p = buf;

    *hdr = (struct prot_hdr) {
        .cmd = *p,
        .stat = *(p + 1)
    };
    p += 2;

    memcpy(&hdr->body_len, p, 8);
    p += 8;

    assert ((p - (uint8_t*)buf) == PROT_HDR_SIZE);
    return p;
}

bool prot_unmarshal_request(struct prot_pdu* pdu, const void* buf)
{
    const uint8_t* p = unmarshal_hdr((struct prot_hdr*)pdu, buf);

    if (pdu->body_len > 0) {
        pdu->filename = (char*)p;
        return true;
    } else {
        pdu->filename = NULL;
        return false;
    }
}

bool prot_unmarshal_ack(struct prot_ack* pdu, const void* buf)
{
    const uint8_t* p = unmarshal_hdr((struct prot_hdr*)pdu, buf);

    if (pdu->body_len != 8)
        return false;

    memcpy(&pdu->file_size, p, 8);

    return true;
}

bool prot_unmarshal_xfer_stat(struct prot_xfer_stat* pdu, const void* buf)
{
    const uint8_t* p = unmarshal_hdr((struct prot_hdr*)pdu, buf);

    if (pdu->body_len != 16)
        return false;

    memcpy(&pdu->file_size, p, 8);
    memcpy(&pdu->new_file_offset, p + 8, 8);

    return true;
}
