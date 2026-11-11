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
        printf("uso: gmb setup <status|TORCH|BROWSER|OBSCURA|OCR|LIBS|ALL|rm> [--force] [--yes] [--torch-local dir]\n");
        printf("  status: RID + cache librerie native (~/.gmbasic/libs/<rid>/)\n");
        printf("  TORCH/BROWSER/OBSCURA/OCR/LIBS/ALL: download on-demand (sha256 verificato)\n");
        return 0;
    }
    if (!strcmp(sub, "rm")) {
        const char* comp = (argc > 3) ? argv[3] : "ALL";
        int force = 0;
        for (int i = 3; i < argc; i++) if (!strcmp(argv[i], "--force")) force = 1;
        return gmbadmin_setup_rm(comp, force);
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

/* `libs ensure|install [--runtime-only|--dev|--components|--all]` — default: tutte.
 * L'indice in cache viene rinfrescato: un indice vecchio farebbe risultare "tutto a posto"
 * e un gruppo nuovo non verrebbe mai scaricato (parità con il vecchio CLI). */
static int libs_check(int argc, char** argv);   /* sotto */

/* `first-use` — PRIMO USO dei compilatori/CLI/REPL: completa la cache (librerie di tutti i
 * gruppi + toolchain zig) secondo `bootstrapScope` nei settings: "host" (default) | "all" |
 * "off". È l'UNICA implementazione (gmb_runtime_c/src/gmb_first_use.c): `gmb`, `gmbnative` e
 * `gmbcs` delegano qui. Non blocca mai l'avvio: ritorna 0 anche se resta qualcosa da scaricare,
 * perché il compilatore deve poter partire con ciò che ha (il motivo è stampato). */
int gmbadmin_first_use(int argc, char** argv)
{
    int dry = 0, force = 0, abi_only = 0;
    for (int i = 2; i < argc; i++)
    {
        if (!strcmp(argv[i], "--dry-run") || !strcmp(argv[i], "--dry")) dry = 1;
        else if (!strcmp(argv[i], "--force") || !strcmp(argv[i], "--retry")) force = 1;
        else if (!strcmp(argv[i], "--abi-dir")) abi_only = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
        {
            printf("uso: gmbadmin first-use [--dry-run] [--force] [--abi-dir]\n"
                   "  completo una volta sola librerie native e toolchain (zig) del RID host.\n"
                   "  Ambito: bootstrapScope nei settings (host | all | off).\n"
                   "  --dry-run  mostra il piano (indice, file, toolchain) senza scaricare\n"
                   "  --force    ignora il marker di primo uso e riprova\n"
                   "  --abi-dir  stampa solo la cartella ABI di LLMEnv (descrittori *.gmbarch)\n"
                   "             e la fonte: niente download, exit 1 se non c'e'\n");
            return 0;
        }
    }
    if (abi_only)   /* modalità STRETTA per la shell: nessun download, nessuna scrittura */
    {
        char abi[2048], how[64];
        if (!gmb_first_use_abi_dir(abi, sizeof abi, how, sizeof how)) return 1;
        printf("%s\n", abi);
        return 0;
    }
    if (force) gmb_first_use_forget();
    int ok = gmb_first_use_ensure(dry);
    if (!ok)
        fprintf(stderr, "?first-use: cache INCOMPLETA (vedi i messaggi sopra). Rimedio: gmb libs ensure\n");
    return 0;   /* mai bloccante: l'avvio del compilatore non dipende da questo */
}

static int libs_ensure(int argc, char** argv)
{
    for (int i = 3; i < argc; i++)
        if (!strcmp(argv[i], "--verify")) return libs_check(argc, argv);   /* solo controllo sha */
    int scope = GMB_SCOPE_ALL;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--runtime-only") || !strcmp(argv[i], "--runtime")) scope = GMB_SCOPE_RUNTIME;
        else if (!strcmp(argv[i], "--dev")) scope = GMB_SCOPE_DEV;
        else if (!strcmp(argv[i], "--components")) scope = GMB_SCOPE_COMPONENTS;
        else if (!strcmp(argv[i], "--all")) scope = GMB_SCOPE_ALL;
    }
    const char* rid = gmb_libs_rid();
    char cache[2048], dir[2300], idx[2400];
    gmb_paths_libs_cache_dir(cache, sizeof cache);
    snprintf(dir, sizeof dir, "%s/%s", cache, rid);
    snprintf(idx, sizeof idx, "%s/index.txt", dir);
    unlink(idx);   /* forza il refresh dell'indice (pochi KB) */
    printf("libs ensure: RID %s, scope %s → %s\n", rid,
           scope == GMB_SCOPE_ALL ? "tutte" : scope & GMB_SCOPE_DEV ? "runtime+dev" :
           scope & GMB_SCOPE_COMPONENTS ? "componenti" : "runtime", dir);
    int r = gmb_bootstrap(scope);
    if (!r) { fprintf(stderr, "?libs ensure: bootstrap incompleto (indice di rete assente o rete non disponibile)\n"); return 1; }
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
    if (!strcmp(sub, "ensure") || !strcmp(sub, "install")) return libs_ensure(argc, argv);
    if (!strcmp(sub, "check")) return libs_check(argc, argv);
    if (!strcmp(sub, "help") || !strcmp(sub, "--help")) {
        printf("uso: gmb libs <status|ensure|check|prune|index|publish>\n");
        printf("  ensure|install [--runtime-only|--dev|--components|--all]: download dalla cache\n");
        printf("  status: cache ~/.gmbasic/libs/<rid>/\n  check [--index file]: sha256 vs indice\n");
        printf("  publish <support|engine|core> <rid> [--yes]: release su GMBasicModules\n");
        printf("  prune [--dry-run]: rimuove dalla cache i file non presenti nel manifest\n");
        printf("  index [--rid R]: rigenera l'indice delle librerie (script di release)\n");
        printf("  publish <support|engine|core> <rid> [--yes]: release su GMBasicModules\n");
        return 0;
    }
    if (!strcmp(sub, "publish")) return gmbadmin_libs_publish(argc, argv);
    if (!strcmp(sub, "prune")) return gmbadmin_libs_prune(argc, argv);
    if (!strcmp(sub, "index")) return gmbadmin_libs_index(argc, argv);
    fprintf(stderr, "?libs: comando sconosciuto '%s' (status|ensure|check|publish)\n", sub);
    return 2;
}

/* ── brain ───────────────────────────────────────────────────────────────── */

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

/* ── web2md / batch: documenti in blocco ─────────────────────────────────── */

/* Estensioni che gmb_doc sa convertire (le stesse di `convfmt`). */
static int doc_ext_known(const char* name)
{
    static const char* exts[] = { ".txt", ".csv", ".md", ".markdown", ".html", ".htm", ".rtf",
                                  ".docx", ".epub", ".odt", ".xlsx", ".pptx", ".pdf", NULL };
    const char* d = strrchr(name, '.');
    if (!d) return 0;
    for (int i = 0; exts[i]; i++) if (!strcasecmp(d, exts[i])) return 1;
    return 0;
}

static int batch_walk(const char* root, const char* rel, const char* outdir, int recursive,
                      int* done, int* failed);

static int batch_one(const char* root, const char* rel, const char* outdir, int* done, int* failed)
{
    char src[4096];
    snprintf(src, sizeof src, "%s/%s", root, rel);
    char* text = gmb_doc_convert(src, "md");
    if (!text) { (*failed)++; return 0; }
    char dst[4600];
    {
        /* `a.txt` → `a.md`, `b.md` → `b.md` (non `b.md.md`) */
        char stem[4096];
        snprintf(stem, sizeof stem, "%s", rel);
        char* dot = strrchr(stem, '.');
        if (dot && dot != stem) *dot = 0;
        snprintf(dst, sizeof dst, "%s/%s.md", outdir, stem);
    }
    /* mantiene le sottocartelle */
    char dir[4600];
    snprintf(dir, sizeof dir, "%s", dst);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; gmb_fs_mkdir_p(dir); }
    FILE* f = fopen(dst, "wb");
    int ok = 0;
    if (f) {
        size_t l = strlen(text);
        ok = (fwrite(text, 1, l, f) == l);
        fclose(f);
    }
    free(text);
    if (ok) (*done)++; else (*failed)++;
    return ok;
}

static int batch_walk(const char* root, const char* rel, const char* outdir, int recursive,
                      int* done, int* failed)
{
    char dir[4096];
    if (rel && *rel) snprintf(dir, sizeof dir, "%s/%s", root, rel);
    else snprintf(dir, sizeof dir, "%s", root);
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (!strcmp(e->d_name, "bin") || !strcmp(e->d_name, "obj") ||
            !strcmp(e->d_name, "node_modules") || !strcmp(e->d_name, "work")) continue;
        char r[4096];
        if (rel && *rel) snprintf(r, sizeof r, "%s/%s", rel, e->d_name);
        else snprintf(r, sizeof r, "%s", e->d_name);
        char full[4600];
        snprintf(full, sizeof full, "%s/%s", root, r);
        if (outdir && *outdir && !strcmp(full, outdir)) continue;   /* mai ri-convertire l'output */
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (recursive) batch_walk(root, r, outdir, recursive, done, failed); continue; }
        if (!S_ISREG(st.st_mode) || !doc_ext_known(e->d_name)) continue;
        batch_one(root, r, outdir, done, failed);
    }
    closedir(d);
    return 1;
}

int gmbadmin_web2md(int argc, char** argv)
{
    const char* url = NULL;
    const char* out = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) { out = argv[++i]; continue; }
        if (!strncmp(argv[i], "--out=", 6)) { out = argv[i] + 6; continue; }
        if (argv[i][0] != '-') { url = argv[i]; break; }
    }
    if (!url) { fprintf(stderr, "?uso: gmb web2md <url> [-o out.md]\n"); return 2; }
    char* md = gmb_doc_web(url, "md");
    if (!md) { fprintf(stderr, "?web2md: conversione fallita (%s)\n", url); return 1; }
    if (!strncmp(md, "?Error:", 7)) { fprintf(stderr, "?web2md: %s\n", md); free(md); return 1; }
    if (out) {
        FILE* f = fopen(out, "wb");
        if (!f) { free(md); fprintf(stderr, "?web2md: scrittura fallita (%s)\n", out); return 1; }
        fwrite(md, 1, strlen(md), f);
        fclose(f);
        printf("[OK] %s (%zu byte)\n", out, strlen(md));
    } else printf("%s", md);
    free(md);
    return 0;
}

int gmbadmin_batch(int argc, char** argv)
{
    const char* dir = NULL;
    const char* out = NULL;
    int recursive = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) { out = argv[++i]; continue; }
        if (!strncmp(argv[i], "--out=", 6)) { out = argv[i] + 6; continue; }
        if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--recursive")) { recursive = 1; continue; }
        if (argv[i][0] != '-' && !dir) { dir = argv[i]; continue; }
    }
    if (!dir) { fprintf(stderr, "?uso: gmb batch <dir> [-o outdir] [-r]\n"); return 2; }
    char outdir[2048];
    if (out && *out) snprintf(outdir, sizeof outdir, "%s", out);
    else snprintf(outdir, sizeof outdir, "%s/md", dir);
    gmb_fs_mkdir_p(outdir);
    int done = 0, failed = 0;
    if (!batch_walk(dir, "", outdir, recursive, &done, &failed)) {
        fprintf(stderr, "?batch: cartella non leggibile (%s)\n", dir);
        return 1;
    }
    printf("batch: %d convertiti, %d errori → %s\n", done, failed, outdir);
    return failed && !done ? 1 : 0;
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
