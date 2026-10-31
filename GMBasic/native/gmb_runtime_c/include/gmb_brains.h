/* ============================================================================
 * gmb_brains.h — cervelli ("Second Brain") come DATI DISTRIBUIBILI.
 *
 * Un cervello è una directory con i suoi file (`rag/<nome>.db`, `graphs/<nome>.json`,
 * `brain.manifest.json`) e si installa come un pacchetto: `contentType: "brain"`, asset di
 * release, **download on demand** con `gmb_fetch` (streaming + resume + sha).
 *
 * DOVE SI TROVA UN CERVELLO (ordine, identico ai pacchetti):
 *   1. `<progetto>/vendor/brains/<nome>/<versione>/`   (pin offline, accanto al programma)
 *   2. `<exe>/vendor/brains/…`                         (installazione locale d'app)
 *   3. `brainsDir` (default `<root>/brains`, `<exe>/brains` nel client)
 *   4. `~/.gmbasic/brains/<nome>/<versione>/`          (cache dei download)
 *
 * La risoluzione vive QUI, in C: gli agenti (C#) e i 4 backend la consumano dalla stessa
 * funzione — `gmb_brain_lookup` — così un cervello è trovato ovunque sia stato messo.
 *
 * FAIL-CLOSED: una sorgente che richiede autorizzazione (`source: "github-asset"`) senza token
 * è un errore ESPLICITO (GMB_BRAIN_E_UNAUTHORIZED), mai un "fatto" silenzioso.
 * ============================================================================ */
#ifndef GMB_BRAINS_H
#define GMB_BRAINS_H

#include "gmb_runtime.h"
#include "gmb_fetch.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GMB_BRAIN_OK = 0,
    GMB_BRAIN_E_ARG = 1,
    GMB_BRAIN_E_NOT_FOUND = 2,      /* non installato / non dichiarato nel manifest */
    GMB_BRAIN_E_NO_MANIFEST = 3,    /* brains-manifest.json assente                */
    GMB_BRAIN_E_UNAUTHORIZED = 4,   /* serve un token (fail-closed)                */
    GMB_BRAIN_E_FETCH = 5,          /* il download è fallito (dettaglio in err)    */
    GMB_BRAIN_E_EXTRACT = 6,        /* estrazione dello zip fallita               */
    GMB_BRAIN_E_INVALID = 7,        /* non è un cervello valido                   */
    GMB_BRAIN_E_IO = 8
};

/* Radici di ricerca, separate da ';' (in ordine di priorità). 1 se almeno una. */
int gmb_brains_search_path(char* out, size_t cap);

/* Directory di un cervello installato. `version` NULL/"" = la più alta disponibile.
 * 1 = trovato (out = percorso assoluto della directory). */
int gmb_brain_lookup(const char* name, const char* version, char* out, size_t cap);

/* Versioni installate (crescenti), separate da ';'. 1 se almeno una. */
int gmb_brain_versions(const char* name, char* out, size_t cap);

/* Tutti i cervelli trovati: "nome@versione;…" e il conteggio (opzionale). 1 se almeno uno. */
int gmb_brain_list(char* out, size_t cap, int* count);

/* Un cervello è "disponibile come RAG/graph" se ha i file attesi. 1/0. */
int gmb_brain_has_rag(const char* dir, const char* name);
int gmb_brain_has_graph(const char* dir, const char* name);
/* `rag` e `graph` in una stringa "rag,graph" (vuota se niente). */
int gmb_brain_modes(const char* dir, const char* name, char* out, size_t cap);

/* Percorso del manifest dei cervelli. 1 se esiste (altrimenti out = percorso canonico). */
int gmb_brains_manifest_path(char* out, size_t cap);

/* Cervelli DICHIARATI nel manifest: "nome@latest;…". -1 = manifest assente. */
int gmb_brains_available(char* out, size_t cap);
/* Come sopra, con un manifest ESPLICITO (test, manifest di terze parti). */
int gmb_brains_available_from(const char* manifest_path, char* out, size_t cap);

/* Installazione dal MANIFEST (F1: `source: "url"`; F2: `github-asset` + token).
 * Idempotente: se già presente ritorna OK e riempie out_dir. */
int gmb_brains_install(const char* name, const char* version, const char* token,
                       const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                       char* out_dir, size_t cap, char* err, size_t errcap);

/* Come sopra, ma con un manifest ESPLICITO (diagnostica, test, manifest di terze parti).
 * `manifest_path` NULL = quello risolto da `gmb_paths_brains_manifest`. */
int gmb_brains_install_from_manifest(const char* manifest_path, const char* name, const char* version,
                                     const char* token, const gmb_fetch_transport* tr,
                                     const gmb_fetch_opts* opts, char* out_dir, size_t cap,
                                     char* err, size_t errcap);

/* Installazione in una radice ESPLICITA: `<dest_root>/<nome>/<versione>` (es. `<exe>/brains` per
 * un programma distribuito, che la risoluzione consulta PRIMA della cache). `dest_root` NULL =
 * cache utente. */
int gmb_brains_install_to(const char* manifest_path, const char* name, const char* version,
                          const char* dest_root, const char* token,
                          const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                          char* out_dir, size_t cap, char* err, size_t errcap);

/* Indice `<exe>.brains.txt` (TSV `nome<TAB>versione`, # = commento): installa ogni voce in
 * `<dest_root>/brains/<nome>/<versione>`. `only` limita a un nome. Ritorna il numero installate
 * (0 = niente da fare), negativo = voci fallite. */
int gmb_brains_fetch_index(const char* index_path, const char* dest_root, const char* only,
                           const char* token, const gmb_fetch_transport* tr,
                           const gmb_fetch_opts* opts, char* err, size_t errcap);

/* Scrive l'indice leggendo i cervelli DICHIARATI dal programma (`"brains": ["nome"|"nome@ver"]`
 * nel suo app.settings.json); la versione, se assente, è `latest` del manifest. Nessuna
 * dichiarazione → nessun file (ritorna 0). Usata dal bundle (shell C99 e C#) e da
 * `gmb_brain_fetch --emit-index`. */
int gmb_brains_emit_index(const char* app_settings, const char* manifest_path, const char* out_path);

/* Installazione da una SORGENTE esplicita — semantica di `AI BRAIN INSTALL nome, sorgente`:
 * URL http(s) di uno zip `.gmbrain`, percorso di uno zip, oppure una directory già pronta. */
int gmb_brains_install_source(const char* name, const char* source,
                              const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                              char* out_dir, size_t cap, char* err, size_t errcap);

/* Rimuove un cervello dalla CACHE (mai da vendor/exe). 1 = rimosso. */
int gmb_brain_remove(const char* name, const char* version);

/* Messaggio leggibile per un codice GMB_BRAIN_E_*. */
const char* gmb_brain_strerror(int code);

/* Token GitHub per gli asset PRIVATI (`gmb brain login --github`).
 * Sorgenti, nell'ordine: `<configDir>/github.token`, `~/.gmbasic/github.token`.
 * Ritorna 1 se trovato e leggibile. Un file leggibile da gruppo/altri è RIFIUTATO
 * (fail-closed): un token con permessi larghi è un token compromesso. */
int gmb_brains_github_token(char* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_BRAINS_H */
