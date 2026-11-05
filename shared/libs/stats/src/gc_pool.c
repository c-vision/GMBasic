/* gc_pool.c — allocatore a pool di blocchi fissi (da research-papers). */
#include "gc_pool.h"

#include <stdlib.h>
#include <string.h>

struct gc_pool {
    unsigned char *base;
    size_t block_size;
    size_t n_blocks;
    size_t n_free;
    void *free_head;   /* free-list: ogni slot libero contiene il puntatore al successivo */
};

gc_pool *gc_pool_create(size_t block_size, size_t n_blocks)
{
    gc_pool *p;
    size_t stride, i;
    if (n_blocks == 0) return NULL;
    if (block_size < sizeof(void *)) block_size = sizeof(void *);
    stride = (block_size + 63) & ~(size_t)63;   /* allinea a 64 byte */
    p = (gc_pool *)malloc(sizeof(gc_pool));
    if (!p) return NULL;
    p->base = (unsigned char *)gc_aligned_alloc(64, stride * n_blocks);
    if (!p->base) { free(p); return NULL; }
    p->block_size = stride;
    p->n_blocks = n_blocks;
    p->n_free = n_blocks;
    /* costruisci la free-list */
    for (i = 0; i < n_blocks; i++) {
        void **slot = (void **)(p->base + i * stride);
        *slot = (i + 1 < n_blocks) ? (void *)(p->base + (i + 1) * stride) : NULL;
    }
    p->free_head = p->base;
    return p;
}

void *gc_pool_alloc(gc_pool *p)
{
    void *blk;
    if (!p || !p->free_head) return NULL;
    blk = p->free_head;
    p->free_head = *(void **)blk;
    p->n_free--;
    return blk;
}

void gc_pool_reset(gc_pool *p)
{
    size_t i;
    if (!p) return;
    for (i = 0; i < p->n_blocks; i++) {
        void **slot = (void **)(p->base + i * p->block_size);
        *slot = (i + 1 < p->n_blocks) ? (void *)(p->base + (i + 1) * p->block_size) : NULL;
    }
    p->free_head = p->base;
    p->n_free = p->n_blocks;
}

void gc_pool_destroy(gc_pool *p)
{
    if (!p) return;
    gc_aligned_free(p->base);
    free(p);
}

size_t gc_pool_capacity(const gc_pool *p) { return p ? p->n_blocks : 0; }
size_t gc_pool_available(const gc_pool *p) { return p ? p->n_free : 0; }

void gc_prefetch(const void *addr, int rw, int locality)
{
    (void)rw; (void)locality;
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(addr, 0, 3);
#else
    (void)addr;
#endif
}
