/*
  Copyright (c) 2015, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <errno.h>
#include <string.h>

#include "protocol_server.h"
#include "../responses.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

#pragma GCC diagnostic pop

bool prot_unmarshal_request(struct prot_request* pdu,
                            const void* buf, const size_t size)
{
    if (size < PROT_REQ_MINSIZE)
        return false;

    const int cmd = sfd_get_cmd(buf);

    if (cmd != PROT_CMD_SEND &&
        cmd != PROT_CMD_READ &&
        cmd != PROT_CMD_FILE_OPEN) {
        return false;
    }

    if (sfd_get_stat(buf) != SFD_STAT_OK)
        return false;

    /* Check that filename is NUL-terminated */
    if (*((char*)buf + (size - 1)) != '\0')
        return false;

    const size_t fname_len = (size - PROT_REQ_BASE_SIZE - 1);

    if (fname_len > PROT_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        return false;
    }

    memcpy(pdu, buf, PROT_REQ_BASE_SIZE);

    /* The rest of the PDU is the filename */

    pdu->filename = (char*)buf + PROT_REQ_BASE_SIZE;
    pdu->filename_len = fname_len;

    return true;
}

bool prot_unmarshal_send_open(struct prot_send_open* pdu, const void* buf)
{
    if (sfd_get_cmd(buf) != PROT_CMD_SEND_OPEN ||
        sfd_get_stat(buf) != SFD_STAT_OK) {
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

void prot_marshal_file_info(struct sfd_file_info* pdu,
                            const size_t file_size,
                            const time_t atime,
                            const time_t mtime,
                            const time_t ctime)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = SFD_FILE_INFO;
    pdu->stat = SFD_STAT_OK;
    pdu->size = file_size;
    pdu->atime = atime;
    pdu->mtime = mtime;
    pdu->ctime = ctime;
}

void prot_marshal_open_file_info(struct sfd_open_file_info* pdu,
                                 const size_t file_size,
                                 const time_t atime,
                                 const time_t mtime,
                                 const time_t ctime,
                                 const size_t txnid)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = SFD_OPEN_FILE_INFO;
    pdu->stat = SFD_STAT_OK;
    pdu->size = file_size;
    pdu->atime = atime;
    pdu->mtime = mtime;
    pdu->ctime = ctime;
    pdu->txnid = txnid;
}

void prot_marshal_xfer_stat(struct sfd_xfer_stat* pdu, const size_t file_size)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = SFD_XFER_STAT;
    pdu->stat = SFD_STAT_OK;
    pdu->size = file_size;
}
