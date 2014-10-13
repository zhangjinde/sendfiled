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

#ifndef FIOD_XFER_TABLE_H
#define FIOD_XFER_TABLE_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

typedef void (*xfer_table_elem_deleter) (void*);

typedef size_t (*xfer_table_hash_func) (void* elem);

struct xfer_table {
    void** elems;
    size_t capacity;
    size_t size;
    xfer_table_hash_func hash;
};

#ifdef __cplusplus
extern "C" {
#endif

    bool xfer_table_construct(struct xfer_table*,
                              xfer_table_hash_func hash,
                              size_t max_xfers);

    void xfer_table_destruct(struct xfer_table*, xfer_table_elem_deleter);

    struct xfer_table* xfer_table_new(xfer_table_hash_func hash,
                                      size_t max_xfers);

    void xfer_table_delete(struct xfer_table*, xfer_table_elem_deleter);

    bool xfer_table_insert(struct xfer_table*, void* elem);

    void xfer_table_erase(struct xfer_table*, size_t hash);

    void* xfer_table_find(const struct xfer_table*, size_t hash);

#ifdef __cplusplus
}
#endif

#endif
