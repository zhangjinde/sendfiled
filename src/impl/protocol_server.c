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

int prot_unmarshal_request(struct prot_request* pdu,
                           const void* buf, const size_t size)
{
    if (size < PROT_REQ_MINSIZE)
        return -1;

    memcpy(pdu, buf, PROT_REQ_BASE_SIZE);

    if (pdu->stat != PROT_STAT_OK)
        return pdu->stat;

    if (pdu->cmd != PROT_CMD_SEND &&
        pdu->cmd != PROT_CMD_READ &&
        pdu->cmd != PROT_CMD_FILE_OPEN) {
        return -1;
    }

    pdu->filename = (char*)((uint8_t*)buf + PROT_REQ_BASE_SIZE);
    pdu->filename_len = (size - PROT_REQ_BASE_SIZE - 1);

    return 0;
}

int prot_unmarshal_send_open(struct prot_send_open* pdu, const void* buf)
{
    memcpy(pdu, buf, sizeof(*pdu));

    if (pdu->stat != PROT_STAT_OK)
        return pdu->stat;

    if (pdu->cmd != PROT_CMD_SEND_OPEN)
        return -1;

    return 0;
}

void prot_marshal_file_info(struct prot_file_info* pdu,
                            const size_t file_size,
                            const time_t atime,
                            const time_t mtime,
                            const time_t ctime)
{
    *pdu = (struct prot_file_info) {
        .cmd = PROT_CMD_FILE_INFO,
        .stat = PROT_STAT_OK,
        .size = file_size,
        .atime = atime,
        .mtime = mtime,
        .ctime = ctime
    };
}

void prot_marshal_open_file_info(struct prot_open_file_info* pdu,
                                 const size_t file_size,
                                 const time_t atime,
                                 const time_t mtime,
                                 const time_t ctime,
                                 const uint32_t txnid)
{
    *pdu = (struct prot_open_file_info) {
        .cmd = PROT_CMD_OPEN_FILE_INFO,
        .stat = PROT_STAT_OK,
        .size = file_size,
        .atime = atime,
        .mtime = mtime,
        .ctime = ctime,
        .txnid = txnid
    };
}

void prot_marshal_xfer_stat(struct prot_xfer_stat* pdu, const size_t file_size)
{
    *pdu = (struct prot_xfer_stat) {
        .cmd = PROT_CMD_XFER_STAT,
        .stat = PROT_STAT_OK,
        .size = file_size
    };
}
