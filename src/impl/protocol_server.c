#include <string.h>

#include "protocol_server.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

#pragma GCC diagnostic pop

enum prot_cmd prot_get_cmd(const void* buf)
{
    return ((const uint8_t*)buf)[0];
}

int prot_get_stat(const void* buf)
{
    return ((const uint8_t*)buf)[1];
}

int prot_unmarshal_request(struct prot_request* pdu, const void* buf)
{
    const uint8_t* p = prot_unmarshal_hdr((struct prot_hdr*)pdu, buf);

    if (pdu->stat != PROT_STAT_OK)
        return pdu->stat;

    if (pdu->cmd != PROT_CMD_SEND &&
        pdu->cmd != PROT_CMD_READ &&
        pdu->cmd != PROT_CMD_FILE_OPEN) {
        return -1;
    }

    /* offset + len + 1-char filename and its NUL (minimum filename length) */
    if (pdu->body_len < (8 + 8 + 2)) {
        pdu->filename = NULL;
        return -1;
    }

    memcpy(&pdu->offset, p, 8);
    p += 8;
    memcpy(&pdu->len, p, 8);
    p += 8;

    pdu->filename = (char*)p;

    return 0;
}

int prot_unmarshal_send_open(struct prot_send_open* pdu, const void* buf)
{
    const uint8_t* p = prot_unmarshal_hdr((struct prot_hdr*)pdu, buf);

    if (pdu->stat != PROT_STAT_OK)
        return pdu->stat;

    if (pdu->cmd != PROT_CMD_SEND_OPEN || pdu->body_len != PROT_TXNID_SIZE)
        return -1;

    memcpy(&pdu->txnid, p, PROT_TXNID_SIZE);

    return 0;
}

#define INSERT_FIELD(p, f)                      \
    memcpy(p, &f, sizeof(f));                   \
    p += sizeof(f)

void prot_marshal_file_info(struct prot_file_info_m* pdu,
                            const size_t file_size,
                            const time_t atime,
                            const time_t mtime,
                            const time_t ctime)
{
    uint8_t* p = prot_marshal_hdr(pdu->data,
                                  PROT_CMD_FILE_INFO,
                                  PROT_STAT_OK,
                                  PROT_FILE_INFO_BODY_LEN);

    INSERT_FIELD(p, file_size);
    INSERT_FIELD(p, atime);
    INSERT_FIELD(p, mtime);
    INSERT_FIELD(p, ctime);
}

void prot_marshal_open_file_info(struct prot_open_file_info_m* pdu,
                                 const size_t file_size,
                                 const time_t atime,
                                 const time_t mtime,
                                 const time_t ctime,
                                 const uint32_t xfer_id)
{
    uint8_t* p = prot_marshal_hdr(pdu->data,
                                  PROT_CMD_OPEN_FILE_INFO,
                                  PROT_STAT_OK,
                                  PROT_OPEN_FILE_INFO_BODY_LEN);

    INSERT_FIELD(p, file_size);
    INSERT_FIELD(p, atime);
    INSERT_FIELD(p, mtime);
    INSERT_FIELD(p, ctime);
    INSERT_FIELD(p, xfer_id);
}

void prot_marshal_xfer_stat(struct prot_xfer_stat_m* pdu, const size_t file_size)
{
    uint8_t* p = prot_marshal_hdr(pdu->data, PROT_CMD_XFER_STAT, PROT_STAT_OK, 8);
    memcpy(p, &file_size, 8);
}
