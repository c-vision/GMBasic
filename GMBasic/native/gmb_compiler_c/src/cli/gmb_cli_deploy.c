/* gmb_cli_deploy.c — bundle di distribuzione del programma compilato.
 *
 * FLAGS (stessi in `gmb` e `gmbcs`):
 *   --libs[=used|all|runtime|dev|ffi|llmmotor|torch|ml|chromium|obscura|ocr|nome1,nome2]
 *   --plugins[=all|nome1,nome2]   --skills[=all|nome1,nome2]   --brains[=all|nome1,nome2]
 *
 * DOVE FINISCE TUTTO (sottocartelle nella destinazione = cartella dell'eseguibile):
 *   <out>/libs/<rid>/          librerie native (sempre `libgmb_runtime_ai`; con --libs: la
 *                              chiusura delle dipendenze, oppure tutto/gruppi/nomi scelti)
 *   <out>/libs/<comp>/<rid>/   componenti (ml, chromium, obscura, ocr)
 *   <out>/plugins/<nome>/      plugin
 *   <out>/skills/<nome>/       skill
 *   <out>/brains/<nome>/<ver>/ cervelli
 *
 * La CACHE (`~/.gmbasic/libs`, `~/.gmbasic/brains`, `shared/plugins`, `shared/skills`) resta
 * la sorgente: `--libs` senza selettore copia solo ciò che il programma USA (chiusura via
 * otool/readelf), `=all` la cache del RID, i gruppi/nomi il sottoinsieme scelto.
 *
 * Il runtime cerca PRIMA accanto al programma (gateway AI, FFI, plugin, skill: vedi
 * gmb_ai_gateway.c / gmb_ffi_dl.c / gmb_ai.c / gmb_paths.c), poi la cache: un bundle con
 * `--libs --plugins …` funziona su una macchina senza cache e senza checkout.
 */
#include "gmb_cli_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GMB_RID_STR
#define GMB_RID_STR "osx-arm64"
#endif

#define DP_MAX_DIRS 20
#define DP_MAX_SEEN 512

/* ── utilità file ───────────────────────────────────────────────────────────────────────── */

static int dp_copy(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in) return 0;
    /* il file può avere un SOTTOPERCORSO (`include/…`): va creata la catena di directory. */
    {
        char parent[4096];
        snprintf(parent, sizeof parent, "%s", dst);
        char* sl = strrchr(parent, '/');
        if (sl) { *sl = 0; if (parent[0]) { /* mkdir -p */ 
            char tmp[4096]; snprintf(tmp, sizeof tmp, "%s", parent);
            for (char* q = tmp + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
            mkdir(tmp, 0755);
        } }
    }
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    if (ferror(in)) ok = 0;
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) { remove(dst); return 0; }
    chmod(dst, 0755);
    return 1;
}

static int dp_mkdir_p(const char* path)
{
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
    return is_dir(path);
}

/* Copia ricorsiva di una directory (contenuto dentro dst/). Ritorna i file copiati. */
static int dp_copy_tree(const char* src, const char* dst, int quiet)
{
    if (!is_dir(src)) return 0;
    dp_mkdir_p(dst);
    int n = 0;
    DIR* d = opendir(src);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char s[4096], t[4096];
        snprintf(s, sizeof s, "%s/%s", src, e->d_name);
        snprintf(t, sizeof t, "%s/%s", dst, e->d_name);
        struct stat st;
        if (lstat(s, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) n += dp_copy_tree(s, t, quiet);
        else if (S_ISREG(st.st_mode)) {
            if (dp_copy(s, t)) { n++; if (!quiet) fprintf(stderr, "gmb: copia %s\n", t); }
        }
        /* symlink: si copia il CONTENUTO (un bundle non deve puntare al build tree). */
        else if (S_ISLNK(st.st_mode)) {
            struct stat st2;
            if (stat(s, &st2) == 0 && S_ISREG(st2.st_mode) && dp_copy(s, t)) {
                n++; if (!quiet) fprintf(stderr, "gmb: copia %s\n", t);
            }
        }
    }
    closedir(d);
    return n;
}

static int dp_looks_lib(const char* name)
{
    const char* e = strrchr(name, '.');
    if (!e) return 0;
    if (!strcmp(e, ".dylib") || !strcmp(e, ".dll") || !strcmp(e, ".metallib")) return 1;
    if (!strcmp(e, ".so")) return 1;
    /* libfoo.so.1 */
    const char* so = strstr(name, ".so.");
    return so != NULL;
}

/* `sel` è un elenco separato da virgole: token presente? (case-insensitive) */
static int dp_sel_has(const char* sel, const char* name)
{
    if (!sel || !*sel) return 0;
    size_t n = strlen(name);
    const char* p = sel;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        const char* q = p;
        while (*q && *q != ',' && *q != ' ') q++;
        if ((size_t)(q - p) == n && strncasecmp(p, name, n) == 0) return 1;
        p = q;
    }
    return 0;
}

static int dp_sel_is_all(const char* sel)
{
    return !sel || !*sel || dp_sel_has(sel, "all");
}

/* ── librerie native ────────────────────────────────────────────────────────────────────── */

static int dp_is_system(const char* p)
{
    if (!p || !*p) return 1;
    if (strncmp(p, "/usr/lib/", 9) == 0) return 1;
    if (strncmp(p, "/System/", 8) == 0) return 1;
    if (strncmp(p, "/Library/Apple/", 15) == 0) return 1;
    static const char* sys[] = {
        "libSystem", "libc++.", "libobjc", "libc.so", "ld-linux", "libm.so", "libpthread",
        "libdl.so", "libgcc_s", "libstdc++", "libresolv", "librt.so", "libutil",
    };
    for (size_t i = 0; i < sizeof sys / sizeof sys[0]; i++)
        if (strncmp(p, sys[i], strlen(sys[i])) == 0) return 1;
    return 0;
}

static int dp_is_seen(char seen[][256], int n, const char* name)
{
    for (int i = 0; i < n; i++) if (strcmp(seen[i], name) == 0) return 1;
    return 0;
}

static int dp_resolve(const char* dep, char dirs[][CLI_PATH], int ndirs, char* out, size_t cap)
{
    const char* name = dep;
    if (strncmp(dep, "@rpath/", 7) == 0) name = dep + 7;
    else if (strncmp(dep, "@loader_path/", 13) == 0) name = dep + 13;
    else if (strncmp(dep, "@executable_path/", 17) == 0) name = dep + 17;

    if (dep[0] == '/' && is_file(dep)) { snprintf(out, cap, "%s", dep); return 1; }
    for (int i = 0; i < ndirs; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/%s", dirs[i], name);
        if (is_file(p)) { snprintf(out, cap, "%s", p); return 1; }
    }
    return 0;
}

static void dp_each_dep(const char* file, void (*cb)(const char*, void*), void* ctx)
{
    char cmd[CLI_PATH * 2 + 32];
#if defined(__APPLE__)
    snprintf(cmd, sizeof cmd, "otool -L \"%s\" 2>/dev/null", file);
#else
    snprintf(cmd, sizeof cmd, "readelf -d \"%s\" 2>/dev/null", file);
#endif
    FILE* fp = popen(cmd, "r");
    if (!fp) return;
    char line[4096];
    int first = 1;
    while (fgets(line, sizeof line, fp)) {
#if defined(__APPLE__)
        if (first) { first = 0; continue; }              /* "<file>:" */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        char* end = strstr(p, " (");
        if (!end) continue;
        *end = 0;
        cb(p, ctx);
#else
        (void)first;
        char* lb = strchr(line, '[');
        char* rb = lb ? strchr(lb, ']') : NULL;
        if (!lb || !rb || !strstr(line, "NEEDED")) continue;
        *rb = 0;
        cb(lb + 1, ctx);
#endif
    }
    pclose(fp);
}

typedef struct {
    char   dirs[DP_MAX_DIRS][CLI_PATH];
    int    ndirs;
    char   dst[CLI_PATH];
    char   seen[DP_MAX_SEEN][256];
    int    nseen;
    int    quiet;
} DpCtx;

static void dp_visit(const char* dep, void* vctx)
{
    DpCtx* c = (DpCtx*)vctx;
    if (!dep || !*dep || c->nseen >= DP_MAX_SEEN) return;
    if (dp_is_system(dep)) return;

    char src[CLI_PATH];
    if (!dp_resolve(dep, c->dirs, c->ndirs, src, sizeof src)) return;

    const char* slash = strrchr(src, '/');
    const char* name = slash ? slash + 1 : src;
    if (!*name || dp_is_seen(c->seen, c->nseen, name)) return;
    snprintf(c->seen[c->nseen++], sizeof c->seen[0], "%s", name);

    char dst[CLI_PATH];
    snprintf(dst, sizeof dst, "%s/%s", c->dst, name);
    struct stat a, b;
    int same = (stat(src, &a) == 0 && stat(dst, &b) == 0 &&
                a.st_dev == b.st_dev && a.st_ino == b.st_ino);
    if (!same) {
        if (!dp_copy(src, dst)) return;
        if (!c->quiet) fprintf(stderr, "gmb: lib -> %s\n", dst);
    }
    dp_each_dep(src, dp_visit, c);
}

/* Cartelle in cui risolvere le dipendenze: output, cache del RID, build del checkout. */
static void dp_add_shadow_dirs(DpCtx* c, const char* exe, const char* install)
{
#define DP_ADD(fmt, ...) do { \
        if (c->ndirs < DP_MAX_DIRS) { \
            snprintf(c->dirs[c->ndirs], CLI_PATH, fmt, __VA_ARGS__); \
            if (is_dir(c->dirs[c->ndirs])) c->ndirs++; \
        } } while (0)
    DP_ADD("%s", c->dst);
    const char* home = getenv("HOME");
    if (home && *home) DP_ADD("%s/.gmbasic/libs/" GMB_RID_STR, home);
    if (install && *install) {
        DP_ADD("%s/build/libs/" GMB_RID_STR, install);
        DP_ADD("%s/native/gmb_runtime_c/build", install);
        DP_ADD("%s/native/gmb_role_c_build", install);
        DP_ADD("%s/native/gmb_runtime_c/build/gmb_chart_build", install);
        DP_ADD("%s/native/gmb_vm_c/build/gmb_runtime_c_build", install);
        DP_ADD("%s/native/gmb_compiler_c/build", install);
        DP_ADD("%s/native/gmb_db_c/build", install);
        DP_ADD("%s/native/gmb_doc_c/build", install);
        DP_ADD("%s/../LLMEnv/LLMEngine/mlx/build-gmbasic", install);
        DP_ADD("%s/../LLMEnv/build/LLMToken", install);
    }
    DP_ADD("%s", "/opt/homebrew/lib");
    DP_ADD("%s", "/usr/local/lib");
#undef DP_ADD
    (void)exe;
}

/* Chiusura delle dipendenze dinamiche di `exe` dentro `dst`. */
static void dp_closure(const char* exe, const char* install, const char* dst, int quiet)
{
    if (!exe || !*exe) return;
    DpCtx c;
    memset(&c, 0, sizeof c);
    c.quiet = quiet;
    snprintf(c.dst, sizeof c.dst, "%s", dst);
    dp_mkdir_p(dst);
    dp_add_shadow_dirs(&c, exe, install);
    dp_each_dep(exe, dp_visit, &c);
}

static const char* dp_rid(void) { return GMB_RID_STR; }

/* Dipendenze di un file GIÀ copiato (dlopen: non sono nella tabella dell'eseguibile). */
static void dp_closure_file(const char* file, const char* install, const char* dst, int quiet)
{
    DpCtx c;
    memset(&c, 0, sizeof c);
    c.quiet = quiet;
    snprintf(c.dst, sizeof c.dst, "%s", dst);
    dp_mkdir_p(dst);
    dp_add_shadow_dirs(&c, file, install);
    dp_each_dep(file, dp_visit, &c);
}

/* Chiusura di tutti i moduli di una cartella (per i componenti copiati). */
static void dp_closure_dir(const char* dir, const char* install, int quiet)
{
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (lstat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { dp_closure_dir(p, install, quiet); continue; }
        if (S_ISREG(st.st_mode) && dp_looks_lib(e->d_name)) dp_closure_file(p, install, dir, quiet);
    }
    closedir(d);
}

static void dp_cache_rid_dir(char* out, size_t cap)
{
    out[0] = 0;
    const char* home = getenv("HOME");
    if (home && *home) snprintf(out, cap, "%s/.gmbasic/libs/%s", home, dp_rid());
}

/* Nome del file dylib di una voce dell'indice/di un token (`foo` → `libfoo.dylib`). */
static int dp_find_named(const char* token, char dirs[][CLI_PATH], int ndirs, char* out, size_t cap)
{
    static const char* exts[] = { ".dylib", ".so", ".dll", ".metallib" };
    for (int d = 0; d < ndirs; d++) {
        for (size_t i = 0; i < sizeof exts / sizeof exts[0]; i++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/%s%s", dirs[d], token, exts[i]);
            if (is_file(p)) { snprintf(out, cap, "%s", p); return 1; }
            snprintf(p, sizeof p, "%s/lib%s%s", dirs[d], token, exts[i]);
            if (is_file(p)) { snprintf(out, cap, "%s", p); return 1; }
            /* `libfoo.1.dylib` / `libfoo.2.so` */
            DIR* dp = opendir(dirs[d]);
            if (!dp) continue;
            struct dirent* e;
            while ((e = readdir(dp)) != NULL) {
                if (e->d_name[0] == '.') continue;
                char base[512];
                snprintf(base, sizeof base, "%s", e->d_name);
                char* dot = strstr(base, exts[i]);
                if (!dot) continue;
                *dot = 0;
                const char* nm = base;
                if (!strncmp(nm, "lib", 3)) nm += 3;
                if (!strcasecmp(nm, token)) {
                    snprintf(p, sizeof p, "%s/%s", dirs[d], e->d_name);
                    closedir(dp);
                    snprintf(out, cap, "%s", p);
                    return 1;
                }
            }
            closedir(dp);
        }
    }
    return 0;
}

static void dp_copy_file_named(const char* token, char dirs[][CLI_PATH], int ndirs,
                               const char* dstDir, int quiet, int* n)
{
    char src[CLI_PATH], dst[CLI_PATH];
    if (!dp_find_named(token, dirs, ndirs, src, sizeof src)) {
        if (!quiet) fprintf(stderr, "gmb: libreria '%s' non trovata nella cache/build\n", token);
        return;
    }
    const char* name = strrchr(src, '/');
    name = name ? name + 1 : src;
    snprintf(dst, sizeof dst, "%s/%s", dstDir, name);
    if (dp_copy(src, dst)) {
        (*n)++;
        if (!quiet) fprintf(stderr, "gmb: lib -> %s\n", dst);
    }
}

/* ── FFI: lib richieste da `DECLARE … LIB "nome"` ────────────────────────────────────────
 * Le lib FFI sono aperte con dlopen, quindi NON compaiono in `otool -L`: la chiusura
 * "used" non le vede. Con `--libs` vanno raccolte dal sorgente e copiate nel bundle,
 * altrimenti il programma distribuito non le trova (`Library not found`). */
static const char* dp_ci_find(const char* hay, const char* needle)
{
    size_t n = strlen(needle);
    for (const char* p = hay; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == n) return p;
    }
    return NULL;
}

static int dp_ffi_collect(const char* srcFile, char names[][128], int max)
{
    FILE* f = fopen(srcFile, "r");
    if (!f) return 0;
    int n = 0;
    char line[8192];
    while (fgets(line, sizeof line, f) != NULL && n < max) {
        const char* p = line;
        while ((p = dp_ci_find(p, "LIB")) != NULL) {
            const char* q = p + 3;
            if (*q != ' ' && *q != '\t') { p = q; continue; }
            while (*q == ' ' || *q == '\t') q++;
            char quote = *q;
            if (quote != '"' && quote != '\'') { p = q; continue; }
            const char* s = q + 1;
            const char* e = strchr(s, quote);
            if (!e) { p = q; continue; }
            size_t len = (size_t)(e - s);
            if (len > 0 && len < 128) {
                char nm[128];
                int dup = 0;
                memcpy(nm, s, len);
                nm[len] = 0;
                for (int i = 0; i < n; i++) if (!strcmp(names[i], nm)) { dup = 1; break; }
                if (!dup) snprintf(names[n++], 128, "%s", nm);
            }
            p = e + 1;
        }
    }
    fclose(f);
    return n;
}

/* I `DECLARE … LIB` possono stare anche in un MODULO (`PUBLIC DECLARE ...`, importato
 * dall'entry) o in un `.inc`: leggere solo l'entry lasciava fuori la lib dal bundle. Si
 * raccolgono i nomi da tutti i `.bas`/`.inc` del progetto (bin/obj/work esclusi). */
static void dp_ffi_collect_tree(const char* dir, char names[][128], int* n, int max, int depth)
{
    if (depth > 8 || *n >= max) return;
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && *n < max) {
        if (e->d_name[0] == '.') continue;
        if (!strcmp(e->d_name, "bin") || !strcmp(e->d_name, "obj") || !strcmp(e->d_name, "work")
            || !strcmp(e->d_name, "node_modules") || !strcmp(e->d_name, "packages")) continue;
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (lstat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { dp_ffi_collect_tree(p, names, n, max, depth + 1); continue; }
        const char* ext = strrchr(e->d_name, '.');
        if (!S_ISREG(st.st_mode) || !ext
            || (strcasecmp(ext, ".bas") && strcasecmp(ext, ".inc"))) continue;
        char tmp[64][128];
        int nt = dp_ffi_collect(p, tmp, 64);
        for (int i = 0; i < nt && *n < max; i++) {
            int dup = 0;
            for (int k = 0; k < *n; k++) if (!strcmp(names[k], tmp[i])) { dup = 1; break; }
            if (!dup) snprintf(names[(*n)++], 128, "%s", tmp[i]);
        }
    }
    closedir(d);
}

static int dp_deploy_ffi_libs(const char* src, const char* install, const char* dstDir, int quiet)
{
    char names[64][128];
    int nn, nd = 0, copied = 0;
    char dirs[DP_MAX_DIRS][CLI_PATH];
    char srcDir[CLI_PATH];

    if (!src || !*src) return 0;
    nn = dp_ffi_collect(src, names, 64);
    /* moduli/inc del progetto: i DECLARE possono stare lì (PUBLIC DECLARE nel modulo). */
    path_dir(src, srcDir, sizeof srcDir);
    if (srcDir[0]) {
        dp_ffi_collect_tree(srcDir, names, &nn, 64, 0);
        /* rimuovi eventuali doppioni introdotti dall'entry (già presente nel tree) */
        for (int i = 0; i < nn; i++)
            for (int k = i + 1; k < nn; ) {
                if (!strcmp(names[i], names[k])) {
                    memmove(names[k], names[k + 1], (size_t)(nn - k - 1) * 128);
                    nn--;
                } else k++;
            }
    }
    if (nn <= 0) return 0;
    /* (1) accanto al sorgente e nella sua libs/ */
    if (srcDir[0]) {
        snprintf(dirs[nd], CLI_PATH, "%s", srcDir); nd++;
        snprintf(dirs[nd], CLI_PATH, "%s/libs", srcDir); nd++;
    }
    /* (2) CWD + libs/ (esecuzione dall'albero del progetto) */
    {
        char cwd[CLI_PATH];
        if (getcwd(cwd, sizeof cwd)) {
            snprintf(dirs[nd], CLI_PATH, "%s", cwd); nd++;
            snprintf(dirs[nd], CLI_PATH, "%s/libs", cwd); nd++;
        }
    }
    /* (3) risalita dal sorgente: ogni antenato con libs/ (max 5) */
    {
        char up[CLI_PATH];
        snprintf(up, CLI_PATH, "%s", srcDir);
        for (int i = 0; i < 5 && nd < DP_MAX_DIRS - 1 && up[0]; i++) {
            char* sl = strrchr(up, '/');
            if (!sl) break;
            *sl = 0;
            snprintf(dirs[nd], CLI_PATH, "%s/libs", up); nd++;
        }
    }
    /* (4) cache RID del runtime */
    if (nd < DP_MAX_DIRS - 1) { dp_cache_rid_dir(dirs[nd], CLI_PATH); nd++; }

    for (int i = 0; i < nn; i++) {
        char found[CLI_PATH], dst[CLI_PATH];
        const char* base;
        if (!dp_find_named(names[i], dirs, nd, found, sizeof found)) {
            if (!quiet) fprintf(stderr, "gmb: FFI lib '%s' non trovata (non copiata nel bundle)\n", names[i]);
            continue;
        }
        base = strrchr(found, '/');
        base = base ? base + 1 : found;
        snprintf(dst, sizeof dst, "%s/%s", dstDir, base);
        if (dp_copy(found, dst)) {
            copied++;
            dp_closure_file(found, install, dstDir, quiet);
            if (!quiet) fprintf(stderr, "gmb: ffi lib -> %s\n", dst);
        }
    }
    return copied;
}

/* Copia le lib secondo il selettore. Ritorna il numero di file copiati. */
static int dp_deploy_libs_select(const char* exe, const char* install, const char* sel,
                                 const char* dstDir, int quiet)
{
    dp_mkdir_p(dstDir);
    if (!sel || !*sel || !strcasecmp(sel, "used")) {
        dp_closure(exe, install, dstDir, quiet);
        return 1;
    }
    if (!strcasecmp(sel, "all")) {
        char cache[CLI_PATH];
        dp_cache_rid_dir(cache, sizeof cache);
        int n = 0;
        DIR* d = opendir(cache);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.' || !dp_looks_lib(e->d_name)) continue;
                char src[CLI_PATH], dst[CLI_PATH];
                snprintf(src, sizeof src, "%s/%s", cache, e->d_name);
                snprintf(dst, sizeof dst, "%s/%s", dstDir, e->d_name);
                if (is_file(src) && dp_copy(src, dst)) {
                    n++;
                    if (!quiet) fprintf(stderr, "gmb: lib -> %s\n", dst);
                }
            }
            closedir(d);
        }
        return n;
    }
    /* gruppi dell'indice (runtime/dev/ffi/llmmotor/torch) */
    static const char* groups[] = { "runtime", "dev", "ffi", "llmmotor", "torch" };
    for (size_t g = 0; g < sizeof groups / sizeof groups[0]; g++) {
        if (strcasecmp(sel, groups[g]) != 0) continue;
        char idx[CLI_PATH];
        dp_cache_rid_dir(idx, sizeof idx);
        strncat(idx, "/index.txt", sizeof idx - strlen(idx) - 1);
        FILE* f = fopen(idx, "r");
        if (!f) { if (!quiet) fprintf(stderr, "gmb: indice assente (%s)\n", idx); return 0; }
        char line[4096];
        int n = 0;
        while (fgets(line, sizeof line, f)) {
            char* nl = strchr(line, '\n'); if (nl) *nl = 0;
            char* f1 = strchr(line, '\t'); if (!f1) continue;
            char* f2 = strchr(f1 + 1, '\t'); if (!f2) continue;
            char* f3 = strchr(f2 + 1, '\t'); if (!f3) continue;
            char* grp = strchr(f3 + 1, '\t'); if (!grp) continue;
            grp++;
            char* ge = grp; while (*ge && *ge != '\t') ge++;
            *ge = 0;
            if (strcasecmp(grp, groups[g]) != 0) continue;
            *f1 = 0;
            char src[CLI_PATH], dst[CLI_PATH];
            char cache[CLI_PATH];
            dp_cache_rid_dir(cache, sizeof cache);
            snprintf(src, sizeof src, "%s/%s", cache, line);
            snprintf(dst, sizeof dst, "%s/%s", dstDir, line);
            if (is_file(src) && dp_copy(src, dst)) {
                n++;
                if (!quiet) fprintf(stderr, "gmb: lib -> %s\n", dst);
            }
        }
        fclose(f);
        return n;
    }
    /* componenti (sottocartella della cache: ml/chromium/obscura/ocr) */
    static const char* comps[] = { "ml", "chromium", "obscura", "ocr" };
    for (size_t i = 0; i < sizeof comps / sizeof comps[0]; i++) {
        if (strcasecmp(sel, comps[i]) != 0) continue;
        char src[CLI_PATH], base[CLI_PATH], dstRoot[CLI_PATH];
        dp_cache_rid_dir(base, sizeof base);
        char* slash = strrchr(base, '/');           /* togli /<rid> */
        if (slash) *slash = 0;
        snprintf(src, sizeof src, "%s/%s/%s", base, comps[i], dp_rid());
        path_dir(dstDir, dstRoot, sizeof dstRoot);  /* <out>/libs/<rid> → <out>/libs */
        char dst[CLI_PATH];
        snprintf(dst, sizeof dst, "%s/%s/%s", dstRoot, comps[i], dp_rid());
        int n = dp_copy_tree(src, dst, 1);
        dp_closure_dir(dst, install, quiet);
        if (!quiet) fprintf(stderr, "gmb: %d file (componente %s) -> %s\n", n, comps[i], dst);
        return n;
    }
    /* elenco esplicito di nomi */
    DpCtx c;
    memset(&c, 0, sizeof c);
    c.quiet = quiet;
    dp_add_shadow_dirs(&c, exe, install);
    int n = 0;
    const char* p = sel;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        const char* q = p;
        while (*q && *q != ',' && *q != ' ') q++;
        if (q > p) {
            char tok[256];
            size_t len = (size_t)(q - p);
            if (len >= sizeof tok) len = sizeof tok - 1;
            memcpy(tok, p, len); tok[len] = 0;
            dp_copy_file_named(tok, c.dirs, c.ndirs, dstDir, quiet, &n);
        }
        p = q;
    }
    return n;
}

/* ── plugin / skill / cervelli ──────────────────────────────────────────────────────────── */

/* Sceglie, in una cartella che contiene <nome>/…, le voci da copiare. */
static int dp_deploy_entries(const char* srcRoot, const char* dstRoot, const char* sel,
                             int quiet, int want_versions)
{
    (void)quiet;   /* il riepilogo lo stampa il chiamante (niente una riga per file) */
    if (!is_dir(srcRoot)) return 0;
    dp_mkdir_p(dstRoot);
    int n = 0;
    DIR* d = opendir(srcRoot);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' || !strcmp(e->d_name, "index.json")) continue;
        char src[CLI_PATH];
        snprintf(src, sizeof src, "%s/%s", srcRoot, e->d_name);
        if (!is_dir(src)) continue;
        if (!dp_sel_is_all(sel) && !dp_sel_has(sel, e->d_name)) continue;
        char dst[CLI_PATH];
        snprintf(dst, sizeof dst, "%s/%s", dstRoot, e->d_name);
        (void)want_versions;   /* in entrambi i casi: l'albero così com'è (brains/<nome>/<ver>/) */
        n += dp_copy_tree(src, dst, 1);
    }
    closedir(d);
    return n;
}

/* Radice delle risorse condivise del checkout: `<install>/../shared/<nome>` o `<install>/<nome>`. */
static void dp_shared_dir(const char* install, const char* name, char* out, size_t cap)
{
    out[0] = 0;
    char p[CLI_PATH];
    snprintf(p, sizeof p, "%s/../shared/%s", install, name);
    if (is_dir(p)) { snprintf(out, cap, "%s", p); return; }
    snprintf(p, sizeof p, "%s/%s", install, name);
    if (is_dir(p)) { snprintf(out, cap, "%s", p); return; }
}

static void dp_brains_src_dir(char* out, size_t cap)
{
    out[0] = 0;
    const char* home = getenv("HOME");
    if (home && *home) snprintf(out, cap, "%s/.gmbasic/brains", home);
}

/* ── entry point ───────────────────────────────────────────────────────────────────────── */

void gmb_deploy_bundle(const char* exe, const char* install, const DeployOpts* o, int quiet)
{
    if (!exe || !*exe) return;
    char outDir[CLI_PATH];
    path_dir(exe, outDir, sizeof outDir);
    if (!outDir[0]) return;

    char libsDir[CLI_PATH];
    snprintf(libsDir, sizeof libsDir, "%s/libs/%s", outDir, GMB_RID_STR);

    /* `libgmb_runtime_ai` è SEMPRE nel bundle: senza, i comandi AI falliscono. */
    static const char* const aiRe[] = {
        "build/libs/" GMB_RID_STR "/",
        "native/gmb_runtime_c/build/",
        "native/gmb_vm_c/build/gmb_runtime_c_build/",
    };
    const char* aiName = "libgmb_runtime_ai.dylib";
#if defined(_WIN32)
    aiName = "gmb_runtime_ai.dll";
#elif !defined(__APPLE__)
    aiName = "libgmb_runtime_ai.so";
#endif
    if (o && o->libs) {
        dp_deploy_libs_select(exe, install, o->libs_sel, libsDir, quiet);
        /* Le FFI (dlopen) non sono nella chiusura otool: vanno aggiunte esplicitamente. */
        if (o->src && *o->src) dp_deploy_ffi_libs(o->src, install, libsDir, quiet);
    }
    else dp_mkdir_p(libsDir);
    for (size_t i = 0; i < sizeof aiRe / sizeof aiRe[0]; i++) {
        char src[CLI_PATH], dst[CLI_PATH];
        snprintf(src, sizeof src, "%s/%s%s", install, aiRe[i], aiName);
        snprintf(dst, sizeof dst, "%s/%s", libsDir, aiName);
        if (!is_file(src)) continue;
        struct stat a, b;
        if (stat(src, &a) == 0 && stat(dst, &b) == 0 && a.st_dev == b.st_dev && a.st_ino == b.st_ino) break;
        if (dp_copy(src, dst)) {
            if (!quiet) fprintf(stderr, "gmb: runtime AI -> %s\n", dst);
            dp_closure_file(src, install, libsDir, quiet);
        }
        break;
    }

    if (o && o->plugins) {
        char srcRoot[CLI_PATH], dstRoot[CLI_PATH];
        dp_shared_dir(install, "plugins", srcRoot, sizeof srcRoot);
        snprintf(dstRoot, sizeof dstRoot, "%s/plugins", outDir);
        int n = dp_deploy_entries(srcRoot, dstRoot, o->plugins_sel, quiet, 0);
        if (!quiet) fprintf(stderr, "gmb: %d plugin -> %s\n", n, dstRoot);
    }
    if (o && o->skills) {
        char srcRoot[CLI_PATH], dstRoot[CLI_PATH];
        dp_shared_dir(install, "skills", srcRoot, sizeof srcRoot);
        snprintf(dstRoot, sizeof dstRoot, "%s/skills", outDir);
        int n = dp_deploy_entries(srcRoot, dstRoot, o->skills_sel, quiet, 0);
        if (!quiet) fprintf(stderr, "gmb: %d skill -> %s\n", n, dstRoot);
    }
    if (o && o->brains) {
        char srcRoot[CLI_PATH], dstRoot[CLI_PATH];
        dp_brains_src_dir(srcRoot, sizeof srcRoot);
        snprintf(dstRoot, sizeof dstRoot, "%s/brains", outDir);
        int n = dp_deploy_entries(srcRoot, dstRoot, o->brains_sel, quiet, 1);
        if (!quiet) fprintf(stderr, "gmb: %d cervelli (file) -> %s\n", n, dstRoot);
    }
}
