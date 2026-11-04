/* ============================================================================
 * gmb_collections.h — Collezioni della CommonLib C# portate in C.
 * Registry di handle → collezione. Ogni collezione ha un tipo (typeCode BASIC):
 *   9=RedBlack, 10=BITree, 11=Trie, 12=LinkedList, 13=Circular, 14=BinaryHeap,
 *   15=Bag, 16=Cache, 18=Timeline, 19=HashRing, 20=RingBuffer, 21=SparseSet, 26=BPlusTree
 * ============================================================================ */
#ifndef GMB_COLLECTIONS_H
#define GMB_COLLECTIONS_H

#include <stdint.h>
#include <stddef.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Registry ---- */
void gmb_collections_init(void);
void gmb_collections_shutdown(void);

/* Crea una collezione del typeCode dato. Ritorna handle (>0) o -1. */
int64_t gmb_collections_create(int type_code);
void gmb_collections_free(int64_t h);

/* ---- Operazioni comuni (valore generico come gmb_val_t) ---- */
int gmb_coll_add(int64_t h, gmb_val_t v);          /* Bag/LinkedList/Circular/Heap/SparseSet */
int gmb_coll_add_first(int64_t h, gmb_val_t v);    /* LinkedList/Doubly */
int gmb_coll_add_last(int64_t h, gmb_val_t v);     /* LinkedList/Doubly */
int gmb_coll_remove(int64_t h, gmb_val_t v);       /* contiene? rimuove */
int gmb_coll_contains(int64_t h, gmb_val_t v);
void gmb_coll_clear(int64_t h);
int64_t gmb_coll_count(int64_t h);
int64_t gmb_coll_capacity(int64_t h);              /* RingBuffer/SparseSet */

/* Costruttori: applica gli argomenti del costruttore BASIC alla collection appena creata.
   args è un array di gmb_val_t; n_args il conteggio. Semantica per typeCode:
   19 HashRing: args[0] = lista di nodi (stringhe) → li aggiunge.
   20 RingBuffer: args[0] = capacità.
   21 SparseSet: args[0]=maxValue, args[1]=capacity (ignorati: solo dimensionamento).
   26 BPlusTree: args[0]=order (ignorato, lista semplice). */
void gmb_coll_apply_ctor(int64_t h, gmb_val_t* args, int n_args);

/* Queue/Stack/RingBuffer/Heap: enqueue/dequeue/push/pop/peek */
int gmb_coll_enqueue(int64_t h, gmb_val_t v);
gmb_val_t gmb_coll_dequeue(int64_t h);
gmb_val_t gmb_coll_peek(int64_t h);

/* ToArray: lista di gmb_val_t */
gmb_val_t* gmb_coll_to_array(int64_t h, int* out_len);

/* Set algebra su HashSet (typeCode 3): 0=Union, 1=Intersect, 2=Except, 3=SymmetricExcept.
 * Ritorna un NUOVO handle HashSet (o -1). */
int64_t gmb_collection_set_op(int64_t ha, int64_t hb, int op);

/* Accesso POSIZIONALE (FOR EACH/comprehension): N-esimo elemento nell'ordine di
 * iterazione; per le map-like una coppia dict {Key,Value}. NIL se fuori range. */
gmb_val_t gmb_coll_at(int64_t h, int64_t idx);

/* Accesso sequenziale (LinkedList/Doubly/Circular): First/Last/RemoveLast/IndexOf */
gmb_val_t gmb_coll_first(int64_t h);
gmb_val_t gmb_coll_last(int64_t h);
gmb_val_t gmb_coll_remove_last(int64_t h);
int gmb_coll_index_of(int64_t h, gmb_val_t v);

/* Sorting / reversal su liste lineari */
void gmb_coll_sort(int64_t h);
void gmb_coll_reverse(int64_t h);

/* Circular (13): Current/Rotate */
gmb_val_t gmb_coll_current(int64_t h);
void gmb_coll_rotate(int64_t h, int n);

/* ---- HLL (typeCode 28, HyperLogLog — gmb_hll.c) ----
 * Add/Count/Clear passano dalle API comuni (gmb_coll_add/count/clear); queste
 * espongono la stima double e le operazioni specifiche dello sketch. */
double gmb_coll_hll_count(int64_t h);
double gmb_coll_hll_error(int64_t h);
int    gmb_coll_hll_precision(int64_t h);
int    gmb_coll_hll_merge(int64_t dst, int64_t src);        /* 0 ok, -1 precisioni diverse */
int    gmb_coll_hll_serialize(int64_t h, char* out, size_t cap);   /* len o -1 */
int    gmb_coll_hll_load(int64_t h, const char* data, size_t len);  /* 0 ok, -1 formato */

/* Bag (15): UniqueItems (valori distinti in una nuova lista) */
gmb_val_t* gmb_coll_bag_unique_items(int64_t h, int* out_len);

/* Trie (typeCode 11): operazioni stringa */
int gmb_coll_trie_add(int64_t h, const char* s);
int gmb_coll_trie_remove(int64_t h, const char* s);
int gmb_coll_trie_contains(int64_t h, const char* s);
int gmb_coll_trie_startswith(int64_t h, const char* prefix);
/* FindAllWordsWithPrefix (FindAll): tutte le parole con il prefisso dato, ordinate
   in esplorazione DFS (CommonLib: ordine dei figli, qui a-z). Ritorna un array di
   stringhe malloc'd; *out_len = numero. */
char** gmb_coll_trie_find_all(int64_t h, const char* prefix, int* out_len);

/* BITree (typeCode 10): Update(index, delta), Query(index) */
void gmb_coll_bitree_update(int64_t h, int idx, double delta);
double gmb_coll_bitree_query(int64_t h, int idx);
double gmb_coll_bitree_query_range(int64_t h, int lo, int hi);

/* Bag (15): UniqueCount, UniqueItems (conteggio) */
int64_t gmb_coll_bag_unique_count(int64_t h);

/* Map-like (7=Tree, 8=AVL, 9=RedBlack, 26=BPlusTree, 18=Timeline, 19=HashRing):
   coppie chiave→valore. Le chiavi sono stringhe o numeri; i valori generici. */
int gmb_coll_kv_set(int64_t h, gmb_val_t key, gmb_val_t v);
gmb_val_t gmb_coll_kv_get(int64_t h, gmb_val_t key);
int gmb_coll_kv_contains(int64_t h, gmb_val_t key);
int gmb_coll_kv_remove(int64_t h, gmb_val_t key);
gmb_val_t gmb_coll_kv_min_key(int64_t h);
gmb_val_t gmb_coll_kv_min_value(int64_t h);
gmb_val_t gmb_coll_kv_max_key(int64_t h);
gmb_val_t gmb_coll_kv_max_value(int64_t h);
gmb_val_t gmb_coll_kv_extract_min(int64_t h);   /* rimuove e ritorna la coppia {key, value} */
gmb_val_t gmb_coll_kv_extract_max(int64_t h);
/* Chiavi come lista */
gmb_val_t* gmb_coll_kv_keys(int64_t h, int* out_len);
/* Coppie {key, value} in lista unica */
gmb_val_t* gmb_coll_kv_pairs(int64_t h, int* out_len);
/* Timeline (18): range di eventi con chiave nel [lo, hi] (numeriche) */
gmb_val_t* gmb_coll_timeline_range(int64_t h, double lo, double hi, int* out_len);

/* HashRing (19): nodi come lista di stringhe; GetNode(key) → nodo via hash */
int gmb_coll_hashring_add_node(int64_t h, const char* node);
const char* gmb_coll_hashring_get_node(int64_t h, const char* key);

/* SparseSet (21): HasItem = Contains */
int gmb_coll_has_item(int64_t h, gmb_val_t v);

/* Heap (14/27): Peek = min (o max), Extract = rimuove root */
gmb_val_t gmb_coll_heap_peek(int64_t h);
gmb_val_t gmb_coll_heap_extract(int64_t h);

/* Cache (16): chiave-valore con TTL opzionale (CommonLib: ttl<=0 = mai scadere) */
void gmb_coll_cache_set(int64_t h, const char* key, gmb_val_t v, double ttl_seconds);
gmb_val_t gmb_coll_cache_get(int64_t h, const char* key);
int gmb_coll_cache_contains(int64_t h, const char* key);
/* Chiavi valide (non scadute) come array di stringhe */
char** gmb_coll_cache_keys(int64_t h, int* out_len);
/* Valori validi (non scaduti) come array di gmb_val_t */
gmb_val_t* gmb_coll_cache_values(int64_t h, int* out_len);

#ifdef __cplusplus
}
#endif

#endif /* GMB_COLLECTIONS_H */

/* ---- Incapsulamento transpiler: collezione boxed in gmb_val_t (C99) ---- */
gmb_val_t gmb_make_val_collection(int64_t handle);
int64_t gmb_val_collection_handle(gmb_val_t v);

/* Crea collezione + ctor args + box (espressione unica C99). */
gmb_val_t gmb_collection_new(int type_code, gmb_val_t* args, int n_args);

/* FOR EACH: chiavi/valori di una collezione boxed → gmb_list_t. */
gmb_list_t gmb_collection_keys_to_list(gmb_val_t coll);
gmb_list_t gmb_collection_to_list(gmb_val_t coll);

/* Chiavi/coppie dei contenitori tipizzati (TREE/AVL/REDBLACK/BPlusTree/Timeline) e multiset
 * (BAG) — unica sorgente condivisa (§0bis). Semantica allineata al P-Code (oracolo):
 * Min/Max/ExtractMin/ExtractMax/First/Last restituiscono una coppia a 2 ELEMENTI [chiave,
 * valore]; ToArray() una lista di dict {Key,Value}; il Bag è un multiset (Count(item) =
 * molteplicità, ToJson = {"elemento":molteplicità}). */
int gmb_collection_type_of(gmb_val_t v);                    /* type_code, -1 se non è una collezione */
/* Nome canonico (TYPENAME) e lettera Clipper (VALTYPE) di una collezione dal typeCode.
 * Fonte unica condivisa (VM/C + programmi generati); parità IlRuntime.TypeName/ClipperStrings. */
const char* gmb_collection_type_name(int type_code);
const char* gmb_collection_valtype_letter(int type_code);
gmb_list_t gmb_collection_entries_to_list(gmb_val_t coll);  /* ToArray/ToList dei kv: lista di {Key,Value} */
/* Variante HANDLE-based (VM/C, che ha il handle e non il box): stessa lista di {Key,Value}.
 * `*is_kv` = 1 se il contenitore è chiave→valore (e `l` è popolata), 0 altrimenti (`l` vuota:
 * il fallback lo sceglie il chiamante). */
gmb_list_t gmb_coll_kv_entries_to_list(int64_t h, int* is_kv);
gmb_val_t gmb_coll_kv_entry_min(int64_t h);                 /* [chiave, valore] */
gmb_val_t gmb_coll_kv_entry_max(int64_t h);
gmb_val_t gmb_coll_kv_entry_extract_min(int64_t h);
gmb_val_t gmb_coll_kv_entry_extract_max(int64_t h);
gmb_int_t gmb_coll_bag_count_of(int64_t h, gmb_val_t v);
int gmb_coll_bag_remove(int64_t h, gmb_val_t v);                /* rimuove UNA istanza (l'ultima) */
gmb_list_t gmb_collection_bag_unique_list(int64_t h);           /* UniqueItems() come lista */
gmb_dict_t* gmb_coll_bag_counts_dict(int64_t h);            /* chiamante: gmb_dict_free + free */
gmb_string_t gmb_coll_bag_to_json(int64_t h);
/* Materializzazioni per il transpiler C99 (C99Native): FindAll(Trie) come
 * lista di stringhe; GetRange(Timeline) come lista di coppie {Key,Value} (chiave resa
 * come DATETIME boxato → JSON ISO-8601 come il resto del runtime). Entrambe vuote (mai
 * NULL) se il tipo non corrisponde o il range è vuoto. */
gmb_list_t gmb_collection_trie_find_all_list(int64_t h, const char* prefix);
gmb_list_t gmb_collection_timeline_range_list(int64_t h, double lo, double hi);
