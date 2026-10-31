/* gmb_inc_resolve.h — Risoluzione dei moduli BASIC (`*.bas`/`*.inc`) per un programma.
 *
 * PARITÀ con `Executor/Orchestrator.cs` (DiscoverSourceFiles + tokenizzazione
 * `IMPORT`-driven):
 *   • scansione RICORSIVA della radice del programma;
 *   • `importFolders` / `importFiles` dei settings (`system.settings.json` +
 *     `app.settings.json`), relativi al progetto e alla root dell'install;
 *   • cartelle di ricerca del compilatore (`compiler.searchFolders`, default
 *     `include`, `Controls`, `controls`, `{install}/include`, `{install}/Controls`,
 *     `{install}/libs/<nome>`) — con `{project}`/`{install}` come placeholder e `*` finale
 *     per espandere ogni sottocartella;
 *   • il grafo degli `IMPORT` è seguito su OGNI file caricato, non solo sulla entry:
 *     il testo usa i moduli in ordine di dipendenza (dipendenze prima del dipendente).
 * Condiviso dai driver C99Native e bytecode.
 *
 * O8 — mappa diagnostica C→BASIC: la risoluzione restituisce anche i SEGMENTI del testo
 * concatenato, così una riga del C emesso (e quindi un errore di `cc`, o di emissione)
 * può essere attribuita al `.bas`/`.inc` originale invece che alla riga del file unito.
 */
#ifndef GMB_INC_RESOLVE_H
#define GMB_INC_RESOLVE_H

#include <stddef.h>

/* Un sorgente contribuito al testo concatenato: `line` è la prima riga 1-based del
 * segmento nel testo unito; dentro il file si riparte sempre da riga 1 (i moduli sono
 * copiati verbatim). */
typedef struct {
    int line;
    char* file;   /* malloc; liberato da gmb_src_map_free */
} GMB_SRC_SEG;

typedef struct {
    GMB_SRC_SEG* segs;
    int n, cap;
} GMB_SRC_MAP;

void gmb_src_map_init(GMB_SRC_MAP* m);
void gmb_src_map_free(GMB_SRC_MAP* m);

/* Sposta tutti gli anchor di `delta` righe: serve quando dopo la risoluzione viene
 * preposto altro testo allo stesso buffer (es. gli export dei pacchetti REQUIRE). */
void gmb_src_map_shift(GMB_SRC_MAP* m, int delta);

/* Riga `line` (1-based) del testo concatenato → file originale e riga 1-based in quel
 * file. Ritorna 1 se trovata, 0 se la mappa è vuota (il chiamante ricade sul file di
 * ingresso). */
int gmb_src_map_locate(const GMB_SRC_MAP* m, int line, const char** file, int* file_line);

/* Ritorna una NUOVA stringa (malloc) = moduli preposti + src (o copia di src se nessuno). */
char* gmb_resolve_includes(const char* src, const char* input_path);

/* Come gmb_resolve_includes, riempiendo `map` (se non NULL) con i segmenti. */
char* gmb_resolve_includes_mapped(const char* src, const char* input_path, GMB_SRC_MAP* map);

/* Esclude `dir` (e tutto il suo sottoalbero) dalla DISCOVERY dei moduli IMPORT.
 * Da chiamare quando la cache/vendor dei pacchetti sta dentro la cartella dei sorgenti
 * (layout `vendor/`): i suoi `.inc` dichiarano `NAMESPACE X`, la discovery li prepone per
 * l'IMPORT, e il REQUIRE li ha già preposti → dichiarazioni duplicate. `NULL`/"" azzera. */
void gmb_inc_exclude_root(const char* dir);

/* Directory dell'ESEGUIBILE corrente (senza filename): ancora dell'installazione LOCALE dei
 * pacchetti (`<exe>/packages`, modello node_modules). 1 se risolta. */
int gmb_exe_dir(char* out, size_t cap);

/* Elenco (malloc) dei file risolti per `input_path`, nell'ORDINE di emissione, entry
 * esclusa. Serve per diagnostica e test della risoluzione senza passare dall'emitter.
 * Liberare con gmb_resolve_module_files_free. */
char** gmb_resolve_module_files(const char* input_path, int* out_n);
void gmb_resolve_module_files_free(char** files, int n);

/* ── progetto (O7) ───────────────────────────────────────────────────────────────────────
 * API usata dal CLI `gmb check/build/run --project`: la discovery resta UNA sola, quella
 * del compilatore (non una copia nel CLI). */

/* Tutti i candidati di un progetto (ricorsivi, più importFolders/importFiles/searchFolders),
 * esclusi `work/`, `bin/`, `obj/`, `.git/`, `.vs/`, `.build/`, `node_modules/`. Path assoluti
 * malloc: liberare con gmb_string_list_free. Ritorna il numero di file. */
int gmb_project_sources(const char* project_dir, char*** out_files, int* out_n);

/* Namespace dichiarati (`NAMESPACE X`) e namespace IMPORTati da un file; nomi in maiuscolo.
 * Ritornano quanti ne hanno trovati (≤ max). */
int gmb_file_namespaces(const char* path, char out[][64], int max);
int gmb_file_imports(const char* path, char out[][64], int max);

/* 1 se il nome è un namespace BUILTIN (`TRADING`, `CONSOLE`, …): un `IMPORT` verso di lui non
 * richiede un file, quindi non è un import irrisolto. */
int gmb_namespace_is_builtin(const char* ns);

/* Root dell'install GMBasic (GMBASIC.sln), come la usa il resolver. 0 se non trovata. */
int gmb_resolve_install_root(char* out, size_t cap);

void gmb_string_list_free(char** items, int n);

#endif /* GMB_INC_RESOLVE_H */
