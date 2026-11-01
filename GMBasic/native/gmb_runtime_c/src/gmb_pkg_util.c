/* gmb_pkg_util.c — utility del Package Manager (riferimento C). */
#include "gmb_pkg_util.h"
#include "gmb_pkg_manifest.h"
#include "gmb_semver.h"
#include <stdio.h>
#include <string.h>

int gmb_pkg_compat_check(const char* json, const char* edition, char* out_err, size_t cap)
{
    (void)edition;   /* ignorato: non esiste piu' la matrice Edition (ABI invariata) */
    if (out_err && cap) out_err[0] = 0;
    if (!json) return 1;
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(json);
    if (!m) return 1; /* non-oggetto: nessun gate (il resolver lo rifiuta altrove) */

    char name[256] = "", ver[64] = "", minv[32] = "";
    gmb_pkg_manifest_get(m, "name", name, sizeof(name));
    gmb_pkg_manifest_get(m, "version", ver, sizeof(ver));
    int has_min = gmb_pkg_manifest_get(m, "settings.minGmbVersion", minv, sizeof(minv)) && minv[0];
    gmb_pkg_manifest_free(m);

    int ok = 1;
    if (has_min) {
        gmb_semver_t e, mi;
        if (gmb_semver_parse(GMB_PKG_ENGINE_VERSION, &e) && gmb_semver_parse(minv, &mi) && gmb_semver_cmp(&e, &mi) < 0) {
            if (out_err) snprintf(out_err, cap, "'%s@%s' richiede GMBasic >= %s (engine %s).", name, ver, minv, GMB_PKG_ENGINE_VERSION);
            ok = 0;
        }
    }
    /* Nessun gate di edizione: non esiste piu' una matrice Edition. Resta solo il controllo
     * della versione minima dell'engine (settings.minGmbVersion). */
    return ok;
}

int gmb_pkg_safe_dir(const char* name, char* out, size_t cap)
{
    size_t j = 0;
    for (const char* p = (name ? name : ""); *p && j + 1 < cap; p++) out[j++] = (*p == '/' || *p == '\\') ? '_' : *p;
    if (cap) out[j] = 0;
    return 1;
}

int gmb_pkg_platform_key(char* out, size_t cap)
{
    const char* os =
#if defined(__APPLE__)
        "osx";
#elif defined(_WIN32)
        "win";
#elif defined(__linux__)
        "linux";
#else
        "unknown";
#endif
    const char* arch =
#if defined(__aarch64__) || defined(_M_ARM64)
        "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
        "x64";
#else
        "any";
#endif
    snprintf(out, cap, "%s-%s", os, arch);
    return 1;
}
