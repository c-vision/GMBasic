/* gmb_pkg_artifact.c — selezione dell'artefatto (riferimento C del Package Manager). */
#include "gmb_pkg_artifact.h"
#include "gmb_pkg_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

struct gmb_pkg_artifact {
    int ct;
    char* source_dir;
    char* binary_path;
    char* chunk_path;
    char* ffi_link;
    char* declare;
    char* backend;
    char* platform;
    char** ffi_libs; int n_ffi_libs;
    char** ffi_headers; int n_ffi_headers;
};

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
static char* dup_or_null(const char* s) { return (s && s[0]) ? strdup(s) : NULL; }
static int ends_with_ci(const char* s, const char* suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcasecmp(s + a - b, suf) == 0;
}
static int full_file(const char* dir, const char* rel, char* out, size_t cap)
{
    snprintf(out, cap, "%s/%s", dir, rel);
    struct stat st;
    return stat(out, &st) == 0 && S_ISREG(st.st_mode);
}

static int try_select_binary(gmb_pkg_manifest_t* man, const char* idir, const char* backend, const char* platform, char* out, size_t cap)
{
    int nb = gmb_pkg_manifest_count(man, "binaries");
    for (int i = 0; i < nb; i++) {
        char bk[256], bv[4096];
        if (!gmb_pkg_manifest_item(man, "binaries", i, bk, sizeof(bk), bv, sizeof(bv))) continue;
        if (!(strcasecmp(bk, backend) == 0 || strcasecmp(bk, "any") == 0)) continue;
        if (bv[0]) { if (full_file(idir, bv, out, cap)) return 1; continue; } /* v1: platform→path */
        char path[400], rel[2048]; /* v2: backend→{platform→path} */
        snprintf(path, sizeof(path), "binaries.%s.%s", bk, platform);
        if (gmb_pkg_manifest_get(man, path, rel, sizeof(rel)) && rel[0] && full_file(idir, rel, out, cap)) return 1;
        snprintf(path, sizeof(path), "binaries.%s.any-any", bk);
        if (gmb_pkg_manifest_get(man, path, rel, sizeof(rel)) && rel[0] && full_file(idir, rel, out, cap)) return 1;
    }
    return 0;
}

int gmb_pkg_installed_public_key(const char* install_dir, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    if (!install_dir || !out || !cap) return 0;
    char p[2600];
    snprintf(p, sizeof(p), "%s/.gmbpkg.pub", install_dir);
    FILE* f = fopen(p, "rb");
    if (f)
    {
        size_t n = fread(out, 1, cap - 1, f);
        fclose(f);
        out[n] = 0;
        size_t s = 0;
        while (out[s] == ' ' || out[s] == '\t' || out[s] == '\r' || out[s] == '\n') s++;
        size_t e = strlen(out);
        while (e > s && (out[e - 1] == ' ' || out[e - 1] == '\t' || out[e - 1] == '\r' || out[e - 1] == '\n')) e--;
        memmove(out, out + s, e - s);
        out[e - s] = 0;
        if (out[0]) return 1;
    }
    /* fallback: sezione signature.publicKey del manifest su disco */
    char mp[2600];
    snprintf(mp, sizeof(mp), "%s/gmb-package.json", install_dir);
    char* js = read_file(mp);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    int ok = gmb_pkg_manifest_get(man, "signature.publicKey", out, cap) && out[0];
    gmb_pkg_manifest_free(man);
    return ok;
}

int gmb_pkg_binary_path(const char* install_dir, const char* backend, const char* platform, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    if (!install_dir) return 0;
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    const char* be = (backend && backend[0]) ? backend : "il";
    int ok = try_select_binary(man, install_dir, be, (platform && platform[0]) ? platform : "any", out, cap);
    gmb_pkg_manifest_free(man);
    return ok;
}

int gmb_pkg_module_sources(const char* install_dir, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    if (!install_dir) return 0;
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    int ne = gmb_pkg_manifest_count(man, "exports");
    int n = 0;
    size_t used = 0;
    for (int i = 0; i < ne; i++)
    {
        char k[256], v[512];
        if (!gmb_pkg_manifest_item(man, "exports", i, k, sizeof(k), v, sizeof(v)) || !v[0]) continue;
        char full[3000];
        snprintf(full, sizeof(full), "%s/%s", install_dir, v);
        int w = snprintf(out + used, used < cap ? cap - used : 0, "%s%s", n ? "\n" : "", full);
        if (w > 0) used += (size_t)w;
        n++;
    }
    gmb_pkg_manifest_free(man);
    return n;
}

gmb_pkg_artifact_t* gmb_pkg_artifact_select(const char* install_dir, const char* backend, const char* platform)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return NULL;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return NULL;

    gmb_pkg_artifact_t* a = (gmb_pkg_artifact_t*)calloc(1, sizeof(*a));
    a->platform = strdup(platform ? platform : "any");
    const char* be = (backend && backend[0]) ? backend : "il";

    char ctstr[64];
    gmb_pkg_manifest_get(man, "contentType", ctstr, sizeof(ctstr));
    for (char* p = ctstr; *p; p++) *p = (char)tolower((unsigned char)*p);
    int ct = 0;
    if (strcmp(ctstr, "dotnet") == 0) ct = 1;
    else if (strcmp(ctstr, "vm") == 0 || strcmp(ctstr, "vm-chunk") == 0 || strcmp(ctstr, "vmc") == 0) ct = 2;
    else if (strcmp(ctstr, "c-ffi") == 0 || strcmp(ctstr, "cffi") == 0) ct = 3;
    else if (strcmp(ctstr, "c-ffi-source") == 0 || strcmp(ctstr, "cffi-source") == 0) ct = 4;
    else if (strcmp(ctstr, "mixed") == 0) ct = 5;
    else if (strcmp(ctstr, "docs-only") == 0) ct = 6;
    /* CERVELLO: dati (rag/graphs), non codice. Non ha binari né sorgenti da selezionare; si
     * installa con `AI BRAIN INSTALL` / `gmb brain install` (gmb_brains.c). Tenuto distinto da
     * "generico" (0) perché vuol dire "dati pronti all'uso", non "pacchetto senza artefatti". */
    else if (strcmp(ctstr, "brain") == 0) ct = 7;

    int ne = gmb_pkg_manifest_count(man, "exports");
    for (int i = 0; i < ne; i++) {
        char k[256], v[512];
        if (!gmb_pkg_manifest_item(man, "exports", i, k, sizeof(k), v, sizeof(v)) || !v[0]) continue;
        char full[3000];
        snprintf(full, sizeof(full), "%s/%s", install_dir, v);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) { a->source_dir = strdup(full); break; }
    }

    char bin[3000];
    if (try_select_binary(man, install_dir, be, platform ? platform : "any", bin, sizeof(bin))) {
        int ischunk = (strcasecmp(be, "vm") == 0) || ends_with_ci(bin, ".prg") || ends_with_ci(bin, ".chunk");
        if (ischunk) a->chunk_path = strdup(bin);
        else a->binary_path = strdup(bin);
    }

    if (gmb_pkg_manifest_has(man, "ffi")) {
        char tmp[1024];
        a->ffi_link = gmb_pkg_manifest_get(man, "ffi.link", tmp, sizeof(tmp)) ? dup_or_null(tmp) : NULL;
        a->declare = gmb_pkg_manifest_get(man, "ffi.declareExample", tmp, sizeof(tmp)) ? dup_or_null(tmp) : NULL;
        char lp[300], lv[2048];
        snprintf(lp, sizeof(lp), "ffi.libs.%s", platform ? platform : "any");
        if (gmb_pkg_manifest_get(man, lp, lv, sizeof(lv)) && lv[0]) {
            a->ffi_libs = (char**)malloc(sizeof(char*));
            char full[3000];
            snprintf(full, sizeof(full), "%s/%s", install_dir, lv);
            a->ffi_libs[0] = strdup(full);
            a->n_ffi_libs = 1;
        }
        int nh = gmb_pkg_manifest_count(man, "ffi.headers");
        a->ffi_headers = (char**)malloc(sizeof(char*) * (nh > 0 ? nh : 1));
        for (int i = 0; i < nh; i++) {
            char k[256], v[512];
            if (!gmb_pkg_manifest_item(man, "ffi.headers", i, k, sizeof(k), v, sizeof(v)) || !v[0]) continue;
            char full[3000];
            snprintf(full, sizeof(full), "%s/%s", install_dir, v);
            a->ffi_headers[a->n_ffi_headers++] = strdup(full);
        }
    }

    int is_dotnet = a->binary_path && ends_with_ci(a->binary_path, ".dll");
    if (ct == 5) {
        if (a->chunk_path) ct = 2;
        else if (a->binary_path) ct = is_dotnet ? 1 : 3;
        else if (a->source_dir) ct = 0;
        else if (a->n_ffi_libs > 0) ct = 3;
    } else if (ct == 1 && a->binary_path && !is_dotnet) ct = 3;
    if (ct == 1 && !a->binary_path && !a->chunk_path && a->source_dir) ct = 0;
    if (ct == 2 && !a->chunk_path && a->source_dir) ct = 0;
    a->ct = ct;
    a->backend = strdup((a->binary_path || a->chunk_path) ? be : "any");

    gmb_pkg_manifest_free(man);
    return a;
}

void gmb_pkg_artifact_free(gmb_pkg_artifact_t* a)
{
    if (!a) return;
    free(a->source_dir); free(a->binary_path); free(a->chunk_path); free(a->ffi_link); free(a->declare);
    free(a->backend); free(a->platform);
    for (int i = 0; i < a->n_ffi_libs; i++) free(a->ffi_libs[i]);
    for (int i = 0; i < a->n_ffi_headers; i++) free(a->ffi_headers[i]);
    free(a->ffi_libs); free(a->ffi_headers);
    free(a);
}

int gmb_pkg_artifact_ct(gmb_pkg_artifact_t* a) { return a ? a->ct : 0; }
const char* gmb_pkg_artifact_source_dir(gmb_pkg_artifact_t* a) { return a ? a->source_dir : NULL; }
const char* gmb_pkg_artifact_binary(gmb_pkg_artifact_t* a) { return a ? a->binary_path : NULL; }
const char* gmb_pkg_artifact_chunk(gmb_pkg_artifact_t* a) { return a ? a->chunk_path : NULL; }
const char* gmb_pkg_artifact_ffi_link(gmb_pkg_artifact_t* a) { return a ? a->ffi_link : NULL; }
const char* gmb_pkg_artifact_declare(gmb_pkg_artifact_t* a) { return a ? a->declare : NULL; }
const char* gmb_pkg_artifact_backend(gmb_pkg_artifact_t* a) { return a && a->backend ? a->backend : "any"; }
const char* gmb_pkg_artifact_platform(gmb_pkg_artifact_t* a) { return a && a->platform ? a->platform : "any"; }
int gmb_pkg_artifact_ffi_lib_count(gmb_pkg_artifact_t* a) { return a ? a->n_ffi_libs : 0; }
const char* gmb_pkg_artifact_ffi_lib(gmb_pkg_artifact_t* a, int i) { return (a && i >= 0 && i < a->n_ffi_libs) ? a->ffi_libs[i] : NULL; }
int gmb_pkg_artifact_ffi_header_count(gmb_pkg_artifact_t* a) { return a ? a->n_ffi_headers : 0; }
const char* gmb_pkg_artifact_ffi_header(gmb_pkg_artifact_t* a, int i) { return (a && i >= 0 && i < a->n_ffi_headers) ? a->ffi_headers[i] : NULL; }

static void csv_add(char* buf, size_t cap, size_t* used, const char* s)
{
    if (!s || !s[0] || strstr(buf, s)) return;
    if (*used >= cap) return;
    *used += (size_t)snprintf(buf + *used, cap - *used, "%s%s", *used ? "," : "", s);
}

static void add_lib(const char* path, char* libdirs, size_t lcap, size_t* ul, char* libs, size_t bcap, size_t* ub)
{
    if (!path || !path[0]) return;
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = 0; csv_add(libdirs, lcap, ul, dir); }
    const char* fn = slash ? slash + 1 : path;
    char base[1024];
    snprintf(base, sizeof(base), "%s", fn);
    char* dot = strrchr(base, '.');
    if (dot) *dot = 0; /* togli l'estensione (.dylib/.so/.a/.lib) */
    const char* name = strncmp(base, "lib", 3) == 0 ? base + 3 : base;
    csv_add(libs, bcap, ub, name);
}

int gmb_pkg_c99_link_info(int n, const char* const* names, const char* const* install_dirs,
                          const char* backend, const char* platform,
                          char* out_includes, size_t icap, char* out_libdirs, size_t lcap, char* out_libs, size_t bcap)
{
    (void)names;
    if (out_includes && icap) out_includes[0] = 0;
    if (out_libdirs && lcap) out_libdirs[0] = 0;
    if (out_libs && bcap) out_libs[0] = 0;
    size_t ui = 0, ul = 0, ub = 0;
    for (int i = 0; i < n; i++) {
        gmb_pkg_artifact_t* a = gmb_pkg_artifact_select(install_dirs[i], backend, platform);
        if (!a) continue;
        int nh = gmb_pkg_artifact_ffi_header_count(a);
        for (int j = 0; j < nh; j++) {
            const char* h = gmb_pkg_artifact_ffi_header(a, j);
            if (!h || !h[0]) continue;
            char dir[4096];
            snprintf(dir, sizeof(dir), "%s", h);
            char* slash = strrchr(dir, '/');
            if (slash) { *slash = 0; csv_add(out_includes, icap, &ui, dir); }
        }
        int nl = gmb_pkg_artifact_ffi_lib_count(a);
        for (int j = 0; j < nl; j++) add_lib(gmb_pkg_artifact_ffi_lib(a, j), out_libdirs, lcap, &ul, out_libs, bcap, &ub);
        const char* bin = gmb_pkg_artifact_binary(a);
        if (bin) add_lib(bin, out_libdirs, lcap, &ul, out_libs, bcap, &ub);
        const char* link = gmb_pkg_artifact_ffi_link(a);
        if (link) {
            char buf[2048]; snprintf(buf, sizeof(buf), "%s", link);
            char* save = NULL;
            for (char* t = strtok_r(buf, " ", &save); t; t = strtok_r(NULL, " ", &save))
                if (strncmp(t, "-l", 2) == 0) csv_add(out_libs, bcap, &ub, t + 2);
        }
        gmb_pkg_artifact_free(a);
    }
    return 1;
}
