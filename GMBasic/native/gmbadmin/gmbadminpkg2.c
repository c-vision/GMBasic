/* gmbadminpkg2.c — `pack` · `publish` · `login` · `logout` del CLI C (gmbadmin).
 *
 * Package workflow completo in C:
 *   pack     → .gmbpkg (zip di manifest/sorgenti/export/header/sample, senza binari)
 *   publish  → POST base64 al registry (gmb_pkg_registry_post, Bearer)
 *   login    → token + registry nel config utente (~/.gmbasic/config.json, chmod 600)
 *   logout   → rimuove il token
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_commonlib.h"
#include "gmb_compress.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_registry.h"
#include "gmb_pkg_util.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static void config_json(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    if (home && *home) snprintf(out, cap, "%s/.gmbasic/config.json", home);
    else snprintf(out, cap, "config.json");
}

static gmb_val_t load_config(void)
{
    char p[1200];
    config_json(p, sizeof p);
    gmb_val_t v = gmb_json_load_file(p);
    if (v.kind != GMB_TYPE_OBJECT) {
        gmb_dict_t d = gmb_dict_new();
        v = gmb_make_val_dict_take(&d);
    }
    return v;
}

static int save_config(gmb_val_t v)
{
    char p[1200];
    config_json(p, sizeof p);
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", p);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; gmb_fs_mkdir_p(dir); }
    gmb_json_save_val(v, p);
    chmod(p, 0600);
    return 0;
}

static const char* json_text(gmb_val_t root, const char* key)
{
    gmb_val_t v = gmb_val_dict_get(root, key);
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return "";
}

int gmbadmin_login(int argc, char** argv, int logout)
{
    const char* token = NULL;
    const char* registry = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--registry") && i + 1 < argc) registry = argv[++i];
        else if (!strncmp(argv[i], "--registry=", 11)) registry = argv[i] + 11;
        else if (!strcmp(argv[i], "--token") && i + 1 < argc) token = argv[++i];
    }
    if (!logout) {
        if (!token) {
            for (int i = 2; i < argc; i++)
                if (argv[i][0] != '-') { token = argv[i]; break; }
        }
        if (!token) { fprintf(stderr, "?uso: gmb login <token> [--registry url]\n"); return 2; }
    }

    gmb_val_t cfg = load_config();
    gmb_val_dict_set(cfg, "Token", gmb_make_val_string(logout ? "" : token));
    if (registry && *registry) gmb_val_dict_set(cfg, "RegistryUrl", gmb_make_val_string(registry));
    save_config(cfg);

    char p[1200];
    config_json(p, sizeof p);
    if (logout) printf("Token rimosso (%s)\n", p);
    else {
        const char* reg = json_text(cfg, "RegistryUrl");
        if (*reg) printf("Credenziali salvate in %s (registry: %s)\n", p, reg);
        else printf("Credenziali salvate in %s\n", p);
    }
    return 0;
}

/* ── pack ────────────────────────────────────────────────────────────────── */

typedef struct { char** files; int n, cap; const char* root; } filelist_t;

static int skip_dir(const char* name)
{
    return !strcmp(name, "bin") || !strcmp(name, "obj") || !strcmp(name, "build") ||
           !strcmp(name, "work") || !strcmp(name, "vendor") || !strcmp(name, ".git") ||
           !strcmp(name, "node_modules");
}
static int skip_file(const char* name)
{
    const char* d = strrchr(name, '.');
    if (!d) return 0;
    return !strcasecmp(d, ".o") || !strcasecmp(d, ".a") || !strcasecmp(d, ".dylib") ||
           !strcasecmp(d, ".so") || !strcasecmp(d, ".dll") || !strcasecmp(d, ".exe") ||
           !strcasecmp(d, ".pdb") || !strcasecmp(d, ".chunk") || !strcasecmp(d, ".prg") ||
           !strcasecmp(d, ".gmbpkg");
}

static void fl_add(filelist_t* l, const char* path)
{
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 64;
        char** p = (char**)realloc(l->files, (size_t)cap * sizeof(char*));
        if (!p) return;
        l->files = p;
        l->cap = cap;
    }
    l->files[l->n++] = strdup(path);
}

static void collect(filelist_t* l, const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (!skip_dir(de->d_name)) collect(l, p); }
        else if (S_ISREG(st.st_mode) && !skip_file(de->d_name)) fl_add(l, p);
    }
    closedir(d);
}

int gmbadmin_pack(int argc, char** argv)
{
    const char* dir = NULL;
    const char* out = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strncmp(argv[i], "--out=", 6)) out = argv[i] + 6;
        else if (argv[i][0] != '-' && !dir) dir = argv[i];
    }
    if (!dir) { fprintf(stderr, "?uso: gmb pack <dir> [--out file.gmbpkg]\n"); return 2; }

    char mp[4096];
    snprintf(mp, sizeof mp, "%s/gmb-package.json", dir);
    FILE* f = fopen(mp, "rb");
    if (!f) { fprintf(stderr, "?pack: %s non trovato\n", mp); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)n + 1);
    if (!js || fread(js, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(js); return 1; }
    js[n] = 0; fclose(f);
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
    free(js);
    if (!m) { fprintf(stderr, "?pack: manifest non valido\n"); return 1; }
    char name[256] = "", ver[64] = "";
    gmb_pkg_manifest_get(m, "name", name, sizeof name);
    gmb_pkg_manifest_get(m, "version", ver, sizeof ver);
    gmb_pkg_manifest_free(m);
    if (!name[0] || !ver[0]) { fprintf(stderr, "?pack: name/version mancanti nel manifest\n"); return 1; }

    char outbuf[4096];
    if (!out) { snprintf(outbuf, sizeof outbuf, "%s/%s-%s.gmbpkg", dir, name, ver); out = outbuf; }

    filelist_t l;
    memset(&l, 0, sizeof l);
    collect(&l, dir);
    if (l.n == 0) { fprintf(stderr, "?pack: nessun file da includere\n"); return 1; }
    int rc = gmb_zip_files((const char* const*)l.files, l.n, out);
    printf("Pack: %s (%d file, firma: nessuna — F9c)\n", out, l.n);
    for (int i = 0; i < l.n; i++) free(l.files[i]);
    free(l.files);
    return rc == 0 ? 0 : 1;
}

/* ── publish ─────────────────────────────────────────────────────────────── */

typedef struct { char* data; size_t len; int found; } entry_ctx_t;

static int zip_find_manifest(const char* name, const unsigned char* content, size_t clen, void* ctx)
{
    entry_ctx_t* e = (entry_ctx_t*)ctx;
    if (e->found) return 0;
    if (strcmp(name, "gmb-package.json") != 0) return 0;
    e->data = (char*)malloc(clen + 1);
    if (!e->data) return 0;
    memcpy(e->data, content, clen);
    e->data[clen] = 0;
    e->len = clen;
    e->found = 1;
    return 0;
}

int gmbadmin_publish(int argc, char** argv)
{
    const char* file = NULL;
    const char* reg_opt = NULL;
    const char* token_opt = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--registry") && i + 1 < argc) reg_opt = argv[++i];
        else if (!strncmp(argv[i], "--registry=", 11)) reg_opt = argv[i] + 11;
        else if (!strcmp(argv[i], "--token") && i + 1 < argc) token_opt = argv[++i];
        else if (argv[i][0] != '-' && !file) file = argv[i];
    }
    if (!file) { fprintf(stderr, "?uso: gmb publish <file.gmbpkg> [--registry url] [--token t]\n"); return 2; }

    FILE* f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "?publish: %s non trovato\n", file); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(data); return 1; }
    fclose(f);

    entry_ctx_t ec;
    memset(&ec, 0, sizeof ec);
    gmb_zip_foreach_mem(data, (size_t)n, zip_find_manifest, &ec);
    if (!ec.found) { free(data); fprintf(stderr, "?publish: gmb-package.json mancante nel gmbpkg\n"); return 1; }
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(ec.data);
    free(ec.data);
    if (!m) { free(data); fprintf(stderr, "?publish: manifest non valido\n"); return 1; }
    char name[256] = "", ver[64] = "";
    gmb_pkg_manifest_get(m, "name", name, sizeof name);
    gmb_pkg_manifest_get(m, "version", ver, sizeof ver);
    gmb_pkg_manifest_free(m);
    if (!name[0] || !ver[0]) { free(data); fprintf(stderr, "?publish: name/version mancanti\n"); return 1; }

    char* b64 = gmb_base64_encode(data, (size_t)n);
    free(data);
    if (!b64) { fprintf(stderr, "?publish: base64 fallito\n"); return 1; }

    char reg[512] = "";
    if (reg_opt && *reg_opt) snprintf(reg, sizeof reg, "%s", reg_opt);
    else { gmbadmin_registry_url(argc, argv, reg, sizeof reg); }
    const char* token = token_opt ? token_opt : "";

    size_t plen = strlen(b64) + strlen(name) + strlen(ver) + 256;
    char* payload = (char*)malloc(plen);
    if (!payload) { free(b64); return 1; }
    snprintf(payload, plen, "{\"name\":\"%s\",\"version\":\"%s\",\"file\":\"%s\",\"token\":\"%s\"}",
             name, ver, b64, token);
    free(b64);

    char url[1024];
    snprintf(url, sizeof url, "%s/api/packages", reg);
    char out[4096] = "";
    int status = 0;
    int ok = gmb_pkg_registry_post(url, payload, *token ? token : NULL, out, sizeof out, &status);
    free(payload);
    if (!ok) {
        if (status == 0) fprintf(stderr, "?publish: registry NON RAGGIUNGIBILE su %s\n", url);
        else fprintf(stderr, "?publish fallito (%d) su %s: %s\n", status, url, out);
        return 1;
    }
    printf("Publish: %s@%s -> %s\n", name, ver, url);
    if (out[0]) printf("%s\n", out);
    return 0;
}
