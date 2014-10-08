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
