/* gmb_brains.c — risoluzione e installazione dei cervelli ("Second Brain") come DATI.
 *
 * UN CERVELLO È UN PACCHETTO DI DATI. Vive in una directory (`rag/<nome>.db`,
 * `graphs/<nome>.json`, `brain.manifest.json`) e si distribuisce come asset di release
 * (`contentType: "brain"`), scaricato con `gmb_fetch` (streaming + resume + sha).
 *
 * PERCHÉ LA RISOLUZIONE STA QUI E NON NEL C#: gli agenti (C#) e i 4 backend (IL, PCodeVM,
 * VM/C, C99Native) devono vedere GLI STESSI cervelli. Una sola funzione — `gmb_brain_lookup` —
 * con un solo ordine di ricerca (vendor → exe → brainsDir → cache), e tutti la usano: un
 * cervello scaricato dal package manager si trova anche se il programma è stato distribuito
 * senza compilatore.
 *
 * FAIL-CLOSED: una sorgente che richiede autorizzazione senza token è un errore esplicito.
 */

#include "gmb_brains.h"
#include "gmb_commonlib.h"
#include "gmb_pkg_manifest.h"
#include "gmb_compress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BR_PATH 4096
#define BR_MAX_ROOTS 5

const char* gmb_brain_strerror(int code)
{
    switch (code) {
    case GMB_BRAIN_OK:              return "ok";
    case GMB_BRAIN_E_ARG:           return "argomenti non validi";
    case GMB_BRAIN_E_NOT_FOUND:     return "cervello non trovato";
    case GMB_BRAIN_E_NO_MANIFEST:   return "brains-manifest.json non trovato";
    case GMB_BRAIN_E_UNAUTHORIZED:  return "sorgente protetta: serve un token (accesso negato)";
    case GMB_BRAIN_E_FETCH:         return "download del cervello fallito";
    case GMB_BRAIN_E_EXTRACT:       return "estrazione dell'archivio fallita";
    case GMB_BRAIN_E_INVALID:       return "non è un cervello valido";
    case GMB_BRAIN_E_IO:            return "errore di I/O";
    default:                        return "errore sconosciuto";
    }
}

/* ── utilità ─────────────────────────────────────────────────────────────────────────────── */

static void br_mkdir_p(const char* path)
{
    char tmp[BR_PATH];
    if (!path || !*path) return;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        mkdir(tmp, 0755);
        *p = '/';
    }
    mkdir(tmp, 0755);
}

static int br_isdir(const char* p)
{
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

static int br_isfile(const char* p)
{
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

static int br_ieq(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char x = (unsigned char)tolower((unsigned char)*a++);
        unsigned char y = (unsigned char)tolower((unsigned char)*b++);
        if (x != y) return 0;
    }
    return *a == '\0' && *b == '\0';
}

/* Una directory è un cervello se ha i file attesi (non basta che esista: `brains/` contiene
 * anche `.staging-*` e cartelle di lavoro). */
static int br_is_brain_dir(const char* dir, const char* name)
{
    char p[BR_PATH];
    snprintf(p, sizeof p, "%s/brain.manifest.json", dir);
    if (br_isfile(p)) return 1;
    snprintf(p, sizeof p, "%s/rag/%s.db", dir, name);
    if (br_isfile(p)) return 1;
    snprintf(p, sizeof p, "%s/graphs/%s.json", dir, name);
    if (br_isfile(p)) return 1;
    snprintf(p, sizeof p, "%s/rag", dir);
    if (br_isdir(p)) return 1;
    snprintf(p, sizeof p, "%s/graphs", dir);
    return br_isdir(p) ? 1 : 0;
}

int gmb_brain_has_rag(const char* dir, const char* name)
{
    char p[BR_PATH];
    if (!dir || !name) return 0;
    snprintf(p, sizeof p, "%s/rag/%s.db", dir, name);
    if (br_isfile(p)) return 1;
    snprintf(p, sizeof p, "%s/rag", dir);
    return br_isdir(p) ? 1 : 0;
}

int gmb_brain_has_graph(const char* dir, const char* name)
{
    char p[BR_PATH];
    if (!dir || !name) return 0;
    snprintf(p, sizeof p, "%s/graphs/%s.json", dir, name);
    if (br_isfile(p)) return 1;
    snprintf(p, sizeof p, "%s/graphs", dir);
    return br_isdir(p) ? 1 : 0;
}

int gmb_brain_modes(const char* dir, const char* name, char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = '\0';
    int rag = gmb_brain_has_rag(dir, name);
    int graph = gmb_brain_has_graph(dir, name);
    if (rag) snprintf(out, cap, "rag");
    if (graph) snprintf(out, cap, "%s%s", out[0] ? "rag," : "", "graph");
    return rag || graph;
}

/* Confronto di versioni tollerante (1.10.0 > 1.9.0): numerico per componente, poi testuale. */
static int br_vercmp(const char* a, const char* b)
{
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            char* ea = NULL; char* eb = NULL;
            long va = strtol(a, &ea, 10);
            long vb = strtol(b, &eb, 10);
            if (va != vb) return va < vb ? -1 : 1;
            a = ea; b = eb;
            continue;
        }
        if (*a != *b) return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        a++; b++;
    }
    if (*a == *b) return 0;
    return *a ? 1 : -1;
}

/* ── radici di ricerca ───────────────────────────────────────────────────────────────────── */

static int br_roots(char out[BR_MAX_ROOTS][BR_PATH])
{
    int n = 0;
    char cwd[BR_PATH];
    if (getcwd(cwd, sizeof cwd))
        snprintf(out[n++], BR_PATH, "%s/vendor/brains", cwd);
    char exe[BR_PATH] = "";
    if (gmb_paths_exe_dir(exe, sizeof exe) && exe[0]) {
        if (n < BR_MAX_ROOTS) snprintf(out[n++], BR_PATH, "%s/vendor/brains", exe);
        char alt[BR_PATH];
        snprintf(alt, BR_PATH, "%s/brains", exe);
        snprintf(out[n++], BR_PATH, "%s", alt);
    }
    char brains[BR_PATH] = "";
    gmb_paths_brains_dir(brains, sizeof brains);
    if (brains[0] && n < BR_MAX_ROOTS) snprintf(out[n++], BR_PATH, "%s", brains);
    char cache[BR_PATH] = "";
    gmb_paths_brains_cache_dir(cache, sizeof cache);
    if (cache[0] && n < BR_MAX_ROOTS) snprintf(out[n++], BR_PATH, "%s", cache);
    return n;
}

int gmb_brains_search_path(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = '\0';
    char roots[BR_MAX_ROOTS][BR_PATH];
    int n = br_roots(roots);
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        int w = snprintf(out + used, cap - used, "%s%s", used ? ";" : "", roots[i]);
        if (w <= 0 || used + (size_t)w >= cap) break;
        used += (size_t)w;
    }
    return used > 0;
}

/* La versione più alta dentro <root>/<nome>/ (solo directory che sembrano cervelli). */
static int br_pick_highest(const char* root, const char* name, char* ver, size_t cap)
{
    char base[BR_PATH];
    snprintf(base, sizeof base, "%s/%s", root, name);
    DIR* d = opendir(base);
    if (!d) return 0;
    struct dirent* e;
    ver[0] = '\0';
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char dir[BR_PATH];
        snprintf(dir, sizeof dir, "%s/%s", base, e->d_name);
        if (!br_isdir(dir) || !br_is_brain_dir(dir, name)) continue;
        if (!ver[0] || br_vercmp(e->d_name, ver) > 0) snprintf(ver, cap, "%s", e->d_name);
    }
    closedir(d);
    return ver[0] ? 1 : 0;
}

int gmb_brain_lookup(const char* name, const char* version, char* out, size_t cap)
{
    if (out && cap) out[0] = '\0';
    if (!name || !*name || !out || !cap) return 0;
    char roots[BR_MAX_ROOTS][BR_PATH];
    int n = br_roots(roots);

    /* 1) versione esplicita, poi 2) directory non versionata, poi 3) la più alta. */
    if (version && *version) {
        for (int i = 0; i < n; i++) {
            char dir[BR_PATH];
            snprintf(dir, sizeof dir, "%s/%s/%s", roots[i], name, version);
            if (br_isdir(dir) && br_is_brain_dir(dir, name)) { snprintf(out, cap, "%s", dir); return 1; }
        }
        return 0;
    }
    for (int i = 0; i < n; i++) {
        char dir[BR_PATH];
        snprintf(dir, sizeof dir, "%s/%s", roots[i], name);
        if (br_isdir(dir) && br_is_brain_dir(dir, name)) { snprintf(out, cap, "%s", dir); return 1; }
    }
    for (int i = 0; i < n; i++) {
        char v[128];
        if (br_pick_highest(roots[i], name, v, sizeof v)) {
            snprintf(out, cap, "%s/%s/%s", roots[i], name, v);
            return 1;
        }
    }
    return 0;
}

int gmb_brain_versions(const char* name, char* out, size_t cap)
{
    if (!name || !out || !cap) return 0;
    out[0] = '\0';
    char roots[BR_MAX_ROOTS][BR_PATH];
    int n = br_roots(roots);
    char found[64][128];
    int nf = 0;
    for (int i = 0; i < n && nf < 64; i++) {
        char base[BR_PATH];
        snprintf(base, sizeof base, "%s/%s", roots[i], name);
        DIR* d = opendir(base);
        if (!d) continue;
        struct dirent* e;
        while ((e = readdir(d)) != NULL && nf < 64) {
            if (e->d_name[0] == '.') continue;
            char dir[BR_PATH];
            snprintf(dir, sizeof dir, "%s/%s", base, e->d_name);
            if (!br_isdir(dir) || !br_is_brain_dir(dir, name)) continue;
            int dup = 0;
            for (int k = 0; k < nf; k++) if (strcmp(found[k], e->d_name) == 0) { dup = 1; break; }
            if (!dup) snprintf(found[nf++], sizeof found[0], "%s", e->d_name);
        }
        closedir(d);
    }
    /* ordinamento crescente (insertion sort: poche voci) */
    for (int i = 1; i < nf; i++) {
        char key[128];
        snprintf(key, sizeof key, "%s", found[i]);
        int j = i - 1;
        while (j >= 0 && br_vercmp(found[j], key) > 0) { snprintf(found[j + 1], sizeof found[0], "%s", found[j]); j--; }
        snprintf(found[j + 1], sizeof found[0], "%s", key);
    }
    size_t used = 0;
    for (int i = 0; i < nf; i++) {
        int w = snprintf(out + used, cap - used, "%s%s", used ? ";" : "", found[i]);
        if (w <= 0 || used + (size_t)w >= cap) break;
        used += (size_t)w;
    }
    return used > 0;
}

int gmb_brain_list(char* out, size_t cap, int* count)
{
    if (out && cap) out[0] = '\0';
    if (count) *count = 0;
    if (!out || !cap) return 0;
    char roots[BR_MAX_ROOTS][BR_PATH];
    int n = br_roots(roots);
    char names[128][128];
    char vers[128][128];
    int nn = 0;
    for (int i = 0; i < n; i++) {
        DIR* d = opendir(roots[i]);
        if (!d) continue;
        struct dirent* e;
        while ((e = readdir(d)) != NULL && nn < 128) {
            if (e->d_name[0] == '.') continue;
            char base[BR_PATH];
            snprintf(base, sizeof base, "%s/%s", roots[i], e->d_name);
            if (!br_isdir(base)) continue;
            char v[128] = "";
            if (!br_pick_highest(roots[i], e->d_name, v, sizeof v) && br_is_brain_dir(base, e->d_name))
                v[0] = '\0';                 /* cervello non versionato (vendor/brains/<nome>/) */
            else if (!v[0]) continue;        /* né versione né cervello: non è roba nostra */
            int dup = 0;
            for (int k = 0; k < nn; k++) if (br_ieq(names[k], e->d_name)) { dup = 1; break; }
            if (dup) continue;               /* la radice con priorità più alta vince */
            snprintf(names[nn], sizeof names[0], "%s", e->d_name);
            snprintf(vers[nn], sizeof vers[0], "%s", v);
            nn++;
        }
        closedir(d);
    }
    size_t used = 0;
    for (int i = 0; i < nn; i++) {
        int w = snprintf(out + used, cap - used, "%s%s@%s", used ? ";" : "", names[i], vers[i]);
        if (w <= 0 || used + (size_t)w >= cap) break;
        used += (size_t)w;
    }
    if (count) *count = nn;
    return nn > 0;
}

/* ── manifest ────────────────────────────────────────────────────────────────────────────── */

static gmb_pkg_manifest_t* br_load_manifest_at(const char* path_in, char* err, size_t errcap)
{
    char path[BR_PATH];
    if (path_in && *path_in) snprintf(path, sizeof path, "%s", path_in);
    else if (!gmb_paths_brains_manifest(path, sizeof path)) {
        if (err && errcap) snprintf(err, errcap, "manifest dei cervelli non trovato (%s)", path);
        return NULL;
    }
    if (!br_isfile(path)) {
        if (err && errcap) snprintf(err, errcap, "manifest dei cervelli non trovato (%s)", path);
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

int gmb_brains_manifest_path(char* out, size_t cap)
{
    return gmb_paths_brains_manifest(out, cap);
}

int gmb_brains_available_from(const char* manifest_path, char* out, size_t cap)
{
    if (out && cap) out[0] = '\0';
    if (!manifest_path || !*manifest_path) return -1;
    if (!br_isfile(manifest_path)) return -1;
    gmb_pkg_manifest_t* m = br_load_manifest_at(manifest_path, NULL, 0);
    if (!m) return -1;
    int n = gmb_pkg_manifest_count(m, "brains");
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        char p[128], name[256] = "", latest[128] = "";
        snprintf(p, sizeof p, "brains.%d.name", i);
        gmb_pkg_manifest_get(m, p, name, sizeof name);
        snprintf(p, sizeof p, "brains.%d.latest", i);
        gmb_pkg_manifest_get(m, p, latest, sizeof latest);
        if (!name[0]) continue;
        int w = snprintf(out + used, cap - used, "%s%s@%s", used ? ";" : "", name, latest);
        if (w <= 0 || used + (size_t)w >= cap) break;
        used += (size_t)w;
    }
    gmb_pkg_manifest_free(m);
    return (int)used;
}

int gmb_brains_available(char* out, size_t cap)
{
    char path[BR_PATH];
    if (!gmb_paths_brains_manifest(path, sizeof path) || !br_isfile(path)) {
        if (out && cap) out[0] = '\0';
        return -1;
    }
    return gmb_brains_available_from(path, out, cap);
}

/* Trova l'indice dell'entry del cervello nel manifest; -1 se assente. */
static int br_find_entry(gmb_pkg_manifest_t* m, const char* name)
{
    int n = gmb_pkg_manifest_count(m, "brains");
    for (int i = 0; i < n; i++) {
        char p[128], v[256] = "";
        snprintf(p, sizeof p, "brains.%d.name", i);
        if (gmb_pkg_manifest_get(m, p, v, sizeof v) && br_ieq(v, name)) return i;
    }
    return -1;
}

static int br_find_version(gmb_pkg_manifest_t* m, int bi, const char* want)
{
    int n = gmb_pkg_manifest_count(m, "brains");
    if (bi < 0 || bi >= n) return -1;
    char p[160], v[128] = "";
    char latest[128] = "";
    snprintf(p, sizeof p, "brains.%d.latest", bi);
    gmb_pkg_manifest_get(m, p, latest, sizeof latest);
    if (!want || !*want) want = latest;
    if (!want[0]) return -1;
    char vp[160];
    snprintf(vp, sizeof vp, "brains.%d.versions", bi);
    int cnt = gmb_pkg_manifest_count(m, vp);
    for (int j = 0; j < cnt; j++) {
        snprintf(p, sizeof p, "brains.%d.versions.%d.version", bi, j);
        if (gmb_pkg_manifest_get(m, p, v, sizeof v) && strcmp(v, want) == 0) return j;
    }
    return -1;
}

/* ── installazione ──────────────────────────────────────────────────────────────────────── */

/* Validazione di un cervello estratto: deve contenere qualcosa di utile, e se c'è
 * brain.manifest.json il campo `name` deve combaciare (un archivio sbagliato non deve
 * "installarsi" come se fosse il cervello richiesto). */
static int br_validate(const char* dir, const char* name, char* err, size_t errcap)
{
    if (!br_is_brain_dir(dir, name)) {
        if (err && errcap) snprintf(err, errcap, "l'archivio non contiene un cervello (manca rag/ o graphs/)");
        return 0;
    }
    char mp[BR_PATH];
    snprintf(mp, sizeof mp, "%s/brain.manifest.json", dir);
    if (br_isfile(mp)) {
        FILE* f = fopen(mp, "rb");
        if (f) {
            char js[65536];
            size_t n = fread(js, 1, sizeof js - 1, f);
            fclose(f);
            js[n] = '\0';
            gmb_pkg_manifest_t* bm = gmb_pkg_manifest_parse(js);
            if (bm) {
                char got[256] = "";
                gmb_pkg_manifest_get(bm, "name", got, sizeof got);
                gmb_pkg_manifest_free(bm);
                if (got[0] && name && *name && !br_ieq(got, name)) {
                    if (err && errcap)
                        snprintf(err, errcap, "il cervello nell'archivio si chiama \"%s\", non \"%s\"", got, name);
                    return 0;
                }
            }
        }
    }
    return 1;
}

/* Rimozione ricorsiva LIMITATA alla cache (mai vendor/exe). */
static void br_rmtree(const char* path)
{
    DIR* d = opendir(path);
    if (!d) { remove(path); return; }
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char p[BR_PATH];
        snprintf(p, sizeof p, "%s/%s", path, e->d_name);
        if (br_isdir(p)) br_rmtree(p);
        else remove(p);
    }
    closedir(d);
    rmdir(path);
}

int gmb_brain_remove(const char* name, const char* version)
{
    if (!name || !*name) return 0;
    char cache[BR_PATH];
    gmb_paths_brains_cache_dir(cache, sizeof cache);
    if (!cache[0]) return 0;
    char dir[BR_PATH];
    if (version && *version) {
        snprintf(dir, sizeof dir, "%s/%s/%s", cache, name, version);
    } else {
        if (!gmb_brain_lookup(name, NULL, dir, sizeof dir)) return 0;
        /* si rimuove SOLO dentro la cache */
        if (strncmp(dir, cache, strlen(cache)) != 0) return 0;
    }
    if (strncmp(dir, cache, strlen(cache)) != 0) return 0;
    if (!br_isdir(dir)) return 0;
    br_rmtree(dir);
    return br_isdir(dir) ? 0 : 1;
}

/* Estrae `zip` in `<cache>/<nome>/<versione>` passando da una staging (niente cervelli a metà). */
static int br_install_from_zip(const char* name, const char* version, const char* zip,
                               char* out_dir, size_t cap, char* err, size_t errcap)
{
    char cache[BR_PATH];
    gmb_paths_brains_cache_dir(cache, sizeof cache);
    if (!cache[0]) {
        if (err && errcap) snprintf(err, errcap, "cache dei cervelli non risolvibile (HOME?)");
        return GMB_BRAIN_E_IO;
    }
    br_mkdir_p(cache);
    char base[BR_PATH], final[BR_PATH], staging[BR_PATH];
    snprintf(base, sizeof base, "%s/%s", cache, name);
    br_mkdir_p(base);
    snprintf(final, sizeof final, "%s/%s", base, version);
    snprintf(staging, sizeof staging, "%s/.staging-%d", base, (int)getpid());
    br_rmtree(staging);
    br_mkdir_p(staging);

    if (gmb_unzip_files(zip, staging) != 0) {
        br_rmtree(staging);
        if (err && errcap) snprintf(err, errcap, "estrazione di %s fallita", zip);
        return GMB_BRAIN_E_EXTRACT;
    }
    /* alguns zip hanno una directory di primo livello (`<nome>/rag/...`): se la radice non è un
     * cervello ma lo è una sua unica sottocartella, si adotta quella. */
    char root[BR_PATH];
    snprintf(root, sizeof root, "%s", staging);
    if (!br_is_brain_dir(root, name)) {
        DIR* d = opendir(staging);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.') continue;
                char sub[BR_PATH];
                snprintf(sub, sizeof sub, "%s/%s", staging, e->d_name);
                if (br_isdir(sub) && br_is_brain_dir(sub, name)) { snprintf(root, sizeof root, "%s", sub); break; }
            }
            closedir(d);
        }
    }
    if (!br_validate(root, name, err, errcap)) {
        br_rmtree(staging);
        return GMB_BRAIN_E_INVALID;
    }
    if (br_isdir(final)) br_rmtree(final);
    if (rename(root, final) != 0) {
        /* rename fra directory diverse non è sempre possibile: si copia? No — staging è nella
         * stessa cartella, quindi un fallimento è un errore vero (permessi, disco). */
        br_rmtree(staging);
        if (err && errcap) snprintf(err, errcap, "rename verso %s fallito: %s", final, strerror(errno));
        return GMB_BRAIN_E_IO;
    }
    if (strcmp(root, staging) != 0) br_rmtree(staging);   /* resta la cartella esterna vuota */
    if (out_dir && cap) snprintf(out_dir, cap, "%s", final);
    return GMB_BRAIN_OK;
}

/* Legge `version` (e `name`) da brain.manifest.json dentro uno zip già estratto. */
static void br_manifest_meta(const char* dir, char* version, size_t vcap, char* name, size_t ncap)
{
    if (version && vcap) version[0] = '\0';
    if (name && ncap) name[0] = '\0';
    char mp[BR_PATH];
    snprintf(mp, sizeof mp, "%s/brain.manifest.json", dir);
    FILE* f = fopen(mp, "rb");
    if (!f) return;
    char js[65536];
    size_t n = fread(js, 1, sizeof js - 1, f);
    fclose(f);
    js[n] = '\0';
    gmb_pkg_manifest_t* bm = gmb_pkg_manifest_parse(js);
    if (!bm) return;
    if (version && vcap) gmb_pkg_manifest_get(bm, "version", version, vcap);
    if (name && ncap) gmb_pkg_manifest_get(bm, "name", name, ncap);
    gmb_pkg_manifest_free(bm);
}

int gmb_brains_install_source(const char* name, const char* source,
                              const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                              char* out_dir, size_t cap, char* err, size_t errcap)
{
    if (out_dir && cap) out_dir[0] = '\0';
    if (err && errcap) err[0] = '\0';
    if (!name || !*name || !source || !*source) return GMB_BRAIN_E_ARG;

    /* (a) directory già pronta: niente da scaricare, si usa così com'è. */
    if (br_isdir(source)) {
        if (!br_is_brain_dir(source, name)) {
            if (err && errcap) snprintf(err, errcap, "%s non è un cervello", source);
            return GMB_BRAIN_E_INVALID;
        }
        if (out_dir && cap) snprintf(out_dir, cap, "%s", source);
        return GMB_BRAIN_OK;
    }

    /* (b) zip locale o URL: si scarica (se serve) e si estrae. */
    char zip[BR_PATH];
    int is_remote = (strncmp(source, "http://", 7) == 0 || strncmp(source, "https://", 8) == 0);
    if (is_remote) {
        const gmb_fetch_transport* use = tr ? tr : gmb_fetch_transport_curl();
        if (!use) {
            if (err && errcap) snprintf(err, errcap, "nessun trasporto HTTP disponibile");
            return GMB_BRAIN_E_FETCH;
        }
        char cache[BR_PATH];
        gmb_paths_brains_cache_dir(cache, sizeof cache);
        char dl[BR_PATH], dl_dir[BR_PATH];
        snprintf(dl_dir, sizeof dl_dir, "%s/.downloads", cache);
        br_mkdir_p(dl_dir);
        const char* b = strrchr(source, '/');
        b = b ? b + 1 : source;
        char clean[256];
        size_t k = 0;
        for (const char* p = b; *p && k + 1 < sizeof clean; p++) {
            char ch = *p;
            if (ch == '?' || ch == '#') break;
            clean[k++] = (isalnum((unsigned char)ch) || ch == '.' || ch == '-' || ch == '_') ? ch : '_';
        }
        clean[k] = '\0';
        if (!clean[0]) snprintf(clean, sizeof clean, "brain.gmbrain.zip");
        snprintf(dl, sizeof dl, "%s/%s-%s", dl_dir, name, clean);

        gmb_fetch_request rq;
        memset(&rq, 0, sizeof rq);
        rq.url = source;
        rq.dest_path = dl;
        gmb_fetch_opts defo;
        if (!opts) { gmb_fetch_opts_default(&defo); opts = &defo; }
        int rc = gmb_fetch_to_file(&rq, opts, use, err, errcap);
        if (rc != GMB_FETCH_OK) {
            if (err && errcap && !err[0]) snprintf(err, errcap, "%s", gmb_fetch_strerror(rc));
            return GMB_BRAIN_E_FETCH;
        }
        snprintf(zip, sizeof zip, "%s", dl);
    } else {
        if (!br_isfile(source)) {
            if (err && errcap) snprintf(err, errcap, "sorgente non trovata: %s", source);
            return GMB_BRAIN_E_NOT_FOUND;
        }
        snprintf(zip, sizeof zip, "%s", source);
    }

    /* versione: dal manifest interno, altrimenti "local" (uno zip senza versione). */
    char version[128] = "";
    {
        char cache[BR_PATH], tmpdir[BR_PATH];
        gmb_paths_brains_cache_dir(cache, sizeof cache);
        snprintf(tmpdir, sizeof tmpdir, "%s/.peek-%d", cache, (int)getpid());
        br_rmtree(tmpdir);
        br_mkdir_p(tmpdir);
        if (gmb_unzip_files(zip, tmpdir) == 0) {
            char inner[128] = "";
            br_manifest_meta(tmpdir, version, sizeof version, inner, sizeof inner);
            if (inner[0] && !br_ieq(inner, name) && err && errcap)
                snprintf(err, errcap, "attenzione: l'archivio dichiara \"%s\" ma si installa come \"%s\"", inner, name);
        }
        br_rmtree(tmpdir);
    }
    if (!version[0]) snprintf(version, sizeof version, "local");

    int rc = br_install_from_zip(name, version, zip, out_dir, cap, err, errcap);
    if (rc == GMB_BRAIN_OK && is_remote) remove(zip);
    return rc;
}

int gmb_brains_install_from_manifest(const char* manifest_path, const char* name, const char* version,
                                     const char* token, const gmb_fetch_transport* tr,
                                     const gmb_fetch_opts* opts, char* out_dir, size_t cap,
                                     char* err, size_t errcap)
{
    if (out_dir && cap) out_dir[0] = '\0';
    if (err && errcap) err[0] = '\0';
    if (!name || !*name) return GMB_BRAIN_E_ARG;

    /* Già installato: idempotente (nessun download). */
    if (gmb_brain_lookup(name, version, out_dir, cap)) return GMB_BRAIN_OK;

    gmb_pkg_manifest_t* m = br_load_manifest_at(manifest_path, err, errcap);
    if (!m) return GMB_BRAIN_E_NO_MANIFEST;

    int bi = br_find_entry(m, name);
    if (bi < 0) {
        if (err && errcap) snprintf(err, errcap, "il cervello \"%s\" non è dichiarato nel manifest", name);
        gmb_pkg_manifest_free(m);
        return GMB_BRAIN_E_NOT_FOUND;
    }
    int vi = br_find_version(m, bi, version);
    if (vi < 0) {
        if (err && errcap) snprintf(err, errcap, "versione \"%s\" non dichiarata per \"%s\"", version ? version : "latest", name);
        gmb_pkg_manifest_free(m);
        return GMB_BRAIN_E_NOT_FOUND;
    }

    char p[192], url[2048] = "", sha[128] = "", src[64] = "", ver[128] = "";
    double size = 0;
    snprintf(p, sizeof p, "brains.%d.versions.%d.url", bi, vi);
    gmb_pkg_manifest_get(m, p, url, sizeof url);
    snprintf(p, sizeof p, "brains.%d.versions.%d.sha256", bi, vi);
    gmb_pkg_manifest_get(m, p, sha, sizeof sha);
    snprintf(p, sizeof p, "brains.%d.versions.%d.source", bi, vi);
    gmb_pkg_manifest_get(m, p, src, sizeof src);
    snprintf(p, sizeof p, "brains.%d.versions.%d.version", bi, vi);
    gmb_pkg_manifest_get(m, p, ver, sizeof ver);
    snprintf(p, sizeof p, "brains.%d.versions.%d.size", bi, vi);
    gmb_pkg_manifest_get_num(m, p, &size);
    if (!src[0]) snprintf(src, sizeof src, "url");
    if (!ver[0]) snprintf(ver, sizeof ver, "%s", version && *version ? version : "latest");
    gmb_pkg_manifest_free(m);

    if (!url[0]) {
        if (err && errcap) snprintf(err, errcap, "voce senza url per \"%s@%s\"", name, ver);
        return GMB_BRAIN_E_INVALID;
    }

    /* FAIL-CLOSED: una sorgente protetta senza token NON si finge installata. */
    if (br_ieq(src, "github-asset") || br_ieq(src, "github")) {
        if (!token || !*token) {
            if (err && errcap)
                snprintf(err, errcap,
                         "\"%s@%s\" è un asset privato: serve un token (gmb brain login --github). "
                         "Installazione rifiutata.", name, ver);
            return GMB_BRAIN_E_UNAUTHORIZED;
        }
    } else if (!br_ieq(src, "url")) {
        if (err && errcap) snprintf(err, errcap, "sorgente \"%s\" non supportata", src);
        return GMB_BRAIN_E_INVALID;
    }

    const gmb_fetch_transport* use = tr ? tr : gmb_fetch_transport_curl();
    if (!use) {
        if (err && errcap) snprintf(err, errcap, "nessun trasporto HTTP disponibile (libcurl assente)");
        return GMB_BRAIN_E_FETCH;
    }
    gmb_fetch_opts defo;
    if (!opts) { gmb_fetch_opts_default(&defo); opts = &defo; }

    char cache[BR_PATH], dl_dir[BR_PATH], zip[BR_PATH];
    gmb_paths_brains_cache_dir(cache, sizeof cache);
    if (!cache[0]) {
        if (err && errcap) snprintf(err, errcap, "cache dei cervelli non risolvibile (HOME?)");
        return GMB_BRAIN_E_IO;
    }
    snprintf(dl_dir, sizeof dl_dir, "%s/.downloads", cache);
    br_mkdir_p(dl_dir);
    snprintf(zip, sizeof zip, "%s/%s-%s.gmbrain.zip", dl_dir, name, ver);

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = zip;
    rq.sha256 = sha[0] ? sha : NULL;
    rq.token = token;
    rq.size = (long long)size;
    int rc = gmb_fetch_to_file(&rq, opts, use, err, errcap);
    if (rc != GMB_FETCH_OK) {
        if (err && errcap && !err[0]) snprintf(err, errcap, "%s", gmb_fetch_strerror(rc));
        return GMB_BRAIN_E_FETCH;
    }

    rc = br_install_from_zip(name, ver, zip, out_dir, cap, err, errcap);
    if (rc == GMB_BRAIN_OK) remove(zip);
    return rc;
}

int gmb_brains_install(const char* name, const char* version, const char* token,
                       const gmb_fetch_transport* tr, const gmb_fetch_opts* opts,
                       char* out_dir, size_t cap, char* err, size_t errcap)
{
    /* manifest "di sistema": `gmb_paths_brains_manifest` (client: <exe>/settings/…). */
    return gmb_brains_install_from_manifest(NULL, name, version, token, tr, opts,
                                            out_dir, cap, err, errcap);
}

/* ── superficie BASIC (AI BRAIN INSTALL / AI BRAIN UNINSTALL) ───────────────────────────────
 * Un solo punto per i 4 backend: IL/PCodeVM (che chiamano il C# via P/Invoke), VM/C e
 * C99Native chiamano QUESTE funzioni. Ritorno 1/0 come il C# (double), errore su stderr: il
 * programma BASIC non deve abortire per un cervello che non si è potuto scaricare. */
gmb_double_t gmb_brain_install(const char* name, const char* source)
{
    char out[BR_PATH], err[512];
    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    int rc;
    if (!source || !*source) {
        /* `AI BRAIN INSTALL "nome"` = dal MANIFEST (download on demand, sha verificato). */
        rc = gmb_brains_install(name, NULL, NULL, NULL, &op, out, sizeof out, err, sizeof err);
    } else {
        rc = gmb_brains_install_source(name, source, NULL, &op, out, sizeof out, err, sizeof err);
    }
    if (rc != GMB_BRAIN_OK) {
        fprintf(stderr, "?AI BRAIN INSTALL %s: %s%s%s\n", name ? name : "",
                gmb_brain_strerror(rc), err[0] ? " — " : "", err);
        return 0.0;
    }
    /* Parità con IL/PCodeVM (BrainInstaller.Install(..., activate: true)): il comando INSTALL
     * COLLEGA il cervello. Senza, su C99Native/VM-C `AI BRAIN USED()` resterebbe vuoto mentre
     * su IL/PCode conterrebbe il cervello appena installato. */
    if (name && *name) gmb_brain_use(name);
    return 1.0;
}

gmb_double_t gmb_brain_uninstall(const char* name)
{
    if (!name || !*name) return 0.0;
    return gmb_brain_remove(name, NULL) ? 1.0 : 0.0;
}
