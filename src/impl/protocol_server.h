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

#ifndef FIOD_PROTOCOL_SERVER_H
#define FIOD_PROTOCOL_SERVER_H

#include <sys/types.h>

#include <stdbool.h>

#include "protocol.h"

struct fiod_file_info;
struct fiod_open_file_info;
struct fiod_xfer_stat;

#ifdef __cplusplus
extern "C" {
#endif

    bool prot_unmarshal_request(struct prot_request*,
                                const void* buf, size_t size);

    bool prot_unmarshal_send_open(struct prot_send_open*, const void* buf);

    void prot_marshal_file_info(struct fiod_file_info* pdu,
                                size_t size,
                                const time_t atime,
                                const time_t mtime,
                                const time_t ctime);

    void prot_marshal_open_file_info(struct fiod_open_file_info* pdu,
                                     size_t size,
                                     const time_t atime,
                                     const time_t mtime,
                                     const time_t ctime,
                                     size_t txnid);

    void prot_marshal_xfer_stat(struct fiod_xfer_stat* pdu, size_t val);

#ifdef __cplusplus
}
#endif

#endif
