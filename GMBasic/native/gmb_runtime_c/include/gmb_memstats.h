/* gmb_memstats.h — contabilità della MEMORIA VIVA del runtime C (diagnostica).
 *
 * Perché: il `DebugAllocator` di Zig (leak + double-free con lo stack) non ha un equivalente
 * diretto qui — i valori BASIC sono refcounted, quindi "ancora allocato a fine programma" è
 * normale e non è un leak; e il runtime alloca con `malloc` in ~1000 punti. Questo modulo
 * copre ciò che si può contare ESATTAMENTE con coppie alloc/free verificate: le stringhe.
 *
 * Attivazione (nessuna variabile d'ambiente, AGENTS §0): registro opzioni, voce
 * `debug.memory` = 1 — sia con `AI SET OPTION "debug.memory", "1"` sia dalla sezione
 * `engine` dei settings. Spenta = costo nullo (un solo `if` su un flag già in cache).
 *
 * Cosa si vede: a fine esecuzione (atexit) una riga per tipo
 *   [MEM] string: vivi 120 blocchi / 4.2 KB (picco 300 / 12.1 KB)
 * La crescita fra due run — o dentro un loop lungo — è la firma del leak. L'attribuzione
 * esatta con gli stack resta lo sweep ASan (tools/sh/check_gmbnative_memory.sh): questo
 * strumento dice *se* e *di che tipo* la memoria cresce, senza ricompilare.
 */
#ifndef GMB_MEMSTATS_H
#define GMB_MEMSTATS_H

#include <stddef.h>

typedef enum {
    GMB_MEM_STRING = 0,
    GMB_MEM_LIST = 1,
    GMB_MEM_DICT = 2,
    GMB_MEM_KIND_COUNT = 3
} gmb_mem_kind_t;

/* 1 se la contabilità è attiva (legge `debug.memory` dal registro opzioni, una volta). */
int gmb_mem_on(void);

/* Registra/aggiorna/toglie un blocco VIVO. `p` NULL = no-op. */
void gmb_mem_track(void* p, size_t bytes, int kind);
void gmb_mem_resize(void* oldp, void* newp, size_t new_bytes, int kind);
void gmb_mem_untrack(void* p);

/* Report su stderr (una riga per tipo con blocchi/byte vivi e picco). */
void gmb_mem_report(void);

#endif /* GMB_MEMSTATS_H */
