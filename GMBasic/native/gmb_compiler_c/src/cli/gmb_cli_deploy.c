/* gmb_cli_deploy.c — `gmb build|run --alllibs`: porta accanto all'eseguibile TUTTE le
 * librerie che il programma usa.
 *
 * PERCHÉ. La cache `~/.gmbasic/libs/<rid>/` e i build del checkout bastano in sviluppo (rpath
 * assoluto), ma un programma che si distribuisce deve portarsi dietro le sue dipendenze, come
 * fa il compilatore C# quando copia la closure accanto al programma. Senza questo passo un
 * eseguibile copiato su un'altra macchina muore con "Library not loaded: @rpath/libSDL2.dylib"
 * (o, dopo lo split AI, con "runtime AI non disponibile").
 *
 * COME. Si legge la tabella delle dipendenze dinamiche del binario (`otool -L` su macOS,
 * `readelf -d` su ELF), si risolve ogni voce NON di sistema nelle cartelle note (accanto
 * all'eseguibile, cache del RID, build del runtime/db/doc/codec, Homebrew) e si copia col
 * nome del file. Le librerie della cache hanno install-name `@rpath/<nome>` e il link mette
 * `-rpath @loader_path` (macOS) / `$ORIGIN` (Linux): la copia accanto all'eseguibile è quindi
 * trovata SUBITO, prima della cache. La chiusura è ricorsiva (SDL2_ttf → SDL2, …).
 *
 * Non si riscrivono gli install-name: le copie sono identiche alle originali (stesso sha), e
 * `@rpath` risolve per costruzione. Se una dipendenza non si trova si salta (non è un errore:
 * può essere una lib di sistema che non riconosciamo).
 */
#include "gmb_cli_internal.h"

#include <ctype.h>
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

/* Libreria di sistema (POSIX / runtime C/C++ / framework Apple): non si copia. */
static int dp_is_system(const char* p)
{
    if (!p || !*p) return 1;
    if (strncmp(p, "/usr/lib/", 9) == 0) return 1;
    if (strncmp(p, "/System/", 8) == 0) return 1;
    if (strncmp(p, "/Library/Apple/", 15) == 0) return 1;
    if (strncmp(p, "/usr/local/lib/", 15) == 0) return 0;   /* Homebrew x86: può essere una nostra */
    static const char* sys[] = {
        "libSystem", "libc++.", "libobjc", "libc.so", "ld-linux", "libm.so", "libpthread",
        "libdl.so", "libgcc_s", "libstdc++", "libresolv", "librt.so", "libutil",
    };
    for (size_t i = 0; i < sizeof sys / sizeof sys[0]; i++)
        if (strncmp(p, sys[i], strlen(sys[i])) == 0) return 1;
    return 0;
}

static int dp_copy(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in) return 0;
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
    if (!ok) remove(dst);
    else chmod(dst, 0755);
    return ok;
}

static int dp_is_seen(char seen[][256], int n, const char* name)
{
    for (int i = 0; i < n; i++) if (strcmp(seen[i], name) == 0) return 1;
    return 0;
}

/* Risolve una voce della tabella dipendenze in un file esistente. */
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

/* Dipendenze dinamiche di `file`: chiama la callback per ognuna. */
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
    char   exedir[CLI_PATH];
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
    snprintf(dst, sizeof dst, "%s/%s", c->exedir, name);
    /* già accanto all'eseguibile (stesso file) → niente copia, ma si scende comunque. */
    struct stat a, b;
    int same = (stat(src, &a) == 0 && stat(dst, &b) == 0 &&
                a.st_dev == b.st_dev && a.st_ino == b.st_ino);
    if (!same) {
        if (!dp_copy(src, dst)) return;
        if (!c->quiet) fprintf(stderr, "gmb: lib -> %s\n", dst);
    }
    dp_each_dep(src, dp_visit, c);
}

void gmb_deploy_lib_closure(const char* exe, const char* install, int quiet)
{
    if (!exe || !*exe) return;
    DpCtx c;
    memset(&c, 0, sizeof c);
    c.quiet = quiet;
    path_dir(exe, c.exedir, sizeof c.exedir);
    if (!c.exedir[0]) return;

#define DP_ADD(fmt, ...) do { \
        if (c.ndirs < DP_MAX_DIRS) { \
            snprintf(c.dirs[c.ndirs], CLI_PATH, fmt, __VA_ARGS__); \
            if (is_dir(c.dirs[c.ndirs])) c.ndirs++; \
        } } while (0)

    DP_ADD("%s", c.exedir);
    const char* home = getenv("HOME");
    if (home && *home) {
        DP_ADD("%s/.gmbasic/libs/" GMB_RID_STR, home);
        DP_ADD("%s/.gmbasic/libs/" GMB_RID_STR "/lib", home);
    }
    if (install && *install) {
        DP_ADD("%s/build/libs/" GMB_RID_STR, install);
        DP_ADD("%s/native/gmb_runtime_c/build", install);
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

    /* 1) l'eseguibile, 2) ricorsivamente ogni libreria copiata. */
    dp_each_dep(exe, dp_visit, &c);
}
