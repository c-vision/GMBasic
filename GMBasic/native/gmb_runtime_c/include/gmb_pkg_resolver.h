/* ============================================================================
 * gmb_pkg_resolver.h — RESOLVER del Package Manager (riferimento C, "tutto in C").
 * ABI a builder + getter (opaca, scalari/stringhe → P/Invoke semplice).
 *
 * BFS transitivi (+ opzionali + features), vincoli multipli (AND con "||"), overrides,
 * pin (--locked), pacchetti locali (workspace), cache+vendor, gate minGmbVersion.
 * Il C# lo consuma via P/Invoke.
 * ============================================================================ */
#ifndef GMB_PKG_RESOLVER_H
#define GMB_PKG_RESOLVER_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_resolve_ctx gmb_pkg_resolve_ctx_t;
typedef struct gmb_pkg_resolve_result gmb_pkg_resolve_result_t;

gmb_pkg_resolve_ctx_t* gmb_pkg_resolve_ctx_new(void);
void gmb_pkg_resolve_ctx_set(gmb_pkg_resolve_ctx_t* c, const char* cache_root, const char* vendor_dir,
                             const char* backend, const char* platform, const char* edition, int offline);
/* Registry HTTP (per dist-tag e per scaricare i pacchetti mancanti in cache). "" = nessuno. */
void gmb_pkg_resolve_ctx_set_registry(gmb_pkg_resolve_ctx_t* c, const char* registry_url);
/* Root di ricerca AGGIUNTIVA (installazione locale dell'app, `<dir dell'eseguibile>/packages`):
 * viene consultata PRIMA della cache (`cache_root` resta la destinazione dei download). */
void gmb_pkg_resolve_ctx_add_cache_root(gmb_pkg_resolve_ctx_t* c, const char* dir);
void gmb_pkg_resolve_ctx_add_req(gmb_pkg_resolve_ctx_t* c, const char* name, const char* range);
void gmb_pkg_resolve_ctx_add_override(gmb_pkg_resolve_ctx_t* c, const char* name, const char* range);
void gmb_pkg_resolve_ctx_add_feature(gmb_pkg_resolve_ctx_t* c, const char* f);
void gmb_pkg_resolve_ctx_add_locked(gmb_pkg_resolve_ctx_t* c, const char* name, const char* version);
void gmb_pkg_resolve_ctx_add_local(gmb_pkg_resolve_ctx_t* c, const char* name, const char* dir);
/* Registry per-scope: chiave "@org" o "org" → url dedicato per i pacchetti di quello scope. */
void gmb_pkg_resolve_ctx_add_registry_scope(gmb_pkg_resolve_ctx_t* c, const char* scope, const char* url);
void gmb_pkg_resolve_ctx_free(gmb_pkg_resolve_ctx_t* c);

/* Esegue la risoluzione; ritorna un handle al risultato (mai NULL). */
gmb_pkg_resolve_result_t* gmb_pkg_resolve(gmb_pkg_resolve_ctx_t* c);
int gmb_pkg_result_count(gmb_pkg_resolve_result_t* r);
const char* gmb_pkg_result_error(gmb_pkg_resolve_result_t* r); /* NULL se ok */
int gmb_pkg_result_item(gmb_pkg_resolve_result_t* r, int i,
                        char* name, size_t ncap, char* version, size_t vcap,
                        char* install_dir, size_t icap, char* lock_entry, size_t lcap);
void gmb_pkg_result_free(gmb_pkg_resolve_result_t* r);

#endif /* GMB_PKG_RESOLVER_H */
