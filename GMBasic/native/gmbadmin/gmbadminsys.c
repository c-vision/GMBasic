/* gmbadminsys.c — sottocomandi di servizio del CLI C `gmbadmin`:
 *   fixtures · setup · libs · brain · grep · convert/convfmt · keygen
 * Tutto su API del runtime C (fixtures/brain/grep/doc/pkg): il C# diventa un wrapper.
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_fixtures.h"
#include "gmb_grep.h"
#include "gmb_doc.h"
#include "gmb_pkg_ecdsa.h"
#include "gmb_pkg_ed25519.h"
#include "gmb_pkg_lock.h"
#include "gmb_fsutil.h"

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

static const char* apos_from(int argc, char** argv, int start, int n)
{
    int seen = 0;
    for (int i = start; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        if (seen++ == n) return argv[i];
    }
    return NULL;
}

static const char* apos(int argc, char** argv, int n) { return apos_from(argc, argv, 3, n); }

/* ── fixtures ────────────────────────────────────────────────────────────── */

int gmbadmin_fixtures(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "list";
    char man[2048], root[2048];
    gmb_fixtures_manifest_path(man, sizeof man);
    gmb_fixtures_root(root, sizeof root);

    if (!strcmp(sub, "list") || !strcmp(sub, "status") || !strcmp(sub, "help")) {
        char av[4096] = "", pr[4096] = "";
        int n = gmb_fixtures_available(av, sizeof av);
        printf("  manifest: %s\n  cartella: %s\n", man, root);
        if (n < 0) printf("  (manifest assente)\n");
        else printf("  dichiarate: %s\n", av[0] ? av : "(nessuna)");
        if (gmb_fixtures_present(pr, sizeof pr) > 0) printf("  presenti:   %s\n", pr);
        else printf("  presenti:   (nessuna)\n");
        return 0;
    }
    if (!strcmp(sub, "ensure") || !strcmp(sub, "all")) {
        char err[512] = "";
        int n = gmb_fixtures_ensure_all(NULL, NULL, NULL, err, sizeof err);
        if (n < 0) { fprintf(stderr, "?fixtures ensure: %s\n", err[0] ? err : "errore"); return 1; }
        printf(n ? "fixtures: %d scaricate in %s\n" : "fixtures: nessuna mancante\n", n, root);
        return 0;
    }
    if (!strcmp(sub, "pull")) {
        const char* name = apos(argc, argv, 0);
        if (!name) { fprintf(stderr, "?uso: gmb fixtures pull <nome>\n"); return 2; }
        char out_dir[2048], err[512] = "";
        int rc = gmb_fixtures_pull(name, "latest", NULL, 0, NULL, NULL, out_dir, sizeof out_dir, err, sizeof err);
        if (rc != 0) { fprintf(stderr, "?fixtures pull %s: %s\n", name, gmb_fixtures_strerror(rc)); return 1; }
        printf("fixtures: %s -> %s\n", name, out_dir);
        return 0;
    }
    fprintf(stderr, "?uso: gmb fixtures <list|ensure|pull <nome>>\n");
    return 2;
}

/* ── setup / libs ────────────────────────────────────────────────────────── */

static int cache_status(void)
{
    const char* rid = gmb_libs_rid();
    char cd[2048];
    gmb_paths_libs_cache_dir(cd, sizeof cd);
    char full[2300];
    snprintf(full, sizeof full, "%s/%s", cd, rid);
    int n = 0;
    DIR* d = opendir(full);
    if (d) {
        struct dirent* de;
        while ((de = readdir(d))) if (de->d_name[0] != '.') n++;
        closedir(d);
    }
    printf("  RID: %s\n  cache librerie: %s\n  file presenti: %d\n", rid, full, n);
    printf("  per popolarla: `gmb libs ensure` (bootstrap C) oppure `gmb setup LIBS`\n");
    return 0;
}

int gmbadmin_setup(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "status";
    if (!strcmp(sub, "status")) return cache_status();
    if (!strcmp(sub, "help") || !strcmp(sub, "--help")) {
        printf("uso: gmb setup <status|TORCH|BROWSER|OBSCURA|OCR|LIBS|ALL> [--force] [--yes] [--torch-local dir]\n");
        printf("  status: RID + cache librerie native (~/.gmbasic/libs/<rid>/)\n");
        printf("  TORCH/BROWSER/OBSCURA/OCR/LIBS/ALL: download on-demand (sha256 verificato)\n");
        return 0;
    }
    {
        int quiet = 0, yes = 0;
        for (int i = 3; i < argc; i++) {
            if (!strcmp(argv[i], "--quiet")) quiet = 1;
            if (!strcmp(argv[i], "--yes") || !strcmp(argv[i], "-y")) yes = 1;
        }
        return gmbadmin_setup_component(sub, argc, argv, quiet, yes);
    }
}

/* libs.txt accanto all'eseguibile (indice piatto per-file emesso dal build). */
static void libs_index_path(char* out, size_t cap)
{
    char exe[4096] = "";
#if defined(__APPLE__)
    uint32_t sz = (uint32_t)sizeof exe;
    if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0;
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) exe[n] = 0;
#endif
    char* sl = strrchr(exe, '/');
    if (sl) { *sl = 0; snprintf(out, cap, "%s/libs.txt", exe); }
    else snprintf(out, cap, "libs.txt");
}

static int libs_ensure(void)
{
    printf("libs ensure: download dei file dello scope ALL nella cache ~/.gmbasic/libs/<rid>/\n");
    int r = gmb_bootstrap(GMB_SCOPE_ALL);
    if (!r) { fprintf(stderr, "?libs ensure: bootstrap fallito (indice di rete assente o rete non disponibile)\n"); return 1; }
    printf("libs ensure: completato\n");
    return 0;
}

static int libs_check(int argc, char** argv)
{
    char idx[4096], cache[2048];
    const char* explicit = NULL;
    for (int i = 2; i < argc; i++)
        if (!strcmp(argv[i], "--index") && i + 1 < argc) explicit = argv[++i];
    gmb_paths_libs_cache_dir(cache, sizeof cache);
    const char* rid = gmb_libs_rid();
    if (explicit && *explicit) snprintf(idx, sizeof idx, "%s", explicit);
    else {
        libs_index_path(idx, sizeof idx);
        if (access(idx, R_OK) != 0) {
            /* indice della cache utente, poi il bundle dei tool */
            snprintf(idx, sizeof idx, "%s/%s/index.txt", cache, rid);
            if (access(idx, R_OK) != 0) snprintf(idx, sizeof idx, "tools/basic/bin/libs.txt");
        }
    }
    FILE* f = fopen(idx, "r");
    if (!f) { fprintf(stderr, "?libs check: indice non trovato (%s)\n", idx); return 2; }
    char line[4096];
    int ok = 0, bad = 0, missing = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        char name[512] = "", sha[128] = "";
        if (sscanf(line, "%511s %127s", name, sha) != 2) continue;
        char path[2400];
        snprintf(path, sizeof path, "%s/%s/%s", cache, rid, name);
        if (access(path, R_OK) != 0) { missing++; continue; }
        char actual[65] = "";
        if (!gmb_pkg_sha256_file(path, actual, sizeof actual)) { bad++; printf("  BAD   %s (hash non calcolabile)\n", name); continue; }
        if (strcasecmp(actual, sha) == 0) ok++;
        else { bad++; printf("  BAD   %s (sha diverso)\n", name); }
    }
    fclose(f);
    printf("libs check: %d OK, %d corrotti, %d mancanti (cache %s/%s)\n", ok, bad, missing, cache, rid);
    return bad ? 1 : 0;
}

int gmbadmin_libs(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "status";
    if (!strcmp(sub, "status")) return cache_status();
    if (!strcmp(sub, "ensure")) return libs_ensure();
    if (!strcmp(sub, "check")) return libs_check(argc, argv);
    if (!strcmp(sub, "help") || !strcmp(sub, "--help")) {
        printf("uso: gmb libs <status|ensure|check|publish>\n");
        printf("  status: cache ~/.gmbasic/libs/<rid>/\n  check [--index file]: sha256 vs indice\n");
        printf("  publish <support|engine|core> <rid> [--yes]: release su GMBasicModules\n");
        printf("  publish <support|engine|core> <rid> [--yes]: release su GMBasicModules\n");
        return 0;
    }
    if (!strcmp(sub, "publish")) return gmbadmin_libs_publish(argc, argv);
    fprintf(stderr, "?libs: comando sconosciuto '%s' (status|ensure|check|publish)\n", sub);
    return 2;
}

/* ── brain ───────────────────────────────────────────────────────────────── */

int gmbadmin_brain(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "list";
    if (!strcmp(sub, "list") || !strcmp(sub, "help")) {
        char* l = gmb_brain_registry_list();
        if (l && l[0]) printf("%s\n", l);
        else printf("(nessun cervello registrato)\n");
        free(l);
        return 0;
    }
    const char* name = apos(argc, argv, 0);
    if (!strcmp(sub, "info")) {
        if (!name) { fprintf(stderr, "?uso: gmb brain info <nome>\n"); return 2; }
        char* i = gmb_brain_registry_info(name);
        if (i) { printf("%s", i); free(i); return 0; }
        fprintf(stderr, "?brain '%s' non trovato\n", name);
        return 1;
    }
    if (!strcmp(sub, "use")) {
        if (!name) { fprintf(stderr, "?uso: gmb brain use <nome[,nome...]>\n"); return 2; }
        int rc = gmb_brain_use(name);
        printf(rc ? "collegati: %s\n" : "?brain use fallito\n", name);
        return rc ? 0 : 1;
    }
    if (!strcmp(sub, "detach")) {
        if (!name) { fprintf(stderr, "?uso: gmb brain detach <nome>\n"); return 2; }
        int rc = gmb_brain_detach(name);
        printf(rc ? "scollegato: %s\n" : "?brain detach fallito\n", name);
        return rc ? 0 : 1;
    }
    fprintf(stderr, "?brain: comando sconosciuto '%s' (list|info|use|detach)\n", sub);
    return 2;
}

/* ── grep ────────────────────────────────────────────────────────────────── */

int gmbadmin_grep(int argc, char** argv)
{
    if (argc < 3 || !strcmp(argv[2], "help")) {
        printf("uso: gmb grep <pattern> [path] [-i] [-n] [--include G] [--depth N]\n");
        return argc < 2 ? 2 : 0;
    }
    const char* pattern = argv[2];
    const char* path = ".";
    int ignore_case = 0, line_no = 1, depth = -1;
    const char* include = NULL;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-i")) { ignore_case = 1; continue; }
        if (!strcmp(argv[i], "-n")) { line_no = 1; continue; }
        if (!strcmp(argv[i], "--include") && i + 1 < argc) { include = argv[++i]; continue; }
        if (!strcmp(argv[i], "--depth") && i + 1 < argc) { depth = atoi(argv[++i]); continue; }
        if (argv[i][0] != '-') path = argv[i];
    }
    char opts[1024];
    int n = snprintf(opts, sizeof opts,
                     "{\"pattern\":\"%s\",\"recursive\":true,\"line_number\":%s,\"ignore_case\":%s",
                     pattern, line_no ? "true" : "false", ignore_case ? "true" : "false");
    if (include) n += snprintf(opts + n, sizeof opts - n, ",\"include\":\"%s\"", include);
    if (depth >= 0) n += snprintf(opts + n, sizeof opts - n, ",\"max_depth\":%d", depth);
    snprintf(opts + n, sizeof opts - n, "}");

    char* hits = gmb_grep_search_json(opts, path);
    if (!hits) { fprintf(stderr, "?grep: errore\n"); return 1; }
    char* text = gmb_grep_format_json(hits, 1, 0);
    if (text) printf("%s", text);
    gmb_grep_free(text);
    gmb_grep_free(hits);
    return 0;
}

/* ── convert / convfmt (documenti) ───────────────────────────────────────── */

int gmbadmin_doc(int argc, char** argv)
{
    const char* cmd = (argc > 1) ? argv[1] : "convert";
    if (!strcmp(cmd, "convfmt")) {
        char* f = gmb_doc_formats_s();
        printf("Formati input:  %s\n", f ? f : "(n/d)");
        printf("Formati output: md, html, text\n");
        free(f);
        return 0;
    }
    const char* file = apos_from(argc, argv, 2, 0);
    if (!file) { fprintf(stderr, "?uso: gmb convert <file> [-o out.(md|html|txt)]\n"); return 2; }
    const char* out = NULL;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strncmp(argv[i], "--out=", 6)) out = argv[i] + 6;
    }
    const char* fmt = "md";
    if (out) {
        const char* d = strrchr(out, '.');
        if (d) {
            if (!strcasecmp(d, ".html") || !strcasecmp(d, ".htm")) fmt = "html";
            else if (!strcasecmp(d, ".txt")) fmt = "text";
        }
    }
    char* res = gmb_doc_convert(file, fmt);
    if (!res) { fprintf(stderr, "?convert: conversione fallita (%s)\n", file); return 1; }
    if (out) {
        FILE* f = fopen(out, "wb");
        if (!f) { free(res); fprintf(stderr, "?convert: scrittura fallita (%s)\n", out); return 1; }
        fwrite(res, 1, strlen(res), f);
        fclose(f);
        printf("[OK] %s (%zu bytes)\n", out, strlen(res));
    } else printf("%s", res);
    free(res);
    return 0;
}

/* ── keygen ──────────────────────────────────────────────────────────────── */

int gmbadmin_keygen(int argc, char** argv)
{
    int pem = 0;
    const char* dir = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--pem")) { pem = 1; continue; }
        if (argv[i][0] != '-') { dir = argv[i]; break; }
    }
    if (pem) {
        char priv[8192] = "", pub[8192] = "";
        if (!gmb_pkg_ecdsa_keygen_pem(priv, sizeof priv, pub, sizeof pub)) {
            fprintf(stderr, "?keygen --pem: generazione fallita\n");
            return 1;
        }
        printf("PRIVATE KEY:\n%s\nPUBLIC KEY:\n%s\n", priv, pub);
        return 0;
    }
    char priv[256] = "", pub[256] = "";
    if (!gmb_pkg_ed25519_keygen(priv, sizeof priv, pub, sizeof pub)) {
        fprintf(stderr, "?keygen: generazione fallita\n");
        return 1;
    }
    if (!dir) {
        printf("private.key: %s\npublic.key: %s\n", priv, pub);
        return 0;
    }
    gmb_fs_mkdir_p(dir);
    char pp[2400], up[2400];
    snprintf(pp, sizeof pp, "%s/private.key", dir);
    snprintf(up, sizeof up, "%s/public.key", dir);
    FILE* a = fopen(pp, "wb");
    FILE* b = fopen(up, "wb");
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); fprintf(stderr, "?keygen: scrittura fallita\n"); return 1; }
    fprintf(a, "%s", priv);
    fprintf(b, "%s", pub);
    fclose(a); fclose(b);
    chmod(pp, 0600);
    printf("KeyPair Ed25519 generata in %s (private.key / public.key, seed 32B base64)\n", dir);
    return 0;
}
