#include <string.h>

#include "protocol_server.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

#pragma GCC diagnostic pop

int prot_get_cmd(const void* buf)
{
    return ((const uint8_t*)buf)[0];
}

int prot_get_stat(const void* buf)
{
    return ((const uint8_t*)buf)[1];
}

bool prot_unmarshal_request(struct prot_request* pdu,
                            const void* buf, const size_t size)
{
    if (size < PROT_REQ_MINSIZE)
        return false;

    memcpy(pdu, buf, PROT_REQ_BASE_SIZE);

    if (pdu->cmd != PROT_CMD_SEND &&
        pdu->cmd != PROT_CMD_READ &&
        pdu->cmd != PROT_CMD_FILE_OPEN) {
        return false;
    }

    pdu->filename = (char*)((uint8_t*)buf + PROT_REQ_BASE_SIZE);
    pdu->filename_len = (size - PROT_REQ_BASE_SIZE - 1);

    return true;
}

bool prot_unmarshal_send_open(struct prot_send_open* pdu, const void* buf)
{
    memcpy(pdu, buf, sizeof(*pdu));
    /* This check is somewhat redundant because the server checks the command ID
       directly before even unmarshaling the PDU. */
    return (pdu->cmd == PROT_CMD_SEND_OPEN);
}

void prot_marshal_file_info(struct prot_file_info* pdu,
                            const size_t file_size,
                            const time_t atime,
                            const time_t mtime,
                            const time_t ctime)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = PROT_CMD_FILE_INFO;
    pdu->stat = PROT_STAT_OK;
    pdu->size = file_size;
    pdu->atime = atime;
    pdu->mtime = mtime;
    pdu->ctime = ctime;
}

void prot_marshal_open_file_info(struct prot_open_file_info* pdu,
                                 const size_t file_size,
                                 const time_t atime,
                                 const time_t mtime,
                                 const time_t ctime,
                                 const size_t txnid)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = PROT_CMD_OPEN_FILE_INFO;
    pdu->stat = PROT_STAT_OK;
    pdu->size = file_size;
    pdu->atime = atime;
    pdu->mtime = mtime;
    pdu->ctime = ctime;
    pdu->txnid = txnid;
}

void prot_marshal_xfer_stat(struct prot_xfer_stat* pdu, const size_t file_size)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = PROT_CMD_XFER_STAT;
    pdu->stat = PROT_STAT_OK;
    pdu->size = file_size;
}
