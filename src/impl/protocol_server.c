#include <string.h>

#include "protocol_server.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

#pragma GCC diagnostic pop

bool prot_unmarshal_request(struct prot_request* pdu,
                            const void* buf, const size_t size)
{
    if (size < PROT_REQ_MINSIZE)
        return false;

    const int cmd = prot_get_cmd(buf);

    if (cmd != PROT_CMD_SEND &&
        cmd != PROT_CMD_READ &&
        cmd != PROT_CMD_FILE_OPEN) {
        return false;
    }

    if (prot_get_stat(buf) != PROT_STAT_OK)
        return false;

    /* Check that filename is NUL-terminated */
    if (*((uint8_t*)buf + (size - 1)) != '\0')
        return false;

    memcpy(pdu, buf, PROT_REQ_BASE_SIZE);

    /* The rest of the PDU is the filename */

    pdu->filename = (char*)((uint8_t*)buf + PROT_REQ_BASE_SIZE);
    pdu->filename_len = (size - PROT_REQ_BASE_SIZE - 1);

    return true;
}

bool prot_unmarshal_send_open(struct prot_send_open* pdu, const void* buf)
{
    if (prot_get_cmd(buf) != PROT_CMD_SEND_OPEN ||
        prot_get_stat(buf) != PROT_STAT_OK) {
        return false;
    }

    memcpy(pdu, buf, sizeof(*pdu));

    return true;
}

/*
  The marshaling functions below zero the entire PDU structure in order to
  silence Valgrind which complains about the uninitialised alignment padding
  inserted by the compiler.

  Note that using C99 designated aggregate initialisation instead of
  field-at-a-time intialisation undoes the zeroing memset.
*/

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
