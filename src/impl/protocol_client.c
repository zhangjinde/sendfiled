/*
  Copyright (c) 2016, Francois Kritzinger
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

#define _POSIX_C_SOURCE 200809L /* For strnlen */

#include <errno.h>
#include <string.h>

#include "protocol_client.h"
#include "../responses.h"

static bool marshal_req(struct prot_request* pdu,
                        const uint8_t cmd,
                        const off_t offset,
                        const size_t len,
                        const char* filename)
{
    const uint64_t namelen = (filename ?
                              strnlen(filename, PROT_FILENAME_MAX + 1) :
                              0);

    if (namelen == PROT_FILENAME_MAX + 1) {
        errno = ENAMETOOLONG;
        return false;
    }

    /*
      The entire PDU structure is zeroed in order to silence Valgrind which
      complains about the uninitialised alignment padding inserted into the
      structure by the compiler.

      Note that using C99 designated aggregate initialisation instead of
      field-at-a-time intialisation undoes the zeroing memset.
    */
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = cmd;
    pdu->stat = SFD_STAT_OK;
    pdu->offset = offset;
    pdu->len = len;
    pdu->filename = filename;
    pdu->filename_len = namelen;

    return true;
}

bool prot_marshal_read(struct prot_request* req,
                       const char* filename,
                       const off_t offset,
                       const size_t len)
{
    return marshal_req(req,
                       PROT_CMD_READ,
                       offset,
                       len,
                       filename);
}

bool prot_marshal_file_open(struct prot_request* req,
                            const char* filename,
                            off_t offset, size_t len)
{
    return marshal_req(req,
                       PROT_CMD_FILE_OPEN,
                       offset,
                       len,
                       filename);
}

bool prot_marshal_send(struct prot_request* req,
                       const char* filename,
                       const off_t offset,
                       const size_t len)
{
    return marshal_req(req,
                       PROT_CMD_SEND,
                       offset,
                       len,
                       filename);
}

void prot_marshal_send_open(struct prot_send_open* pdu, const size_t txnid)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = PROT_CMD_SEND_OPEN;
    pdu->stat = SFD_STAT_OK;
    pdu->txnid = txnid;
}

void prot_marshal_cancel(struct prot_cancel* pdu, size_t txnid)
{
    memset(pdu, 0, sizeof(*pdu));

    pdu->cmd = PROT_CMD_CANCEL;
    pdu->stat = SFD_STAT_OK;
    pdu->txnid = txnid;
}
