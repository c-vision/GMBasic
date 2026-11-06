/* ============================================================================
 * gmb_tui_cache.c — TinyCache (porting TinyCache.cs).
 * ============================================================================ */
#include "gmb_tui_cache.h"
#include <string.h>

void gmb_tiny_cache_init(gmb_tiny_cache_t* c, int capacity)
{
    memset(c, 0, sizeof(*c));
    memset(c, 0, sizeof(*c));
    if (capacity > GMB_CACHE_MAX) capacity = GMB_CACHE_MAX;
    if (capacity < 0) capacity = 0;
    c->capacity = capacity;
}

int gmb_tiny_cache_capacity(const gmb_tiny_cache_t* c) { return c->capacity; }

static int cache_find(gmb_tiny_cache_t* c, int key)
{
    for (int i = 0; i < GMB_CACHE_MAX; i++)
        if (c->entries[i].used && c->entries[i].key == key) return i;
    return -1;
}

static void cache_evict(gmb_tiny_cache_t* c)
{
    if (c->order_count <= 0) return;
    int idx = c->order[0];
    c->entries[idx].used = false;
    for (int i = 0; i < c->order_count - 1; i++) c->order[i] = c->order[i + 1];
    c->order_count--;
}

static int cache_first_free(gmb_tiny_cache_t* c)
{
    for (int i = 0; i < GMB_CACHE_MAX; i++)
        if (!c->entries[i].used) return i;
    return -1;
}

void gmb_tiny_cache_set(gmb_tiny_cache_t* c, int key, int value)
{
    if (c->capacity <= 0) return;
    int idx = cache_find(c, key);
    if (idx < 0)
    {
        if (c->order_count >= c->capacity)
            cache_evict(c);
        idx = cache_first_free(c);
        if (idx < 0) return;
        c->entries[idx].key = key;
        c->entries[idx].used = true;
        c->order[c->order_count++] = idx;
    }
    c->entries[idx].value = value;
    /* rimuovi e aggiungi in coda (MRU) */
    for (int i = 0; i < c->order_count; i++)
        if (c->order[i] == idx)
        {
            for (int j = i; j < c->order_count - 1; j++) c->order[j] = c->order[j + 1];
            c->order_count--;
            break;
        }
    c->order[c->order_count++] = idx;
}

bool gmb_tiny_cache_try_get(gmb_tiny_cache_t* c, int key, int* value)
{
    int idx = cache_find(c, key);
    if (idx < 0) return false;
    if (value) *value = c->entries[idx].value;
    return true;
}

bool gmb_tiny_cache_is_valid(gmb_tiny_cache_t* c, int key)
{
    return cache_find(c, key) >= 0;
}

int gmb_tiny_cache_get_or_add(gmb_tiny_cache_t* c, int key, int (*factory)(int key, void* ctx), void* ctx)
{
    int existing;
    if (gmb_tiny_cache_try_get(c, key, &existing))
        return existing;
    int v = factory ? factory(key, ctx) : 0;
    gmb_tiny_cache_set(c, key, v);
    return v;
}

void gmb_tiny_cache_invalidate(gmb_tiny_cache_t* c, int key)
{
    int idx = cache_find(c, key);
    if (idx < 0) return;
    c->entries[idx].used = false;
    for (int i = 0; i < c->order_count; i++)
        if (c->order[i] == idx)
        {
            for (int j = i; j < c->order_count - 1; j++) c->order[j] = c->order[j + 1];
            c->order_count--;
            break;
        }
}

void gmb_tiny_cache_clear(gmb_tiny_cache_t* c)
{
    memset(c->entries, 0, sizeof(c->entries));
    c->order_count = 0;
}
