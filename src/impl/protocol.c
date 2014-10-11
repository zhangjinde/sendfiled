#include "protocol.h"

int prot_get_cmd(const void* buf)
{
    return ((const uint8_t*)buf)[0];
}

int prot_get_stat(const void* buf)
{
    return ((const uint8_t*)buf)[1];
}
