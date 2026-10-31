/* ============================================================================
 * gmb_pkg_advisories.h — audit (advisory) + SBOM del Package Manager (riferimento C).
 * Il C# lo consuma via P/Invoke.
 * ============================================================================ */
#ifndef GMB_PKG_ADVISORIES_H
#define GMB_PKG_ADVISORIES_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_audit gmb_pkg_audit_t;

/* Audit: incrocia i pacchetti (nome/versione) con gli advisory (JSON array
   di {id,package,range,severity,summary,fixed}). Ritorna un handle (mai NULL). */
gmb_pkg_audit_t* gmb_pkg_audit(int npkg, const char* const* names, const char* const* versions, const char* advisories_json);
int gmb_pkg_audit_count(gmb_pkg_audit_t* a);
/* i-esimo hit: copia i campi. 1/0. */
int gmb_pkg_audit_item(gmb_pkg_audit_t* a, int i,
                       char* name, size_t ncap, char* version, size_t vcap,
                       char* id, size_t icap, char* severity, size_t scap,
                       char* summary, size_t smcap, char* fixed, size_t fcap);
void gmb_pkg_audit_free(gmb_pkg_audit_t* a);

/* SBOM CycloneDX 1.5 dai pacchetti risolti (nome/versione/installDir): JSON in out. 1/0. */
int gmb_pkg_sbom(int npkg, const char* const* names, const char* const* versions,
                 const char* const* install_dirs, char* out, size_t cap);

#endif /* GMB_PKG_ADVISORIES_H */
