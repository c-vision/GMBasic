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
