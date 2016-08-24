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
#include <stdlib.h>
#include <unistd.h>

#include "file_io.h"
#include "server_resources.h"

struct resrc_xfer* xfer_new(const enum prot_cmd_req cmd,
                            const struct resrc_xfer_file* file,
                            const size_t nbytes,
                            const pid_t client_pid,
                            const int stat_fd,
                            const int dest_fd,
                            const size_t txnid)
{
    struct resrc_xfer* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    *this = (struct resrc_xfer) {
        .dest_fd = dest_fd,
        .tag = XFER_RESRC_TAG,
        .stat_fd = stat_fd,
        .txnid = txnid,
        .file = *file,
        .fio_ctx = fio_ctx_new(file->blksize),
        .nbytes_left = nbytes,
        .cmd = cmd,
        .client_pid = client_pid,
        .defer = NONE
    };

    if (!fio_ctx_valid(this->fio_ctx)) {
        free(this);
        return NULL;
    }

    return this;
}

bool is_xfer(const void* p)
{
    return (((const struct resrc_xfer*)p)->tag == XFER_RESRC_TAG);
}

size_t resrc_xfer_txnid(void* p)
{
    return ((struct resrc_xfer*)p)->txnid;
}

void xfer_delete(void* p)
{
    if (p) {
        struct resrc_xfer* const this = p;

        assert (this->tag == XFER_RESRC_TAG);

        fio_ctx_delete(this->fio_ctx);
        free(this);
    }
}

bool is_response(const void* p)
{
    return (((const struct resrc_resp*)p)->tag == PENDING_RESP_TAG);
}

bool is_timer(const void* p)
{
    return (((const struct resrc_timer*)p)->tag == TIMER_RESRC_TAG);
}

size_t resrc_timer_txnid(void* p)
{
    return ((struct resrc_timer*)p)->txnid;
}

void timer_delete(void* p)
{
    if (p) {
        struct resrc_timer* const this = p;

        assert (this->tag == TIMER_RESRC_TAG);

        close(this->ident);
        free(this);
    }
}
