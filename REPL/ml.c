/* native/repl/ml.c — speculare a CliMl.cs: gestione librerie runtime ML.
 *   ML STATUS   stato cache ~/.gmbasic/libs/ml/<rid> + manifest (GmbiPaths C)
 *   ML RM       elimina la cache (conferma + dimensione)
 *   ML SETUP    copia i file source='build' da build/libs/<rid> nella cache;
 *               i source 'local'/'url' richiedono torch locale / URL (segnalati).
 */
#include "ml.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/param.h>

/* ── RID ── */
static void ml_rid(char* out, int cap)
{
#if defined(__APPLE__)
    #if defined(__aarch64__)
        snprintf(out, cap, "osx-arm64");
    #else
        snprintf(out, cap, "osx-x64");
    #endif
#elif defined(_WIN32)
    snprintf(out, cap, "win-x64");
#else
    snprintf(out, cap, "linux-x64");
#endif
}

static void ml_cache_dir(char* out, int cap, const char* rid)
{
    gmb_paths_ml_dir(rid, out, (size_t)cap);
}

/* trova la root repo (GMBASIC.sln salendo) */
static void ml_repo_root(char* out, int cap)
{
    char cwd[2048];
    getcwd(cwd, sizeof(cwd));
    char cur[2048];
    snprintf(cur, sizeof(cur), "%s", cwd);
    for (;;) {
        char probe[4096];
        snprintf(probe, sizeof(probe), "%s/GMBASIC.sln", cur);
        if (access(probe, F_OK) == 0) { snprintf(out, cap, "%s", cur); return; }
        char* s = strrchr(cur, '/');
        if (!s) { snprintf(out, cap, "%s", cwd); return; }
        *s = 0;
    }
}

/* ── mini-parse del manifest: estrae i "name" dell'array files del blocco runtimes.<rid> ── */
static int ml_manifest_names(const char* js, const char* rid, char names[][128], int maxn)
{
    char pat[256];
    snprintf(pat, sizeof(pat), "\"%s\"", rid);
    const char* p = strstr(js, pat);
    if (!p) return 0;
    p = strstr(p, "\"files\"");
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    /* trova la fine dell'array files (] di chiusura a profondità 0) */
    const char* end = p;
    int depth = 0;
    for (; *end; end++) {
        if (*end == '[') depth++;
        else if (*end == ']') { depth--; if (depth == 0) { break; } }
    }
    int n = 0;
    const char* q = p;
    while ((q = strstr(q, "\"name\"")) && q < end && n < maxn) {
        const char* c = strchr(q, ':');
        if (!c || c >= end) break;
        c = strchr(c, '"');
        if (!c || c >= end) break;
        c++;
        const char* e = strchr(c, '"');
        if (!e || e > end) break;
        int len = (int)(e - c);
        if (len > 0 && len < 127) { memcpy(names[n], c, (size_t)len); names[n][len] = 0; n++; }
        q = e + 1;
    }
    return n;
}

int gmb_ml_status(void)
{
    char rid[64], dir[2048], root[2048];
    ml_rid(rid, sizeof(rid));
    ml_cache_dir(dir, sizeof(dir), rid);
    ml_repo_root(root, sizeof(root));
    char manifest[2048];
    snprintf(manifest, sizeof(manifest), "%s/Resources/ml-manifest.json", root);

    printf("RID     : %s\n", rid);
    printf("manifest: %s (%s)\n", manifest, access(manifest, F_OK) == 0 ? "ok" : "MANCANTE");
    struct stat st;
    printf("cache   : %s (%s)\n", dir, stat(dir, &st) == 0 ? "esiste" : "assente");

    int nfiles = 0; long total = 0;
    DIR* d = opendir(dir);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char fp[2300];
            snprintf(fp, sizeof(fp), "%s/%s", dir, e->d_name);
            if (stat(fp, &st) == 0 && S_ISREG(st.st_mode)) { nfiles++; total += st.st_size; }
        }
        closedir(d);
    }
    printf("stato   : %s (%d file, %.1f MB)\n", nfiles > 0 ? "parziale (usa ML SETUP)" : "assente -> ML SETUP", nfiles, total / 1048576.0);
    return 0;
}

int gmb_ml_rm(void)
{
    char rid[64], dir[2048];
    ml_rid(rid, sizeof(rid));
    ml_cache_dir(dir, sizeof(dir), rid);
    struct stat st;
    if (stat(dir, &st) != 0) { printf("[ml] nessuna cache da eliminare.\n"); return 0; }
    long size = 0;
    DIR* d = opendir(dir);
    if (d) { struct dirent* e; while ((e = readdir(d))) { char fp[2300]; snprintf(fp, sizeof(fp), "%s/%s", dir, e->d_name); if (stat(fp, &st) == 0 && S_ISREG(st.st_mode)) size += st.st_size; } closedir(d); }
    printf("[ml] eliminare %s (%.0f MB)? [y/N] ", dir, size / 1048576.0);
    fflush(stdout);
    char ans[64];
    if (!fgets(ans, sizeof(ans), stdin)) return 0;
    if (ans[0] == 'y' || ans[0] == 'Y') {
        char cmd[2400];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dir);
        if (system(cmd) == 0) printf("[ml] cache eliminata.\n");
        else fprintf(stderr, "?ml rm fallito\n");
    } else printf("[ml] annullato.\n");
    return 0;
}

int gmb_ml_setup(bool force)
{
    char rid[64], dir[2048], root[2048];
    ml_rid(rid, sizeof(rid));
    ml_cache_dir(dir, sizeof(dir), rid);
    ml_repo_root(root, sizeof(root));
    char manifest[2048];
    snprintf(manifest, sizeof(manifest), "%s/Resources/ml-manifest.json", root);

    FILE* mf = fopen(manifest, "r");
    if (!mf) { fprintf(stderr, "?ml setup: manifest mancante: %s\n", manifest); return 1; }
    char js[65536];
    size_t n = fread(js, 1, sizeof(js) - 1, mf);
    js[n] = 0;
    fclose(mf);

    if (force) { char c[2400]; snprintf(c, sizeof(c), "rm -rf \"%s\"", dir); system(c); }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { perror("?ml setup mkdir"); return 1; }

    char names[64][128];
    int cnt = ml_manifest_names(js, rid, names, 64);
    if (cnt == 0) { fprintf(stderr, "?ml setup: nessun file nel manifest per RID %s\n", rid); return 1; }

    int copied = 0, pending = 0;
    for (int i = 0; i < cnt; i++) {
        /* source='build' -> artefatti di build (`build/libs/<rid>`) o cache utente */
        char src[2300];
        snprintf(src, sizeof(src), "%s/build/libs/%s/%s", root, rid, names[i]);
        if (access(src, F_OK) != 0)
            snprintf(src, sizeof(src), "%s/.gmbasic/libs/%s/%s", getenv("HOME") ? getenv("HOME") : ".", rid, names[i]);
        if (access(src, F_OK) == 0) {
            char cmd[4600];
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/%s\"", src, dir, names[i]);
            if (system(cmd) == 0) { printf("[ml] copiato %s\n", names[i]); copied++; }
            else { fprintf(stderr, "?ml setup: copia fallita %s\n", names[i]); pending++; }
        } else {
            printf("[ml] %s: sorgente non presente (local/url: serve torch locale o URL)\n", names[i]);
            pending++;
        }
    }
    printf("[ml] setup: %d copiati, %d da sorgente local/url (ML STATUS per dettagli)\n", copied, pending);
    return pending == 0 ? 0 : 2;
}
