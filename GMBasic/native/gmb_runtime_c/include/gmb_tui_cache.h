/* ============================================================================
 * gmb_tui_cache.h — TinyCache (specchio ClipperLib/Clipper TinyCache.cs).
 * Cache LRU-ish bounded con eviction del più vecchio. Puro.
 * ============================================================================ */
#ifndef GMB_TUI_CACHE_H
#define GMB_TUI_CACHE_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_CACHE_MAX 64

typedef struct gmb_cache_entry {
    int key;
    int value;
    bool used;
} gmb_cache_entry_t;

typedef struct gmb_tiny_cache {
    int capacity;
    gmb_cache_entry_t entries[GMB_CACHE_MAX];
    int order[GMB_CACHE_MAX];   /* ordine di inserimento (indici) */
    int order_count;
} gmb_tiny_cache_t;

void gmb_tiny_cache_init(gmb_tiny_cache_t* c, int capacity);
int  gmb_tiny_cache_capacity(const gmb_tiny_cache_t* c);
/* GetOrAdd: ritorna il valore, calcolandolo con factory se assente. */
int  gmb_tiny_cache_get_or_add(gmb_tiny_cache_t* c, int key, int (*factory)(int key, void* ctx), void* ctx);
bool gmb_tiny_cache_try_get(gmb_tiny_cache_t* c, int key, int* value);
bool gmb_tiny_cache_is_valid(gmb_tiny_cache_t* c, int key);
void gmb_tiny_cache_set(gmb_tiny_cache_t* c, int key, int value);
void gmb_tiny_cache_invalidate(gmb_tiny_cache_t* c, int key);
void gmb_tiny_cache_clear(gmb_tiny_cache_t* c);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_CACHE_H */
