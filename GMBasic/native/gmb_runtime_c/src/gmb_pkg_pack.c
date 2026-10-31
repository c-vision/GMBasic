/* gmb_pkg_pack.c — creazione del .gmbpkg (riferimento C del Package Manager). */
#include "gmb_pkg_pack.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_glob.h"
#include "gmb_compress.h"
#include "gmb_commonlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

struct gmb_pkg_pack_files { char** items; int n; int cap; };

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (l < 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)l + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)l, f);
    b[rd] = 0;
    fclose(f);
    return b;
}
static int is_dir(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int is_file(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }

static void pf_add(struct gmb_pkg_pack_files* f, const char* rel)
{
    for (int i = 0; i < f->n; i++) if (strcasecmp(f->items[i], rel) == 0) return;
    if (f->n >= f->cap) { f->cap = f->cap ? f->cap * 2 : 32; f->items = (char**)realloc(f->items, sizeof(char*) * f->cap); }
    f->items[f->n++] = strdup(rel);
}
static void collect_rec(const char* dir, const char* rel, struct gmb_pkg_pack_files* f)
{
    char relbuf[4096];
    snprintf(relbuf, sizeof(relbuf), "%s", rel ? rel : "");
    size_t rl = strlen(relbuf);
    while (rl > 0 && (relbuf[rl - 1] == '/' || relbuf[rl - 1] == '\\')) relbuf[--rl] = 0;
    rel = relbuf;
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", dir, rel);
    if (is_file(full)) { pf_add(f, rel); return; }
    if (!is_dir(full)) return;
    DIR* d = opendir(full);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[4096];
        snprintf(sub, sizeof(sub), "%s/%s", rel, de->d_name);
        collect_rec(dir, sub, f);
    }
    closedir(d);
}

static int backend_wanted(const char* backends_csv, const char* bk)
{
    if (!backends_csv || !backends_csv[0] || strcasecmp(backends_csv, "all") == 0) return 1;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", backends_csv);
    char* save = NULL;
    for (char* t = strtok_r(buf, ",", &save); t; t = strtok_r(NULL, ",", &save))
        if (strcasecmp(t, bk) == 0 || strcasecmp(bk, "any") == 0) return 1;
    return 0;
}

gmb_pkg_pack_files_t* gmb_pkg_pack_files(const char* dir, const char* backends_csv)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", dir);
    char* js = read_file(mpath);
    if (!js) return NULL;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return NULL;

    struct gmb_pkg_pack_files* f = (struct gmb_pkg_pack_files*)calloc(1, sizeof(*f));
    pf_add(f, "gmb-package.json");

    int ne = gmb_pkg_manifest_count(man, "exports");
    for (int i = 0; i < ne; i++) { char k[64], v[512]; if (gmb_pkg_manifest_item(man, "exports", i, k, sizeof(k), v, sizeof(v)) && v[0]) { collect_rec(dir, v, f); } }

    int nb = gmb_pkg_manifest_count(man, "binaries");
    for (int i = 0; i < nb; i++) {
        char bk[256], bv[4096];
        if (!gmb_pkg_manifest_item(man, "binaries", i, bk, sizeof(bk), bv, sizeof(bv))) continue;
        if (!backend_wanted(backends_csv, bk)) continue;
        if (bv[0]) { collect_rec(dir, bv, f); continue; } /* v1 */
        char path[400];
        snprintf(path, sizeof(path), "binaries.%s", bk);
        int np = gmb_pkg_manifest_count(man, path);
        for (int j = 0; j < np; j++) { char pk[128], pv[2048]; if (gmb_pkg_manifest_item(man, path, j, pk, sizeof(pk), pv, sizeof(pv)) && pv[0]) collect_rec(dir, pv, f); }
    }

    if (gmb_pkg_manifest_has(man, "ffi")) {
        int nh = gmb_pkg_manifest_count(man, "ffi.headers");
        for (int i = 0; i < nh; i++) { char k[64], v[512]; if (gmb_pkg_manifest_item(man, "ffi.headers", i, k, sizeof(k), v, sizeof(v)) && v[0]) collect_rec(dir, v, f); }
        int nl = gmb_pkg_manifest_count(man, "ffi.libs");
        for (int i = 0; i < nl; i++) { char k[64], v[512]; if (gmb_pkg_manifest_item(man, "ffi.libs", i, k, sizeof(k), v, sizeof(v)) && v[0]) collect_rec(dir, v, f); }
    }

    int ns = gmb_pkg_manifest_count(man, "samples");
    if (ns > 0) {
        for (int i = 0; i < ns; i++) { char k[64], v[512]; if (gmb_pkg_manifest_item(man, "samples", i, k, sizeof(k), v, sizeof(v)) && v[0]) collect_rec(dir, v, f); }
    } else {
        char s[3000];
        snprintf(s, sizeof(s), "%s/samples", dir);
        if (is_dir(s)) collect_rec(dir, "samples", f);
    }

    /* files (allowlist) + ignore (glob) */
    int nfiles = gmb_pkg_manifest_count(man, "files");
    int nignore = gmb_pkg_manifest_count(man, "ignore");
    char** keep = (char**)malloc(sizeof(char*) * (f->n > 0 ? f->n : 1));
    int nk = 0;
    for (int i = 0; i < f->n; i++) {
        const char* rel = f->items[i];
        if (strcmp(rel, "gmb-package.json") != 0) {
            int allow = (nfiles <= 0); /* nessuna allowlist `files` → tutto ammesso */
            for (int j = 0; j < nfiles && !allow; j++) { char k[64], v[512]; if (gmb_pkg_manifest_item(man, "files", j, k, sizeof(k), v, sizeof(v)) && v[0] && gmb_pkg_glob_match(v, rel)) allow = 1; }
            if (!allow) continue;
            int ign = 0;
            for (int j = 0; j < nignore && !ign; j++) { char k[64], v[512]; if (gmb_pkg_manifest_item(man, "ignore", j, k, sizeof(k), v, sizeof(v)) && v[0] && gmb_pkg_glob_match(v, rel)) ign = 1; }
            if (ign) continue;
        }
        keep[nk++] = f->items[i];
    }
    for (int i = 0; i < f->n; i++) { int kept = 0; for (int j = 0; j < nk; j++) if (keep[j] == f->items[i]) { kept = 1; break; } if (!kept) free(f->items[i]); }
    free(f->items);
    f->items = keep;
    f->n = nk;

    gmb_pkg_manifest_free(man);
    return f;
}

int gmb_pkg_pack_files_count(gmb_pkg_pack_files_t* f) { return f ? f->n : 0; }
const char* gmb_pkg_pack_files_get(gmb_pkg_pack_files_t* f, int i) { return (f && i >= 0 && i < f->n) ? f->items[i] : NULL; }
void gmb_pkg_pack_files_free(gmb_pkg_pack_files_t* f)
{
    if (!f) return;
    for (int i = 0; i < f->n; i++) free(f->items[i]);
    free(f->items);
    free(f);
}

int gmb_pkg_pack(const char* dir, const char* out_file, const char* hmac_key)
{
    gmb_pkg_pack_files_t* f = gmb_pkg_pack_files(dir, NULL);
    if (!f) return 0;
    /* verifica che il manifest sia un oggetto (parse ok). */

    /* staging: copia/collega i file in una dir temporanea (path relativi) così lo zip ha nomi relativi. */
    char stage[2300];
    snprintf(stage, sizeof(stage), "%s/.gmbpack-stage", dir);
    char cmd[2600];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", stage, stage);
    system(cmd);

    char** abs = (char**)malloc(sizeof(char*) * (f->n + 4));
    int na = 0;
    for (int i = 0; i < f->n; i++) {
        char src[4096], dst[4096];
        snprintf(src, sizeof(src), "%s/%s", dir, f->items[i]);
        snprintf(dst, sizeof(dst), "%s/%s", stage, f->items[i]);
        char dc[4096];
        snprintf(dc, sizeof(dc), "%s", dst);
        char* slash = strrchr(dc, '/');
        if (slash) { *slash = 0; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dc); system(cmd); }
        snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", src, dst);
        system(cmd);
        abs[na++] = strdup(dst);
    }

    /* firma HMAC del manifest (.gmbpkg.sig), se richiesta */
    if (hmac_key && hmac_key[0]) {
        char mpath[2600];
        snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", dir);
        char* js = read_file(mpath);
        if (js) {
            unsigned char d[32];
            gmb_hmac_sha256((const unsigned char*)hmac_key, strlen(hmac_key), (const unsigned char*)js, strlen(js), d);
            static const char* H = "0123456789abcdef";
            char hex[65];
            for (int i = 0; i < 32; i++) { hex[2 * i] = H[d[i] >> 4]; hex[2 * i + 1] = H[d[i] & 15]; }
            hex[64] = 0;
            char sigpath[2400];
            snprintf(sigpath, sizeof(sigpath), "%s/.gmbpkg.sig", stage);
            FILE* sf = fopen(sigpath, "w");
            if (sf) { fputs(hex, sf); fclose(sf); abs[na++] = strdup(sigpath); }
            free(js);
        }
    }

    /* zip (store) */
    unlink(out_file);
    int rc = gmb_zip_files((const char* const*)abs, na, out_file);
    for (int i = 0; i < na; i++) free(abs[i]);
    free(abs);
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", stage);
    system(cmd);
    gmb_pkg_pack_files_free(f);
    return rc == 0;
}
