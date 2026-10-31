/* FILE: native/gmb_db_c/src/Databases/LruCache.c */
/* Port 1:1 di common/Databases/LruCache.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.LruCache.cs
//
//using System.Runtime.CompilerServices;
//
//// ReSharper disable InconsistentNaming
//
//namespace Basic;
//
///// <summary>
///// Implementa una cache di tipo "Least Recently Used" (LRU).
///// Quando la cache raggiunge la sua capacità massima, l'elemento che non è stato
///// utilizzato per più tempo viene automaticamente rimosso per fare spazio.
///// Le operazioni di accesso e inserimento hanno una complessità media di O(1).
///// </summary>
///// <typeparam name="TKey">Il tipo della chiave.</typeparam>
///// <typeparam name="TValue">Il tipo del valore.</typeparam>
//internal class LruCache<TKey, TValue> where TKey : notnull
//{
//    private readonly int _capacity;
//    private readonly Dictionary<TKey, LinkedListNode<CacheItem>> _cache;
//    private readonly LinkedList<CacheItem> _lruList;
//    private readonly HashSet<TKey> _pinned = new();
//
//    /// <summary>
//    /// Rappresenta un singolo elemento all'interno della cache.
//    /// </summary>
//    private class CacheItem(TKey key, TValue value)
//    {
//        public TKey Key { get; } = key;
//        public TValue Value { get; set; } = value;
//    }
//
//    public LruCache(int capacity)
//    {
//        if (capacity <= 0)
//            throw new ArgumentException("La capacità della cache deve essere maggiore di zero.", nameof(capacity));
//        
//        _capacity = capacity;
//        _cache = new Dictionary<TKey, LinkedListNode<CacheItem>>(capacity);
//        _lruList = new LinkedList<CacheItem>();
//    }
//
//    /// <summary>
//    /// Tenta di ottenere un valore dalla cache. Se l'elemento esiste,
//    /// viene contrassegnato come usato di recente.
//    /// </summary>
//    /// <param name="key">La chiave da cercare.</param>
//    /// <param name="value">Il valore associato alla chiave, se trovato.</param>
//    /// <returns>True se la chiave è presente nella cache, altrimenti False.</returns>
//    [MethodImpl(MethodImplOptions.Synchronized)]
//    public bool TryGetValue(TKey key, out TValue? value)
//    {
//        if (_cache.TryGetValue(key, out var node))
//        {
//            _lruList.Remove(node);
//            _lruList.AddFirst(node);
//            value = node.Value.Value;
//            return true;
//        }
//        value = default;
//        return false;
//    }
//
//    /// <summary>
//    /// Aggiunge o aggiorna un elemento nella cache.
//    /// Se l'elemento viene aggiunto e la cache è piena, l'elemento meno
//    /// usato di recente viene rimosso.
//    /// </summary>
//    /// <param name="key">La chiave dell'elemento.</param>
//    /// <param name="value">Il valore dell'elemento.</param>
//    [MethodImpl(MethodImplOptions.Synchronized)]
//    public void Pin(TKey key) => _pinned.Add(key);
//
//    [MethodImpl(MethodImplOptions.Synchronized)]
//    public void Unpin(TKey key) => _pinned.Remove(key);
//
//    [MethodImpl(MethodImplOptions.Synchronized)]
//    public void AddOrUpdate(TKey key, TValue value)
//    {
//        if (_cache.TryGetValue(key, out var existingNode))
//        {
//            existingNode.Value.Value = value;
//            _lruList.Remove(existingNode);
//            _lruList.AddFirst(existingNode);
//        }
//        else
//        {
//            if (_cache.Count >= _capacity)
//            {
//                var victim = _lruList.Last;
//                while (victim != null && _pinned.Contains(victim.Value.Key))
//                {
//                    victim = victim.Previous;
//                }
//
//                if (victim != null)
//                {
//                    _cache.Remove(victim.Value.Key);
//                    _lruList.Remove(victim);
//                }
//            }
//
//            var newItem = new CacheItem(key, value);
//            var newNode = new LinkedListNode<CacheItem>(newItem);
//            _cache[key] = newNode;
//            _lruList.AddFirst(newNode);
//        }
//    }
//
//    /// <summary>
//    /// Rimuove un elemento dalla cache.
//    /// </summary>
//    /// <param name="key">La chiave dell'elemento da rimuovere.</param>
//    [MethodImpl(MethodImplOptions.Synchronized)]
//    public void Remove(TKey key)
//    {
//        if (_cache.TryGetValue(key, out var node))
//        {
//            _lruList.Remove(node);
//            _cache.Remove(key);
//        }
//    }
//    
//    /// <summary>
//    /// Svuota la cache.
//    /// </summary>
//    [MethodImpl(MethodImplOptions.Synchronized)]
//    public void Clear()
//    {
//        _cache.Clear();
//        _lruList.Clear();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

typedef struct LruNode {
    uint64_t key;
    void* value;
    struct LruNode *prev, *next;
} LruNode;

struct LruCache {
    int capacity;
    int count;
    LruNode* head;
    LruNode* tail;
    LruNode** buckets;
    int bucketN;
    uint64_t* pinned;
    int pinnedN, pinnedCap;
    pthread_mutex_t mu;
};

/* hash buckets reserved for O(1) upgrade; list scan is the current ceiling. */
LruCache* LruCache_New(int capacity)
{
    LruCache* self;
    if (capacity <= 0) return NULL;
    self = (LruCache*)calloc(1, sizeof(*self));
    self->capacity = capacity;
    self->bucketN = capacity * 2 + 1;
    self->buckets = (LruNode**)calloc((size_t)self->bucketN, sizeof(LruNode*));
    pthread_mutex_init(&self->mu, NULL);
    return self;
}

void LruCache_Dispose(LruCache* self)
{
    LruNode* n;
    if (!self) return;
    n = self->head;
    while (n) { LruNode* nx = n->next; free(n); n = nx; }
    free(self->buckets);
    free(self->pinned);
    pthread_mutex_destroy(&self->mu);
    free(self);
}

static LruNode* LruCache_find(LruCache* self, uint64_t key)
{
    LruNode* n = self->head;
    while (n) { if (n->key == key) return n; n = n->next; }
    return NULL;
}

static int LruCache_is_pinned(LruCache* self, uint64_t key)
{
    int i;
    for (i = 0; i < self->pinnedN; i++) if (self->pinned[i] == key) return 1;
    return 0;
}

int LruCache_TryGetValue(LruCache* self, uint64_t key, void** value)
{
    LruNode* n;
    pthread_mutex_lock(&self->mu);
    n = LruCache_find(self, key);
    if (!n) { pthread_mutex_unlock(&self->mu); return 0; }
    if (n != self->head) {
        if (n->prev) n->prev->next = n->next;
        if (n->next) n->next->prev = n->prev;
        if (self->tail == n) self->tail = n->prev;
        n->prev = NULL;
        n->next = self->head;
        if (self->head) self->head->prev = n;
        self->head = n;
    }
    *value = n->value;
    pthread_mutex_unlock(&self->mu);
    return 1;
}

void LruCache_Pin(LruCache* self, uint64_t key)
{
    pthread_mutex_lock(&self->mu);
    if (!LruCache_is_pinned(self, key)) {
        if (self->pinnedN >= self->pinnedCap) {
            self->pinnedCap = self->pinnedCap ? self->pinnedCap * 2 : 4;
            self->pinned = (uint64_t*)realloc(self->pinned, (size_t)self->pinnedCap * sizeof(uint64_t));
        }
        self->pinned[self->pinnedN++] = key;
    }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_Unpin(LruCache* self, uint64_t key)
{
    int i;
    pthread_mutex_lock(&self->mu);
    for (i = 0; i < self->pinnedN; i++)
        if (self->pinned[i] == key) {
            memmove(self->pinned + i, self->pinned + i + 1, (size_t)(self->pinnedN - i - 1) * sizeof(uint64_t));
            self->pinnedN--;
            break;
        }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_AddOrUpdate(LruCache* self, uint64_t key, void* value)
{
    LruNode* existing;
    pthread_mutex_lock(&self->mu);
    existing = LruCache_find(self, key);
    if (existing) {
        existing->value = value;
        if (existing != self->head) {
            if (existing->prev) existing->prev->next = existing->next;
            if (existing->next) existing->next->prev = existing->prev;
            if (self->tail == existing) self->tail = existing->prev;
            existing->prev = NULL;
            existing->next = self->head;
            if (self->head) self->head->prev = existing;
            self->head = existing;
        }
    } else {
        LruNode* node;
        if (self->count >= self->capacity) {
            LruNode* victim = self->tail;
            while (victim && LruCache_is_pinned(self, victim->key)) victim = victim->prev;
            if (victim) {
                if (victim->prev) victim->prev->next = victim->next;
                if (victim->next) victim->next->prev = victim->prev;
                if (self->head == victim) self->head = victim->next;
                if (self->tail == victim) self->tail = victim->prev;
                free(victim);
                self->count--;
            }
        }
        node = (LruNode*)calloc(1, sizeof(*node));
        node->key = key;
        node->value = value;
        node->next = self->head;
        if (self->head) self->head->prev = node;
        self->head = node;
        if (!self->tail) self->tail = node;
        self->count++;
    }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_Remove(LruCache* self, uint64_t key)
{
    LruNode* n;
    pthread_mutex_lock(&self->mu);
    n = LruCache_find(self, key);
    if (n) {
        if (n->prev) n->prev->next = n->next;
        if (n->next) n->next->prev = n->prev;
        if (self->head == n) self->head = n->next;
        if (self->tail == n) self->tail = n->prev;
        free(n);
        self->count--;
    }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_Clear(LruCache* self)
{
    LruNode* n;
    pthread_mutex_lock(&self->mu);
    n = self->head;
    while (n) { LruNode* nx = n->next; free(n); n = nx; }
    self->head = self->tail = NULL;
    self->count = 0;
    pthread_mutex_unlock(&self->mu);
}
