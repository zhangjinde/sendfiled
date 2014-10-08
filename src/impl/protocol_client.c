#define _POSIX_C_SOURCE 200809L /* For strnlen */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "protocol_client.h"

static bool marshal_req(struct prot_request* pdu,
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

    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = cmd;
    pdu->stat = PROT_STAT_OK;
    pdu->offset = offset;
    pdu->len = len;
    pdu->filename = filename;
    pdu->filename_len = namelen;

    return true;
}

bool prot_marshal_read(struct prot_request* req,
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

bool prot_marshal_file_open(struct prot_request* req,
                            const char* filename,
                            loff_t offset, size_t len)
{
    return marshal_req(req,
                       PROT_CMD_FILE_OPEN,
                       offset,
                       len,
                       filename);
}

bool prot_marshal_send(struct prot_request* req,
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

void prot_marshal_send_open(struct prot_send_open* pdu, const size_t txnid)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = PROT_CMD_SEND_OPEN;
    pdu->stat = PROT_STAT_OK;
    pdu->txnid = txnid;
}

static int check_hdr(struct prot_hdr* hdr,
                     const enum prot_cmd_resp expected_cmd)
{
    if (hdr->cmd != expected_cmd)
        return -1;

    if (hdr->stat != PROT_STAT_OK)
        return hdr->stat;

    return 0;
}

int prot_unmarshal_file_info(struct prot_file_info* pdu, const void* buf)
{
    memcpy(pdu, buf, sizeof(*pdu));

    return check_hdr((struct prot_hdr*)pdu, PROT_CMD_FILE_INFO);
}

int prot_unmarshal_open_file_info(struct prot_open_file_info* pdu,
                                  const void* buf)
{
    memcpy(pdu, buf, sizeof(*pdu));

    return check_hdr((struct prot_hdr*)pdu, PROT_CMD_OPEN_FILE_INFO);
}

int prot_unmarshal_xfer_stat(struct prot_xfer_stat* pdu, const void* buf)
{
    memcpy(pdu, buf, sizeof(*pdu));

    return check_hdr((struct prot_hdr*)pdu, PROT_CMD_XFER_STAT);
}
