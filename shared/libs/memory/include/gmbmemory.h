/* gmbmemory.h — API della lib `memory` (shared/libs/memory).
 *
 * KV store su SQLite in-memory, usato dal namespace MEMORY via FFI.
 * Le stringhe di ritorno sono char* malloc'd (il bridge FFI le copia e libera).
 * SQLite è una libreria di piattaforma/supporto di GMBasic, non una dipendenza
 * da installare.
 */
#ifndef GMBMEMORY_H
#define GMBMEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Inserisce/sostituisce key → value (testo). Ritorna 1 se scritto, 0 altrimenti. */
int ofx_mem_store(const char* key, const char* value, int long_term);

/* Valore di key ("" se assente o se il valore non è una stringa). */
char* ofx_mem_load(const char* key);

/* Cancella key. Ritorna 1 se eliminata, 0 altrimenti. */
int ofx_mem_delete(const char* key);

/* Chiavi ordinate, separate da '\n' ("" se vuoto). */
char* ofx_mem_list(void);

/* Chiavi che contengono pattern (LIKE %pattern%, case-insensitive), ordinate,
 * separate da '\n'. */
char* ofx_mem_search(const char* pattern);

#ifdef __cplusplus
}
#endif

#endif /* GMBMEMORY_H */
