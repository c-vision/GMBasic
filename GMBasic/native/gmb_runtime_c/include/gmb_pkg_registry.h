/* ============================================================================
 * gmb_pkg_registry.h — client del registry (HTTP) del Package Manager (riferimento C).
 * Usa gmb_net_http_get (testo) / gmb_net_http_request (binario, download).
 * Il C# lo consuma via P/Invoke.
 * ============================================================================ */
#ifndef GMB_PKG_REGISTRY_H
#define GMB_PKG_REGISTRY_H

#include "gmb_runtime.h"

/* GET testuale; copia il corpo in out (cap). 1/0. */
int gmb_pkg_registry_get(const char* url, char* out, size_t cap);

/* POST JSON autenticato (Bearer). Scrive il corpo risposta in out (cap) + lo status in out_status.
   Ritorna 1 se 2xx, 0 altrimenti (il corpo d'errore e' comunque in out). */
int gmb_pkg_registry_post(const char* url, const char* json_body, const char* bearer,
                          char* out, size_t cap, int* out_status);

/* GET autenticato (Bearer). Come sopra. */
int gmb_pkg_registry_get_auth(const char* url, const char* bearer,
                              char* out, size_t cap, int* out_status);

/* Dettaglio del pacchetto (dal JSON): numero versioni + i-esima. */
typedef struct gmb_pkg_registry_detail gmb_pkg_registry_detail_t;
gmb_pkg_registry_detail_t* gmb_pkg_registry_detail_from_json(const char* json); /* per test/offline */
gmb_pkg_registry_detail_t* gmb_pkg_registry_detail(const char* base_url, const char* name); /* GET /api/packages/{name} */
int gmb_pkg_registry_detail_count(gmb_pkg_registry_detail_t* d);
int gmb_pkg_registry_detail_version(gmb_pkg_registry_detail_t* d, int i, char* out, size_t cap);
void gmb_pkg_registry_detail_free(gmb_pkg_registry_detail_t* d);

/* Versione del dist-tag (GET /api/packages/{name}/tags). 1/0. */
int gmb_pkg_registry_tag(const char* base_url, const char* name, const char* tag, char* out, size_t cap);

/* Abilita/disabilita la barra di avanzamento (su stderr) durante i download. */
void gmb_pkg_registry_set_progress(int on);

/* Scarica <base>/download/<name>/<version><qs> in dest_path (binario, streaming). 1/0. */
int gmb_pkg_registry_download(const char* base_url, const char* name, const char* version, const char* qs, const char* dest_path);

#endif /* GMB_PKG_REGISTRY_H */
