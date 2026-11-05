/* gmb_cc_defaults.h — Ricetta UNICA per compilare e linkare il C emesso dal compilatore C.
 *
 * PERCHÉ ESISTE. Il C generato dall'emitter include gli header del runtime (`gmb_runtime.h`,
 * `gmb_collections.h`, …) e va linkato con le statiche del runtime: servono una lista di `-I`
 * e un lib-set. Quella ricetta era scritta DUE volte — nel CLI `gmb build` e, in forma
 * incompleta, nel driver `gmbnative` (che riceveva `-I`/`--lib` dal chiamante). Il sintomo
 * reale: `bin/gmbnative prog.bas --exe prog`, invocato dalla `<root>/bin`, moriva con
 *     fatal error: 'gmb_runtime.h' file not found
 * poi, aggiunti gli `-I`, con "Undefined symbols" — perché nessuno passava le statiche.
 *
 * Qui c'è UNA implementazione (AGENTS §0bis: niente copie). La usano:
 *   • il driver C99Native (default di compilazione/link, quando il chiamante non li passa);
 *   • il CLI `gmb build|run` (stessa lista, quindi stesso verdetto sullo stesso sorgente);
 *   • `gmb doctor`, che dichiara lo stato dell'ambiente con gli STESSI `-I` di `cc`.
 *
 * I path sono RISOLTI, non hardcoded: `install` viene da `gmb_resolve_install_root()`
 * (funziona anche per un tool invocato da `<monorepo>/bin` o dal PATH) e la root di LLMEnv da
 * `gmb_resolve_llm_root()` (repo separato, può stare altrove). Le cartelle che non esistono
 * NON vengono restituite: una `-I`/`-l` che punta al nulla produce un errore di `cc` che
 * accusa il programma invece dell'ambiente.
 */
#ifndef GMB_CC_DEFAULTS_H
#define GMB_CC_DEFAULTS_H

#include <stddef.h>

/* Capienza di un path di questa ricetta (uguale a CLI_PATH del CLI). */
#define GMB_CC_PATH 4096

/* Cartelle `-I` REALI (solo quelle esistenti), in ordine di priorità: runtime e domini
 * dell'install → LLMEnv (root risolta) → header delle terze parti (gruppo `dev` della cache).
 * Scrive al massimo `max` voci in `out` e ritorna quante ne ha scritte (0 se `install` è
 * vuota: il chiamante diagnostica, non inventa path). */
int gmb_cc_inc_dirs(const char* install, char (*out)[GMB_CC_PATH], int max);

/* 1 se il sorgente usa UI/grafica/AI/DB (quindi serve il runtime COMPLETO): parola chiave a
 * inizio riga, `IMPORT <namespace>` oppure `NAMESPACE.` dentro un'espressione. Non è un
 * parser: è una scelta PRUDENTE (in dubbio → completo). 1 anche se il file non si legge. */
int gmb_cc_source_needs_full_runtime(const char* entry);

/* Path della statica del runtime da linkare: la MINIMA (testuale, senza dipendenze esterne)
 * quando `need_full` è 0, altrimenti la COMPLETA. 1 se trovata (un archivio di pochi byte,
 * cioè una build interrotta, non è una libreria e viene scartato), 0 se assente. */
int gmb_cc_runtime_lib(const char* install, int need_full, char* out, size_t cap);

/* Statiche che il runtime referenzia e che vanno quindi presenti al link dell'eseguibile:
 * `libgmb_chart.a` (matrici/chart) e `libllmdesc.a` (policy ABI usata da `gmb_abi.o`).
 * 1 se trovate. */
int gmb_cc_chart_lib(const char* install, char* out, size_t cap);
int gmb_cc_llmdesc_lib(const char* install, char* out, size_t cap);

/* Cache delle librerie NATIVE del RID di build (`~/.gmbasic/libs/<rid>/`): è dove `cc`
 * trova le dylib versionate (zstd, brotli, pcre2, SDL…). 1 se la cartella esiste. */
int gmb_cc_cache_libdir(char* out, size_t cap);

#endif /* GMB_CC_DEFAULTS_H */
