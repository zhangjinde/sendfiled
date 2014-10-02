#define _POSIX_C_SOURCE 200809L /* For strnlen */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "protocol_client.h"

static bool marshal_req(struct prot_request_m* req,
                        const uint8_t cmd,
                        const loff_t offset,
                        const size_t len,
                        const char* filename)
{
    const uint64_t namelen = (filename ?
                              strnlen(filename, PROT_FILENAME_MAX + 1) :
                              0);

    if (namelen == PROT_FILENAME_MAX + 1) {
        errno = ENAMETOOLONG;
        return false;
    }

    /* 16 for the offset and len fields; +1 for terminating '\0' */
    const size_t body_len = sizeof(offset) + sizeof(len) + namelen + 1;

    uint8_t* p = prot_marshal_hdr(req->hdr, cmd, PROT_STAT_OK, body_len);

    INSERT_FIELD(p, offset);
    INSERT_FIELD(p, len);

    req->filename = filename;

    req->iovs[0].iov_base = req->hdr;
    req->iovs[0].iov_len = (size_t)(p - req->hdr);

    req->iovs[1].iov_base = (void*)req->filename;
    req->iovs[1].iov_len = namelen + 1; /* Include the terminating '\0' */

    return true;
}

bool prot_marshal_read(struct prot_request_m* req,
                       const char* filename,
                       const loff_t offset,
                       const size_t len)
{
    return marshal_req(req,
                       PROT_CMD_READ,
                       offset,
                       len,
                       filename);
}

bool prot_marshal_file_open(struct prot_request_m* req,
                            const char* filename,
                            loff_t offset, size_t len)
{
    return marshal_req(req,
                       PROT_CMD_FILE_OPEN,
                       offset,
                       len,
                       filename);
}

bool prot_marshal_send(struct prot_request_m* req,
                       const char* filename,
                       const loff_t offset,
                       const size_t len)
{
    return marshal_req(req,
                       PROT_CMD_SEND,
                       offset,
                       len,
                       filename);
}

void prot_marshal_send_open(prot_send_open_buf buf, const uint32_t txnid)
{
    uint8_t* p = prot_marshal_hdr(buf,
                                  PROT_CMD_SEND_OPEN,
                                  PROT_STAT_OK,
                                  sizeof(txnid));

    INSERT_FIELD(p, txnid);
}

static int check_pdu(struct prot_hdr* hdr,
                     const enum prot_cmd expected_cmd,
                     const uint64_t expected_body_len)
{
    if (hdr->cmd != expected_cmd)
        return -1;

    if (hdr->body_len == 0)     /* Error message; no body */
        return hdr->stat;

    if (hdr->body_len != expected_body_len)
        return -1;

    return 0;
}

int prot_unmarshal_file_info(struct prot_file_info* pdu, const void* buf)
{
    const uint8_t* p = prot_unmarshal_hdr((struct prot_hdr*)pdu, buf);

    const int err = check_pdu((struct prot_hdr*)pdu,
                              PROT_CMD_FILE_INFO,
                              PROT_FILE_INFO_SIZE - PROT_HDR_SIZE);
    if (err != 0)
        return err;

    EXTRACT_FIELD(p, pdu->size);
    EXTRACT_FIELD(p, pdu->atime);
    EXTRACT_FIELD(p, pdu->mtime);
    EXTRACT_FIELD(p, pdu->ctime);

    return 0;
}

int prot_unmarshal_open_file_info(struct prot_open_file_info* pdu,
                                  const void* buf)
{
    const uint8_t* p = prot_unmarshal_hdr((struct prot_hdr*)pdu, buf);

    const int err = check_pdu((struct prot_hdr*)pdu,
                              PROT_CMD_OPEN_FILE_INFO,
                              PROT_SIZEOF(prot_open_file_info, txnid));
    if (err != 0)
        return err;

    EXTRACT_FIELD(p, pdu->size);
    EXTRACT_FIELD(p, pdu->atime);
    EXTRACT_FIELD(p, pdu->mtime);
    EXTRACT_FIELD(p, pdu->ctime);
    EXTRACT_FIELD(p, pdu->txnid);

    return 0;
}

int prot_unmarshal_xfer_stat(struct prot_xfer_stat* pdu, const void* buf)
{
    const uint8_t* p = prot_unmarshal_hdr((struct prot_hdr*)pdu, buf);

    const int err = check_pdu((struct prot_hdr*)pdu, PROT_CMD_XFER_STAT, 8);
    if (err != 0)
        return err;

    EXTRACT_FIELD(p, pdu->size);

    return 0;
}
