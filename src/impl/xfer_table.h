#ifndef FIOD_XFER_TABLE_H
#define FIOD_XFER_TABLE_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

#pragma GCC diagnostic push
/* #pragma GCC diagnostic ignored "-Wpadded" */

#define XFER_TABLE_BUCKET_SIZE 5

typedef void (*xfer_table_elem_deleter) (void*);

typedef size_t (*xfer_table_hash_func) (void* elem);

struct xfer_table_bucket {
    void* elems [XFER_TABLE_BUCKET_SIZE];
    size_t size;
};

struct xfer_table {
    struct xfer_table_bucket* buckets;
    xfer_table_hash_func hash;
    size_t nbuckets;
};

#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

    bool xfer_table_construct(struct xfer_table*,
                              xfer_table_hash_func hash,
                              size_t max_xfers);

    void xfer_table_destruct(struct xfer_table*, xfer_table_elem_deleter);

    bool xfer_table_insert(struct xfer_table*, void* elem);

    void xfer_table_erase(struct xfer_table*, size_t hash);

    void* xfer_table_find(const struct xfer_table*, size_t hash);

#ifdef __cplusplus
}
#endif

#endif
