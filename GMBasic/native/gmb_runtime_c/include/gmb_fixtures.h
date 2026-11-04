/* ============================================================================
 * gmb_fixtures.h — FIXTURE di mercato distribuite on demand.
 *
 * Le fixture grandi (markets, option_chains, rates, darkpool) NON stanno in git: sono asset
 * PUBBLICI della release `fixtures-v1` di c-vision/GMBasicModules e si scaricano con
 * `gmb fixtures pull` (motore gmbfetch: streaming + resume + sha256) in
 * `<progetto>/shared/samples/Fixtures/<nome>/` — il percorso che il codice legge SEMPRE
 * (AGENTS §11.4). Le fixture piccole restano versionate.
 *
 * Un checkout di sviluppo può tenerle su un disco esterno (symlink): se `<dest>/<nome>` risolve
 * a una cartella, il pull NON tocca nulla (solo `--force` la sostituisce).
 * ============================================================================ */
#ifndef GMB_FIXTURES_H
#define GMB_FIXTURES_H

#include "gmb_runtime.h"
#include "gmb_fetch.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GMB_FIX_OK = 0,
    GMB_FIX_E_ARG = 1,
    GMB_FIX_E_NO_MANIFEST = 2,
    GMB_FIX_E_NOT_FOUND = 3,      /* fixture non dichiarata nel manifest            */
    GMB_FIX_E_FETCH = 4,          /* download fallito (dettaglio in err)            */
    GMB_FIX_E_EXTRACT = 5,
    GMB_FIX_E_IO = 6
};

/* Manifest delle fixture (`shared/settings/fixtures-manifest.json`). 1 = esiste. */
int gmb_fixtures_manifest_path(char* out, size_t cap);

/* Cartella delle fixture del progetto (`<progetto>/shared/samples/Fixtures`). 1 = risolta. */
int gmb_fixtures_root(char* out, size_t cap);

/* Fixture DICHIARATE nel manifest: "nome@latest;…". -1 = manifest assente. */
int gmb_fixtures_available(char* out, size_t cap);

/* Fixture PRESENTI su disco (cartella risolta): "nome;…". 1 se almeno una. */
int gmb_fixtures_present(char* out, size_t cap);

/* Scarica + estrae una fixture in `<dest_root>/<nome>/`. `dest_root` NULL = gmb_fixtures_root().
 * `force` = sostituisce anche una cartella già presente. Ritorna GMB_FIX_*. */
int gmb_fixtures_pull(const char* name, const char* version, const char* dest_root, int force,
                      const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                      char* out_dir, size_t cap, char* err, size_t errcap);

/* Scarica le fixture MANCANTI dichiarate nel manifest (tutte, o `only_csv`), sempre in
 * `samples/Fixtures/<nome>/`. Ritorna il numero scaricate (0 = niente da fare), <0 = errori. */
int gmb_fixtures_ensure_all(const char* only_csv, const gmb_fetch_transport* tr,
                            const gmb_fetch_opts* opts, char* err, size_t errcap);

/* Fixture dichiarate dal PROGRAMMA (`"fixtures": ["markets"]` nel suo app.settings.json):
 * il compilatore/REPL le assicura prima di eseguire. 0 = nessuna dichiarazione. */
int gmb_fixtures_ensure_declared(const char* app_settings, const gmb_fetch_transport* tr,
                                 const gmb_fetch_opts* opts, char* err, size_t errcap);

/* Varianti con manifest e DESTINAZIONE espliciti (test, manifest di terze parti). */
int gmb_fixtures_ensure_all_from(const char* manifest_path, const char* dest_root, const char* only_csv,
                                 const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                                 char* err, size_t errcap);
int gmb_fixtures_ensure_declared_from(const char* manifest_path, const char* dest_root,
                                      const char* app_settings, const gmb_fetch_transport* tr,
                                      const gmb_fetch_opts* opts, char* err, size_t errcap);

/* Come sopra con un manifest ESPLICITO (test, manifest di terze parti). */
int gmb_fixtures_pull_from(const char* manifest_path, const char* name, const char* version,
                           const char* dest_root, int force, const gmb_fetch_transport* tr,
                           const gmb_fetch_opts* opts, char* out_dir, size_t cap,
                           char* err, size_t errcap);

const char* gmb_fixtures_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FIXTURES_H */
