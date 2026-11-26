/* ============================================================================
 * gmb_collections.c — Collezioni della CommonLib C# portate in C.
 * Registry di handle → collezione. Ogni collezione ha un typeCode BASIC:
 *   9=RedBlack, 10=BITree, 11=Trie, 12=LinkedList, 13=Circular, 14=BinaryHeap,
 *   15=Bag, 16=Cache, 18=Timeline, 19=HashRing, 20=RingBuffer, 21=SparseSet, 26=BPlusTree
 * Gli elementi sono gmb_val_t (tipo unificato condiviso con il transpiler).
 * ============================================================================ */
#include "gmb_collections.h"
#include "gmb_hll.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
typedef struct { volatile LONG locked; } gmb_coll_lock_t;
static void gmb_coll_lock(gmb_coll_lock_t* m) { while (_InterlockedExchange(&m->locked, 1)) { ; } }
static void gmb_coll_unlock(gmb_coll_lock_t* m) { _InterlockedExchange(&m->locked, 0); }
#else
#include <pthread.h>
typedef struct { volatile int locked; } gmb_coll_lock_t;
static void gmb_coll_lock(gmb_coll_lock_t* m) { while (__sync_lock_test_and_set(&m->locked, 1)) { ; } }
static void gmb_coll_unlock(gmb_coll_lock_t* m) { __sync_lock_release(&m->locked); }
#endif

/* ---- Nome canonico / lettera Clipper di una collezione dal typeCode ----
 * Fonte UNICA (VM/C e programmi generati): parità IlRuntime.TypeName/TypeInfo e
 * ClipperStrings.ValType (il golden C99 usa gli stessi nomi). I nomi sono quelli CLR
 * osservati su IL/golden, non quelli BASIC: es. SortedDictionary/Tree/AVL/RedBlack/
 * BITree/BPlusTree → "SORTEDDICTIONARY`2"; un HashSet NON implementa ICollection
 * non-generico → VALTYPE "O". */
const char* gmb_collection_type_name(int type_code)
{
    switch (type_code)
    {
        case 1:  return "LIST`1";
        case 2:  return "DICTIONARY`2";
        case 3:  return "HASHSET`1";
        case 4:  return "QUEUE`1";
        case 5:  return "STACK`1";
        case 6:  case 7: case 8: case 9: case 10: case 26:
                 return "SORTEDDICTIONARY`2";
        case 11: return "TRIE";
        case 12: return "LINKEDLIST`1";
        case 13: return "LIST`1";
        case 14: case 27: return "LIST`1";
        case 15: return "DICTIONARY`2";
        case 16: return "CACHE`2";
        case 17: return "GRAPH`1";
        case 18: return "TIMELINE`1";
        case 19: return "CONSISTENTHASH`1";
        case 20: return "CIRCULARQUEUE`1";
        case 21: return "SPARSESET";
        case 30: return "HLL";
        default: return "OBJECT";
    }
}

const char* gmb_collection_valtype_letter(int type_code)
{
    switch (type_code)
    {
        /* Non implementano `System.Collections.ICollection` (non-generico) → "O". */
        case 3: case 11: case 16: case 17: case 18: case 19: case 20: case 21:
            return "O";
        case 1: case 2: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
        case 12: case 13: case 14: case 15: case 26: case 27:
            return "A";
        default: return "O";
    }
}

/* ---- OWNERSHIP dei valori ----
 * Le collezioni POSSIEDONO i gmb_val_t OBJECT contati (release_fn != NULL: oggetti heap della VM)
 * che contengono — chiave e valore dei nodi, slot vivi del ring, valore delle entry della cache:
 * retain all'inserimento, release quando l'elemento viene scartato (remove, clear, overwrite, free).
 * Gli accessori che FANNO USCIRE un elemento (dequeue, remove_last, extract_min, heap_extract)
 * TRASFERISCONO il riferimento al chiamante e non rilasciano (al peggio un leak, mai un
 * use-after-free). Valori in prestito (release_fn NULL) e non-OBJECT: mai toccati.
 * Helper condivisi: gmb_val_retain / gmb_val_release (gmb_core.c). */

/* ---- Rappresentazione: lista di gmb_val_t (per la maggior parte) ---- */
typedef struct gmb_coll_item { gmb_val_t key; gmb_val_t v; struct gmb_coll_item* next; } gmb_coll_item_t;

typedef struct gmb_collection {
    int type_code;
    /* lista semplice per: Bag(15), LinkedList(12), Doubly(13 impl come linked),
       Circular(13), Heap(14), SparseSet(21) */
    gmb_coll_item_t* head;
    gmb_coll_item_t* tail;
    int64_t count;
    int64_t capacity;       /* RingBuffer(20): default 8; SparseSet(21) */
    /* RingBuffer: elementi ciclici */
    gmb_val_t* ring;        /* quando type_code==20 */
    int64_t ring_head, ring_tail, ring_len;
    /* BITree(10): array fenwick */
    double* bit;
    int64_t bit_n;
    /* Trie(11): struttura annidata semplice */
    struct gmb_trie_node* trie;
    /* Cache(16): lista di coppie chiave-valore */
    struct gmb_coll_cache_entry* cache;
    /* HLL(28): sketch HyperLogLog (runtime condiviso, gmb_hll.c). */
    gmb_hll_t* hll;
} gmb_collection_t;

/* Trie node */
typedef struct gmb_trie_node {
    char c;
    int is_word;
    struct gmb_trie_node* children[26]; /* solo minuscole a-z semplificato */
    struct gmb_trie_node* next_sibling;
} gmb_trie_node_t;

/* Cache entry */
typedef struct gmb_coll_cache_entry {
    char* key;
    gmb_val_t value;
    double expires; /* tempo di scadenza assoluto (epoch secondi); 0 = mai scadere (CommonLib) */
    struct gmb_coll_cache_entry* next;
} gmb_coll_cache_entry_t;

/* Inserisce una parola nella struttura trie SENZA toccare c->count (il conteggio di un
 * Trie resta quello della lista companion: count/FOR EACH/Contains non cambiano). */
static void trie_mirror_word(gmb_collection_t* c, const char* s);

/* ---- Registry ---- */
#define GMB_COLL_MAX 512
static gmb_collection_t* g_registry[GMB_COLL_MAX];
static int g_registry_count = 0;
static gmb_coll_lock_t g_lock;
static int g_ready = 0;

void gmb_collections_init(void)
{
    if (g_ready) return;
    g_lock.locked = 0;
    for (int i = 0; i < GMB_COLL_MAX; i++) g_registry[i] = NULL;
    g_registry_count = 0;
    g_ready = 1;
}

void gmb_collections_shutdown(void)
{
    if (!g_ready) return;
    for (int i = 0; i < GMB_COLL_MAX; i++)
        if (g_registry[i]) { gmb_collections_free(i + 1); g_registry[i] = NULL; }
    g_registry_count = 0;
    g_ready = 0;
}

int64_t gmb_collections_create(int type_code)
{
    if (!g_ready) gmb_collections_init();
    gmb_collection_t* c = (gmb_collection_t*)calloc(1, sizeof(gmb_collection_t));
    if (!c) return -1;
    c->type_code = type_code;
    if (type_code == 20) /* RingBuffer */
    {
        /* Default 64: parità col default C# CircularQueue (P-Code/IL) — prima 8 → divergenza
         * su CollectionsGraphMissing (RINGBUFFER.Capacity: PCode=64 vs VM-C/C99=8). */
        c->capacity = 64;
        c->ring = (gmb_val_t*)calloc(64, sizeof(gmb_val_t));
        if (!c->ring) { free(c); return -1; }
    }
    else if (type_code == 10) /* BITree */
    {
        c->bit_n = 16;
        c->bit = (double*)calloc(17, sizeof(double));
        if (!c->bit) { free(c); return -1; }
    }
    else if (type_code == 30) /* HLL (HyperLogLog) */
    {
        c->hll = gmb_hll_new(GMB_HLL_DEFAULT_PRECISION);
        if (!c->hll) { free(c); return -1; }
    }
    gmb_coll_lock(&g_lock);
    int slot = -1;
    for (int i = 0; i < GMB_COLL_MAX; i++)
        if (g_registry[i] == NULL) { slot = i; break; }
    if (slot < 0) { gmb_coll_unlock(&g_lock); free(c); return -1; }
    g_registry[slot] = c;
    if (slot >= g_registry_count) g_registry_count = slot + 1;
    gmb_coll_unlock(&g_lock);
    return slot + 1;
}

static gmb_collection_t* coll_get(int64_t h)
{
    if (h <= 0 || h > GMB_COLL_MAX) return NULL;
    return g_registry[(int)(h - 1)];
}

/* ---- Costruttori: applica gli argomenti del costruttore BASIC ---- */
void gmb_coll_apply_ctor(int64_t h, gmb_val_t* args, int n_args)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !args) return;
    if (c->type_code == 30) /* HLL(p): precisione opzionale (default 14) */
    {
        int p = GMB_HLL_DEFAULT_PRECISION;
        if (n_args >= 1)
        {
            double d = (args[0].kind == GMB_TYPE_INT) ? (double)args[0].v_int
                     : (args[0].kind == GMB_TYPE_DOUBLE) ? args[0].v_double : 0.0;
            if (d > 0) p = (int)d;
        }
        if (c->hll) gmb_hll_free(c->hll);
        c->hll = gmb_hll_new(p);
        return;
    }
    if (c->type_code == 19 && n_args >= 1) /* HashRing: nodo singolo (stringa) o LISTA di nodi */
    {
        gmb_val_t n = args[0];
        if (n.kind == GMB_TYPE_STRING && n.v_string.data)
            gmb_coll_hashring_add_node(h, n.v_string.data);
        else
        {
            /* `NEW HASHRING(servers)` col vettore dei nodi (emitter C#: una sola arg lista). */
            gmb_list_t l = gmb_val_as_list(n);
            gmb_int_t ln = gmb_list_count(l);
            for (gmb_int_t i = 0; i < ln; i++)
            {
                gmb_val_t e = gmb_list_get(l, i);
                if (e.kind == GMB_TYPE_STRING && e.v_string.data)
                    gmb_coll_hashring_add_node(h, e.v_string.data);
            }
        }
    }
    else if (c->type_code == 20 && n_args >= 1) /* RingBuffer: capacità */
    {
        int64_t cap = 0;
        if (args[0].kind == GMB_TYPE_DOUBLE) cap = (int64_t)args[0].v_double;
        else if (args[0].kind == GMB_TYPE_INT) cap = (int64_t)args[0].v_int;
        if (cap <= 0) cap = 8;
        if (c->ring) free(c->ring);
        c->capacity = cap;
        c->ring = (gmb_val_t*)calloc((size_t)cap, sizeof(gmb_val_t));
        if (c->ring) { c->ring_head = c->ring_tail = c->ring_len = 0; }
    }
    else if (c->type_code == 21 && n_args >= 2) /* SparseSet: (maxValue, capacity) */
    {
        if (c->capacity <= 0) c->capacity = 8;
    }
    else if (c->type_code == 15) /* Bag: argomenti costruttore = elementi da aggiungere
                                    (bag-literal `= ["a","b"]` del transpiler C99, C99-01) */
    {
        for (int i = 0; i < n_args; i++) gmb_coll_add(h, args[i]);
    }
    else if (c->type_code == 14 || c->type_code == 27) /* Heap(14)/MaxHeap(27): gli argomenti del
                                    letterale `= [4,1,7,3]` sono ELEMENTI. Senza questo caso erano
                                    scartati in silenzio e la catena conteneva solo i successivi
                                    Add → MinHeap.Peek() dava 2 invece di 1. Min/max è calcolato su
                                    tutta la catena (heap_root_val), quindi non serve heapificare. */
    {
        for (int i = 0; i < n_args; i++) gmb_coll_add_last(h, args[i]);
    }
    else if (c->type_code == 11) /* Trie: ogni argomento stringa è una parola del letterale
                                    `= ["car","cat"]`: si costruisce la struttura (StartsWith/
                                    FindAll) E si tiene la lista companion (count/FOR EACH). */
    {
        for (int i = 0; i < n_args; i++)
        {
            if (args[i].kind != GMB_TYPE_STRING || !args[i].v_string.data) continue;
            trie_mirror_word(c, args[i].v_string.data);
            gmb_coll_add_last(h, args[i]);
        }
    }
    else if (c->type_code == 10 && n_args >= 1) /* BITree: valori iniziali in una lista
                                    (`NEW BITREE<T>([1,2,3,4,5])` → array fenwick 1-based). */
    {
        gmb_list_t l = gmb_val_as_list(args[0]);
        int n = (int)gmb_list_count(l);
        if (n > 0)
        {
            if (c->bit) free(c->bit);
            c->bit_n = n;
            if (c->capacity < n + 1) c->capacity = n + 1;
            c->bit = (double*)calloc((size_t)n + 1, sizeof(double));
            if (c->bit)
            {
                for (int i = 1; i <= n; i++)
                {
                    gmb_val_t e = gmb_list_get(l, i - 1);
                    double v = e.kind == GMB_TYPE_DOUBLE ? e.v_double
                             : e.kind == GMB_TYPE_INT ? (double)e.v_int : 0;
                    c->bit[i] += v;
                    int k = i + (i & -i);
                    if (k <= n) c->bit[k] += c->bit[i];
                }
            }
        }
    }
    /* 26 BPlusTree(order): lista semplice, l'ordine non cambia la semantica osservabile */
}

static void coll_item_free(gmb_coll_item_t* it)
{
    while (it)
    {
        gmb_coll_item_t* nx = it->next;
        gmb_val_release(it->key);
        gmb_val_release(it->v);
        free(it);
        it = nx;
    }
}

void gmb_collections_free(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return;
    coll_item_free(c->head);
    if (c->ring)
    {
        for (int64_t i = 0; i < c->ring_len; i++) gmb_val_release(c->ring[(c->ring_head + i) % c->capacity]);
        free(c->ring);
    }
    if (c->bit) free(c->bit);
    if (c->trie)
    {
        /* libera ricorsivamente (semplice: stack) */
        gmb_trie_node_t* stack[1024];
        int sp = 0;
        stack[sp++] = c->trie;
        while (sp > 0)
        {
            gmb_trie_node_t* n = stack[--sp];
            for (int i = 0; i < 26; i++)
                if (n->children[i] && sp < 1024) stack[sp++] = n->children[i];
            free(n);
        }
    }
    gmb_coll_cache_entry_t* ce = c->cache;
    while (ce) { gmb_coll_cache_entry_t* nx = ce->next; free(ce->key); gmb_val_release(ce->value); free(ce); ce = nx; }
    if (c->hll) gmb_hll_free(c->hll);
    free(c);
    g_registry[(int)(h - 1)] = NULL;
}

/* ---- helper item push ---- */
static gmb_coll_item_t* item_new(gmb_val_t v)
{
    gmb_coll_item_t* it = (gmb_coll_item_t*)calloc(1, sizeof(gmb_coll_item_t));
    if (!it) return NULL;
    it->v = v;
    gmb_val_retain(v);
    return it;
}

/* ---- Add (testa) / AddLast (coda) ---- */
int gmb_coll_add_first(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return -1;
    gmb_coll_item_t* it = item_new(v);
    if (!it) return -1;
    it->next = c->head;
    c->head = it;
    if (!c->tail) c->tail = it;
    c->count++;
    return 0;
}

int gmb_coll_add_last(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return -1;
    gmb_coll_item_t* it = item_new(v);
    if (!it) return -1;
    if (c->tail) c->tail->next = it;
    else c->head = it;
    c->tail = it;
    c->count++;
    return 0;
}

int gmb_coll_add(int64_t h, gmb_val_t v)
{
    /* Bag: conta duplicati (count++). LinkedList: AddLast. Heap: push. SparseSet: Add. */
    gmb_collection_t* c = coll_get(h);
    if (!c) return -1;
    /* HLL(28): Add = inserisci l'hash nel registro (NON memorizza l'elemento). */
    if (c->type_code == 30 && c->hll) { gmb_hll_add_val(c->hll, v); return 0; }
    if (c->type_code == 20) return gmb_coll_enqueue(h, v);
    /* STACK (5): push in TESTA (LIFO) — il letterale [a,b,c] ha 'a' in cima e l'enumerazione
     * head→tail dà [c,b,a] (parity IL/PCode). Prima: add_last → ordine di inserimento
     * (ArraysPrintFormat.bas: st = [top,mid,bot] invece di [bot,mid,top]). */
    if (c->type_code == 5) return gmb_coll_add_first(h, v);
    /* HASHSET/SET (3) e SPARSESET (21): semantica di UNICITÀ — salta i duplicati (parity IL
     *). Prima: add_last incondizionato → il letterale DIM hs AS HASHSET =
     * [3,1,4,1,5] restava [3,1,4,1,5] (ArraysPrintFormat.bas). */
    if (c->type_code == 3 || c->type_code == 21)
        return gmb_coll_contains(h, v) ? 0 : gmb_coll_add_last(h, v);
    /* TRIE (11): la parola va anche nella struttura trie (StartsWith/FindAll), oltre che nella
     * lista companion che resta l'autorità di count/FOR EACH/Contains. DEDUP: una parola già
     * presente non entra due volte (parità CommonLib.Trie/IL). */
    if (c->type_code == 11 && v.kind == GMB_TYPE_STRING && v.v_string.data)
    {
        if (gmb_coll_trie_contains(h, v.v_string.data)) return 0;
        trie_mirror_word(c, v.v_string.data);
    }
    return gmb_coll_add_last(h, v);
}

/* ---- Contains / Remove (per valore) ---- */
static int val_equal(gmb_val_t a, gmb_val_t b)
{
    if (a.kind != b.kind) return 0;
    if (a.kind == GMB_TYPE_DOUBLE) return a.v_double == b.v_double;
    if (a.kind == GMB_TYPE_INT) return a.v_int == b.v_int;
    if (a.kind == GMB_TYPE_STRING && a.v_string.data && b.v_string.data)
        return strcmp(a.v_string.data, b.v_string.data) == 0;
    return 0;
}

int gmb_coll_contains(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
        if (val_equal(it->v, v)) return -1;
    return 0;
}

int gmb_coll_remove(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    gmb_coll_item_t* prev = NULL;
    gmb_coll_item_t* it = c->head;
    while (it)
    {
        if (val_equal(it->v, v))
        {
            if (prev) prev->next = it->next;
            else c->head = it->next;
            if (c->tail == it) c->tail = prev;
            gmb_val_release(it->key);
            gmb_val_release(it->v);
            free(it);
            c->count--;
            return -1;
        }
        prev = it;
        it = it->next;
    }
    return 0;
}

void gmb_coll_clear(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return;
    if (c->type_code == 30 && c->hll) gmb_hll_clear(c->hll);
    coll_item_free(c->head);
    c->head = c->tail = NULL;
    c->count = 0;
    if (c->ring)
    {
        for (int64_t i = 0; i < c->ring_len; i++) gmb_val_release(c->ring[(c->ring_head + i) % c->capacity]);
        memset(c->ring, 0, 8 * sizeof(gmb_val_t)); c->ring_head = c->ring_tail = c->ring_len = 0;
    }
    if (c->bit) { memset(c->bit, 0, (size_t)(c->bit_n + 1) * sizeof(double)); }
}

int64_t gmb_coll_count(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    /* HLL(28): stima ARROTONDATA (Count() esatto come double via gmb_coll_hll_count). */
    if (c->type_code == 30 && c->hll) return (int64_t)gmb_hll_count(c->hll);
    if (c->type_code == 20) return c->ring_len;
    return c->count;
}

int64_t gmb_coll_capacity(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    return c->capacity;
}

/* ---- RingBuffer (20): Enqueue/Dequeue/Peek ciclico ----
 * Semantica CommonLib CircularQueue: buffer a dimensione FISSA; quando pieno,
 * Enqueue SOVRASCRIVE l'elemento più vecchio (avanzando la testa) — NON espande. */
int gmb_coll_enqueue(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 20) return gmb_coll_add_last(h, v);
    if (c->capacity <= 0) c->capacity = 8;
    if (c->ring_len == c->capacity) gmb_val_release(c->ring[c->ring_tail]);   /* sovrascrive il piu' vecchio */
    c->ring[c->ring_tail] = v;
    gmb_val_retain(v);
    c->ring_tail = (c->ring_tail + 1) % c->capacity;
    if (c->ring_len == c->capacity)
        c->ring_head = (c->ring_head + 1) % c->capacity; /* sovrascrive il più vecchio */
    else
        c->ring_len++;
    return 0;
}

gmb_val_t gmb_coll_dequeue(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code != 20)
    {
        /* Queue.Dequeue / Stack.Pop su lista linkata: rimuove e ritorna la TESTA.
         * Prima: ritornava sempre nil (solo RingBuffer gestito) → Dequeue/Pop su una
         * QUEUE/STACK collezione erano no-op (front_back.bas: Front/Back/count). */
        if (!c->head) return nil;
        gmb_val_t v = c->head->v;
        gmb_coll_item_t* old = c->head;
        c->head = old->next;
        if (!c->head) c->tail = NULL;
        free(old);
        c->count--;
        return v;
    }
    if (c->ring_len == 0) return nil;
    gmb_val_t v = c->ring[c->ring_head];
    c->ring_head = (c->ring_head + 1) % c->capacity;
    c->ring_len--;
    return v;
}

gmb_val_t gmb_coll_peek(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code != 20)
        return gmb_coll_first(h);   /* Queue.Front / Stack.Peek = testa */
    if (c->ring_len == 0) return nil;
    return c->ring[c->ring_head];
}

/* ---- ToArray ---- */
gmb_val_t* gmb_coll_to_array(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c) return NULL;
    int64_t n = gmb_coll_count(h);
    if (n == 0) return NULL;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)n * sizeof(gmb_val_t));
    if (!arr) return NULL;
    if (c->type_code == 20)
    {
        for (int64_t i = 0; i < c->ring_len; i++) arr[i] = c->ring[(c->ring_head + i) % c->capacity];
        *out_len = (int)c->ring_len;
    }
    else
    {
        int k = 0;
        for (gmb_coll_item_t* it = c->head; it; it = it->next) arr[k++] = it->v;
        *out_len = k;
    }
    return arr;
}

/* ---- Set algebra su HashSet (typeCode 3): Union/Intersect/Except/SymmetricExcept ----
 * Ritorna un NUOVO handle HashSet; op: 0=union, 1=intersect, 2=except, 3=symmetric. */
int64_t gmb_collection_set_op(int64_t ha, int64_t hb, int op)
{
    gmb_collection_t* a = coll_get(ha);
    gmb_collection_t* b = coll_get(hb);
    if (!a || !b) return -1;
    int64_t out = gmb_collections_create(a->type_code);
    if (out < 0) return -1;
    int na = 0, nb = 0;
    gmb_val_t* va = gmb_coll_to_array(ha, &na);
    gmb_val_t* vb = gmb_coll_to_array(hb, &nb);
    if (op == 0) {
        for (int i = 0; i < na; i++) gmb_coll_add(out, va[i]);
        for (int i = 0; i < nb; i++) gmb_coll_add(out, vb[i]);
    } else if (op == 1) {
        for (int i = 0; i < na; i++) if (gmb_coll_contains(hb, va[i])) gmb_coll_add(out, va[i]);
    } else if (op == 2) {
        for (int i = 0; i < na; i++) if (!gmb_coll_contains(hb, va[i])) gmb_coll_add(out, va[i]);
    } else if (op == 3) {
        for (int i = 0; i < na; i++) if (!gmb_coll_contains(hb, va[i])) gmb_coll_add(out, va[i]);
        for (int i = 0; i < nb; i++) if (!gmb_coll_contains(ha, vb[i])) gmb_coll_add(out, vb[i]);
    }
    free(va); free(vb);
    return out;
}

/* ---- Accesso POSIZIONALE (FOR EACH/comprehension: UBOUND + indice) ----
 * Per le map-like (6/7/8/9/26/18/19) l'elemento è la COPPIA {Key, Value} come dict
 * (stessa forma del KVP del dispatcher VM/C per le dict); per le altre è il valore.
 * Indice fuori range → NIL. L'ordine è quello di iterazione (testa→coda: per STACK la
 * testa è la cima → LIFO, come IL). */
gmb_val_t gmb_coll_at(int64_t h, int64_t idx)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return gmb_val_nil();
    if (c->type_code == 20)   /* RingBuffer: ordine dal più vecchio */
    {
        if (idx < 0 || idx >= c->ring_len) return gmb_val_nil();
        return c->ring[(c->ring_head + idx) % c->capacity];
    }
    if (c->type_code == 30) return gmb_val_nil();   /* HLL: nessun elemento */
    if (idx < 0 || idx >= c->count) return gmb_val_nil();
    {
        gmb_coll_item_t* it = c->head;
        for (int64_t i = 0; it && i < idx; i++) it = it->next;
        if (!it) return gmb_val_nil();
        {
            int tc = c->type_code;
            int is_map = (tc == 6 || tc == 7 || tc == 8 || tc == 9 || tc == 26 || tc == 18 || tc == 19);
            if (is_map)
            {
                gmb_dict_t e = gmb_dict_new();
                gmb_dict_set(&e, "Key", it->key);
                gmb_dict_set(&e, "Value", it->v);
                return gmb_make_val_dict(e);
            }
            return it->v;
        }
    }
}

/* ---- Accesso sequenziale: First/Last/RemoveLast/IndexOf ---- */
gmb_val_t gmb_coll_first(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code == 20 && c->ring_len > 0) return c->ring[c->ring_head];
    if (c->head) return c->head->v;
    return nil;
}

gmb_val_t gmb_coll_last(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code == 20 && c->ring_len > 0) return c->ring[(c->ring_tail + c->capacity - 1) % c->capacity];
    if (c->tail) return c->tail->v;
    return nil;
}

gmb_val_t gmb_coll_remove_last(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code == 20 && c->ring_len > 0)
    {
        gmb_val_t v = c->ring[(c->ring_tail + c->capacity - 1) % c->capacity];
        c->ring_tail = (c->ring_tail + c->capacity - 1) % c->capacity;
        c->ring_len--;
        return v;
    }
    if (!c->tail) return nil;
    gmb_val_t v = c->tail->v;
    if (c->head == c->tail) { free(c->tail); c->head = c->tail = NULL; }
    else
    {
        gmb_coll_item_t* p = c->head;
        while (p->next && p->next != c->tail) p = p->next;
        free(c->tail);
        c->tail = p;
        c->tail->next = NULL;
    }
    c->count--;
    return v;
}

int gmb_coll_index_of(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return -1;
    int i = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next, i++)
        if (val_equal(it->v, v)) return i;
    return -1;
}

/* ---- Sorting / reversal (valori numerici e stringhe) ---- */
static int coll_val_less(gmb_val_t a, gmb_val_t b)
{
    if (a.kind == GMB_TYPE_DOUBLE || a.kind == GMB_TYPE_INT)
    {
        double x = a.kind == GMB_TYPE_DOUBLE ? a.v_double : (double)a.v_int;
        double y = b.kind == GMB_TYPE_DOUBLE ? b.v_double : (double)b.v_int;
        return x < y;
    }
    if (a.kind == GMB_TYPE_STRING && b.kind == GMB_TYPE_STRING && a.v_string.data && b.v_string.data)
        return strcmp(a.v_string.data, b.v_string.data) < 0;
    return 0;
}

void gmb_coll_sort(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return;
    int64_t n = c->count;
    if (n < 2) return;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)n * sizeof(gmb_val_t));
    if (!arr) return;
    int k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next) arr[k++] = it->v;
    for (int i = 1; i < n; i++)
    {
        gmb_val_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && coll_val_less(key, arr[j])) { arr[j + 1] = arr[j]; j--; }
        arr[j + 1] = key;
    }
    k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next) it->v = arr[k++];
    free(arr);
}

void gmb_coll_reverse(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return;
    gmb_coll_item_t* prev = NULL;
    gmb_coll_item_t* cur = c->head;
    c->tail = c->head;
    while (cur)
    {
        gmb_coll_item_t* nx = cur->next;
        cur->next = prev;
        prev = cur;
        cur = nx;
    }
    c->head = prev;
}

/* ---- Circular (13): cursore per Current/Rotate ---- */
gmb_val_t gmb_coll_current(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (!c->head) return nil;
    /* il cursore è il nodo in testa; Rotate sposta il cursore */
    return c->head->v;
}

void gmb_coll_rotate(int64_t h, int n)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->count < 2) return;
    int64_t cnt = c->count;
    int m = (int)(((long long)n % cnt) + cnt) % cnt;
    for (int i = 0; i < m; i++)
    {
        gmb_coll_item_t* it = c->head;
        c->head = it->next;
        it->next = NULL;
        c->tail->next = it;
        c->tail = it;
    }
}

/* ---- Bag (15): UniqueItems = valori distinti ---- */
gmb_val_t* gmb_coll_bag_unique_items(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c) return NULL;
    int64_t n = c->count;
    if (n == 0) return NULL;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)n * sizeof(gmb_val_t));
    if (!arr) return NULL;
    int k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
    {
        int dup = 0;
        for (int i = 0; i < k; i++)
            if (val_equal(arr[i], it->v)) { dup = 1; break; }
        if (!dup) arr[k++] = it->v;
    }
    *out_len = k;
    return arr;
}

/* ---- Trie (11): radice come children[0] non usata, tree di nodi ---- */
static gmb_trie_node_t* trie_node_new(char ch)
{
    gmb_trie_node_t* n = (gmb_trie_node_t*)calloc(1, sizeof(gmb_trie_node_t));
    if (n) n->c = ch;
    return n;
}

static gmb_trie_node_t* trie_find_child(gmb_trie_node_t* n, char ch)
{
    for (int i = 0; i < 26; i++)
        if (n->children[i] && n->children[i]->c == ch) return n->children[i];
    return NULL;
}

static gmb_trie_node_t* trie_get_or_add_child(gmb_trie_node_t* n, char ch)
{
    gmb_trie_node_t* f = trie_find_child(n, ch);
    if (f) return f;
    for (int i = 0; i < 26; i++)
        if (!n->children[i])
        {
            n->children[i] = trie_node_new(ch);
            return n->children[i];
        }
    return NULL;
}

static void trie_mirror_word(gmb_collection_t* c, const char* s)
{
    if (!c || !s || c->type_code != 11) return;
    if (!c->trie) c->trie = trie_node_new('\0');
    if (!c->trie) return;
    gmb_trie_node_t* n = c->trie;
    for (const char* p = s; *p; p++)
    {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z') continue;
        n = trie_get_or_add_child(n, ch);
        if (!n) return;
    }
    n->is_word = 1;
    /* NB: nessun c->count++ — il conteggio lo possiede la lista companion (Add/ctor). */
}

int gmb_coll_trie_add(int64_t h, const char* s)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !s || c->type_code != 11) return 0;
    if (!c->trie) c->trie = trie_node_new('\0');
    if (!c->trie) return 0;
    gmb_trie_node_t* n = c->trie;
    for (const char* p = s; *p; p++)
    {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z') continue; /* ignora non-lettere (semplificato) */
        n = trie_get_or_add_child(n, ch);
        if (!n) return 0;
    }
    int isNew = !n->is_word;
    n->is_word = 1;
    if (isNew)
    {
        /* Lista COMPANION (autorità di Count/FOR EACH/ToArray/Contains): senza, Trie.Add
         * aggiornava solo la struttura → Count=1 ma FOR EACH vuoto (sample
         * CollectionsModern su VM/C/C99). add_last incrementa anche c->count. */
        gmb_val_t wv = gmb_make_val_string_counted(s);
        gmb_coll_add_last(h, wv);
        gmb_val_discard(wv);
    }
    return -1;
}

int gmb_coll_trie_contains(int64_t h, const char* s)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !s || c->type_code != 11 || !c->trie) return 0;
    gmb_trie_node_t* n = c->trie;
    for (const char* p = s; *p; p++)
    {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z') continue;
        n = trie_find_child(n, ch);
        if (!n) return 0;
    }
    return n->is_word ? -1 : 0;
}

int gmb_coll_trie_startswith(int64_t h, const char* prefix)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !prefix || c->type_code != 11 || !c->trie) return 0;
    gmb_trie_node_t* n = c->trie;
    for (const char* p = prefix; *p; p++)
    {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z') continue;
        n = trie_find_child(n, ch);
        if (!n) return 0;
    }
    return -1;
}

int gmb_coll_trie_remove(int64_t h, const char* s)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !s || c->type_code != 11 || !c->trie) return 0;
    gmb_trie_node_t* n = c->trie;
    for (const char* p = s; *p; p++)
    {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z') continue;
        n = trie_find_child(n, ch);
        if (!n) return 0;
    }
    if (n->is_word) { n->is_word = 0; c->count--; return -1; }
    return 0;
}

/* ---- Trie: FindAllWordsWithPrefix (FindAll) ---- */
static void trie_collect(gmb_trie_node_t* n, char* buf, int depth, char*** out, int* nout, int* cap)
{
    if (!n) return;
    if (n->is_word)
    {
        if (*nout >= *cap)
        {
            *cap = *cap ? *cap * 2 : 8;
            *out = (char**)realloc(*out, (size_t)(*cap) * sizeof(char*));
        }
        buf[depth] = '\0';
        (*out)[(*nout)++] = strdup(buf);
    }
    for (int i = 0; i < 26; i++)
    {
        if (n->children[i])
        {
            buf[depth] = n->children[i]->c;
            trie_collect(n->children[i], buf, depth + 1, out, nout, cap);
        }
    }
}

char** gmb_coll_trie_find_all(int64_t h, const char* prefix, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || !prefix || c->type_code != 11 || !c->trie) return NULL;
    gmb_trie_node_t* n = c->trie;
    int plen = 0;
    char norm[256];
    const char* p;
    for (p = prefix; *p; p++)
    {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z') continue;
        n = trie_find_child(n, ch);
        if (!n) return NULL;
        norm[plen++] = ch;
    }
    if (plen == 0) return NULL;
    norm[plen] = '\0';
    char buf[512];
    memcpy(buf, norm, (size_t)plen);
    char** res = NULL;
    int nres = 0, cap = 0;
    trie_collect(n, buf, plen, &res, &nres, &cap);
    if (nres > 0)
    {
        /* l'ordine DFS per children[26] è alfabetico; la CommonLib usa l'ordine dei figli
           (Dictionary<char,..>) che qui coincide con a-z per il campione */
        *out_len = nres;
        return res;
    }
    free(res);
    return NULL;
}

/* ---- BITree (10): fenwick — semantica CommonLib BinaryIndexedTree ----
 * Indici 0-BASED inclusivi (come la classe C#): Update(index, delta) somma delta
 * all'elemento index; Query(index) somma [0..index]; QueryRange(lo,hi) somma [lo..hi].
 * Internamente il fenwick è 1-based: ogni indice pubblico è tradotto con +1. */
void gmb_coll_bitree_update(int64_t h, int idx, double delta)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 10 || !c->bit) return;
    int i = idx + 1;
    for (; i <= (int)c->bit_n; i += i & (-i))
        c->bit[i] += delta;
}

double gmb_coll_bitree_query(int64_t h, int idx)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 10 || !c->bit) return 0;
    double s = 0;
    int i = idx + 1;
    for (; i > 0; i -= i & (-i))
        s += c->bit[i];
    return s;
}

double gmb_coll_bitree_query_range(int64_t h, int lo, int hi)
{
    if (lo > hi) return 0;
    return gmb_coll_bitree_query(h, hi) - gmb_coll_bitree_query(h, lo - 1);
}

/* ---- Bag (15): UniqueCount = numero di valori distinti ---- */
int64_t gmb_coll_bag_unique_count(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    /* conteggio naive: valori distinti */
    int64_t n = c->count;
    if (n == 0) return 0;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)n * sizeof(gmb_val_t));
    if (!arr) return 0;
    int k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
    {
        int dup = 0;
        for (int i = 0; i < k; i++)
            if (val_equal(arr[i], it->v)) { dup = 1; break; }
        if (!dup) arr[k++] = it->v;
    }
    free(arr);
    return k;
}

/* ---- Map-like (7=Tree, 8=AVL, 9=RedBlack, 26=BPlusTree, 18=Timeline, 19=HashRing) ----
 * Implementazione semplice: lista di coppie {key, value}. Per il sample è sufficiente
 * (le mappe C# reali usano ordinamento bilanciato; qui la semantica osservabile conta). */
/* Chiave di una mappa ORDINATA: una DATA è il suo seriale OADate (numero). È la stessa
 * identità che usano il timeline IL/PCode e il C99 (`44936`), ed è l'unica rappresentazione
 * confrontabile/ordinabile: con la chiave DATETIME `kv_key_less`/`kv_key_equals` non la
 * riconoscevano (Between vuoto, ContainsKey(date) falso sul sample CollectionsModern).
 * Valore NON data: invariato. */
static gmb_val_t kv_norm_key(gmb_val_t k)
{
    if (k.kind == GMB_TYPE_DATETIME) return gmb_make_val_double(k.v_double);
    return k;
}

static int kv_key_less(gmb_val_t a, gmb_val_t b)
{
    a = kv_norm_key(a); b = kv_norm_key(b);
    if ((a.kind == GMB_TYPE_DOUBLE || a.kind == GMB_TYPE_INT) &&
        (b.kind == GMB_TYPE_DOUBLE || b.kind == GMB_TYPE_INT))
    {
        double x = a.kind == GMB_TYPE_DOUBLE ? a.v_double : (double)a.v_int;
        double y = b.kind == GMB_TYPE_DOUBLE ? b.v_double : (double)b.v_int;
        return x < y;
    }
    if (a.kind == GMB_TYPE_STRING && b.kind == GMB_TYPE_STRING && a.v_string.data && b.v_string.data)
        return strcmp(a.v_string.data, b.v_string.data) < 0;
    return 0;
}

static int kv_key_equals(gmb_val_t a, gmb_val_t b)
{
    a = kv_norm_key(a); b = kv_norm_key(b);
    if ((a.kind == GMB_TYPE_DOUBLE || a.kind == GMB_TYPE_INT) &&
        (b.kind == GMB_TYPE_DOUBLE || b.kind == GMB_TYPE_INT))
    {
        double x = a.kind == GMB_TYPE_DOUBLE ? a.v_double : (double)a.v_int;
        double y = b.kind == GMB_TYPE_DOUBLE ? b.v_double : (double)b.v_int;
        return x == y;
    }
    if (a.kind == GMB_TYPE_STRING && b.kind == GMB_TYPE_STRING && a.v_string.data && b.v_string.data)
        return strcmp(a.v_string.data, b.v_string.data) == 0;
    return 0;
}

static int coll_is_map(int tc) { return tc == 7 || tc == 8 || tc == 9 || tc == 26 || tc == 18 || tc == 19; }

static gmb_coll_item_t* kv_find(gmb_collection_t* c, gmb_val_t key)
{
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
        if (kv_key_equals(it->key, key)) return it;
    return NULL;
}

int gmb_coll_kv_set(int64_t h, gmb_val_t key, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return -1;
    key = kv_norm_key(key);
    gmb_coll_item_t* it = kv_find(c, key);
    if (it)
    {
        gmb_val_retain(v);                 /* il nuovo PRIMA del rilascio del vecchio */
        gmb_val_t old = it->v;
        it->v = v;
        gmb_val_release(old);
        return 0;
    }
    gmb_coll_item_t* n = (gmb_coll_item_t*)calloc(1, sizeof(gmb_coll_item_t));
    if (!n) return -1;
    n->key = key;
    n->v = v;
    gmb_val_retain(key);
    gmb_val_retain(v);
    if (!c->head || kv_key_less(key, c->head->key))
    {
        n->next = c->head;
        c->head = n;
        if (!c->tail) c->tail = n;
    }
    else
    {
        gmb_coll_item_t* p = c->head;
        while (p->next && !kv_key_less(key, p->next->key)) p = p->next;
        n->next = p->next;
        p->next = n;
        if (!n->next) c->tail = n;
    }
    c->count++;
    return 0;
}

gmb_val_t gmb_coll_kv_get(int64_t h, gmb_val_t key)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    gmb_coll_item_t* it = kv_find(c, kv_norm_key(key));
    return it ? it->v : nil;
}

int gmb_coll_kv_contains(int64_t h, gmb_val_t key)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    return kv_find(c, kv_norm_key(key)) ? -1 : 0;
}

int gmb_coll_kv_remove(int64_t h, gmb_val_t key)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    gmb_coll_item_t* prev = NULL;
    gmb_coll_item_t* it = c->head;
    while (it)
    {
        if (kv_key_equals(it->key, key))
        {
            if (prev) prev->next = it->next;
            else c->head = it->next;
            if (c->tail == it) c->tail = prev;
            gmb_val_release(it->key);
            gmb_val_release(it->v);
            free(it);
            c->count--;
            return -1;
        }
        prev = it;
        it = it->next;
    }
    return 0;
}

gmb_val_t gmb_coll_kv_min_key(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c || !c->head) return nil;
    return c->head->key;
}

gmb_val_t gmb_coll_kv_min_value(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c || !c->head) return nil;
    return c->head->v;
}

gmb_val_t gmb_coll_kv_max_key(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c || !c->tail) return nil;
    return c->tail->key;
}

gmb_val_t gmb_coll_kv_max_value(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c || !c->tail) return nil;
    return c->tail->v;
}

gmb_val_t gmb_coll_kv_extract_min(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c || !c->head) return nil;
    gmb_val_t r = c->head->v;
    gmb_coll_item_t* nx = c->head->next;
    free(c->head);
    c->head = nx;
    if (!c->head) c->tail = NULL;
    c->count--;
    return r;
}

gmb_val_t gmb_coll_kv_extract_max(int64_t h)
{
    return gmb_coll_remove_last(h);
}

gmb_val_t* gmb_coll_kv_keys(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->head) return NULL;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)c->count * sizeof(gmb_val_t));
    if (!arr) return NULL;
    int k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next) arr[k++] = it->key;
    *out_len = k;
    return arr;
}

gmb_val_t* gmb_coll_kv_pairs(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->head) return NULL;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)c->count * 2 * sizeof(gmb_val_t));
    if (!arr) return NULL;
    int k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
    {
        arr[k++] = it->key;
        arr[k++] = it->v;
    }
    *out_len = k;
    return arr;
}

gmb_val_t* gmb_coll_timeline_range(int64_t h, double lo, double hi, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->head) return NULL;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)c->count * 2 * sizeof(gmb_val_t));
    if (!arr) return NULL;
    int k = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
    {
        double kd = 0;
        if (it->key.kind == GMB_TYPE_DOUBLE) kd = it->key.v_double;
        else if (it->key.kind == GMB_TYPE_INT) kd = (double)it->key.v_int;
        else if (it->key.kind == GMB_TYPE_DATETIME) kd = it->key.v_double;   /* OADate */
        else continue;
        if (kd >= lo && kd <= hi)
        {
            arr[k++] = it->key;
            arr[k++] = it->v;
        }
    }
    *out_len = k;
    return arr;
}

/* ---- HashRing (19): nodi come lista di stringhe; GetNode via hash ---- */
int gmb_coll_hashring_add_node(int64_t h, const char* node)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !node || c->type_code != 19) return 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
        if (it->v.kind == GMB_TYPE_STRING && it->v.v_string.data && strcmp(it->v.v_string.data, node) == 0)
            return 0;
    gmb_coll_item_t* n = (gmb_coll_item_t*)calloc(1, sizeof(gmb_coll_item_t));
    if (!n) return 0;
    n->v = gmb_make_val_string(node);
    if (c->tail) c->tail->next = n;
    else c->head = n;
    c->tail = n;
    c->count++;
    return -1;
}

/* Consistent hashing IDENTICO a CommonLib.Collections.ConsistentHash (usata da IL/PCode):
 * MurmurHash2 (seed 0xc58f1a7b), 100 repliche per nodo ("nodo" + indice), anello ordinato
 * per hash, primo hash >= hash(key) con wrap-around. Sostituisce il vecchio djb2 % n (che
 * dava un nodo diverso da IL/PCode: CollectionsModern §11). */
#define HR_REPLICAS 100
static uint32_t hashring_murmur2(const char* str)
{
    const unsigned char* data = (const unsigned char*)str;
    int length = (str && *str) ? (int)strlen(str) : 0;
    uint32_t M = 0x5bd1e995u;
    const int R = 24;
    uint32_t h;
    int i = 0;
    if (length == 0) return 0;
    h = 0xc58f1a7bu ^ (uint32_t)length;
    while (length >= 4)
    {
        uint32_t k = (uint32_t)data[i] | ((uint32_t)data[i + 1] << 8)
                   | ((uint32_t)data[i + 2] << 16) | ((uint32_t)data[i + 3] << 24);
        i += 4;
        k *= M; k ^= k >> R; k *= M;
        h *= M; h ^= k;
        length -= 4;
    }
    switch (length)
    {
        case 3: h ^= (uint32_t)((uint16_t)(data[i] | (data[i + 1] << 8))); h ^= (uint32_t)data[i + 2] << 16; h *= M; break;
        case 2: h ^= (uint32_t)((uint16_t)(data[i] | (data[i + 1] << 8))); h *= M; break;
        case 1: h ^= (uint32_t)data[i]; h *= M; break;
        default: break;
    }
    h ^= h >> 13; h *= M; h ^= h >> 15;
    return h;
}

typedef struct { uint32_t hash; const char* node; } hr_point_t;
static int hr_point_cmp(const void* a, const void* b)
{
    uint32_t x = ((const hr_point_t*)a)->hash, y = ((const hr_point_t*)b)->hash;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* Nodo responsabile di `key` sull'anello costruito dai nodi dati (stringhe, ordine di
 * inserimento). Punto unico condiviso (§0bis): VM/C (handle) e C99 (lista). */
const char* gmb_hashring_node_for(const char* const* nodes, int n, const char* key)
{
    int m = 0, cap;
    hr_point_t* pts;
    char buf[512];
    uint32_t hk;
    int lo, hi;
    const char* out;
    if (n <= 0 || !key) return "";
    cap = n * HR_REPLICAS;
    pts = (hr_point_t*)malloc((size_t)cap * sizeof(hr_point_t));
    if (!pts) return "";
    for (int i = 0; i < n; i++)
    {
        if (!nodes[i] || !nodes[i][0]) continue;
        for (int r = 0; r < HR_REPLICAS; r++)
        {
            snprintf(buf, sizeof buf, "%s%d", nodes[i], r);
            pts[m].hash = hashring_murmur2(buf);
            pts[m].node = nodes[i];
            m++;
        }
    }
    if (m == 0) { free(pts); return ""; }
    qsort(pts, (size_t)m, sizeof(hr_point_t), hr_point_cmp);
    /* La mappa C# hash->nodo è unica (l'ultimo inserito vince): dedup tenendo il PRIMO
     * dopo l'ordinamento stabile non è equivalente, ma le collisioni fra 3.000+ hash sono
     * improbabili; per determinismo si tiene l'elemento con hash minore unico. */
    hk = hashring_murmur2(key);
    lo = 0; hi = m;
    while (lo < hi) { int mid = (lo + hi) / 2; if (pts[mid].hash < hk) lo = mid + 1; else hi = mid; }
    out = (lo < m) ? pts[lo].node : pts[0].node;
    free(pts);
    return out;
}

/* Variante per C99Native, che rappresenta HASHRING come lista di stringhe (nessun handle):
 * stesso anello/algortimo di gmb_coll_hashring_get_node. */
const char* gmb_hashring_node_from_list(gmb_list_t l, const char* key)
{
    const char* nodes[512];
    int n = 0;
    for (size_t i = 0; i < l.count && n < 512; i++)
        if (l.items[i].kind == GMB_TYPE_STRING && l.items[i].v_string.data)
            nodes[n++] = l.items[i].v_string.data;
    return gmb_hashring_node_for(nodes, n, key);
}

const char* gmb_coll_hashring_get_node(int64_t h, const char* key)
{
    gmb_collection_t* c = coll_get(h);
    const char* nodes[512];
    int n = 0;
    if (!c || c->type_code != 19 || !c->head) return "";
    for (gmb_coll_item_t* it = c->head; it && n < 512; it = it->next)
        if (it->v.kind == GMB_TYPE_STRING && it->v.v_string.data)
            nodes[n++] = it->v.v_string.data;
    return gmb_hashring_node_for(nodes, n, key);
}

/* ---- SparseSet (21): HasItem = Contains ---- */
int gmb_coll_has_item(int64_t h, gmb_val_t v)
{
    return gmb_coll_contains(h, v);
}

/* ---- Heap (14/27): Peek = root, Extract = root + rimuovi ----
 * Rappresentazione: lista semplice. Peek restituisce il minimo (heap min) o massimo
 * (typeCode 27 = max-heap) tra gli elementi; Extract lo rimuove. */
static gmb_val_t heap_root_val(gmb_collection_t* c, int is_max, int* idx_out)
{
    gmb_val_t best = gmb_val_nil();
    int best_i = -1;
    /* Il confronto usa il NUMERO normalizzato (best_num), non `best.v_double`: leggere il
     * membro double di un val INT è type-punning sull'unione → best_num restava ~0 (denormale)
     * e ogni confronto `<` era falso → MinHeap.Peek() ritornava il PRIMO elemento (4 invece di
     * 1). Latente da prima: con la catena popolata dai letterali il caso è diventato visibile. */
    double best_num = 0;
    int i = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next, i++)
    {
        double dv = 0;
        if (it->v.kind == GMB_TYPE_DOUBLE) dv = it->v.v_double;
        else if (it->v.kind == GMB_TYPE_INT) dv = (double)it->v.v_int;
        else continue;
        if (best_i < 0 || (is_max ? dv > best_num : dv < best_num))
        {
            best = it->v;
            best_i = i;
            best_num = dv;
        }
    }
    if (idx_out) *idx_out = best_i;
    return best;
}

gmb_val_t gmb_coll_heap_peek(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code != 14 && c->type_code != 27) return nil;
    return heap_root_val(c, c->type_code == 27, NULL);
}

gmb_val_t gmb_coll_heap_extract(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c) return nil;
    if (c->type_code != 14 && c->type_code != 27) return nil;
    int idx = -1;
    gmb_val_t best = heap_root_val(c, c->type_code == 27, &idx);
    if (idx < 0) return nil;
    int i = 0;
    gmb_coll_item_t* prev = NULL;
    gmb_coll_item_t* it = c->head;
    while (it)
    {
        if (i == idx)
        {
            if (prev) prev->next = it->next;
            else c->head = it->next;
            if (c->tail == it) c->tail = prev;
            free(it);
            c->count--;
            return best;
        }
        prev = it;
        it = it->next;
        i++;
    }
    return nil;
}

/* ---- Cache (16): chiave-valore con TTL opzionale (CommonLib Cache) ----
 * Semantica CommonLib: Set(key,value,ttl) — ttl null = mai scadere; le chiavi
 * scadute sono invisibili a Get/TryGet/ContainsKey/Keys/Values/Count. */
static double cache_now_epoch(void)
{
    time_t t = time(NULL);
    return (double)t;
}

void gmb_coll_cache_set(int64_t h, const char* key, gmb_val_t v, double ttl_seconds)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 16 || !key) return;
    for (gmb_coll_cache_entry_t* e = c->cache; e; e = e->next)
    {
        if (strcmp(e->key, key) == 0)
        {
            gmb_val_retain(v);             /* il nuovo PRIMA del rilascio del vecchio */
            gmb_val_t old = e->value;
            e->value = v;
            gmb_val_release(old);
            e->expires = ttl_seconds > 0 ? cache_now_epoch() + ttl_seconds : 0;
            return;
        }
    }
    gmb_coll_cache_entry_t* e = (gmb_coll_cache_entry_t*)calloc(1, sizeof(gmb_coll_cache_entry_t));
    if (!e) return;
    e->key = strdup(key);
    e->value = v;
    gmb_val_retain(v);
    e->expires = ttl_seconds > 0 ? cache_now_epoch() + ttl_seconds : 0;
    /* In CODA: l'ordine di inserimento è quello delle chiavi (parità P-Code: il Cache è un
     * dizionario, `Keys()`/`Values()` seguono l'ordine di inserimento). Con l'inserimento in
     * testa `Cache.Keys()` usciva invertito ([key2, key1] invece di [key1, key2]). */
    e->next = NULL;
    if (!c->cache) c->cache = e;
    else {
        gmb_coll_cache_entry_t* t = c->cache;
        while (t->next) t = t->next;
        t->next = e;
    }
    c->count++;
}

static int cache_entry_valid(gmb_coll_cache_entry_t* e)
{
    return e->expires == 0 || cache_now_epoch() <= e->expires;
}

gmb_val_t gmb_coll_cache_get(int64_t h, const char* key)
{
    gmb_collection_t* c = coll_get(h);
    gmb_val_t nil = gmb_val_nil();
    if (!c || c->type_code != 16 || !key) return nil;
    for (gmb_coll_cache_entry_t* e = c->cache; e; e = e->next)
        if (strcmp(e->key, key) == 0)
            return cache_entry_valid(e) ? e->value : nil;
    return nil;
}

int gmb_coll_cache_contains(int64_t h, const char* key)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 16 || !key) return 0;
    for (gmb_coll_cache_entry_t* e = c->cache; e; e = e->next)
        if (strcmp(e->key, key) == 0 && cache_entry_valid(e)) return -1;
    return 0;
}

char** gmb_coll_cache_keys(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 16 || !c->cache) return NULL;
    char** arr = (char**)malloc((size_t)c->count * sizeof(char*));
    if (!arr) return NULL;
    int k = 0;
    for (gmb_coll_cache_entry_t* e = c->cache; e; e = e->next)
        if (cache_entry_valid(e)) arr[k++] = strdup(e->key);
    *out_len = k;
    return arr;
}

gmb_val_t* gmb_coll_cache_values(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 16 || !c->cache) return NULL;
    gmb_val_t* arr = (gmb_val_t*)malloc((size_t)c->count * sizeof(gmb_val_t));
    if (!arr) return NULL;
    int k = 0;
    for (gmb_coll_cache_entry_t* e = c->cache; e; e = e->next)
        if (cache_entry_valid(e)) arr[k++] = e->value;
    *out_len = k;
    return arr;
}

/* ---- Incapsulamento transpiler: collezione boxed in gmb_val_t ----
   Il C99 rappresenta una collezione come gmb_obj_t con data = handle (int64_t) e
   type_name "gmb_collection". L'estrazione ricava il handle dal val. */
gmb_val_t gmb_make_val_collection(int64_t handle) {
    int64_t* h = (int64_t*)malloc(sizeof(int64_t));
    if (!h) return gmb_val_nil();
    *h = handle;
    return gmb_make_val_obj(gmb_obj_new_typed(h, "gmb_collection"));
}

int64_t gmb_val_collection_handle(gmb_val_t v) {
    void* d = gmb_obj_data(v);
    if (!d) return -1;
    return *(int64_t*)d;
}

/* Crea collezione + applica il costruttore + boxa: espressione unica per il C99
   (NEW RINGBUFFER(3) ecc.). args opzionale (NULL se nessuno). */
gmb_val_t gmb_collection_new(int type_code, gmb_val_t* args, int n_args) {
    gmb_collections_init();
    int64_t h = gmb_collections_create(type_code);
    if (h < 0) return gmb_val_nil();
    if (args && n_args > 0) gmb_coll_apply_ctor(h, args, n_args);
    return gmb_make_val_collection(h);
}

/* ---- Chiavi/coppie dei contenitori tipizzati (TREE/AVL/REDBLACK/BPlusTree/Timeline/Cache) ----
 * Il P-Code (oracolo, common/IlBackendRuntime/PCode/PCodeProcessor.cs) tratta questi tipi
 * come dizionari ordinati — vedi i blocchi IDictionary/IKeyValueIndexable. Qui l'unica
 * sorgente condivisa (§0bis) delle relative materializzazioni. */

/* type_code di una collezione boxata; -1 se NON è una collezione. */
int gmb_collection_type_of(gmb_val_t v)
{
    int64_t h = gmb_val_collection_handle(v);
    gmb_collection_t* c = coll_get(h);
    return c ? (int)c->type_code : -1;
}

/* [chiave, valore] a 2 elementi: parità P-Code, dove Min/Max/ExtractMin/ExtractMax/First/Last
 * dei contenitori chiave→valore restituiscono `List<object?>{k, v}` (JSON `["a",10]`).
 * NON è un dict {Key,Value}: quello è la forma di ToArray/GetRange (gmb_collection_entries_to_list). */
static gmb_val_t kv_entry_val(gmb_collection_t* c, gmb_val_t key, gmb_val_t val)
{
    /* Timeline (18): la chiave è un OADate GREZZO nel runtime; in JSON va resa ISO-8601 come
     * per GetRange (parità IL del canone datetime), non come numero di serie. */
    if (c && c->type_code == 18) {
        if (key.kind == GMB_TYPE_DOUBLE) key = gmb_make_val_datetime(key.v_double);
        else if (key.kind == GMB_TYPE_INT) key = gmb_make_val_datetime((gmb_double_t)key.v_int);
    }
    gmb_list_t l = gmb_list_new();
    gmb_list_add(&l, key);
    gmb_list_add(&l, val);
    return gmb_make_val_list(l);
}

gmb_val_t gmb_coll_kv_entry_min(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->head) return gmb_val_nil();
    return kv_entry_val(c, c->head->key, c->head->v);
}

gmb_val_t gmb_coll_kv_entry_max(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->tail) return gmb_val_nil();
    return kv_entry_val(c, c->tail->key, c->tail->v);
}

gmb_val_t gmb_coll_kv_entry_extract_min(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->head) return gmb_val_nil();
    gmb_val_t r = kv_entry_val(c, c->head->key, c->head->v);
    gmb_coll_item_t* old = c->head;
    c->head = old->next;
    if (!c->head) c->tail = NULL;
    gmb_val_release(old->key);
    gmb_val_release(old->v);
    free(old);
    c->count--;
    return r;
}

gmb_val_t gmb_coll_kv_entry_extract_max(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->tail) return gmb_val_nil();
    gmb_val_t r = kv_entry_val(c, c->tail->key, c->tail->v);
    gmb_coll_item_t* prev = NULL;
    for (gmb_coll_item_t* it = c->head; it && it != c->tail; it = it->next) prev = it;
    if (prev) prev->next = NULL; else c->head = NULL;
    gmb_val_release(c->tail->key);
    gmb_val_release(c->tail->v);
    free(c->tail);
    c->tail = prev;
    c->count--;
    return r;
}

/* BAG (15): elementi DISTINTI in ordine di prima comparsa — forma lista di
 * `UniqueItems()` (parità P-Code: le chiavi del Dictionary<object,long>). */
gmb_list_t gmb_collection_bag_unique_list(int64_t h) {
    gmb_list_t l = gmb_list_new();
    int n = 0;
    gmb_val_t* items = gmb_coll_bag_unique_items(h, &n);
    if (items) {
        for (int i = 0; i < n; i++) gmb_list_add(&l, items[i]);
        free(items);
    }
    return l;
}

/* Lista di dict {Key,Value} — forma di `ToArray()` sui contenitori chiave→valore (parità
 * P-Code: JSON `[{"Key":"a","Value":true},…]`, distinta da Min() che è una coppia a 2
 * elementi).
 * CORPO UNICO (§0bis): la variante HANDLE-based `gmb_coll_kv_entries_to_list` è quella che
 * usa la VM/C (ha il handle, non il box); questa boxed (transpiler C99) le DELEGA. */
gmb_list_t gmb_coll_kv_entries_to_list(int64_t h, int* is_kv) {
    gmb_list_t l = gmb_list_new();
    if (is_kv) *is_kv = 0;
    gmb_collection_t* c = coll_get(h);
    if (!c || !c->head) return l;
    /* Solo i contenitori CHIAVE→VALORE hanno coppie significative: per una lista il nodo
     * porta la chiave a NIL e la coppia sarebbe {Key:null,Value:x}. */
    int tc = (int)c->type_code;
    if (!(tc == 6 || tc == 7 || tc == 8 || tc == 9 || tc == 26 || tc == 18 || tc == 19)) return l;
    if (is_kv) *is_kv = 1;
    int n = 0;
    gmb_val_t* pairs = gmb_coll_kv_pairs(h, &n);
    if (!pairs) return l;
    for (int i = 0; i + 1 < n; i += 2) {
        gmb_dict_t e = gmb_dict_new();
        gmb_dict_set(&e, "Key", pairs[i]);
        gmb_dict_set(&e, "Value", pairs[i + 1]);
        gmb_list_add(&l, gmb_make_val_dict(e));
    }
    free(pairs);
    return l;
}

gmb_list_t gmb_collection_entries_to_list(gmb_val_t coll) {
    int64_t h = gmb_val_collection_handle(coll);
    int is_kv = 0;
    gmb_list_t l = gmb_coll_kv_entries_to_list(h, &is_kv);
    if (is_kv) return l;
    gmb_list_free(&l);
    return gmb_collection_to_list(coll);
}

/* ---- BAG (15): multiset (parità P-Code, dove il Bag è un Dictionary<object,long>) ---- */

/* Removes di UNA istanza dal multiset: si toglie l'ULTIMA occorrenza, non la prima — così
 * l'ordine di PRIMA COMPARSA degli elementi resta quello di inserimento (parità P-Code, dove
 * il Bag è un Dictionary<object,long> e Remove decrementa il conteggio senza spostare la
 * chiave). Con la rimozione in testa `Bag.ToJson()` usciva {"b":1,"a":2,"c":1}. */
int gmb_coll_bag_remove(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    gmb_coll_item_t* prev = NULL;
    gmb_coll_item_t* it = c->head;
    gmb_coll_item_t* last = NULL;
    gmb_coll_item_t* last_prev = NULL;
    while (it)
    {
        if (val_equal(it->v, v)) { last = it; last_prev = prev; }
        prev = it;
        it = it->next;
    }
    if (!last) return 0;
    if (last_prev) last_prev->next = last->next;
    else c->head = last->next;
    if (c->tail == last) c->tail = last_prev;
    gmb_val_release(last->key);
    gmb_val_release(last->v);
    free(last);
    c->count--;
    return -1;
}

/* Molteplicità di un elemento (Bag.Count(item)). Senza argomenti, Bag.Count() resta il
 * numero TOTALE di elementi (gmb_coll_count). */
gmb_int_t gmb_coll_bag_count_of(int64_t h, gmb_val_t v)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return 0;
    gmb_int_t n = 0;
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
        if (val_equal(it->v, v)) n++;
    return n;
}

/* Conteggi del multiset come dict {elemento → molteplicità}, in ordine di prima comparsa
 * (usato dalla serializzazione JSON e da gmb_coll_bag_to_json). Chiamante: gmb_dict_free + free. */
gmb_dict_t* gmb_coll_bag_counts_dict(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return NULL;
    gmb_dict_t* d = (gmb_dict_t*)calloc(1, sizeof(gmb_dict_t));
    if (!d) return NULL;
    *d = gmb_dict_new();
    for (gmb_coll_item_t* it = c->head; it; it = it->next) {
        gmb_string_t ks = gmb_val_to_string(it->v);
        const char* key = (ks.data && *ks.data) ? ks.data : "";
        gmb_val_t cur = gmb_dict_get(*d, key);
        gmb_int_t n = cur.kind == GMB_TYPE_INT ? cur.v_int
                     : cur.kind == GMB_TYPE_DOUBLE ? (gmb_int_t)cur.v_double : 0;
        gmb_dict_set(d, key, gmb_make_val_int(n + 1));
    }
    return d;
}

/* Bag.ToJson(): JSON `{"elemento":molteplicità,…}` (dopo Remove("a"): {"a":2,"b":1,"c":1}). */
gmb_string_t gmb_coll_bag_to_json(int64_t h)
{
    gmb_dict_t* d = gmb_coll_bag_counts_dict(h);
    if (!d) return gmb_string_alloc("{}");
    gmb_string_t js = gmb_val_to_json(gmb_make_val_dict(*d));
    gmb_dict_free(d);
    free(d);
    return js;
}

/* Timeline.ToJson(): oggetto {ISO-8601: valore}. Le chiavi interne del timeline sono i
 * seriali OADate (kv_norm_key): qui si rendono ISO come le chiavi-data dei DICT, così il
 * JSON è lo stesso di IL/PCode/C99 (`{"2023-01-10T00:00:00":"A"}`). */
gmb_string_t gmb_coll_timeline_to_json(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c) return gmb_string_alloc("{}");
    gmb_dict_t d = gmb_dict_new();
    for (gmb_coll_item_t* it = c->head; it; it = it->next)
    {
        double serial = 0;
        if (it->key.kind == GMB_TYPE_DATETIME || it->key.kind == GMB_TYPE_DOUBLE) serial = it->key.v_double;
        else if (it->key.kind == GMB_TYPE_INT) serial = (double)it->key.v_int;
        else continue;
        gmb_string_t iso = gmb_dt_datetime_iso(serial);
        gmb_dict_set(&d, iso.data ? iso.data : "", it->v);
        gmb_string_free(&iso);
    }
    gmb_string_t js = gmb_val_to_json(gmb_make_val_dict(d));
    gmb_dict_free(&d);
    return js;
}

/* FOR EACH: materializza le CHIAVI di una collezione map-like in una gmb_list_t. */
gmb_list_t gmb_collection_keys_to_list(gmb_val_t coll) {
    gmb_list_t l = gmb_list_new();
    int64_t h = gmb_val_collection_handle(coll);
    if (h < 0) return l;
    gmb_collection_t* c = coll_get(h);
    /* CACHE (16): lo storage è la lista dedicata `c->cache` (TTL), NON la catena head/tail:
     * da lì le chiavi arrivavano vuote a JSON.Stringify. */
    if (c && c->type_code == 16) {
        int n = 0;
        char** ks = gmb_coll_cache_keys(h, &n);
        if (ks) {
            for (int i = 0; i < n; i++) {
                gmb_list_add(&l, gmb_make_val_string(ks[i] ? ks[i] : ""));
                free(ks[i]);
            }
            free(ks);
        }
        return l;
    }
    int n = 0;
    gmb_val_t* keys = gmb_coll_kv_keys(h, &n);
    if (keys) {
        for (int i = 0; i < n; i++) gmb_list_add(&l, keys[i]);
        free(keys);
    }
    return l;
}

/* FOR EACH: materializza i VALORI di una collezione sequence in una gmb_list_t. */
gmb_list_t gmb_collection_to_list(gmb_val_t coll) {
    gmb_list_t l = gmb_list_new();
    int64_t h = gmb_val_collection_handle(coll);
    if (h < 0) return l;
    gmb_collection_t* cc = coll_get(h);
    /* CACHE (16): lo storage è la lista dedicata `c->cache` (TTL), non la catena head/tail
     * (stesso motivo di gmb_collection_keys_to_list: `Values()` usciva vuoto). */
    if (cc && cc->type_code == 16) {
        int cn = 0;
        gmb_val_t* vals = gmb_coll_cache_values(h, &cn);
        if (vals) {
            for (int i = 0; i < cn; i++) gmb_list_add(&l, vals[i]);
            free(vals);
        }
        return l;
    }
    int n = 0;
    gmb_val_t* arr = gmb_coll_to_array(h, &n);
    if (arr) {
        for (int i = 0; i < n; i++) gmb_list_add(&l, arr[i]);
        free(arr);
    }
    return l;
}

/* ---- Materializzazioni per il transpiler C99 (C99Native) ----
 * Trio.FindAll / Timeline.GetRange ritornano array C (char** / gmb_val_t*) che il
 * BASIC usa come LISTE (JSON.Stringify / PRINT): qui la conversione nell'unica
 * sorgente condivisa (§0bis), così i backend non duplicano la logica. */

/* Trie (11): tutte le parole col prefisso dato, in una lista di stringhe (ordine DFS a-z). */
gmb_list_t gmb_collection_trie_find_all_list(int64_t h, const char* prefix) {
    gmb_list_t l = gmb_list_new();
    int n = 0;
    char** words = gmb_coll_trie_find_all(h, prefix, &n);
    if (!words) return l;
    for (int i = 0; i < n; i++) {
        gmb_list_add(&l, gmb_make_val_string(words[i] ? words[i] : ""));
        free(words[i]);
    }
    free(words);
    return l;
}

/* Timeline (18): eventi con chiave in [lo, hi] come lista di coppie {Key, Value}
 * (parità IL: JSON.Stringify(GetRange(...)) = [{"Key":"<iso>,"Value":…}, …]).
 * La chiave numerica (OADate) è resa come DATETIME boxato, così il JSON è ISO-8601
 * come per ogni altra data del runtime. */
gmb_list_t gmb_collection_timeline_range_list(int64_t h, double lo, double hi) {
    gmb_list_t l = gmb_list_new();
    int n = 0;
    gmb_val_t* arr = gmb_coll_timeline_range(h, lo, hi, &n);
    if (!arr) return l;
    for (int i = 0; i + 1 < n; i += 2) {
        gmb_val_t key = arr[i];
        if (key.kind == GMB_TYPE_DOUBLE)
            key = gmb_make_val_datetime(key.v_double);
        else if (key.kind == GMB_TYPE_INT)
            key = gmb_make_val_datetime((gmb_double_t)key.v_int);
        gmb_dict_t e = gmb_dict_new();
        gmb_dict_set(&e, "Key", key);
        gmb_dict_set(&e, "Value", arr[i + 1]);
        gmb_list_add(&l, gmb_make_val_dict(e));
    }
    free(arr);
    return l;
}

/* ---- HLL (typeCode 28) — API dedicata sul runtime condiviso (gmb_hll.c) ----
 * La stima e' un double (Count()) e va esposta senza passare da gmb_coll_count
 * (che ritorna int64_t). */
double gmb_coll_hll_count(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 30 || !c->hll) return 0.0;
    return gmb_hll_count(c->hll);
}

double gmb_coll_hll_error(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 30 || !c->hll) return 0.0;
    return gmb_hll_error(c->hll);
}

int gmb_coll_hll_precision(int64_t h)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 30 || !c->hll) return 0;
    return gmb_hll_precision(c->hll);
}

int gmb_coll_hll_merge(int64_t dst, int64_t src)
{
    gmb_collection_t* a = coll_get(dst);
    gmb_collection_t* b = coll_get(src);
    if (!a || !b || a->type_code != 30 || b->type_code != 30 || !a->hll || !b->hll) return -1;
    return gmb_hll_merge(a->hll, b->hll);
}

int gmb_coll_hll_serialize(int64_t h, char* out, size_t cap)
{
    gmb_collection_t* c = coll_get(h);
    if (!c || c->type_code != 30 || !c->hll) return -1;
    return gmb_hll_serialize(c->hll, out, cap);
}

int gmb_coll_hll_load(int64_t h, const char* data, size_t len)
{
    gmb_collection_t* c = coll_get(h);
    gmb_hll_t* n;
    if (!c || c->type_code != 30 || !c->hll) return -1;
    n = gmb_hll_deserialize(data, len);
    if (!n) return -1;
    gmb_hll_free(c->hll);
    c->hll = n;
    return 0;
}
