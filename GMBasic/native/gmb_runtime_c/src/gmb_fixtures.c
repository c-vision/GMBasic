/* gmb_fixtures.c — fixture di mercato on demand (manifest + gmbfetch + estrazione).
 *
 * Le fixture sono DATI DI SVILUPPO: servono al compilatore e ai test, non al programma
 * distribuito (che non porta `samples/`). Per questo vivono in `shared/samples/Fixtures/<nome>/`
 * e si scaricano con un comando esplicito, non a runtime.
 */

#include "gmb_fixtures.h"
#include "gmb_pkg_manifest.h"
#include "gmb_compress.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include "gmb_compat_win.h"   /* lstat (solo su Windows) */

#define FX_PATH 4096

const char* gmb_fixtures_strerror(int code)
{
    switch (code) {
    case GMB_FIX_OK:            return "ok";
    case GMB_FIX_E_ARG:         return "argomenti non validi";
    case GMB_FIX_E_NO_MANIFEST: return "fixtures-manifest.json non trovato";
    case GMB_FIX_E_NOT_FOUND:   return "fixture non dichiarata nel manifest";
    case GMB_FIX_E_FETCH:       return "download della fixture fallito";
    case GMB_FIX_E_EXTRACT:     return "estrazione della fixture fallita";
    case GMB_FIX_E_IO:          return "errore di I/O";
    default:                    return "errore sconosciuto";
    }
}

/* Manifest: `paths.fixturesManifest`, poi accanto all'eseguibile, poi in shared/settings del
 * monorepo (la root del runtime è <monorepo>/GMBasic), poi in shared/resources del bundle. */
int gmb_fixtures_manifest_path(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = '\0';
    char root[FX_PATH] = "", exe[FX_PATH] = "";
    (void)gmb_paths_repo_root(root, sizeof root);
    (void)gmb_paths_exe_dir(exe, sizeof exe);
    char cand[6][FX_PATH * 2];
    snprintf(cand[0], sizeof cand[0], "%s/settings/fixtures-manifest.json", exe);
    snprintf(cand[1], sizeof cand[1], "%s/../shared/settings/fixtures-manifest.json", root);
    snprintf(cand[2], sizeof cand[2], "%s/shared/settings/fixtures-manifest.json", root);
    snprintf(cand[3], sizeof cand[3], "%s/Resources/fixtures-manifest.json", exe);
    snprintf(cand[4], sizeof cand[4], "%s/../shared/resources/fixtures-manifest.json", root);
    for (int i = 0; i < 5; i++) {
        if (cand[i][0] != '/') continue;
        if (gmb_fs_isfile(cand[i])) { snprintf(out, cap, "%s", cand[i]); return 1; }
    }
    snprintf(out, cap, "%s", cand[1]);
    return 0;
}

int gmb_fixtures_root(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = '\0';
    char root[FX_PATH] = "";
    if (gmb_paths_repo_root(root, sizeof root) && root[0]) {
        /* monorepo: <monorepo>/GMBasic → <monorepo>/shared/samples/Fixtures */
        char p[FX_PATH * 2];
        snprintf(p, sizeof p, "%s/../shared/samples/Fixtures", root);
        if (gmb_fs_isdir(p)) { snprintf(out, cap, "%s", p); return 1; }
        snprintf(p, sizeof p, "%s/samples/Fixtures", root);
        if (gmb_fs_isdir(p)) { snprintf(out, cap, "%s", p); return 1; }
        snprintf(out, cap, "%s/../shared/samples/Fixtures", root);
        return 1;
    }
    snprintf(out, cap, "samples/Fixtures");
    return 0;
}

static gmb_pkg_manifest_t* fx_manifest(const char* path_in, char* err, size_t errcap)
{
    char path[FX_PATH];
    if (path_in && *path_in) snprintf(path, sizeof path, "%s", path_in);
    else if (!gmb_fixtures_manifest_path(path, sizeof path) || !gmb_fs_isfile(path)) {
        if (err && errcap) snprintf(err, errcap, "fixtures-manifest.json non trovato (%s)", path);
        return NULL;
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        if (err && errcap) snprintf(err, errcap, "impossibile leggere %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char* js = (char*)malloc((size_t)n + 1);
    if (!js) { fclose(f); return NULL; }
    size_t rd = fread(js, 1, (size_t)n, f);
    fclose(f);
    js[rd] = '\0';
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
    free(js);
    if (!m && err && errcap) snprintf(err, errcap, "JSON non valido in %s", path);
    return m;
}

int gmb_fixtures_available(char* out, size_t cap)
{
    if (out && cap) out[0] = '\0';
    gmb_pkg_manifest_t* m = fx_manifest(NULL, NULL, 0);
    if (!m) return -1;
    int n = gmb_pkg_manifest_count(m, "fixtures");
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        char p[128], name[256] = "", latest[128] = "";
        snprintf(p, sizeof p, "fixtures.%d.name", i);
        gmb_pkg_manifest_get(m, p, name, sizeof name);
        snprintf(p, sizeof p, "fixtures.%d.latest", i);
        gmb_pkg_manifest_get(m, p, latest, sizeof latest);
        if (!name[0]) continue;
        int w = snprintf(out + used, cap - used, "%s%s@%s", used ? ";" : "", name, latest);
        if (w <= 0 || used + (size_t)w >= cap) break;
        used += (size_t)w;
    }
    gmb_pkg_manifest_free(m);
    return (int)used;
}

int gmb_fixtures_present(char* out, size_t cap)
{
    if (out && cap) out[0] = '\0';
    char root[FX_PATH];
    if (!gmb_fixtures_root(root, sizeof root) || !gmb_fs_isdir(root)) return 0;
    gmb_pkg_manifest_t* m = fx_manifest(NULL, NULL, 0);
    if (!m) return 0;
    int n = gmb_pkg_manifest_count(m, "fixtures");
    size_t used = 0;
    int found = 0;
    for (int i = 0; i < n; i++) {
        char p[128], name[256] = "";
        snprintf(p, sizeof p, "fixtures.%d.name", i);
        gmb_pkg_manifest_get(m, p, name, sizeof name);
        if (!name[0]) continue;
        char dir[FX_PATH * 2];
        snprintf(dir, sizeof dir, "%s/%s", root, name);
        if (!gmb_fs_isdir(dir)) continue;
        int w = snprintf(out + used, cap - used, "%s%s", used ? ";" : "", name);
        if (w <= 0 || used + (size_t)w >= cap) break;
        used += (size_t)w;
        found++;
    }
    gmb_pkg_manifest_free(m);
    return found;
}

static int fx_pull(const char* manifest_path, const char* name, const char* version,
                   const char* dest_root, int force, const gmb_fetch_transport* tr,
                   const gmb_fetch_opts* opts, char* out_dir, size_t cap, char* err, size_t errcap)
{
    if (out_dir && cap) out_dir[0] = '\0';
    if (err && errcap) err[0] = '\0';
    if (!name || !*name) return GMB_FIX_E_ARG;

    char root[FX_PATH];
    if (dest_root && *dest_root) snprintf(root, sizeof root, "%s", dest_root);
    else if (!gmb_fixtures_root(root, sizeof root) || !root[0]) return GMB_FIX_E_ARG;

    char final[FX_PATH * 2];
    snprintf(final, sizeof final, "%s/%s", root, name);

    /* Già disponibile (anche via symlink a un disco esterno): non si tocca — a meno che non si
     * chieda una versione DIVERSA da quella installata (marcatore `.fixture-version`, scritto
     * all'installazione). Un symlink a un disco esterno non ha il marcatore: si assume "presente". */
    if (gmb_fs_isdir(final) && !force) {
        int same = 1;
        if (version && *version) {
            char mark[FX_PATH * 2 + 32];
            snprintf(mark, sizeof mark, "%s/.fixture-version", final);
            FILE* mf = fopen(mark, "rb");
            if (mf) {
                char have[128] = "";
                size_t r = fread(have, 1, sizeof have - 1, mf);
                fclose(mf);
                have[r] = '\0';
                while (r > 0 && (have[r - 1] == '\n' || have[r - 1] == '\r' || have[r - 1] == ' ')) have[--r] = '\0';
                if (have[0] && strcmp(have, version) != 0) same = 0;
            }
        }
        if (same) {
            if (out_dir && cap) snprintf(out_dir, cap, "%s", final);
            return GMB_FIX_OK;
        }
    }

    gmb_pkg_manifest_t* m = fx_manifest(manifest_path, err, errcap);
    if (!m) return GMB_FIX_E_NO_MANIFEST;

    int nf = gmb_pkg_manifest_count(m, "fixtures");
    int fi = -1;
    for (int i = 0; i < nf; i++) {
        char p[128], v[256] = "";
        snprintf(p, sizeof p, "fixtures.%d.name", i);
        if (gmb_pkg_manifest_get(m, p, v, sizeof v) &&
            strlen(v) == strlen(name) && strncasecmp(v, name, strlen(name)) == 0) { fi = i; break; }
    }
    if (fi < 0) {
        gmb_pkg_manifest_free(m);
        if (err && errcap) snprintf(err, errcap, "la fixture \"%s\" non è dichiarata nel manifest", name);
        return GMB_FIX_E_NOT_FOUND;
    }

    char want[128] = "", url[2048] = "", sha[128] = "", src[32] = "";
    double size = 0;
    if (version && *version) snprintf(want, sizeof want, "%s", version);
    else {
        char p[160];
        snprintf(p, sizeof p, "fixtures.%d.latest", fi);
        gmb_pkg_manifest_get(m, p, want, sizeof want);
    }
    if (!want[0]) {
        gmb_pkg_manifest_free(m);
        if (err && errcap) snprintf(err, errcap, "nessuna versione dichiarata per \"%s\"", name);
        return GMB_FIX_E_NOT_FOUND;
    }
    char vpath[160];
    snprintf(vpath, sizeof vpath, "fixtures.%d.versions", fi);
    int nv = gmb_pkg_manifest_count(m, vpath);
    int vi = -1;
    for (int j = 0; j < nv; j++) {
        char p[192], v[128] = "";
        snprintf(p, sizeof p, "fixtures.%d.versions.%d.version", fi, j);
        if (gmb_pkg_manifest_get(m, p, v, sizeof v) && strcmp(v, want) == 0) { vi = j; break; }
    }
    if (vi < 0) {
        gmb_pkg_manifest_free(m);
        if (err && errcap) snprintf(err, errcap, "versione \"%s\" non dichiarata per \"%s\"", want, name);
        return GMB_FIX_E_NOT_FOUND;
    }
    char p[192];
    snprintf(p, sizeof p, "fixtures.%d.versions.%d.url", fi, vi);
    gmb_pkg_manifest_get(m, p, url, sizeof url);
    snprintf(p, sizeof p, "fixtures.%d.versions.%d.sha256", fi, vi);
    gmb_pkg_manifest_get(m, p, sha, sizeof sha);
    snprintf(p, sizeof p, "fixtures.%d.versions.%d.source", fi, vi);
    gmb_pkg_manifest_get(m, p, src, sizeof src);
    snprintf(p, sizeof p, "fixtures.%d.versions.%d.size", fi, vi);
    gmb_pkg_manifest_get_num(m, p, &size);
    gmb_pkg_manifest_free(m);

    if (!src[0]) snprintf(src, sizeof src, "url");
    if (strcasecmp(src, "url") != 0) {
        if (err && errcap) snprintf(err, errcap, "sorgente \"%s\" non supportata", src);
        return GMB_FIX_E_ARG;
    }
    if (url[0] == '\0') {
        if (err && errcap) snprintf(err, errcap, "voce senza url per \"%s@%s\"", name, want);
        return GMB_FIX_E_ARG;
    }

    const gmb_fetch_transport* use = tr ? tr : gmb_fetch_transport_curl();
    if (!use) {
        if (err && errcap) snprintf(err, errcap, "nessun trasporto HTTP disponibile (libcurl assente)");
        return GMB_FIX_E_FETCH;
    }
    gmb_fetch_opts defo;
    if (!opts) { gmb_fetch_opts_default(&defo); opts = &defo; }

    gmb_fs_mkdir_p(root);
    char zip[FX_PATH * 2];
    snprintf(zip, sizeof zip, "%s/.%s-%s.zip", root, name, want);

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = zip;
    rq.sha256 = sha[0] ? sha : NULL;
    rq.size = (long long)size;
    int rc = gmb_fetch_to_file(&rq, opts, use, err, errcap);
    if (rc != GMB_FETCH_OK) {
        if (err && errcap && !err[0]) snprintf(err, errcap, "%s", gmb_fetch_strerror(rc));
        return GMB_FIX_E_FETCH;
    }

    /* Staging accanto alla destinazione (stesso filesystem → rename atomico). */
    char staging[FX_PATH * 2 + 32];
    snprintf(staging, sizeof staging, "%s/.staging-%s-%d", root, name, (int)getpid());
    gmb_fs_rmtree(staging);
    gmb_fs_mkdir_p(staging);
    if (gmb_unzip_files(zip, staging) != 0) {
        gmb_fs_rmtree(staging);
        remove(zip);
        if (err && errcap) snprintf(err, errcap, "estrazione di %s fallita", zip);
        return GMB_FIX_E_EXTRACT;
    }
    /* se la vecchia destinazione esiste (symlink rotto o cartella con --force) si toglie:
     * su un symlink si rimuove il LINK, mai il contenuto del disco esterno. */
    if (gmb_fs_isdir(final) || gmb_fs_isfile(final)) gmb_fs_rmtree(final);
    else {
        struct stat st;
        if (lstat(final, &st) == 0) remove(final);   /* symlink rotto */
    }
    if (rename(staging, final) != 0) {
        gmb_fs_rmtree(staging);
        remove(zip);
        if (err && errcap) snprintf(err, errcap, "rename verso %s fallito", final);
        return GMB_FIX_E_IO;
    }
    remove(zip);
    /* marcatore di versione: permette di distinguere "già presente" da "versione diversa". */
    {
        char mark[FX_PATH * 2 + 32];
        snprintf(mark, sizeof mark, "%s/.fixture-version", final);
        FILE* mf = fopen(mark, "wb");
        if (mf) { fprintf(mf, "%s\n", want); fclose(mf); }
    }
    if (out_dir && cap) snprintf(out_dir, cap, "%s", final);
    return GMB_FIX_OK;
}

int gmb_fixtures_pull(const char* name, const char* version, const char* dest_root, int force,
                      const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                      char* out_dir, size_t cap, char* err, size_t errcap)
{
    return fx_pull(NULL, name, version, dest_root, force, tr, opts, out_dir, cap, err, errcap);
}

int gmb_fixtures_pull_from(const char* manifest_path, const char* name, const char* version,
                           const char* dest_root, int force, const gmb_fetch_transport* tr,
                           const gmb_fetch_opts* opts, char* out_dir, size_t cap,
                           char* err, size_t errcap)
{
    return fx_pull(manifest_path, name, version, dest_root, force, tr, opts, out_dir, cap, err, errcap);
}

/* ── ensure: le fixture che MANCANO si scaricano, sempre e solo in samples/Fixtures ───────── */

/* Scarica le fixture dichiarate nel manifest che non sono presenti. `only_csv` (opzionale)
 * limita a quei nomi. Ritorna il numero scaricate (0 = niente da fare), negativo = voci fallite. */
static int fx_ensure_all(const char* manifest_path, const char* dest_root, const char* only_csv,
                         const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                         char* err, size_t errcap)
{
    if (err && errcap) err[0] = '\0';
    char avail[4096];
    int n;
    {
        /* elenco dal manifest (esplicito o di sistema) */
        gmb_pkg_manifest_t* m = fx_manifest(manifest_path, err, errcap);
        if (!m) return -1;
        int cnt = gmb_pkg_manifest_count(m, "fixtures");
        size_t used = 0;
        avail[0] = '\0';
        for (int i = 0; i < cnt; i++) {
            char p2[128], nm[256] = "", lt[128] = "";
            snprintf(p2, sizeof p2, "fixtures.%d.name", i);
            gmb_pkg_manifest_get(m, p2, nm, sizeof nm);
            snprintf(p2, sizeof p2, "fixtures.%d.latest", i);
            gmb_pkg_manifest_get(m, p2, lt, sizeof lt);
            if (!nm[0]) continue;
            int w = snprintf(avail + used, sizeof avail - used, "%s%s@%s", used ? ";" : "", nm, lt);
            if (w <= 0 || used + (size_t)w >= sizeof avail) break;
            used += (size_t)w;
        }
        gmb_pkg_manifest_free(m);
        n = (int)used;
    }
    if (n < 0) {
        if (err && errcap) snprintf(err, errcap, "%s", gmb_fixtures_strerror(GMB_FIX_E_NO_MANIFEST));
        return -1;
    }
    char root[FX_PATH];
    if (dest_root && *dest_root) snprintf(root, sizeof root, "%s", dest_root);
    else gmb_fixtures_root(root, sizeof root);
    int done = 0, failed = 0;
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", avail);
    char* save = NULL;
    for (char* tok = strtok_r(buf, ";", &save); tok; tok = strtok_r(NULL, ";", &save)) {
        char name[256];
        snprintf(name, sizeof name, "%s", tok);
        char* at = strchr(name, '@');
        if (at) *at = '\0';
        if (!name[0]) continue;
        if (only_csv && *only_csv) {
            char needle[300];
            snprintf(needle, sizeof needle, ",%s,", name);
            char hay[1024];
            snprintf(hay, sizeof hay, ",%s,", only_csv);
            if (strstr(hay, needle) == NULL) continue;
        }
        char path[FX_PATH * 2];
        snprintf(path, sizeof path, "%s/%s", root, name);
        if (gmb_fs_isdir(path)) continue;            /* già presente: nessun download */
        char out[FX_PATH * 2], lerr[512] = "";
        int rc = fx_pull(manifest_path, name, NULL, dest_root, 0, tr, opts, out, sizeof out, lerr, sizeof lerr);
        if (rc == GMB_FIX_OK) done++;
        else {
            failed++;
            if (err && errcap && !err[0])
                snprintf(err, errcap, "%s: %s%s%s", name, gmb_fixtures_strerror(rc),
                         lerr[0] ? " — " : "", lerr);
        }
    }
    if (failed) return -failed;
    return done;
}

/* Fixture DICHIARATE dal programma (`"fixtures": ["markets", ...]` nel suo app.settings.json):
 * è il contratto con cui il compilatore/REPL sa cosa gli serve prima di eseguire. */
static int fx_ensure_declared(const char* manifest_path, const char* dest_root,
                             const char* app_settings, const gmb_fetch_transport* tr,
                             const gmb_fetch_opts* opts, char* err, size_t errcap)
{
    if (err && errcap) err[0] = '\0';
    if (!app_settings || !*app_settings || !gmb_fs_isfile(app_settings)) return 0;   /* niente da fare */
    FILE* f = fopen(app_settings, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    char* js = (char*)malloc((size_t)n + 1);
    if (!js) { fclose(f); return GMB_FIX_E_IO; }
    size_t rd = fread(js, 1, (size_t)n, f);
    fclose(f);
    js[rd] = '\0';
    gmb_val_t root = gmb_json_parse(js);
    free(js);
    if (root.kind != GMB_TYPE_OBJECT) { gmb_val_release(root); return 0; }
    gmb_val_t arr = gmb_val_dict_get(root, "fixtures");
    int is_list = (arr.kind == GMB_TYPE_OBJECT && gmb_obj_type(arr) != NULL &&
                   strstr(gmb_obj_type(arr), "dict") == NULL);
    if (!is_list) { gmb_val_release(root); return 0; }   /* nessuna dichiarazione */
    gmb_list_t l = gmb_val_as_list(arr);
    int cnt = (int)gmb_list_count(l);
    int done = 0, failed = 0;
    for (int i = 0; i < cnt; i++) {
        gmb_val_t v = gmb_list_get(l, i);
        if (v.kind != GMB_TYPE_STRING || !v.v_string.data) continue;
        const char* name = v.v_string.data;
        char rootdir[FX_PATH], path[FX_PATH * 2];
        if (dest_root && *dest_root) snprintf(rootdir, sizeof rootdir, "%s", dest_root);
        else gmb_fixtures_root(rootdir, sizeof rootdir);
        snprintf(path, sizeof path, "%s/%s", rootdir, name);
        if (gmb_fs_isdir(path)) continue;
        char out[FX_PATH * 2], lerr[512] = "";
        int rc = fx_pull(manifest_path, name, NULL, dest_root, 0, tr, opts, out, sizeof out, lerr, sizeof lerr);
        if (rc == GMB_FIX_OK) done++;
        else {
            failed++;
            if (err && errcap && !err[0])
                snprintf(err, errcap, "%s: %s%s%s", name, gmb_fixtures_strerror(rc),
                         lerr[0] ? " — " : "", lerr);
        }
    }
    gmb_val_release(root);
    if (failed) return -failed;
    return done;
}

int gmb_fixtures_ensure_all(const char* only_csv, const gmb_fetch_transport* tr,
                            const gmb_fetch_opts* opts, char* err, size_t errcap)
{
    return fx_ensure_all(NULL, NULL, only_csv, tr, opts, err, errcap);
}

int gmb_fixtures_ensure_declared(const char* app_settings, const gmb_fetch_transport* tr,
                                 const gmb_fetch_opts* opts, char* err, size_t errcap)
{
    return fx_ensure_declared(NULL, NULL, app_settings, tr, opts, err, errcap);
}

int gmb_fixtures_ensure_all_from(const char* manifest_path, const char* dest_root, const char* only_csv,
                                 const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                                 char* err, size_t errcap)
{
    return fx_ensure_all(manifest_path, dest_root, only_csv, tr, opts, err, errcap);
}

int gmb_fixtures_ensure_declared_from(const char* manifest_path, const char* dest_root,
                                      const char* app_settings, const gmb_fetch_transport* tr,
                                      const gmb_fetch_opts* opts, char* err, size_t errcap)
{
    return fx_ensure_declared(manifest_path, dest_root, app_settings, tr, opts, err, errcap);
}
