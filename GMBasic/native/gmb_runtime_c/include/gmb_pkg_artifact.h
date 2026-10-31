/* ============================================================================
 * gmb_pkg_artifact.h — selezione dell'artefatto di un pacchetto (riferimento C).
 * Dato installDir + backend/platform, sceglie binario/chunk/sorgente/ffi e classifica
 * il contentType (parità con PackageManager.SelectArtifact). Il C# lo consuma via P/Invoke.
 * Enum contentType: 0=Source 1=DotNet 2=VmChunk 3=CFFI 4=CFFISource 5=Mixed 6=DocsOnly.
 * ============================================================================ */
#ifndef GMB_PKG_ARTIFACT_H
#define GMB_PKG_ARTIFACT_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_artifact gmb_pkg_artifact_t;

/* Seleziona l'artefatto. NULL se il manifest non è leggibile. */
gmb_pkg_artifact_t* gmb_pkg_artifact_select(const char* install_dir, const char* backend, const char* platform);
void gmb_pkg_artifact_free(gmb_pkg_artifact_t* a);

int gmb_pkg_artifact_ct(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_source_dir(gmb_pkg_artifact_t* a);   /* NULL se assente */
const char* gmb_pkg_artifact_binary(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_chunk(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_ffi_link(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_declare(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_backend(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_platform(gmb_pkg_artifact_t* a);
int gmb_pkg_artifact_ffi_lib_count(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_ffi_lib(gmb_pkg_artifact_t* a, int i);
int gmb_pkg_artifact_ffi_header_count(gmb_pkg_artifact_t* a);
const char* gmb_pkg_artifact_ffi_header(gmb_pkg_artifact_t* a, int i);

/* Chiave pubblica di firma di un pacchetto installato: <install_dir>/.gmbpkg.pub (trim) o,
   se assente, la sezione signature.publicKey del manifest. Scrive in out. 1/0. */
int gmb_pkg_installed_public_key(const char* install_dir, char* out, size_t cap);

/* Path del binario scelto per backend+platform (parità PackageManager.TrySelectBinary).
   Scrive il path assoluto in out. 1/0. */
int gmb_pkg_binary_path(const char* install_dir, const char* backend, const char* platform, char* out, size_t cap);

/* Directory sorgente esportate (.bas) di un pacchetto (install_dir/<export> per ogni export),
   una per riga in out. Ritorna il numero di directory. */
int gmb_pkg_module_sources(const char* install_dir, char* out, size_t cap);

/* Flag di link C99 (come PackageManager.GetC99LinkInfo): dir degli header (-I), dir delle lib
 * (-L), e nomi lib (-l). Scrive tre CSV (deduplicati) in out_*. 1/0. */
int gmb_pkg_c99_link_info(int n, const char* const* names, const char* const* install_dirs,
                          const char* backend, const char* platform,
                          char* out_includes, size_t icap, char* out_libdirs, size_t lcap, char* out_libs, size_t bcap);

#endif /* GMB_PKG_ARTIFACT_H */
