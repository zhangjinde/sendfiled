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
    const size_t capacity = clp2(max_xfers);

    *this = (struct xfer_table) {
        .elems = calloc(capacity, sizeof(void*)),
        .capacity = capacity,
        .hash = hash
    };

    return (bool)this->elems;
}

void xfer_table_destruct(struct xfer_table* this,
                         xfer_table_elem_deleter delete_elem)
{
    if (delete_elem) {
        for (size_t i = 0; i < this->capacity; i++) {
            if (this->elems[i])
                delete_elem(this->elems[i]);
        }
    }

    free(this->elems);
}

struct xfer_table* xfer_table_new(xfer_table_hash_func hash,
                                  size_t max_xfers)
{
    struct xfer_table* this = malloc(sizeof(*this));
    if (!this)
        return NULL;

    xfer_table_construct(this, hash, max_xfers);

    return this;
}

void xfer_table_delete(struct xfer_table* this,
                       xfer_table_elem_deleter delete_elem)
{
    xfer_table_destruct(this, delete_elem);
    free(this);
}

static size_t indexof(const struct xfer_table* this, const size_t hash)
{
    return (hash & (this->capacity - 1));
}

bool xfer_table_insert(struct xfer_table* this, void* elem)
{
    const size_t idx = indexof(this, this->hash(elem));

    if (this->elems[idx])
        return false;

    this->elems[idx] = elem;
    this->size++;

    return true;
}

void xfer_table_erase(struct xfer_table* this, const size_t hash)
{
    this->elems[indexof(this, hash)] = NULL;
    this->size--;
}

void* xfer_table_find(const struct xfer_table* this, const size_t hash)
{
    return this->elems[indexof(this, hash)];
}
