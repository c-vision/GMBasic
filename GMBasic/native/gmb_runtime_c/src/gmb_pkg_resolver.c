/* gmb_pkg_resolver.c — resolver del Package Manager (riferimento C, "tutto in C"). */
#include "gmb_pkg_resolver.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_lock.h"
#include "gmb_pkg_registry.h"
#include "gmb_pkg_util.h"
#include "gmb_semver.h"
#include "gmb_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static char* dupstr(const char* s) { return strdup(s ? s : ""); }

struct gmb_pkg_resolve_ctx {
    char* cache_root; char* vendor_dir; char* backend; char* platform; int offline;
    char* registry_url;
    char** extra_roots; int n_extra; int cap_extra;   /* root locali (app), PRIMA della cache */
    char** req_names; char** req_ranges; int n_req; int cap_req;
    char** ovr_names; char** ovr_ranges; int n_ovr; int cap_ovr;
    char** features; int n_feat; int cap_feat;
    char** locked_names; char** locked_versions; int n_locked; int cap_locked;
    char** local_names; char** local_dirs; int n_local; int cap_local;
    char** rscope_scopes; char** rscope_urls; int n_rscope; int cap_rscope;
};

typedef struct {
    char* name; char* version; char* install_dir; char* lock_entry;
} resolved_t;

struct gmb_pkg_resolve_result {
    resolved_t* items;
    int count;
    char* error;
};

/* --- builder --- */
static void push2(char*** a, char*** b, int* n, int* cap, const char* x, const char* y)
{
    if (*n >= *cap) { *cap = *cap ? *cap * 2 : 8; *a = (char**)realloc(*a, sizeof(char*) * (*cap)); *b = (char**)realloc(*b, sizeof(char*) * (*cap)); }
    (*a)[*n] = dupstr(x); (*b)[*n] = dupstr(y); (*n)++;
}
static void push1(char*** v, int* n, int* cap, const char* x)
{
    if (*n >= *cap) { *cap = *cap ? *cap * 2 : 8; *v = (char**)realloc(*v, sizeof(char*) * (*cap)); }
    (*v)[(*n)++] = dupstr(x);
}

gmb_pkg_resolve_ctx_t* gmb_pkg_resolve_ctx_new(void)
{
    return (gmb_pkg_resolve_ctx_t*)calloc(1, sizeof(gmb_pkg_resolve_ctx_t));
}
void gmb_pkg_resolve_ctx_set(gmb_pkg_resolve_ctx_t* c, const char* cache_root, const char* vendor_dir,
                             const char* backend, const char* platform, const char* edition, int offline)
{
    if (!c) return;
    free(c->cache_root); free(c->vendor_dir); free(c->backend); free(c->platform);
    c->cache_root = dupstr(cache_root); c->vendor_dir = dupstr(vendor_dir); c->backend = dupstr(backend);
    (void)edition;   /* ignorato: non esiste piu' la matrice Edition (ABI invariata) */
    c->platform = dupstr(platform); c->offline = offline;
}
void gmb_pkg_resolve_ctx_set_registry(gmb_pkg_resolve_ctx_t* c, const char* registry_url)
{
    if (!c) return;
    free(c->registry_url);
    c->registry_url = dupstr(registry_url);
}
/* Root di ricerca AGGIUNTIVA (installazione LOCALE dell'app, <dir dell'eseguibile>/packages):
 * ha la PRECEDENZA sulla cache. `cache_root` resta la destinazione dei download. */
void gmb_pkg_resolve_ctx_add_cache_root(gmb_pkg_resolve_ctx_t* c, const char* dir)
{
    if (!c || !dir || !*dir) return;
    if (c->n_extra >= c->cap_extra) {
        int nc = c->cap_extra ? c->cap_extra * 2 : 4;
        char** p = (char**)realloc(c->extra_roots, sizeof(char*) * (size_t)nc);
        if (!p) return;
        c->extra_roots = p; c->cap_extra = nc;
    }
    c->extra_roots[c->n_extra++] = dupstr(dir);
}
void gmb_pkg_resolve_ctx_add_req(gmb_pkg_resolve_ctx_t* c, const char* name, const char* range) { if (c) push2(&c->req_names, &c->req_ranges, &c->n_req, &c->cap_req, name, range); }
void gmb_pkg_resolve_ctx_add_override(gmb_pkg_resolve_ctx_t* c, const char* name, const char* range) { if (c) push2(&c->ovr_names, &c->ovr_ranges, &c->n_ovr, &c->cap_ovr, name, range); }
void gmb_pkg_resolve_ctx_add_feature(gmb_pkg_resolve_ctx_t* c, const char* f) { if (c) push1(&c->features, &c->n_feat, &c->cap_feat, f); }
void gmb_pkg_resolve_ctx_add_locked(gmb_pkg_resolve_ctx_t* c, const char* name, const char* version) { if (c) push2(&c->locked_names, &c->locked_versions, &c->n_locked, &c->cap_locked, name, version); }
void gmb_pkg_resolve_ctx_add_local(gmb_pkg_resolve_ctx_t* c, const char* name, const char* dir) { if (c) push2(&c->local_names, &c->local_dirs, &c->n_local, &c->cap_local, name, dir); }
void gmb_pkg_resolve_ctx_add_registry_scope(gmb_pkg_resolve_ctx_t* c, const char* scope, const char* url) {
    if (!c) return;
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", url ? url : "");
    size_t l = strlen(tmp);
    while (l > 0 && tmp[l - 1] == '/') tmp[--l] = 0;   /* niente '/' finale: evita "//download" */
    push2(&c->rscope_scopes, &c->rscope_urls, &c->n_rscope, &c->cap_rscope, scope, tmp);
}

void gmb_pkg_resolve_ctx_free(gmb_pkg_resolve_ctx_t* c)
{
    if (!c) return;
    free(c->cache_root); free(c->vendor_dir); free(c->backend); free(c->platform); free(c->registry_url);
    for (int i = 0; i < c->n_extra; i++) free(c->extra_roots[i]);
    free(c->extra_roots);
    for (int i = 0; i < c->n_req; i++) { free(c->req_names[i]); free(c->req_ranges[i]); }
    free(c->req_names); free(c->req_ranges);
    for (int i = 0; i < c->n_ovr; i++) { free(c->ovr_names[i]); free(c->ovr_ranges[i]); }
    free(c->ovr_names); free(c->ovr_ranges);
    for (int i = 0; i < c->n_feat; i++) free(c->features[i]);
    free(c->features);
    for (int i = 0; i < c->n_locked; i++) { free(c->locked_names[i]); free(c->locked_versions[i]); }
    free(c->locked_names); free(c->locked_versions);
    for (int i = 0; i < c->n_local; i++) { free(c->local_names[i]); free(c->local_dirs[i]); }
    free(c->local_names); free(c->local_dirs);
    for (int i = 0; i < c->n_rscope; i++) { free(c->rscope_scopes[i]); free(c->rscope_urls[i]); }
    free(c->rscope_scopes); free(c->rscope_urls);
    free(c);
}

/* --- helper I/O --- */
static void safe_dir(const char* name, char* out, size_t cap)
{
    size_t j = 0;
    for (const char* p = (name ? name : ""); *p && j + 1 < cap; p++) out[j++] = (*p == '/' || *p == '\\') ? '_' : *p;
    out[j] = 0;
}
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
static int dir_exists(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int find_base(const gmb_pkg_resolve_ctx_t* ctx, const char* name, char* out, size_t cap)
{
    char d[1024];
    safe_dir(name, d, sizeof(d));
    /* ORDINE (documentato, identico al compilatore C): vendor -> locale (<exe>/packages) -> cache.
     * Prima il vendor veniva DOPO la cache: un pacchetto venduto (pin offline) poteva essere
     * scavalcato da una copia in cache, mentre il compilatore C sceglieva quella venduta —
     * due mondi, due versioni. */
    if (ctx->vendor_dir && ctx->vendor_dir[0]) {
        snprintf(out, cap, "%s/%s", ctx->vendor_dir, d);
        if (dir_exists(out)) return 1;
    }
    for (int i = 0; i < ctx->n_extra; i++) {
        if (!ctx->extra_roots[i] || !ctx->extra_roots[i][0]) continue;
        snprintf(out, cap, "%s/%s", ctx->extra_roots[i], d);
        if (dir_exists(out)) return 1;
    }
    snprintf(out, cap, "%s/%s", ctx->cache_root, d);
    return dir_exists(out);
}
static int list_versions(const char* base, char*** out, int* n)
{
    DIR* d = opendir(base);
    if (!d) return 0;
    int cap = 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[4096];
        snprintf(sub, sizeof(sub), "%s/%s", base, de->d_name);
        if (!dir_exists(sub)) continue;
        if (*n >= cap) { cap = cap ? cap * 2 : 8; *out = (char**)realloc(*out, sizeof(char*) * cap); }
        (*out)[(*n)++] = dupstr(de->d_name);
    }
    closedir(d);
    return 1;
}

struct rng { char* name; char** ranges; int n; int cap; };
struct q { char** names; char** ranges; int n; int cap; };

static void cmap_add(struct rng** m, int* nm, int* cap, const char* name, const char* range)
{
    for (int i = 0; i < *nm; i++)
        if (strcmp((*m)[i].name, name) == 0) {
            struct rng* e = &(*m)[i];
            if (e->n >= e->cap) { e->cap = e->cap ? e->cap * 2 : 4; e->ranges = (char**)realloc(e->ranges, sizeof(char*) * e->cap); }
            e->ranges[e->n++] = dupstr(range ? range : "*");
            return;
        }
    if (*nm >= *cap) { *cap = *cap ? *cap * 2 : 8; *m = (struct rng*)realloc(*m, sizeof(struct rng) * (*cap)); }
    struct rng* e = &(*m)[(*nm)++];
    memset(e, 0, sizeof(*e));
    e->name = dupstr(name);
    e->cap = 4; e->ranges = (char**)malloc(sizeof(char*) * 4); e->ranges[e->n++] = dupstr(range ? range : "*");
}
static struct rng* cmap_find(struct rng* m, int nm, const char* name) { for (int i = 0; i < nm; i++) if (strcmp(m[i].name, name) == 0) return &m[i]; return NULL; }
static int has_name(char** v, int n, const char* s) { for (int i = 0; i < n; i++) if (strcmp(v[i], s) == 0) return 1; return 0; }
static void namel_add(char*** v, int* n, int* cap, const char* s)
{
    if (has_name(*v, *n, s)) return;
    if (*n >= *cap) { *cap = *cap ? *cap * 2 : 8; *v = (char**)realloc(*v, sizeof(char*) * (*cap)); }
    (*v)[(*n)++] = dupstr(s);
}
static void qpush(struct q* Q, const char* name, const char* range)
{
    if (Q->n >= Q->cap) { Q->cap = Q->cap ? Q->cap * 2 : 16; Q->names = (char**)realloc(Q->names, sizeof(char*) * Q->cap); Q->ranges = (char**)realloc(Q->ranges, sizeof(char*) * Q->cap); }
    Q->names[Q->n] = dupstr(name); Q->ranges[Q->n] = dupstr(range ? range : "*"); Q->n++;
}
static const char* ovr_for(const gmb_pkg_resolve_ctx_t* ctx, const char* name) { for (int i = 0; i < ctx->n_ovr; i++) if (strcmp(ctx->ovr_names[i], name) == 0) return ctx->ovr_ranges[i]; return NULL; }
/* Override ANNIDATO ("parent>child"): si applica al range accodato per `child` quando e' `parent`
 * ad accodarlo (riscrive il vincolo di QUELLA dipendenza del parent). */
static const char* ovr_nested_for(const gmb_pkg_resolve_ctx_t* ctx, const char* parent, const char* name) {
    if (!parent || !parent[0] || !name || !name[0]) return NULL;
    char key[600];
    snprintf(key, sizeof(key), "%s>%s", parent, name);
    for (int i = 0; i < ctx->n_ovr; i++) if (strcmp(ctx->ovr_names[i], key) == 0) return ctx->ovr_ranges[i];
    return NULL;
}
static const char* locked_for(const gmb_pkg_resolve_ctx_t* ctx, const char* name) { for (int i = 0; i < ctx->n_locked; i++) if (strcmp(ctx->locked_names[i], name) == 0) return ctx->locked_versions[i]; return NULL; }
static const char* local_for(const gmb_pkg_resolve_ctx_t* ctx, const char* name) { for (int i = 0; i < ctx->n_local; i++) if (strcmp(ctx->local_names[i], name) == 0) return ctx->local_dirs[i]; return NULL; }
/* Registry per SCOPE ("@org/name" → url dedicato), fallback al registry di default. Le chiavi
 * possono essere "@org" o "org". */
static const char* registry_for(const gmb_pkg_resolve_ctx_t* ctx, const char* name) {
    if (name && name[0] == '@') {
        const char* slash = strchr(name, '/');
        size_t slen = slash ? (size_t)(slash - name) : strlen(name);
        for (int i = 0; i < ctx->n_rscope; i++) {
            const char* sc = ctx->rscope_scopes[i];
            if (!sc || !sc[0]) continue;
            size_t l = strlen(sc);
            if (sc[0] == '@' && l == slen && strncmp(sc, name, slen) == 0) return ctx->rscope_urls[i];
            if (sc[0] != '@' && l + 1 == slen && strncmp(sc, name + 1, l) == 0) return ctx->rscope_urls[i];
        }
    }
    return ctx->registry_url;
}

static void ct_name(gmb_pkg_manifest_t* man, char* out, size_t cap)
{
    char ct[64];
    gmb_pkg_manifest_get(man, "contentType", ct, sizeof(ct));
    for (char* p = ct; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strcmp(ct, "dotnet") == 0) snprintf(out, cap, "DotNet");
    else if (strcmp(ct, "vm") == 0 || strcmp(ct, "vm-chunk") == 0 || strcmp(ct, "vmc") == 0) snprintf(out, cap, "VmChunk");
    else if (strcmp(ct, "c-ffi") == 0 || strcmp(ct, "cffi") == 0) snprintf(out, cap, "CFFI");
    else if (strcmp(ct, "c-ffi-source") == 0 || strcmp(ct, "cffi-source") == 0) snprintf(out, cap, "CFFISource");
    else if (strcmp(ct, "mixed") == 0) snprintf(out, cap, "Mixed");
    else if (strcmp(ct, "docs-only") == 0) snprintf(out, cap, "DocsOnly");
    else snprintf(out, cap, "Source");
}

static int is_bare_tag(const char* c)
{
    if (!c || !*c) return 0;
    const char* p = c;
    while (*p == ' ') p++;
    if (strcasecmp(p, "latest") == 0) return 0;
    gmb_semver_t v;
    if (gmb_semver_parse(p, &v)) return 0;
    if (strchr(p, '^') || strchr(p, '~') || strchr(p, '*') || strchr(p, '|')) return 0;
    if (p[0] == '>' || p[0] == '<' || p[0] == '=') return 0;
    return isalpha((unsigned char)p[0]) != 0;
}

static void mkdir_p(const char* path)
{
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

static int pick(const gmb_pkg_resolve_ctx_t* ctx, struct rng* e, char** vers, int nv, char* out, size_t cap)
{
    const char* lk = locked_for(ctx, e->name);
    if (lk && lk[0]) { if (has_name(vers, nv, lk)) { snprintf(out, cap, "%s", lk); return 1; } return 0; }
    const char* ovr = ovr_for(ctx, e->name);
    if (ovr && ovr[0]) return gmb_semver_find_best(1, &ovr, nv, (const char* const*)vers, out, cap);
    /* risolvi i bare tag (es. "next") via registry → versioni concrete */
    char* cons[256];
    int nc = 0;
    for (int i = 0; i < e->n && nc < 256; i++) {
        if (is_bare_tag(e->ranges[i])) {
            char tv[64];
            const char* rurl = registry_for(ctx, e->name);
            if (rurl && rurl[0] && gmb_pkg_registry_tag(rurl, e->name, e->ranges[i], tv, sizeof(tv))) cons[nc++] = strdup(tv);
            else { for (int k = 0; k < nc; k++) free(cons[k]); return 0; }
        } else cons[nc++] = strdup(e->ranges[i]);
    }
    int ok = gmb_semver_find_best(nc, (const char* const*)cons, nv, (const char* const*)vers, out, cap);
    for (int k = 0; k < nc; k++) free(cons[k]);
    return ok;
}

/* Se il pacchetto non è in cache/vendor, c'è un registry e non è offline: scarica+estrae in cache. */
static int ensure_cached(const gmb_pkg_resolve_ctx_t* ctx, const char* name, struct rng* e)
{
    const char* rurl = registry_for(ctx, name);
    if (ctx->offline || !rurl || !rurl[0]) return 0;
    char d[1024];
    safe_dir(name, d, sizeof(d));
    char base[2048];
    snprintf(base, sizeof(base), "%s/%s", ctx->cache_root, d);
    if (dir_exists(base)) return 1;
    gmb_pkg_registry_detail_t* dt = gmb_pkg_registry_detail(rurl, name);
    if (!dt) return 0;
    int nvd = gmb_pkg_registry_detail_count(dt);
    char** vers = (char**)malloc(sizeof(char*) * (nvd > 0 ? nvd : 1));
    int nv = 0;
    for (int i = 0; i < nvd; i++) { char v[128]; if (gmb_pkg_registry_detail_version(dt, i, v, sizeof(v))) vers[nv++] = strdup(v); }
    gmb_pkg_registry_detail_free(dt);
    char best[64];
    int ok = nv > 0 && pick(ctx, e, vers, nv, best, sizeof(best));
    for (int i = 0; i < nv; i++) free(vers[i]);
    free(vers);
    if (!ok) return 0;
    char qs[320];
    snprintf(qs, sizeof(qs), "?backend=%s&platform=%s",
             (ctx->backend && ctx->backend[0]) ? ctx->backend : "il",
             (ctx->platform && ctx->platform[0]) ? ctx->platform : "any");
    char tmp[2200];
    snprintf(tmp, sizeof(tmp), "%s.tmp.zip", base);
    /* crea cache_root/base PRIMA del download: gmb_net_http_download apre il file e la sua
     * directory deve esistere (bug con cache root fresca). */
    mkdir_p(ctx->cache_root);
    mkdir_p(base);
    if (!gmb_pkg_registry_download(rurl, name, best, qs, tmp)) return 0;
    char dest[2300];
    snprintf(dest, sizeof(dest), "%s/%s", base, best);
    int zr = gmb_unzip_files(tmp, dest);
    unlink(tmp);
    return zr == 0;
}

static void enqueue_manifest(const char* js, const gmb_pkg_resolve_ctx_t* ctx, struct q* Q,
                             char*** opt, int* nopt, int* copt, const char* parent)
{
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    if (!man) return;
    int nd = gmb_pkg_manifest_count(man, "dependencies");
    for (int i = 0; i < nd; i++) { char k[512], v[128]; if (gmb_pkg_manifest_item(man, "dependencies", i, k, sizeof(k), v, sizeof(v))) { const char* ov = ovr_nested_for(ctx, parent, k); qpush(Q, k, ov ? ov : v); } }
    int no = gmb_pkg_manifest_count(man, "optionalDependencies");
    for (int i = 0; i < no; i++) { char k[512], v[128]; if (gmb_pkg_manifest_item(man, "optionalDependencies", i, k, sizeof(k), v, sizeof(v))) { const char* ov = ovr_nested_for(ctx, parent, k); qpush(Q, k, ov ? ov : v); namel_add(opt, nopt, copt, k); } }
    /* peerDependencies: AUTO-INSTALL se disponibili (npm v7). Marcati opzionali cosi' un peer
     * mancante/insoddisfacibile NON e' fatale e resta segnalato da gmb_pkg_check_peers. */
    int np = gmb_pkg_manifest_count(man, "peerDependencies");
    for (int i = 0; i < np; i++) { char k[512], v[128]; if (gmb_pkg_manifest_item(man, "peerDependencies", i, k, sizeof(k), v, sizeof(v))) { const char* ov = ovr_nested_for(ctx, parent, k); qpush(Q, k, ov ? ov : v); namel_add(opt, nopt, copt, k); } }
    int nf = gmb_pkg_manifest_count(man, "features");
    for (int i = 0; i < nf; i++) {
        char fk[256], fv[64];
        if (!gmb_pkg_manifest_item(man, "features", i, fk, sizeof(fk), fv, sizeof(fv))) continue;
        int enable = (strcmp(fk, "default") == 0);
        for (int f = 0; f < ctx->n_feat && !enable; f++) if (strcmp(ctx->features[f], fk) == 0) enable = 1;
        if (!enable) continue;
        char path[300];
        snprintf(path, sizeof(path), "features.%s", fk);
        int cnt = gmb_pkg_manifest_count(man, path);
        int islist = gmb_pkg_manifest_is_list(man, path);
        for (int j = 0; j < cnt; j++) { char ik[256], iv[256]; if (gmb_pkg_manifest_item(man, path, j, ik, sizeof(ik), iv, sizeof(iv))) qpush(Q, islist ? iv : ik, "*"); }
    }
    gmb_pkg_manifest_free(man);
}

static void fill_lock(const char* install_dir, const char* name, const char* version,
                      const gmb_pkg_resolve_ctx_t* ctx, char* hashfp, char* entry)
{
    char mpath[2600], ct[64] = "Source";
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    hashfp[0] = 0;
    char* js = read_file(mpath);
    if (js) {
        gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
        if (man) { ct_name(man, ct, sizeof(ct)); gmb_pkg_manifest_free(man); }
        gmb_pkg_pkg_hash_files(install_dir, js, hashfp, 1 << 21);
        free(js);
    }
    snprintf(entry, 1 << 21, "%s@%s|%s|%s|%s|%s", name, version, ct,
             (ctx->backend && ctx->backend[0]) ? ctx->backend : "any",
             (ctx->platform && ctx->platform[0]) ? ctx->platform : "any",
             hashfp[0] ? hashfp : "unknown");
}

gmb_pkg_resolve_result_t* gmb_pkg_resolve(gmb_pkg_resolve_ctx_t* ctx)
{
    gmb_pkg_resolve_result_t* R = (gmb_pkg_resolve_result_t*)calloc(1, sizeof(*R));
    if (!ctx || !ctx->cache_root || !ctx->cache_root[0]) { R->error = dupstr("ctx/cache_root nullo"); return R; }

    struct rng* cm = NULL; int ncm = 0, ccm = 0;
    struct q Q; memset(&Q, 0, sizeof(Q));
    for (int i = 0; i < ctx->n_req; i++) qpush(&Q, ctx->req_names[i], ctx->req_ranges[i]);

    char** expanded = NULL; int nexp = 0, cexp = 0;
    char** opt = NULL; int nopt = 0, copt = 0;
    int qi = 0;
    while (qi < Q.n) {
        const char* name = Q.names[qi];
        const char* range = Q.ranges[qi];
        qi++;
        cmap_add(&cm, &ncm, &ccm, name, range);
        if (has_name(expanded, nexp, name)) continue;
        if (nexp >= cexp) { cexp = cexp ? cexp * 2 : 16; expanded = (char**)realloc(expanded, sizeof(char*) * cexp); }
        expanded[nexp++] = dupstr(name);

        const char* ldir = local_for(ctx, name);
        if (ldir && ldir[0]) {
            char mpath[2400];
            snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", ldir);
            char* js = read_file(mpath);
            if (js) {
                char cerr[512];
                if (!gmb_pkg_compat_check(js, NULL, cerr, sizeof(cerr))) { R->error = dupstr(cerr); free(js); goto done; }
                enqueue_manifest(js, ctx, &Q, &opt, &nopt, &copt, name);
                free(js);
            }
            continue;
        }
        struct rng* e = cmap_find(cm, ncm, name);
        ensure_cached(ctx, name, e); /* scarica dal registry se manca in cache */
        char base[2048];
        find_base(ctx, name, base, sizeof(base));
        char** vers = NULL; int nv = 0;
        list_versions(base, &vers, &nv);
        char best[64];
        if (nv > 0 && pick(ctx, e, vers, nv, best, sizeof(best))) {
            char mpath[2400];
            snprintf(mpath, sizeof(mpath), "%s/%s/gmb-package.json", base, best);
            char* js = read_file(mpath);
            if (js) {
                char cerr[512];
                if (!gmb_pkg_compat_check(js, NULL, cerr, sizeof(cerr))) { R->error = dupstr(cerr); free(js); goto done; }
                enqueue_manifest(js, ctx, &Q, &opt, &nopt, &copt, e->name);
                free(js);
            }
        }
        for (int i = 0; i < nv; i++) free(vers[i]);
        free(vers);
    }

    R->items = (resolved_t*)calloc(ncm ? ncm : 1, sizeof(resolved_t));
    char* hashfp = (char*)malloc(1 << 21);
    char* entry = (char*)malloc(1 << 21);
    for (int i = 0; i < ncm; i++) {
        struct rng* e = &cm[i];
        resolved_t* it = &R->items[R->count++];
        it->name = dupstr(e->name);
        char idir[2300];
        const char* ldir = local_for(ctx, e->name);
        if (ldir && ldir[0]) {
            char ver[64] = "";
            char mpath[2400];
            snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", ldir);
            char* js = read_file(mpath);
            if (js) { gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js); if (man) { gmb_pkg_manifest_get(man, "version", ver, sizeof(ver)); gmb_pkg_manifest_free(man); } free(js); }
            it->version = dupstr(ver);
            snprintf(idir, sizeof(idir), "%s", ldir);
            it->install_dir = dupstr(idir);
            fill_lock(idir, e->name, ver, ctx, hashfp, entry);
            it->lock_entry = dupstr(entry);
            continue;
        }
        ensure_cached(ctx, e->name, e); /* scarica dal registry se manca in cache */
        char base[2048];
        find_base(ctx, e->name, base, sizeof(base));
        char** vers = NULL; int nv = 0;
        list_versions(base, &vers, &nv);
        char best[64];
        int ok = nv > 0 && pick(ctx, e, vers, nv, best, sizeof(best));
        if (!ok) {
            if (has_name(opt, nopt, e->name)) { R->count--; free(it->name); for (int k = 0; k < nv; k++) free(vers[k]); free(vers); continue; } /* opzionale mancante: non fatale */
            char msg[512];
            snprintf(msg, sizeof(msg), "Nessuna versione del pacchetto '%s' soddisfa i vincoli tra quelle disponibili.", e->name);
            R->error = dupstr(msg);
            for (int k = 0; k < nv; k++) free(vers[k]);
            free(vers);
            free(hashfp); free(entry);
            goto done;
        }
        it->version = dupstr(best);
        snprintf(idir, sizeof(idir), "%s/%s", base, best);
        it->install_dir = dupstr(idir);
        fill_lock(idir, e->name, best, ctx, hashfp, entry);
        it->lock_entry = dupstr(entry);
        for (int k = 0; k < nv; k++) free(vers[k]);
        free(vers);
    }
    free(hashfp); free(entry);

done:
    for (int i = 0; i < ncm; i++) { free(cm[i].name); for (int j = 0; j < cm[i].n; j++) free(cm[i].ranges[j]); free(cm[i].ranges); }
    free(cm);
    for (int i = 0; i < Q.n; i++) { free(Q.names[i]); free(Q.ranges[i]); }
    free(Q.names); free(Q.ranges);
    for (int i = 0; i < nexp; i++) free(expanded[i]);
    free(expanded);
    for (int i = 0; i < nopt; i++) free(opt[i]);
    free(opt);
    return R;
}

int gmb_pkg_result_count(gmb_pkg_resolve_result_t* r) { return r ? r->count : 0; }
const char* gmb_pkg_result_error(gmb_pkg_resolve_result_t* r) { return (r && r->error) ? r->error : NULL; }

int gmb_pkg_result_item(gmb_pkg_resolve_result_t* r, int i,
                        char* name, size_t ncap, char* version, size_t vcap,
                        char* install_dir, size_t icap, char* lock_entry, size_t lcap)
{
    if (!r || i < 0 || i >= r->count) return 0;
    resolved_t* it = &r->items[i];
    if (name) snprintf(name, ncap, "%s", it->name);
    if (version) snprintf(version, vcap, "%s", it->version);
    if (install_dir) snprintf(install_dir, icap, "%s", it->install_dir);
    if (lock_entry) snprintf(lock_entry, lcap, "%s", it->lock_entry);
    return 1;
}

void gmb_pkg_result_free(gmb_pkg_resolve_result_t* r)
{
    if (!r) return;
    for (int i = 0; i < r->count; i++) { free(r->items[i].name); free(r->items[i].version); free(r->items[i].install_dir); free(r->items[i].lock_entry); }
    free(r->items);
    free(r->error);
    free(r);
}
