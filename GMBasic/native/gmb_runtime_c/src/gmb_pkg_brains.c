/* gmb_pkg_brains.c — PONTE P/Invoke dei cervelli.
 *
 * La logica vive nel RUNTIME condiviso (`gmb_brains.c`): così i 4 backend (IL, PCodeVM, VM/C,
 * C99Native) usano la stessa risoluzione. Il C# però carica la dylib del Package Manager
 * (`libgmb_pkg`), quindi qui ci sono solo delegazioni sottili verso `gmb_brain_*`: è ciò che
 * rende i simboli visibili e stabili per il P/Invoke. Nessuna logica duplicata (AGENTS §0bis).
 */
#include "gmb_brains.h"

#include <stdio.h>
#include <string.h>

int gmb_pkg_brain_lookup(const char* name, const char* version, char* out, size_t cap)
{
    return gmb_brain_lookup(name, version, out, cap);
}

int gmb_pkg_brain_versions(const char* name, char* out, size_t cap)
{
    return gmb_brain_versions(name, out, cap);
}

int gmb_pkg_brain_list(char* out, size_t cap)
{
    int n = 0;
    gmb_brain_list(out, cap, &n);
    return n;
}

int gmb_pkg_brain_modes(const char* dir, const char* name, char* out, size_t cap)
{
    return gmb_brain_modes(dir, name, out, cap);
}

int gmb_pkg_brain_search_path(char* out, size_t cap)
{
    return gmb_brains_search_path(out, cap);
}

int gmb_pkg_brain_manifest_path(char* out, size_t cap)
{
    return gmb_brains_manifest_path(out, cap);
}

int gmb_pkg_brain_available(char* out, size_t cap)
{
    int n = gmb_brains_available(out, cap);
    return n < 0 ? 0 : n;
}

int gmb_pkg_brain_available_from(const char* manifest_path, char* out, size_t cap)
{
    int n = gmb_brains_available_from(manifest_path, out, cap);
    return n < 0 ? 0 : n;
}

/* Installazione dal manifest di sistema (nome + versione). 0 = ok, altrimenti GMB_BRAIN_E_*. */
int gmb_pkg_brain_install(const char* name, const char* version,
                          char* out, size_t out_cap, char* err, size_t err_cap)
{
    return gmb_brains_install(name, version, NULL, NULL, NULL, out, out_cap, err, err_cap);
}

/* Installazione da sorgente esplicita (URL/zip/directory): `AI BRAIN INSTALL nome, sorgente`. */
int gmb_pkg_brain_install_source(const char* name, const char* source,
                                 char* out, size_t out_cap, char* err, size_t err_cap)
{
    return gmb_brains_install_source(name, source, NULL, NULL, out, out_cap, err, err_cap);
}

int gmb_pkg_brain_remove(const char* name, const char* version)
{
    return gmb_brain_remove(name, version);
}

int gmb_pkg_brain_has_rag(const char* dir, const char* name)   { return gmb_brain_has_rag(dir, name); }
int gmb_pkg_brain_has_graph(const char* dir, const char* name) { return gmb_brain_has_graph(dir, name); }

int gmb_pkg_brain_emit_index(const char* app_settings, const char* manifest_path, const char* out_path)
{
    return gmb_brains_emit_index(app_settings, manifest_path, out_path);
}
