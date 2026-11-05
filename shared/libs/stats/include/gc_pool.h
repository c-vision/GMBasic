/*
 * gc_pool.h — allocatore a pool di blocchi a dimensione fissa (da research-papers).
 *
 * Pre-alloca un blocco contiguo diviso in `n_blocks` slot di `block_size` byte
 * (allineati). gc_pool_alloc è O(1) (free-list), gc_pool_reset libera tutti gli
 * slot in O(1). Utile per buffer di scratch nei kernel (evita malloc/free).
 * Inoltre gc_prefetch: hint di prefetch in cache (__builtin_prefetch).
 */
#ifndef GMBCOMPUTE_GC_POOL_H
#define GMBCOMPUTE_GC_POOL_H

#include "gc_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gc_pool gc_pool;

/* Crea un pool con n_blocks slot da block_size byte (arrotondato ad almeno
 * sizeof(void*) e allineato a 64 byte). NULL su errore. */
gc_pool *gc_pool_create(size_t block_size, size_t n_blocks);
/* Alloca uno slot (O(1)). NULL se esaurito. */
void *gc_pool_alloc(gc_pool *p);
/* Riporta il pool allo stato iniziale (tutti gli slot liberi), O(1). */
void gc_pool_reset(gc_pool *p);
void gc_pool_destroy(gc_pool *p);
/* Numero di slot totali e liberi. */
size_t gc_pool_capacity(const gc_pool *p);
size_t gc_pool_available(const gc_pool *p);

/* Prefetch di un indirizzo in cache (locality 0..3). No-op se non supportato. */
void gc_prefetch(const void *addr, int rw, int locality);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_POOL_H */
