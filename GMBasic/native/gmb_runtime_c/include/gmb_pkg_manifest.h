/* ============================================================================
 * gmb_pkg_manifest.h — parser del manifest di pacchetto (gmb-package.json).
 * Riferimento C del Package Manager (§"il backend di riferimento è C"); il C#
 * lo CONSUMA via P/Invoke (NativePkg), non tiene una copia della logica.
 *
 * ABI compatta e generica basata su PATH (segmenti separati da '.'):
 *   gmb_pkg_manifest_get(m, "name", ...)          scalar
 *   gmb_pkg_manifest_get(m, "ffi.api", ...)        annidato
 *   gmb_pkg_manifest_get(m, "binaries.c99.osx-arm64", ...)
 *   gmb_pkg_manifest_count(m, "dependencies")      # elementi (array o mappa)
 *   gmb_pkg_manifest_item(m, "dependencies", i, key, val)
 * Copre TUTTI i campi senza un getter per campo.
 * ============================================================================ */
#ifndef GMB_PKG_MANIFEST_H
#define GMB_PKG_MANIFEST_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_manifest gmb_pkg_manifest_t;

/* Parsa gmb-package.json. Ritorna NULL se il JSON non è un oggetto. */
gmb_pkg_manifest_t* gmb_pkg_manifest_parse(const char* json);
void gmb_pkg_manifest_free(gmb_pkg_manifest_t* m);

/* Copia il valore scalare al path (stringa/numero/bool). Ritorna 1 se trovato, 0 altrimenti. */
int gmb_pkg_manifest_get(gmb_pkg_manifest_t* m, const char* path, char* out, size_t cap);

/* Numero al path (via get + atof/to_string). Ritorna 1 se trovato (o flag found opzionale). */
int gmb_pkg_manifest_get_num(gmb_pkg_manifest_t* m, const char* path, double* out);

/* Conteggio elementi (array o oggetto) al path; -1 se assente o non-collezione. */
int gmb_pkg_manifest_count(gmb_pkg_manifest_t* m, const char* path);

/* i-esimo elemento della collezione al path: per un array key = indice ("0"...), per un oggetto
 * key = chiave; val = valore scalare ("" se oggetto/array annidato, da materializzare via path
 * più profondo). Ritorna 1 se l'elemento esiste, 0 altrimenti. */
int gmb_pkg_manifest_item(gmb_pkg_manifest_t* m, const char* path, int i,
                          char* key, size_t kcap, char* val, size_t vcap);

/* 1 se il path esiste (qualunque tipo), 0 altrimenti. */
int gmb_pkg_manifest_has(gmb_pkg_manifest_t* m, const char* path);

/* 1 se il valore al path è una lista (array JSON), 0 altrimenti. */
int gmb_pkg_manifest_is_list(gmb_pkg_manifest_t* m, const char* path);

/* Edit del manifest su disco: aggiunge/aggiorna una dipendenza in `section`
   ("dependencies"|"devDependencies"|...), o la rimuove da dependencies E devDependencies.
   Riscrive il file (JSON canonico del runtime). 1/0. */
int gmb_pkg_manifest_set_dep(const char* manifest_path, const char* section, const char* name, const char* spec);
int gmb_pkg_manifest_remove_dep(const char* manifest_path, const char* name);

#endif /* GMB_PKG_MANIFEST_H */
