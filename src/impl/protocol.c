#define _POSIX_C_SOURCE 200809L /* For strnlen */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "protocol.h"

static bool marshal(struct prot_pdu* pdu,
                    const int cmd,
                    const int stat,
                    const char* filename)
{
    const size_t namelen = (filename ?
                            strnlen(filename, PROT_FILENAME_MAX + 1) :
                            0);

    if (namelen == PROT_FILENAME_MAX + 1) {
        errno = ENAMETOOLONG;
        return false;
    }

    *pdu = (struct prot_pdu) {
        .cmd = (uint8_t)cmd,
        .stat = (uint8_t)stat,
        .filename_len = (uint16_t)namelen,
        .filename = filename,

        .iovs = {
            (struct iovec) {
                .iov_base = pdu,
                .iov_len = (offsetof(struct prot_pdu, filename_len) + 2)
            },
            (struct iovec) {
                .iov_base = (void*)filename,
                .iov_len = namelen
            }
        }
    };

    return true;
}

bool prot_marshal_send(struct prot_pdu* pdu, const char* filename)
{
    return marshal(pdu, PROT_CMD_SEND, PROT_STAT_OK, filename);
}

bool prot_marshal_read(struct prot_pdu* pdu, const char* filename)
{
    return marshal(pdu, PROT_CMD_READ, PROT_STAT_OK, filename);
}

ssize_t prot_marshal_stat(void* buf, size_t bufsize, enum prot_stat stat,
                          const size_t file_size, const size_t new_file_offset)
{
    if (bufsize < sizeof(prot_stat_buf))
        return 0;

    uint8_t* p = buf;

    *p++ = PROT_CMD_STAT;
    *p++ = stat;
    memcpy(p, &file_size, 8);
    p += 8;
    memcpy(p, &new_file_offset, 8);
    p += 8;

    return (p - (uint8_t*)buf);
}

ssize_t prot_unmarshal(const void* buf, const size_t size, struct prot_pdu* pdu)
{
    if (size == 0)
        return -1;

    /* Uses UDP, so we should have an entire message if we have anything */
    assert (size >= PROT_HDR_SIZE);

    const uint8_t* p = buf;

    *pdu = (struct prot_pdu) {
        .cmd = *p,
        .stat = *(p + 1),
        .filename_len = (uint16_t)(*(p + 2) | (*(p + 3) << 8))
    };
    p += 4;

    if (pdu->filename_len == 0)
        return (p - (uint8_t*)buf);

    pdu->filename = (char*)p;

    p += pdu->filename_len;

    return (p - (uint8_t*)buf);
}
