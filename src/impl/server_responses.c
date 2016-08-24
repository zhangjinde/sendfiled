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

#include <assert.h>
#include <unistd.h>

#include "protocol_server.h"
#include "server_responses.h"
#include "../responses.h"

bool send_pdu(const int fd, const void* pdu, const size_t size)
{
    const ssize_t n = write(fd, pdu, size);
    assert (n == -1 || (size_t)n == size);
    return ((size_t)n == size);
}

bool send_file_info(int cli_fd,
                    const size_t txnid,
                    const struct fio_stat* info)
{
    struct sfd_file_info pdu;

    prot_marshal_file_info(&pdu,
                           info->size,
                           info->atime, info->mtime, info->ctime,
                           txnid);

    return send_pdu(cli_fd, &pdu, sizeof(pdu));
}

bool send_xfer_stat(const int fd, const size_t file_size)
{
    struct sfd_xfer_stat pdu;
    prot_marshal_xfer_stat(&pdu, file_size);
    return send_pdu(fd, &pdu, sizeof(pdu));
}

bool send_req_err(const int fd, const int stat)
{
    assert (stat > 0);
    assert (stat <= 0xFF);

    const struct prot_hdr pdu = {
        .cmd = SFD_FILE_INFO,
        .stat = (uint8_t)stat
    };

    return send_pdu(fd, &pdu, sizeof(pdu));
}

bool send_xfer_err(const int fd, const int stat)
{
    const struct prot_hdr pdu = {
        .cmd = SFD_XFER_STAT,
        .stat = (uint8_t)stat
    };

    return send_pdu(fd, &pdu, sizeof(pdu));
}
