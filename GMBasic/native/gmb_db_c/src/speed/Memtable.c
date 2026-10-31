/* FILE: native/gmb_db_c/src/SPEED/Memtable.c */
/* Port 1:1 di modules/Database/SPEED/Memtable.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Memtable.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Collections;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Comparer per array di byte, essenziale per le strutture dati ordinate come SortedList.
///// </summary>
//internal sealed class ByteArrayComparer : IComparer<byte[]>
//{
//    public int Compare(byte[]? x, byte[]? y)
//    {
//        if (ReferenceEquals(x, y)) return 0;
//        if (x is null) return -1;
//        if (y is null) return 1;
//
//        var minLength = Math.Min(x.Length, y.Length);
//        for (var i = 0; i < minLength; i++)
//        {
//            var cmp = x[i].CompareTo(y[i]);
//            if (cmp != 0) return cmp;
//        }
//
//        return x.Length.CompareTo(y.Length);
//    }
//}
//
///// <summary>
///// Rappresenta la tabella in memoria (Memtable) in un'architettura LSM-Tree.
///// Accetta scritture e le mantiene ordinate in memoria prima che vengano scritte su disco (flush).
///// </summary>
//internal class Memtable : IEnumerable<KeyValuePair<byte[], byte[]?>>
//{
//    /// <summary>
//    /// Struttura dati sottostante che memorizza le coppie chiave-valore in ordine.
//    /// Un valore nullo (tombstone) indica una cancellazione.
//    /// </summary>
//    private readonly SortedList<byte[], byte[]?> _table;
//
//    /// <summary>
//    /// Dimensione stimata in byte dei dati contenuti nella Memtable.
//    /// </summary>
//    public long EstimatedSize { get; private set; }
//
//    /// <summary>
//    /// Numero di chiavi presenti nella Memtable (incluse tombstone).
//    /// </summary>
//    internal int KeyCount => _table.Count;
//
//    public Memtable()
//    {
//        _table = new SortedList<byte[], byte[]?>(new ByteArrayComparer());
//        EstimatedSize = 0;
//    }
//
//    /// <summary>
//    /// Inserisce o aggiorna una coppia chiave-valore.
//    /// </summary>
//    /// <param name="key">La chiave del dato.</param>
//    /// <param name="value">Il valore del dato.</param>
//    public void Set(byte[] key, byte[] value)
//    {
//        ArgumentNullException.ThrowIfNull(key);
//        ArgumentNullException.ThrowIfNull(value);
//
//        if (_table.TryGetValue(key, out var oldValue))
//        {
//            EstimatedSize -= key.Length + (oldValue?.Length ?? 0);
//        }
//
//        _table[key] = value;
//        EstimatedSize += key.Length + value.Length;
//    }
//
//    /// <summary>
//    /// Marca una chiave come cancellata inserendo una "tombstone" (valore nullo).
//    /// </summary>
//    /// <param name="key">La chiave da cancellare.</param>
//    public void Delete(byte[] key)
//    {
//        ArgumentNullException.ThrowIfNull(key);
//        
//        if (_table.TryGetValue(key, out var oldValue))
//        {
//            EstimatedSize -= key.Length + (oldValue?.Length ?? 0);
//        }
//
//        _table[key] = null; // Tombstone
//        EstimatedSize += key.Length;
//    }
//
//    /// <summary>
//    /// Recupera un valore associato a una chiave.
//    /// </summary>
//    /// <param name="key">La chiave da cercare.</param>
//    /// <param name="value">Il valore trovato, o null se la chiave non esiste o è stata cancellata.</param>
//    /// <returns>True se la chiave esiste nella Memtable (anche se cancellata), altrimenti False.</returns>
//    public bool Get(byte[] key, out byte[]? value)
//    {
//        return _table.TryGetValue(key, out value);
//    }
//
//    /// <summary>
//    /// Restituisce un enumeratore per iterare sulla collezione ordinata.
//    /// </summary>
//    public IEnumerator<KeyValuePair<byte[], byte[]?>> GetEnumerator()
//    {
//        return _table.GetEnumerator();
//    }
//
//    IEnumerator IEnumerable.GetEnumerator()
//    {
//        return GetEnumerator();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

/* SKIPLIST INDICIZZABILE (con span) al posto dell'array ordinato: insert/find/Nth O(log n).
 * L'array ordinato aveva insert O(n) (memmove) -> bulk load O(n^2) per chiavi fuori ordine.
 * `links[i].span` = numero di nodi saltati al livello i (per il rank di Memtable_Nth).
 * Vedi bug SPEED-Memtable-On-e-Durability-Non-Cablata causa #1. */
#define MEM_SKIP_MAXLEVEL 24

typedef struct MemLink { struct MemNode* next; int span; } MemLink;
typedef struct MemNode {
    uint8_t* key; int keyLen;
    uint8_t* value; int valueLen;
    int tombstone;
    MemLink links[];   /* un livello per indice 0..level */
} MemNode;

struct Memtable {
    MemNode* head;     /* sentinella (key NULL) */
    int level;         /* livello massimo in uso */
    int count;         /* numero di chiavi (incluse tombstone) */
    int64_t EstimatedSize;
};

/* PRNG xorshift (nessun rand() di stato globale: la memtable può girare su thread diversi). */
static uint32_t mem_rand_state = 0x2545F491u;
static uint32_t mem_rand(void)
{
    uint32_t x = mem_rand_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    mem_rand_state = x;
    return x;
}
static int mem_random_level(void)
{
    int lvl = 0;
    while (lvl < MEM_SKIP_MAXLEVEL && (mem_rand() & 3u) == 0) lvl++;   /* p = 1/4 */
    return lvl;
}

static MemNode* mem_node_new(int level, const uint8_t* key, int keyLen,
                             const uint8_t* value, int valueLen, int tombstone)
{
    MemNode* n = (MemNode*)calloc(1, sizeof(MemNode) + (size_t)(level + 1) * sizeof(MemLink));
    if (!n) return NULL;
    n->key = (uint8_t*)malloc((size_t)(keyLen > 0 ? keyLen : 1));
    if (n->key && keyLen > 0) memcpy(n->key, key, (size_t)keyLen);
    n->keyLen = keyLen;
    if (value && valueLen > 0) {
        n->value = (uint8_t*)malloc((size_t)valueLen);
        if (n->value) memcpy(n->value, value, (size_t)valueLen);
    }
    n->valueLen = value ? valueLen : 0;
    n->tombstone = tombstone;
    return n;
}

static void mem_node_free(MemNode* n)
{
    if (!n) return;
    free(n->key);
    free(n->value);
    free(n);
}

/* Ricerca: ritorna il nodo con la chiave o NULL; se `update`!=NULL riempie l'ultimo nodo con
 * chiave < target per ogni livello e in `rank[i]` il numero di nodi prima di update[i]. */
static MemNode* mem_find(const Memtable* self, const uint8_t* key, int keyLen,
                         MemNode** update, int* rank)
{
    MemNode* x = self->head;
    int r[MEM_SKIP_MAXLEVEL + 1];
    for (int i = self->level; i >= 0; i--) {
        r[i] = (i == self->level) ? 0 : r[i + 1];
        while (x->links[i].next &&
               ByteArrayComparer_Compare(x->links[i].next->key, x->links[i].next->keyLen, key, keyLen) < 0) {
            r[i] += x->links[i].span;
            x = x->links[i].next;
        }
        if (update) update[i] = x;
        if (rank) rank[i] = r[i];
    }
    {
        MemNode* cand = x->links[0].next;
        if (cand && ByteArrayComparer_Compare(cand->key, cand->keyLen, key, keyLen) == 0) return cand;
    }
    return NULL;
}

/* Inserisce un nodo NUOVO (update/rank già calcolati da mem_find con esito NULL). */
static void mem_insert_at(Memtable* self, MemNode** update, int* rank,
                          const uint8_t* key, int keyLen, const uint8_t* value, int valueLen, int tombstone)
{
    int lvl = mem_random_level();
    MemNode* n;
    if (lvl > self->level) {
        for (int i = self->level + 1; i <= lvl; i++) { rank[i] = 0; update[i] = self->head; update[i]->links[i].span = self->count; }
        self->level = lvl;
    }
    n = mem_node_new(lvl, key, keyLen, value, valueLen, tombstone);
    if (!n) return;
    for (int i = 0; i <= lvl; i++) {
        n->links[i].next = update[i]->links[i].next;
        update[i]->links[i].next = n;
        n->links[i].span = update[i]->links[i].span - (rank[0] - rank[i]);
        update[i]->links[i].span = (rank[0] - rank[i]) + 1;
    }
    for (int i = lvl + 1; i <= self->level; i++) update[i]->links[i].span++;
    self->count++;
}

Memtable* Memtable_New(void)
{
    Memtable* m = (Memtable*)calloc(1, sizeof(Memtable));
    if (!m) return NULL;
    m->head = (MemNode*)calloc(1, sizeof(MemNode) + (size_t)(MEM_SKIP_MAXLEVEL + 1) * sizeof(MemLink));
    if (!m->head) { free(m); return NULL; }
    return m;
}

void Memtable_Dispose(Memtable* self)
{
    MemNode* n;
    if (!self) return;
    n = self->head->links[0].next;
    while (n) { MemNode* nx = n->links[0].next; mem_node_free(n); n = nx; }
    free(self->head);
    free(self);
}

int Memtable_Set(Memtable* self, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen)
{
    MemNode* update[MEM_SKIP_MAXLEVEL + 1];
    int rank[MEM_SKIP_MAXLEVEL + 1];
    MemNode* found;
    if (!key || !value) return GMB_DB_ARG;
    found = mem_find(self, key, keyLen, update, rank);
    if (found) {
        self->EstimatedSize -= found->keyLen + (found->tombstone ? 0 : found->valueLen);
        free(found->value);
        found->value = (uint8_t*)malloc((size_t)valueLen);
        if (found->value) memcpy(found->value, value, (size_t)valueLen);
        found->valueLen = valueLen;
        found->tombstone = 0;
        self->EstimatedSize += keyLen + valueLen;
        return GMB_DB_OK;
    }
    mem_insert_at(self, update, rank, key, keyLen, value, valueLen, 0);
    self->EstimatedSize += keyLen + valueLen;
    return GMB_DB_OK;
}

int Memtable_Delete(Memtable* self, const uint8_t* key, int keyLen)
{
    MemNode* update[MEM_SKIP_MAXLEVEL + 1];
    int rank[MEM_SKIP_MAXLEVEL + 1];
    MemNode* found;
    if (!key) return GMB_DB_ARG;
    found = mem_find(self, key, keyLen, update, rank);
    if (found) {
        self->EstimatedSize -= found->keyLen + (found->tombstone ? 0 : found->valueLen);
        free(found->value);
        found->value = NULL;
        found->valueLen = 0;
        found->tombstone = 1;
        self->EstimatedSize += keyLen;
        return GMB_DB_OK;
    }
    mem_insert_at(self, update, rank, key, keyLen, NULL, 0, 1);
    self->EstimatedSize += keyLen;
    return GMB_DB_OK;
}

int Memtable_Get(Memtable* self, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen, int* isTombstone)
{
    MemNode* n = mem_find(self, key, keyLen, NULL, NULL);
    if (!n) return 0;
    *isTombstone = n->tombstone;
    *valueLen = n->valueLen;
    *value = n->value;
    return 1;
}

int64_t Memtable_get_EstimatedSize(const Memtable* self) { return self->EstimatedSize; }
int Memtable_get_KeyCount(const Memtable* self) { return self->count; }

int Memtable_Nth(const Memtable* self, int index, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen, int* isTombstone)
{
    /* Rank walk O(log n) con gli span: rank 1-based (rank 1 = primo nodo). */
    int rank = index + 1;
    int traversed = 0;
    MemNode* x = self->head;
    if (index < 0 || index >= self->count) return GMB_DB_ARG;
    for (int i = self->level; i >= 0; i--) {
        while (x->links[i].next && traversed + x->links[i].span <= rank) {
            traversed += x->links[i].span;
            x = x->links[i].next;
        }
        if (traversed == rank) break;
    }
    *key = x->key;
    *keyLen = x->keyLen;
    *value = x->value;
    *valueLen = x->valueLen;
    *isTombstone = x->tombstone;
    return GMB_DB_OK;
}
