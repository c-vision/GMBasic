/* FILE: native/gmb_db_c/src/ISAM/IsamLruCache.c */
/* Port 1:1 di modules/Database/ISAM/IsamLruCache.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamLruCache
//
//namespace Basic.ISAM;
//
///// <summary>
///// LRU condivisa ISAM (ISAM-C1): cap 128 entry, intra-process only.
///// ponytail: pin su radice/cursore attivo; no ref-count globale (upgrade: SQLite pcache).
///// </summary>
//internal sealed class IsamLruCache<TKey, TValue> where TKey : notnull
//{
//    internal const int DefaultCapacity = 128;
//
//    private readonly LruCache<TKey, TValue> _inner = new(DefaultCapacity);
//
//    public bool TryGetValue(TKey key, out TValue? value) => _inner.TryGetValue(key, out value);
//
//    public void AddOrUpdate(TKey key, TValue value) => _inner.AddOrUpdate(key, value);
//
//    public void Pin(TKey key) => _inner.Pin(key);
//
//    public void Unpin(TKey key) => _inner.Unpin(key);
//
//    public void Remove(TKey key) => _inner.Remove(key);
//
//    public void Clear() => _inner.Clear();
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <pthread.h>
#include <time.h>
struct IsamLruCache { LruCache* inner; };

IsamLruCache* IsamLruCache_New(void)
{
    IsamLruCache* self = (IsamLruCache*)calloc(1, sizeof(*self));
    self->inner = LruCache_New(IsamLruCache_DefaultCapacity);
    return self;
}
void IsamLruCache_Dispose(IsamLruCache* self) { if (!self) return; LruCache_Dispose(self->inner); free(self); }
int IsamLruCache_TryGetValue(IsamLruCache* self, uint64_t key, void** value) { return LruCache_TryGetValue(self->inner, key, value); }
void IsamLruCache_AddOrUpdate(IsamLruCache* self, uint64_t key, void* value) { LruCache_AddOrUpdate(self->inner, key, value); }
void IsamLruCache_Pin(IsamLruCache* self, uint64_t key) { LruCache_Pin(self->inner, key); }
void IsamLruCache_Unpin(IsamLruCache* self, uint64_t key) { LruCache_Unpin(self->inner, key); }
void IsamLruCache_Remove(IsamLruCache* self, uint64_t key) { LruCache_Remove(self->inner, key); }
void IsamLruCache_Clear(IsamLruCache* self) { LruCache_Clear(self->inner); }
