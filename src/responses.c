#include <string.h>

#include "impl/protocol.h"

#include "responses.h"

int fiod_get_cmd(const void* buf)
{
    return ((const uint8_t*)buf)[0];
}

int fiod_get_stat(const void* buf)
{
    return ((const uint8_t*)buf)[1];
}

#define HDR_OK(buf, cmd)                                              \
    (fiod_get_cmd(buf) == cmd && fiod_get_stat(buf) == PROT_STAT_OK)

bool fiod_unmarshal_file_info(struct fiod_file_info* pdu, const void* buf)
{
    if (!HDR_OK(buf, FIOD_FILE_INFO))
        return false;

    memcpy(pdu, buf, sizeof(*pdu));

    return true;
}

bool fiod_unmarshal_open_file_info(struct fiod_open_file_info* pdu,
                                   const void* buf)
{
    if (!HDR_OK(buf, FIOD_OPEN_FILE_INFO))
        return false;

    memcpy(pdu, buf, sizeof(*pdu));

    return true;
}

bool fiod_unmarshal_xfer_stat(struct fiod_xfer_stat* pdu, const void* buf)
{
    if (!HDR_OK(buf, FIOD_XFER_STAT))
        return false;

    memcpy(pdu, buf, sizeof(*pdu));

    return true;
}

bool fiod_xfer_complete(const struct fiod_xfer_stat* this)
{
    return (this->size == PROT_XFER_COMPLETE);
}
