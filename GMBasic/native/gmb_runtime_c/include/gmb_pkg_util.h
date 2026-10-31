/* gmb_pkg_util.h — utility del Package Manager (riferimento C): gate di compatibilità
 * (minGmbVersion/requiresCommercial/edition), safe package dir, platform key. */
#ifndef GMB_PKG_UTIL_H
#define GMB_PKG_UTIL_H

#include "gmb_runtime.h"

/* Versione dell'engine GMBasic (gate minGmbVersion). */
#define GMB_PKG_ENGINE_VERSION "1.0.0"

/* Gate: 1 se il manifest è compatibile con `edition`; 0 + messaggio in out_err. */
int gmb_pkg_compat_check(const char* manifest_json, const char* edition, char* out_err, size_t cap);

/* Nome cartella sicuro: '/' e '\' → '_'. */
int gmb_pkg_safe_dir(const char* name, char* out, size_t cap);

/* Chiave piattaforma corrente (os-arch, es. "osx-arm64"). */
int gmb_pkg_platform_key(char* out, size_t cap);

#endif /* GMB_PKG_UTIL_H */
