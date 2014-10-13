/*
  Copyright (c) 2014, Francois Kritzinger
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
