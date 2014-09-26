#include <string.h>

#include "protocol_server.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

#pragma GCC diagnostic pop

int prot_unmarshal_request(struct prot_request* pdu, const void* buf)
{
    const uint8_t* p = prot_unmarshal_hdr((struct prot_hdr*)pdu, buf);

    if (pdu->cmd != PROT_CMD_SEND && pdu->cmd != PROT_CMD_READ)
        return -1;

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

void prot_marshal_stat(struct prot_file_stat_m* pdu, const size_t file_size)
{
    uint8_t* p = prot_marshal_hdr(pdu->data, PROT_CMD_STAT, PROT_STAT_OK, 8);
    memcpy(p, &file_size, 8);
}
