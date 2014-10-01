#include <stdlib.h>

#include "xfer_table.h"

/**
   A ceil() which returns powers of 2.

   Clarity over efficiency because it will only be executed at startup.
*/
static size_t clp2(const size_t x)
{
    size_t i = 0;
    while ((1 << i) < x) { i++; }
    return (1 << i);
}

bool xfer_table_construct(struct xfer_table* this,
                          xfer_table_hash_func hash,
                          const size_t max_xfers)
{
    const size_t nbuckets = clp2(max_xfers / XFER_TABLE_BUCKET_SIZE);

    *this = (struct xfer_table) {
        .nbuckets = nbuckets,
        .buckets = calloc(nbuckets, sizeof(struct xfer_table_bucket)),
        .hash = hash
    };

    if (!this->buckets)
        return false;

    return true;
}

void xfer_table_destruct(struct xfer_table* this,
                         xfer_table_elem_deleter delete_elem)
{
    if (delete_elem) {
        for (size_t i = 0; i < this->nbuckets; i++) {
            const struct xfer_table_bucket* const b = &this->buckets[i];
            for (size_t j = 0; j < b->size; j++)
                delete_elem(b->elems[j]);
        }
    }

    free(this->buckets);
}

static struct xfer_table_bucket* get_bucket(const struct xfer_table* this,
                                            const size_t hash)
{
    return &this->buckets[hash & (this->nbuckets - 1)];
}

bool xfer_table_insert(struct xfer_table* this, void* elem)
{
    struct xfer_table_bucket* const b = get_bucket(this, this->hash(elem));

    if (b->size == XFER_TABLE_BUCKET_SIZE)
        return false;

    b->elems[b->size] = elem;
    b->size++;

    return true;
}

void xfer_table_erase(struct xfer_table* this, const size_t hash)
{
    struct xfer_table_bucket* const b = get_bucket(this, hash);

    for (size_t i = 0; i < b->size; i++) {
        if (this->hash(b->elems[i]) == hash) {
            b->elems[i] = b->elems[b->size - 1];
            b->size--;
            break;
        }
    }
}

void* xfer_table_find(const struct xfer_table* this, const size_t hash)
{
    const struct xfer_table_bucket* const b = get_bucket(this, hash);

    for (size_t i = 0; i < b->size; i++) {
        if (this->hash(b->elems[i]) == hash)
            return b->elems[i];
    }

    return NULL;
}
