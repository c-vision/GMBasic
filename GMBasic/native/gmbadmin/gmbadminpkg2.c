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
#include "gmb_pkg_pack.h"
#include "gmb_fetch.h"
#include "gmb_pkg_ed25519.h"
#include "gmb_fsutil.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>

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

/* Legge la chiave Ed25519: file (seed base64) oppure segreto derivato con SHA-256, come
 * PackageBuilder.Pack del C# (una firma simmetrica non sarebbe verificabile). */
static int read_priv_key(const char* key_file, const char* secret, char* out, size_t cap)
{
    if (key_file && *key_file) {
        FILE* f = fopen(key_file, "rb");
        if (!f) return 0;
        size_t n = fread(out, 1, cap - 1, f);
        fclose(f);
        out[n] = 0;
        while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = 0;
        return n > 0;
    }
    if (secret && *secret) {
        uint8_t d[32];
        gmb_sha256((const uint8_t*)secret, strlen(secret), d);
        return gmb_base64_encode(d, 32) != NULL &&
               (snprintf(out, cap, "%s", gmb_base64_encode(d, 32)), 1);
    }
    return 0;
}

int gmbadmin_pack(int argc, char** argv)
{
    const char* dir = NULL;
    const char* out = NULL;
    const char* key_file = NULL;
    const char* secret = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strncmp(argv[i], "--out=", 6)) out = argv[i] + 6;
        else if (!strcmp(argv[i], "--key-file") && i + 1 < argc) key_file = argv[++i];
        else if (!strncmp(argv[i], "--key-file=", 11)) key_file = argv[i] + 11;
        else if (!strcmp(argv[i], "--key") && i + 1 < argc) secret = argv[++i];
        else if (!strncmp(argv[i], "--key=", 6)) secret = argv[i] + 6;
        else if (argv[i][0] != '-' && !dir) dir = argv[i];
    }
    if (!dir) { fprintf(stderr, "?uso: gmb pack <dir> [--out f.gmbpkg] [--key-file priv|--key segreto]\n"); return 2; }

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

    /* --dry-run: elenca i file che entrerebbero nell'archivio (nessuna scrittura). */
    {
        int dry = 0;
        for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "--dry-run") || !strcmp(argv[i], "-D")) dry = 1;
        if (dry) {
            gmb_pkg_pack_files_t* dl = gmb_pkg_pack_files(dir, "all");
            int dn = dl ? gmb_pkg_pack_files_count(dl) : 0;
            printf("DRY-RUN pack %s@%s -> %s (%d file)\n", name, ver, out, dn);
            for (int i = 0; i < dn; i++) printf("  %s\n", gmb_pkg_pack_files_get(dl, i));
            if (dl) gmb_pkg_pack_files_free(dl);
            return 0;
        }
    }

    char priv[512] = "";
    int sign = 0;
    if ((key_file && *key_file) || (secret && *secret)) {
        sign = read_priv_key(key_file, secret, priv, sizeof priv);
        if (!sign) { fprintf(stderr, "?pack: chiave non leggibile (--key-file %s)\n", key_file ? key_file : ""); return 1; }
    }

    int ok = sign ? gmb_pkg_pack_signed(dir, out, priv) : gmb_pkg_pack(dir, out, NULL);
    if (!ok) { fprintf(stderr, "?pack: creazione archivio fallita\n"); return 1; }
    gmb_pkg_pack_files_t* fl = gmb_pkg_pack_files(dir, "all");
    int nf = fl ? gmb_pkg_pack_files_count(fl) : 0;
    if (fl) gmb_pkg_pack_files_free(fl);
    printf("Pack: %s (%d file, firma: %s)\n", out, nf, sign ? "ed25519 (.gmbpkg.sig/.pub)" : "nessuna");
    return 0;
}

/* ── verify: firma detached (.gmbpkg.sig/.gmbpkg.pub) ─────────────────────── */

typedef struct { char* manifest; size_t mlen; int have_m; char* sig; int have_s; char* pub; int have_p; } sigbag_t;

static int sig_entry(const char* name, const unsigned char* content, size_t clen, void* ctx)
{
    sigbag_t* b = (sigbag_t*)ctx;
    char** slot = NULL;
    int* flag = NULL;
    if (!strcmp(name, "gmb-package.json")) { slot = &b->manifest; flag = &b->have_m; }
    else if (!strcmp(name, ".gmbpkg.sig")) { slot = &b->sig; flag = &b->have_s; }
    else if (!strcmp(name, ".gmbpkg.pub")) { slot = &b->pub; flag = &b->have_p; }
    else return 0;
    if (*flag) return 0;
    char* d = (char*)malloc(clen + 1);
    if (!d) return 0;
    memcpy(d, content, clen);
    d[clen] = 0;
    *slot = d;
    *flag = 1;
    if (name[1] == 'g') b->mlen = clen;
    return 0;
}

int gmbadmin_verify_pkg(int argc, char** argv)
{
    const char* file = NULL;
    for (int i = 3; i < argc; i++) if (argv[i][0] != '-') { file = argv[i]; break; }
    if (!file) { fprintf(stderr, "?uso: gmb pkg verify <file.gmbpkg>\n"); return 2; }
    FILE* f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "?verify: %s non trovato\n", file); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(data); return 1; }
    fclose(f);
    sigbag_t b;
    memset(&b, 0, sizeof b);
    gmb_zip_foreach_mem(data, (size_t)n, sig_entry, &b);
    free(data);
    int rc = 1;
    if (!b.have_s || !b.have_p) {
        printf("non firmato (%s)\n", b.have_s ? "manca .gmbpkg.pub" : "manca .gmbpkg.sig");
        rc = 0;   /* firma assente = accettata come "non firmato", come VerifyGmbpkg del C# */
    } else if (!b.have_m) {
        fprintf(stderr, "?verify: gmb-package.json assente nell'archivio\n");
    } else if (gmb_pkg_ed25519_verify(b.pub, (const unsigned char*)b.manifest, strlen(b.manifest), b.sig)) {
        printf("firma OK (ed25519) — %s\n", file);
        rc = 0;
    } else {
        fprintf(stderr, "?verify: FIRMA NON VALIDA — %s\n", file);
    }
    free(b.manifest); free(b.sig); free(b.pub);
    return rc;
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

/* ── register / admin (registry autenticato) ─────────────────────────────── */

static const char* cfg_token(gmb_val_t cfg) { return json_text(cfg, "Token"); }

static const char* argval(int argc, char** argv, const char* name)
{
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], name) && i + 1 < argc) return argv[i + 1];
        size_t n = strlen(name);
        if (!strncmp(argv[i], name, n) && argv[i][n] == '=') return argv[i] + n + 1;
    }
    return NULL;
}

static const char* argpos_from(int argc, char** argv, int start, int n)
{
    int seen = 0;
    for (int i = start; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strchr(argv[i], '=') && i + 1 < argc && argv[i + 1][0] != '-') i++;
            continue;
        }
        if (seen++ == n) return argv[i];
    }
    return NULL;
}

static const char* argpos(int argc, char** argv, int n) { return argpos_from(argc, argv, 2, n); }

/* `gmb admin <sub> [args]`: argv[2] e' il sottocomando, gli argomenti partono da argv[3]. */
static const char* asub_pos(int argc, char** argv, int n) { return argpos_from(argc, argv, 3, n); }

static int registry_of(int argc, char** argv, char* out, size_t cap)
{
    const char* r = argval(argc, argv, "--registry");
    if (r && *r) { snprintf(out, cap, "%s", r); return 0; }
    gmbadmin_registry_url(argc, argv, out, cap);
    return 0;
}

static void json_escape(const char* in, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = in; *p && o + 2 < cap; p++) {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = *p; }
        else out[o++] = *p;
    }
    out[o] = 0;
}

int gmbadmin_register(int argc, char** argv)
{
    const char* name = argpos(argc, argv, 0);
    if (!name) { fprintf(stderr, "?uso: gmb register <nome> [--email e] [--registry u]\n"); return 2; }
    const char* email = argval(argc, argv, "--email");
    char reg[512];
    registry_of(argc, argv, reg, sizeof reg);
    char es_name[512], es_email[512], body[1024];
    json_escape(name, es_name, sizeof es_name);
    json_escape(email ? email : "", es_email, sizeof es_email);
    snprintf(body, sizeof body, "{\"name\":\"%s\",\"email\":\"%s\"}", es_name, es_email);
    char url[1024];
    snprintf(url, sizeof url, "%s/api/users/register", reg);
    char out[4096] = "";
    int status = 0;
    if (!gmb_pkg_registry_post(url, body, NULL, out, sizeof out, &status)) {
        if (status == 0) fprintf(stderr, "?register: registry NON RAGGIUNGIBILE su %s\n", url);
        else fprintf(stderr, "?register fallito (%d) su %s: %s\n", status, url, out);
        return 1;
    }
    const char* t = strstr(out, "\"token\":\"");
    if (!t) { fprintf(stderr, "?register: token non restituito dal registry\n"); return 1; }
    t += 9;
    const char* e = strchr(t, '"');
    if (!e) return 1;
    char tok[512];
    size_t l = (size_t)(e - t);
    if (l >= sizeof tok) l = sizeof tok - 1;
    memcpy(tok, t, l); tok[l] = 0;

    gmb_val_t cfg = load_config();
    gmb_val_dict_set(cfg, "Token", gmb_make_val_string(tok));
    gmb_val_dict_set(cfg, "RegistryUrl", gmb_make_val_string(reg));
    save_config(cfg);
    printf("Registrato: %s (token salvato nel config utente)\n", name);
    return 0;
}

static int admin_post(const char* reg, const char* path, const char* body, const char* token, const char* label)
{
    char url[1024], out[4096] = "";
    snprintf(url, sizeof url, "%s%s", reg, path);
    int status = 0;
    if (!gmb_pkg_registry_post(url, body, (token && *token) ? token : NULL, out, sizeof out, &status)) {
        if (status == 0) fprintf(stderr, "?%s: registry NON RAGGIUNGIBILE su %s\n", label, url);
        else fprintf(stderr, "?%s fallito (%d) su %s: %s\n", label, status, url, out);
        return 1;
    }
    printf("%s\n", out);
    return 0;
}

int gmbadmin_admin(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "";
    char reg[512];
    registry_of(argc, argv, reg, sizeof reg);
    gmb_val_t cfg = load_config();
    const char* tok = argval(argc, argv, "--token");
    if (!tok) tok = cfg_token(cfg);

    if (!strcmp(sub, "reserve")) {
        const char* name = asub_pos(argc, argv, 0);
        if (!name) { fprintf(stderr, "?uso: gmb admin reserve <name>\n"); return 2; }
        char body[512], es[256];
        json_escape(name, es, sizeof es);
        snprintf(body, sizeof body, "{\"name\":\"%s\"}", es);
        return admin_post(reg, "/api/admin/reserve", body, tok, "reserve");
    }
    if (!strcmp(sub, "assign")) {
        const char* name = asub_pos(argc, argv, 0);
        const char* owner = asub_pos(argc, argv, 1);
        if (!name || !owner) { fprintf(stderr, "?uso: gmb admin assign <name> <owner>\n"); return 2; }
        char body[1024], a[256], b[256];
        json_escape(name, a, sizeof a); json_escape(owner, b, sizeof b);
        snprintf(body, sizeof body, "{\"name\":\"%s\",\"owner\":\"%s\"}", a, b);
        return admin_post(reg, "/api/admin/assign", body, tok, "assign");
    }
    if (!strcmp(sub, "revoke")) {
        const char* name = asub_pos(argc, argv, 0);
        if (!name) { fprintf(stderr, "?uso: gmb admin revoke <name> [--keep-reserved]\n"); return 2; }
        char body[512], a[256];
        json_escape(name, a, sizeof a);
        snprintf(body, sizeof body, "{\"name\":\"%s\",\"reserve\":\"%s\"}", a,
                 argval(argc, argv, "--keep-reserved") ? "1" : "0");
        return admin_post(reg, "/api/admin/revoke", body, tok, "revoke");
    }
    if (!strcmp(sub, "password")) {
        const char* nt = asub_pos(argc, argv, 0);
        if (!nt) { fprintf(stderr, "?uso: gmb admin password <newToken>\n"); return 2; }
        char body[1024], a[512];
        json_escape(nt, a, sizeof a);
        snprintf(body, sizeof body, "{\"newToken\":\"%s\"}", a);
        return admin_post(reg, "/api/admin/password", body, tok, "password");
    }
    if (!strcmp(sub, "user")) {
        const char* act = asub_pos(argc, argv, 0);
        if (!act) { fprintf(stderr, "?uso: gmb admin user <create|setkey|status|list> ...\n"); return 2; }
        if (!strcmp(act, "create")) {
            const char* name = asub_pos(argc, argv, 1);
            if (!name) return 2;
            char body[512], a[256];
            json_escape(name, a, sizeof a);
            snprintf(body, sizeof body, "{\"name\":\"%s\"}", a);
            return admin_post(reg, "/api/admin/users/create", body, tok, "user create");
        }
        if (!strcmp(act, "setkey")) {
            const char* name = asub_pos(argc, argv, 1);
            const char* key = asub_pos(argc, argv, 2);
            if (!name || !key) return 2;
            char body[2048], a[256], b[1024];
            json_escape(name, a, sizeof a); json_escape(key, b, sizeof b);
            snprintf(body, sizeof body, "{\"name\":\"%s\",\"publicKey\":\"%s\"}", a, b);
            return admin_post(reg, "/api/admin/users/setkey", body, tok, "user setkey");
        }
        if (!strcmp(act, "status")) {
            const char* name = asub_pos(argc, argv, 1);
            const char* st = asub_pos(argc, argv, 2);
            if (!name || !st) return 2;
            char body[512], a[256], b[64];
            json_escape(name, a, sizeof a); json_escape(st, b, sizeof b);
            snprintf(body, sizeof body, "{\"name\":\"%s\",\"status\":\"%s\"}", a, b);
            return admin_post(reg, "/api/admin/users/status", body, tok, "user status");
        }
        if (!strcmp(act, "list")) {
            char url[1024], out[8192] = "";
            snprintf(url, sizeof url, "%s/api/admin/users/list", reg);
            int status = 0;
            if (!gmb_pkg_registry_get_auth(url, (tok && *tok) ? tok : NULL, out, sizeof out, &status)) {
                if (status == 0) fprintf(stderr, "?user list: registry NON RAGGIUNGIBILE su %s\n", url);
                else fprintf(stderr, "?user list fallito (%d) su %s: %s\n", status, url, out);
                return 1;
            }
            printf("%s\n", out);
            return 0;
        }
    }
    fprintf(stderr, "?uso: gmb admin reserve|assign|revoke|password|user <create|setkey|status|list>\n");
    return 2;
}

/* ── publish --repo: verifica del repo contributor + register ─────────────── */

static int run_in(const char* dir, char* const av[], char* out, size_t cap)
{
    int fds[2];
    int have_pipe = (out && cap) ? (pipe(fds) == 0) : 0;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (dir && *dir && chdir(dir) != 0) _exit(127);
        if (have_pipe) { dup2(fds[1], 1); dup2(fds[1], 2); close(fds[0]); close(fds[1]); }
        execvp(av[0], av);
        _exit(127);
    }
    int st = 0;
    if (have_pipe) {
        close(fds[1]);
        size_t off = 0;
        ssize_t r;
        while ((r = read(fds[0], out + off, cap - 1 - off)) > 0 && off + (size_t)r < cap - 1) off += (size_t)r;
        out[off] = 0;
        close(fds[0]);
    }
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int gmbadmin_publish_repo(int argc, char** argv)
{
    const char* repo = NULL;
    const char* name_tag = NULL;
    for (int i = 2; i < argc; i++) {   /* argv[1] = "publish" */
        if (!strcmp(argv[i], "--repo") && i + 1 < argc) { repo = argv[++i]; continue; }
        if (!strncmp(argv[i], "--repo=", 7)) { repo = argv[i] + 7; continue; }
        if (argv[i][0] == '-') {
            if (!strchr(argv[i], '=') && i + 1 < argc && argv[i + 1][0] != '-') i++;
            continue;
        }
        if (!name_tag) name_tag = argv[i];
    }
    if (!repo || !name_tag) {
        fprintf(stderr, "?uso: gmb publish --repo <git-url|path> <nome@tag> [--registry url] [--token t]\n");
        return 2;
    }
    /* name@tag: separatore = ULTIMO '@' (nome può essere @scope/name) */
    char name[256], tag[128] = "HEAD";
    const char* at = strrchr(name_tag, '@');
    if (at && at != name_tag) {
        size_t l = (size_t)(at - name_tag);
        if (l >= sizeof name) l = sizeof name - 1;
        memcpy(name, name_tag, l); name[l] = 0;
        snprintf(tag, sizeof tag, "%s", at + 1);
    } else snprintf(name, sizeof name, "%s", name_tag);

    char work[1200];
    const char* tmp = getenv("TMPDIR");
    snprintf(work, sizeof work, "%s/gmb_repo_%ld", (tmp && *tmp) ? tmp : "/tmp", (long)getpid());
    gmb_fs_rmtree(work);
    gmb_fs_mkdir_p(work);

    char out[8192];
    char* av_git_init[] = { "git", "init", "-q", NULL };
    if (run_in(work, av_git_init, out, sizeof out) != 0) {
        fprintf(stderr, "?publish --repo: git init fallito in %s: %s\n", work, out);
        gmb_fs_rmtree(work);
        return 1;
    }
    char* av_remote[] = { "git", "remote", "add", "origin", (char*)repo, NULL };
    if (run_in(work, av_remote, out, sizeof out) != 0) {
        fprintf(stderr, "?publish --repo: remote add fallito: %s\n", out);
        gmb_fs_rmtree(work);
        return 1;
    }
    char ref[256];
    snprintf(ref, sizeof ref, "%s", !strcmp(tag, "HEAD") ? "HEAD" : "");
    char tagref[300];
    if (ref[0]) snprintf(tagref, sizeof tagref, "%s", ref);
    else snprintf(tagref, sizeof tagref, "refs/tags/%s", tag);
    char* av_fetch[] = { "git", "fetch", "-q", "--depth", "1", "origin", tagref, NULL };
    if (run_in(work, av_fetch, out, sizeof out) != 0) {
        fprintf(stderr, "?publish --repo: fetch di '%s' fallito: %s\n", tagref, out);
        gmb_fs_rmtree(work);
        return 1;
    }
    char* av_co[] = { "git", "checkout", "-q", "FETCH_HEAD", NULL };
    if (run_in(work, av_co, out, sizeof out) != 0) {
        fprintf(stderr, "?publish --repo: checkout fallito: %s\n", out);
        gmb_fs_rmtree(work);
        return 1;
    }

    /* [1] manifest in root e valido */
    char mp[1600];
    snprintf(mp, sizeof mp, "%s/gmb-package.json", work);
    FILE* mf = fopen(mp, "rb");
    if (!mf) { fprintf(stderr, "?publish --repo: gmb-package.json mancante in root del repo\n"); gmb_fs_rmtree(work); return 1; }
    fseek(mf, 0, SEEK_END); long ml = ftell(mf); fseek(mf, 0, SEEK_SET);
    char* mjs = (char*)malloc((size_t)ml + 1);
    if (!mjs || fread(mjs, 1, (size_t)ml, mf) != (size_t)ml) { fclose(mf); free(mjs); gmb_fs_rmtree(work); return 1; }
    mjs[ml] = 0; fclose(mf);
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(mjs);
    if (!man) { free(mjs); fprintf(stderr, "?publish --repo: manifest non valido\n"); gmb_fs_rmtree(work); return 1; }
    char mname[256] = "", mver[64] = "";
    gmb_pkg_manifest_get(man, "name", mname, sizeof mname);
    gmb_pkg_manifest_get(man, "version", mver, sizeof mver);
    gmb_pkg_manifest_free(man);
    if (!mname[0] || !mver[0]) { free(mjs); fprintf(stderr, "?publish --repo: name/version mancanti\n"); gmb_fs_rmtree(work); return 1; }
    printf("  [1] manifest OK: %s@%s\n", mname, mver);

    /* [2] il manifest è versionato */
    char* av_ls[] = { "git", "ls-files", "--error-unmatch", "gmb-package.json", NULL };
    if (run_in(work, av_ls, out, sizeof out) != 0) {
        free(mjs);
        fprintf(stderr, "?publish --repo: gmb-package.json non è versionato nel repo\n");
        gmb_fs_rmtree(work);
        return 1;
    }
    printf("  [2] git OK (manifest versionato, tag: %s)\n", tag);

    /* [3]/[4] compilazione + self-test: `gmb run --project <work> --entry <entry>`
     * (il compilatore C compila ED esegue; l'output è il self-test). */
    const char* entry = NULL;
    char entry_buf[1024] = "";
    {
        extern int gmbadmin_find_bas(const char* dir, const char* rel, char* out, size_t cap);
        if (gmbadmin_find_bas(work, "", entry_buf, sizeof entry_buf) && entry_buf[0]) entry = entry_buf;
    }
    char gmb[2048];
    const char* path = getenv("PATH");
    int have_gmb = 0;
    if (path) {
        char tmp2[4096];
        snprintf(tmp2, sizeof tmp2, "%s", path);
        for (char* dd = strtok(tmp2, ":"); dd; dd = strtok(NULL, ":")) {
            char cand[4096];
            snprintf(cand, sizeof cand, "%s/gmb", dd);
            if (access(cand, X_OK) == 0) { snprintf(gmb, sizeof gmb, "%s", cand); have_gmb = 1; break; }
        }
    }
    if (!have_gmb) {
        /* checkout: `gmb` accanto a gmbadmin (native/gmb_compiler_c/build/gmb) */
        char exe[4096] = "";
#if defined(__APPLE__)
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) == 0) {
#else
        {
            ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
            if (n > 0) { exe[n] = 0;
#endif
            char* sl = strrchr(exe, '/');
            if (sl) {
                *sl = 0;
                static const char* rels[] = {
                    "../gmb_compiler_c/build/gmb", "../../gmb_compiler_c/build/gmb",
                    "../../../gmb_compiler_c/build/gmb", NULL
                };
                for (int i = 0; rels[i]; i++) {
                    char c[4200];
                    snprintf(c, sizeof c, "%s/%s", exe, rels[i]);
                    if (access(c, X_OK) == 0) { snprintf(gmb, sizeof gmb, "%s", c); have_gmb = 1; break; }
                }
            }
        }
    }
    char test_output[4096] = "";
    int test_exit = 0;
    if (entry && have_gmb) {
        char* av_run[] = { gmb, "run", "--project", work, "--entry", entry, "--quiet", NULL };
        int rc = run_in(NULL, av_run, test_output, sizeof test_output);
        if (rc != 0) {
            free(mjs);
            fprintf(stderr, "?publish --repo: compilazione/esecuzione dell'entry '%s' FALLITA (exit %d):\n%s\n",
                    entry, rc, test_output);
            gmb_fs_rmtree(work);
            return 1;
        }
        test_exit = 0;
        printf("  [3] compilazione OK (%s)\n", entry);
        printf("  [4] self-test OK (exit 0, %zu byte output)\n", strlen(test_output));
    } else {
        printf("  [3] compilazione SALTATA: nessuna entry eseguibile (o `gmb` non nel PATH)\n");
        printf("  [4] self-test SALTATO (niente da eseguire)\n");
    }

    /* commit */
    char commit[128] = "";
    {
        char* av_head[] = { "git", "rev-parse", "HEAD", NULL };
        char out2[256] = "";
        if (run_in(work, av_head, out2, sizeof out2) == 0) {
            snprintf(commit, sizeof commit, "%s", out2);
            char* nl = strchr(commit, '\n');
            if (nl) *nl = 0;
        }
    }
    (void)commit;

    char registry[512];
    gmbadmin_registry_url(argc, argv, registry, sizeof registry);
    const char* token = NULL;
    for (int i = 2; i < argc; i++)
        if (!strcmp(argv[i], "--token") && i + 1 < argc) token = argv[i + 1];

    char* report = NULL;
    {
        /* verify_report: manifest completo (manifestJson) + esito test (max 2000 byte) */
        char out_esc[4200];
        size_t k = 0;
        const char* to = test_output;
        size_t tl = strlen(to);
        if (tl > 2000) tl = 2000;
        for (size_t i = 0; i < tl && k < sizeof out_esc - 8; i++) {
            unsigned char c = (unsigned char)to[i];
            if (c == '"' || c == '\\') { out_esc[k++] = '\\'; out_esc[k++] = (char)c; }
            else if (c == '\n') { out_esc[k++] = '\\'; out_esc[k++] = 'n'; }
            else if (c == '\r') { out_esc[k++] = '\\'; out_esc[k++] = 'r'; }
            else if (c == '\t') { out_esc[k++] = '\\'; out_esc[k++] = 't'; }
            else if (c < 0x20) { k += (size_t)snprintf(out_esc + k, sizeof out_esc - k, "\\u%04x", c); }
            else out_esc[k++] = (char)c;
        }
        out_esc[k] = 0;
        char mjs_esc[8192];
        size_t mk = 0;
        for (const char* q = mjs; *q && mk < sizeof mjs_esc - 8; q++) {
            unsigned char c = (unsigned char)*q;
            if (c == '"' || c == '\\') { mjs_esc[mk++] = '\\'; mjs_esc[mk++] = (char)c; }
            else if (c == '\n') { mjs_esc[mk++] = '\\'; mjs_esc[mk++] = 'n'; }
            else if (c == '\r') { mjs_esc[mk++] = '\\'; mjs_esc[mk++] = 'r'; }
            else if (c < 0x20) { mk += (size_t)snprintf(mjs_esc + mk, sizeof mjs_esc - mk, "\\u%04x", c); }
            else mjs_esc[mk++] = (char)c;
        }
        mjs_esc[mk] = 0;
        report = (char*)malloc(sizeof mjs_esc + sizeof out_esc + strlen(repo) + 2048);
        if (!report) { free(mjs); gmb_fs_rmtree(work); return 1; }
        sprintf(report,
                "{\"repo\":\"%s\",\"name\":\"%s\",\"tag\":\"%s\",\"verify_report\":{"
                "\"name\":\"%s\",\"repo\":\"%s\",\"tag\":\"%s\","
                "\"manifest\":{\"name\":\"%s\",\"version\":\"%s\"},"
                "\"manifestJson\":\"%s\",\"backend\":\"c\",\"testOutput\":\"%s\","
                "\"testExitCode\":%d,\"commit\":\"%s\"}}",
                repo, name, tag, name, repo, tag, mname, mver, mjs_esc, out_esc, test_exit, commit);
    }
    free(mjs);

    /* [5] register */
    char url[1024], resp[8192] = "";
    snprintf(url, sizeof url, "%s/api/packages/register", registry);
    int status = 0;
    if (!gmb_pkg_registry_post(url, report, (token && *token) ? token : NULL, resp, sizeof resp, &status)) {
        if (status == 0) fprintf(stderr, "?publish --repo: registry NON RAGGIUNGIBILE su %s\n", url);
        else fprintf(stderr, "?publish --repo: registrazione fallita (%d): %s\n", status, resp);
        free(report);
        gmb_fs_rmtree(work);
        return 1;
    }
    free(report);
    printf("  [5] registrato: %s@%s (status %d)\n", name, tag, status);

    /* [6] pubblica l'ARTEFATTO (.gmbpkg) così il pacchetto è installabile */
    {
        char pkg[1400];
        snprintf(pkg, sizeof pkg, "%s/%s-%s.gmbpkg", work, name, mver);
        if (gmb_pkg_pack(work, pkg, NULL)) {
            /* riusa la POST base64 di `publish <file>` */
            FILE* f = fopen(pkg, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long n = ftell(f);
                fseek(f, 0, SEEK_SET);
                unsigned char* data = (unsigned char*)malloc((size_t)n);
                if (data && fread(data, 1, (size_t)n, f) == (size_t)n) {
                    fclose(f);
                    char* b64 = gmb_base64_encode(data, (size_t)n);
                    free(data);
                    if (b64) {
                        size_t plen = strlen(b64) + strlen(name) + strlen(mver) + 256;
                        char* payload = (char*)malloc(plen);
                        if (payload) {
                            snprintf(payload, plen,
                                     "{\"name\":\"%s\",\"version\":\"%s\",\"file\":\"%s\",\"token\":\"%s\"}",
                                     name, mver, b64, token ? token : "");
                            char purl[1200], presp[4096] = "";
                            int pst = 0;
                            snprintf(purl, sizeof purl, "%s/api/packages", registry);
                            if (gmb_pkg_registry_post(purl, payload, (token && *token) ? token : NULL,
                                                      presp, sizeof presp, &pst)) {
                                printf("  [6] pubblicato: %s@%s (installabile)\n", name, mver);
                            } else {
                                printf("  [6] pubblicazione SALTATA (%d): %s\n", pst, presp);
                            }
                            free(payload);
                        }
                        free(b64);
                    }
                } else {
                    fclose(f);
                    free(data);
                }
                unlink(pkg);
            }
        } else {
            printf("  [6] pack SALTATO (manifest senza file impacchettabili)\n");
        }
    }

    /* [7] materializzazione degli asset binari dichiarati in manifest.binaries */
    {
        char mpath2[1600];
        snprintf(mpath2, sizeof mpath2, "%s/gmb-package.json", work);
        char* mj = NULL;
        FILE* f = fopen(mpath2, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            mj = (char*)malloc((size_t)n + 1);
            if (mj && fread(mj, 1, (size_t)n, f) != (size_t)n) { free(mj); mj = NULL; }
            if (mj) mj[n] = 0;
            fclose(f);
        }
        gmb_val_t root = mj ? gmb_json_parse(mj) : (gmb_val_t){0};
        free(mj);
        gmb_val_t bins = gmb_val_dict_get(root, "binaries");
        if (bins.kind == GMB_TYPE_OBJECT) {
            int nassets = 0, failed = 0;
            char* assets = (char*)malloc(1 << 20);
            size_t alen = 0;
            if (assets) { strcpy(assets, "{"); alen = 1; }
            gmb_int_t nb = 0;
            gmb_string_t* bkeys = gmb_val_dict_keys_array(bins, &nb);
            for (int i = 0; bkeys && i < (int)nb; i++) {
                if (!bkeys[i].data) continue;
                gmb_val_t bv = gmb_val_dict_get(bins, bkeys[i].data);
                if (bv.kind == GMB_TYPE_OBJECT) {
                    gmb_int_t np = 0;
                    gmb_string_t* pkeys = gmb_val_dict_keys_array(bv, &np);
                    for (int k = 0; pkeys && k < (int)np; k++) {
                        if (!pkeys[k].data) continue;
                        gmb_val_t uv = gmb_val_dict_get(bv, pkeys[k].data);
                        if (uv.kind != GMB_TYPE_STRING || !uv.v_string.data) continue;
                        if (strncmp(uv.v_string.data, "http", 4) != 0) continue;
                        char tmpf[1200];
                        snprintf(tmpf, sizeof tmpf, "%s/.asset-%d-%d", work, i, k);
                        gmb_fetch_request rq;
                        memset(&rq, 0, sizeof rq);
                        rq.url = uv.v_string.data;
                        rq.dest_path = tmpf;
                        gmb_fetch_opts o;
                        gmb_fetch_opts_default(&o);
                        o.quiet = 1;
                        char err[256] = "";
                        if (gmb_fetch_to_file(&rq, &o, gmb_fetch_transport_curl(), err, sizeof err) != 0) {
                            failed++;
                            printf("  [7] asset %s/%s NON scaricato (%s)\n", bkeys[i].data, pkeys[k].data, err);
                            continue;
                        }
                        FILE* af = fopen(tmpf, "rb");
                        if (af) {
                            fseek(af, 0, SEEK_END);
                            long an = ftell(af);
                            fseek(af, 0, SEEK_SET);
                            unsigned char* ab = (unsigned char*)malloc((size_t)an);
                            if (ab && fread(ab, 1, (size_t)an, af) == (size_t)an) {
                                char* ab64 = gmb_base64_encode(ab, (size_t)an);
                                if (ab64 && assets) {
                                    size_t need = strlen(ab64) + strlen(bkeys[i].data) + strlen(pkeys[k].data) + 64;
                                    if (alen + need + 4 < (1 << 20)) {
                                        alen += (size_t)sprintf(assets + alen, "%s\"%s/%s\":\"%s\"",
                                                                nassets ? "," : "", bkeys[i].data, pkeys[k].data, ab64);
                                        nassets++;
                                    }
                                }
                                free(ab64);
                            }
                            free(ab);
                            fclose(af);
                        }
                        unlink(tmpf);
                    }
                } else if (bv.kind == GMB_TYPE_STRING && bv.v_string.data && !strncmp(bv.v_string.data, "http", 4)) {
                    char tmpf[1200];
                    snprintf(tmpf, sizeof tmpf, "%s/.asset-any-%d", work, i);
                    gmb_fetch_request rq;
                    memset(&rq, 0, sizeof rq);
                    rq.url = bv.v_string.data;
                    rq.dest_path = tmpf;
                    gmb_fetch_opts o;
                    gmb_fetch_opts_default(&o);
                    o.quiet = 1;
                    char err[256] = "";
                    if (gmb_fetch_to_file(&rq, &o, gmb_fetch_transport_curl(), err, sizeof err) == 0) {
                        FILE* af = fopen(tmpf, "rb");
                        if (af) {
                            fseek(af, 0, SEEK_END);
                            long an = ftell(af);
                            fseek(af, 0, SEEK_SET);
                            unsigned char* ab = (unsigned char*)malloc((size_t)an);
                            if (ab && fread(ab, 1, (size_t)an, af) == (size_t)an) {
                                char* ab64 = gmb_base64_encode(ab, (size_t)an);
                                if (ab64 && assets) {
                                    size_t need = strlen(ab64) + strlen(bkeys[i].data) + 64;
                                    if (alen + need + 4 < (1 << 20)) {
                                        alen += (size_t)sprintf(assets + alen, "%s\"any/%s\":\"%s\"",
                                                                nassets ? "," : "", bkeys[i].data, ab64);
                                        nassets++;
                                    }
                                }
                                free(ab64);
                            }
                            free(ab);
                            fclose(af);
                        }
                        unlink(tmpf);
                    } else failed++;
                }
            }
            if (assets) strcat(assets, "}");
            if (nassets > 0) {
                char murl[1200], mresp[4096] = "";
                int mst = 0;
                snprintf(murl, sizeof murl, "%s/api/packages/%s/materialize", registry, name);
                size_t plen = strlen(assets) + strlen(name) + strlen(mver) + 256;
                char* payload = (char*)malloc(plen);
                if (payload) {
                    snprintf(payload, plen, "{\"name\":\"%s\",\"version\":\"%s\",\"assets\":%s}", name, mver, assets);
                    if (gmb_pkg_registry_post(murl, payload, (token && *token) ? token : NULL, mresp, sizeof mresp, &mst))
                        printf("  [7] materialize: %d asset (status %d)\n", nassets, mst);
                    else
                        printf("  [7] materialize SALTATO (%d): %s\n", mst, mresp);
                    free(payload);
                }
            } else {
                printf("  [7] materialize: nessun asset binario da caricare (%d errori)\n", failed);
            }
            free(assets);
        } else {
            printf("  [7] materialize: manifest senza binaries (source-only)\n");
        }
    }

    gmb_fs_rmtree(work);
    return 0;
}

/* Primo sample .bas eseguibile sotto `dir` (path relativo). */
int gmbadmin_find_bas(const char* dir, const char* rel, char* out, size_t cap)
{
    char d[2048];
    if (rel && *rel) snprintf(d, sizeof d, "%s/%s", dir, rel);
    else snprintf(d, sizeof d, "%s", dir);
    DIR* dh = opendir(d);
    if (!dh) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(dh))) {
        if (e->d_name[0] == '.') continue;
        if (!strcmp(e->d_name, "bin") || !strcmp(e->d_name, "obj") ||
            !strcmp(e->d_name, "build") || !strcmp(e->d_name, "work") ||
            !strcmp(e->d_name, ".git")) continue;
        char r[2048];
        if (rel && *rel) snprintf(r, sizeof r, "%s/%s", rel, e->d_name);
        else snprintf(r, sizeof r, "%s", e->d_name);
        char full[2400];
        snprintf(full, sizeof full, "%s/%s", dir, r);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISREG(st.st_mode)) {
            size_t l = strlen(e->d_name);
            if (l > 4 && !strcasecmp(e->d_name + l - 4, ".bas")) {
                snprintf(out, cap, "%s", r);
                found = 1;
            }
        } else if (S_ISDIR(st.st_mode)) {
            found = gmbadmin_find_bas(dir, r, out, cap);
        }
    }
    closedir(dh);
    return found;
}
