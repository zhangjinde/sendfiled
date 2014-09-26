#include <assert.h>
#include <string.h>

#include "protocol.h"

uint8_t* prot_marshal_hdr(void* buf,
                          const enum prot_cmd cmd,
                          const int stat,
                          const size_t len)
{
    assert (stat >= 0 && stat <= 0xFF);

    uint8_t* p = buf;

    *p++ = cmd;
    *p++ = (uint8_t)stat;
    memcpy(p, &len, 8);

    return (p + 8);
}

const uint8_t* prot_unmarshal_hdr(struct prot_hdr* hdr, const void* buf)
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
