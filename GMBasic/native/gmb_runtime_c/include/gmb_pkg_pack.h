/* ============================================================================
 * gmb_pkg_pack.h — creazione del .gmbpkg (riferimento C del Package Manager).
 * Elenca i file (manifest+exports+binaries+ffi+samples, con files/ignore) e li zippa.
 * Il C# lo consuma via P/Invoke.
 * ============================================================================ */
#ifndef GMB_PKG_PACK_H
#define GMB_PKG_PACK_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_pack_files gmb_pkg_pack_files_t;

/* Elenca i file (relativi) che `pack` includerebbe. backends_csv NULL/"all" = tutti.
   Ritorna NULL se il manifest è illeggibile. */
gmb_pkg_pack_files_t* gmb_pkg_pack_files(const char* dir, const char* backends_csv);
int gmb_pkg_pack_files_count(gmb_pkg_pack_files_t* f);
const char* gmb_pkg_pack_files_get(gmb_pkg_pack_files_t* f, int i);
void gmb_pkg_pack_files_free(gmb_pkg_pack_files_t* f);

/* Crea il .gmbpkg (zip dei file). hmac_key != NULL/"" → firma HMAC-SHA256 del manifest
   (.gmbpkg.sig dentro lo zip). 1/0. */
int gmb_pkg_pack(const char* dir, const char* out_file, const char* hmac_key);

#endif /* GMB_PKG_PACK_H */
