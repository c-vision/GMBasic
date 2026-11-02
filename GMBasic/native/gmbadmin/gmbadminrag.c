/* gmbadminrag.c — `rag` e `graph` del CLI C (`gmbadmin`).
 *
 * Porting C dei comandi `gmb rag …` / `gmb graph …` (ex GMBasicSharp/Cli/CliRagGraph.cs):
 *   rag   build|update <wikiDir> [--rag d] [--name n] [--collection c] [--chunk N]
 *                                [--overlap N] [--dry-run] [--prune|--no-prune]
 *   rag   status [--rag d] [--name n]
 *   rag   query "<q>" [--rag d] [--name n] [--topk N] [--collection c]
 *   graph build|update <wikiDir> [--graphs d] [--name n] [--dry-run]
 *   graph status [--graphs d]
 *
 * Sorgente unica: `gmb_rag_index_source` / `gmb_rag_search` / `gmb_rag_list_sources`
 * (runtime C) e `gmb_graph_build` (secondbrain) — nessuna logica duplicata in C#.
 * L'indicizzazione richiede un provider di embedding: senza modello risponde con l'errore
 * esplicito del runtime (mai un fallback silenzioso).
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_doc.h"
#include "gmb_brain_tools.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ── utility ─────────────────────────────────────────────────────────────── */

static const char* vopt(int argc, char** argv, int start, const char* name)
{
    for (int i = start; i < argc; i++) {
        if (!strcmp(argv[i], name) && i + 1 < argc) return argv[i + 1];
        size_t n = strlen(name);
        if (!strncmp(argv[i], name, n) && argv[i][n] == '=') return argv[i] + n + 1;
    }
    return NULL;
}

static int hasflag(int argc, char** argv, int start, const char* name)
{
    for (int i = start; i < argc; i++) if (!strcmp(argv[i], name)) return 1;
    return 0;
}

static const char* posn(int argc, char** argv, int start, int n)
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

/* Somma grezza degli mtime dei .md sotto `dir` (rileva modifiche senza inotify). */
static long graph_mtime_sum(const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    long sum = 0;
    struct dirent* e;
    char** subs = NULL;
    int nsubs = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            subs = (char**)realloc(subs, (size_t)(nsubs + 1) * sizeof(char*));
            if (subs) subs[nsubs++] = strdup(p);
            continue;
        }
        size_t l = strlen(e->d_name);
        if (l > 3 && (!strcasecmp(e->d_name + l - 3, ".md") || !strcasecmp(e->d_name + l - 9, ".markdown")))
            sum += (long)st.st_mtime + (long)st.st_size;
    }
    closedir(d);
    for (int i = 0; i < nsubs; i++) { sum += graph_mtime_sum(subs[i]); free(subs[i]); }
    free(subs);
    return sum;
}

static const char* now_stamp_str(void)
{
    static char buf[32];
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof buf, "%H:%M:%S", &tmv);
    return buf;
}

static void base_name(const char* path, char* out, size_t cap)
{
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", path ? path : "");
    size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = 0;
    const char* sl = strrchr(tmp, '/');
    snprintf(out, cap, "%s", sl ? sl + 1 : tmp);
}

static int is_doc(const char* name)
{
    static const char* exts[] = { ".md", ".markdown", ".txt", NULL };
    const char* d = strrchr(name, '.');
    if (!d) return 0;
    for (int i = 0; exts[i]; i++) if (!strcasecmp(d, exts[i])) return 1;
    return 0;
}

typedef struct { char** v; int n, cap; } slist_t;

static void sl_push(slist_t* l, const char* s)
{
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 64;
        char** p = (char**)realloc(l->v, (size_t)cap * sizeof(char*));
        if (!p) return;
        l->v = p; l->cap = cap;
    }
    l->v[l->n++] = strdup(s);
}

static int sl_has(slist_t* l, const char* s)
{
    for (int i = 0; i < l->n; i++) if (!strcmp(l->v[i], s)) return 1;
    return 0;
}

static void sl_free(slist_t* l)
{
    for (int i = 0; i < l->n; i++) free(l->v[i]);
    free(l->v);
    memset(l, 0, sizeof *l);
}

static void walk(const char* root, const char* rel, slist_t* out)
{
    char dir[4096];
    if (rel && *rel) snprintf(dir, sizeof dir, "%s/%s", root, rel);
    else snprintf(dir, sizeof dir, "%s", root);
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        if (!strcmp(de->d_name, "bin") || !strcmp(de->d_name, "obj") ||
            !strcmp(de->d_name, "node_modules") || !strcmp(de->d_name, "work")) continue;
        char r[4096];
        if (rel && *rel) snprintf(r, sizeof r, "%s/%s", rel, de->d_name);
        else snprintf(r, sizeof r, "%s", de->d_name);
        char full[4600];
        snprintf(full, sizeof full, "%s/%s", root, r);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk(root, r, out);
        else if (S_ISREG(st.st_mode) && is_doc(de->d_name)) sl_push(out, r);
    }
    closedir(d);
}

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)n, f);
    b[rd] = 0;
    fclose(f);
    return b;
}

/* ── graph ───────────────────────────────────────────────────────────────── */

static int graph_status(const char* dir)
{
    printf("GRAPH STATUS — %s\n", dir);
    DIR* d = opendir(dir);
    if (!d) { printf("  (cartella assente)\n"); return 0; }
    struct dirent* de;
    int n = 0;
    while ((de = readdir(d))) {
        size_t l = strlen(de->d_name);
        if (de->d_name[0] == '.' || l < 5 || strcasecmp(de->d_name + l - 5, ".json")) continue;
        char p[4400];
        snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        char when[32] = "";
        struct tm tmv;
        if (localtime_r(&st.st_mtime, &tmv)) strftime(when, sizeof when, "%Y-%m-%d %H:%M", &tmv);
        printf("  %-28s %8lld bytes  %s\n", de->d_name, (long long)st.st_size, when);
        n++;
    }
    closedir(d);
    if (!n) printf("  (nessun grafo)\n");
    return 0;
}

int gmbadmin_graph(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "status";
    char dir[2048];
    const char* g = vopt(argc, argv, 3, "--graphs");
    if (!g) g = vopt(argc, argv, 3, "-g");
    if (g && *g) snprintf(dir, sizeof dir, "%s", g);
    else gmb_paths_graphs_dir(dir, sizeof dir);

    if (!strcmp(sub, "status") || !strcmp(sub, "help") || !strcmp(sub, "-h") || !strcmp(sub, "--help")) {
        if (!strcmp(sub, "help") || !strcmp(sub, "-h") || !strcmp(sub, "--help")) {
            printf("uso: gmb graph <build|update|status> <wikiDir> [--graphs d] [--name n] [--dry-run]\n");
            return 0;
        }
        return graph_status(dir);
    }
    if (!strcmp(sub, "build") || !strcmp(sub, "update") || !strcmp(sub, "refresh")) {
        const char* wiki = posn(argc, argv, 3, 0);
        if (!wiki) { fprintf(stderr, "?uso: gmb graph build <wikiDir> [--graphs d] [--name n]\n"); return 2; }
        const char* dry = hasflag(argc, argv, 3, "--dry-run") || hasflag(argc, argv, 3, "-D") ? " (dry-run)" : "";
        char base[512];
        base_name(wiki, base, sizeof base);
        const char* name = vopt(argc, argv, 3, "--name");
        if (!name) name = vopt(argc, argv, 3, "-n");
        if (!name || !*name) name = base;
        printf("%s GRAPH: %s -> %s/%s.json%s\n",
               !strcmp(sub, "update") ? "UPDATE" : "BUILD", wiki, dir, name, dry);
        if (dry[0]) return 0;
        gmb_fs_mkdir_p(dir);
        int64_t nn = gmb_graph_build(wiki, dir);
        if (nn < 0) { fprintf(stderr, "?graph build: wiki non trovata o non leggibile (%s)\n", wiki); return 1; }
        if (strcmp(name, base) != 0) {
            char from[4400], to[4600];
            snprintf(from, sizeof from, "%s/%s.json", dir, base);
            snprintf(to, sizeof to, "%s/%s.json", dir, name);
            if (rename(from, to) != 0) fprintf(stderr, "?graph build: rename %s -> %s fallito\n", from, to);
        }
        printf("[OK] grafo %s: %lld nodi -> %s/%s.json\n", !strcmp(sub, "update") ? "aggiornato" : "costruito",
               (long long)nn, dir, name);
        return 0;
    }
    if (!strcmp(sub, "export-obsidian")) {
        const char* nm = vopt(argc, argv, 3, "--name");
        if (!nm) nm = vopt(argc, argv, 3, "-n");
        if (!nm) nm = posn(argc, argv, 3, 0);
        if (!nm) { fprintf(stderr, "?uso: gmb graph export-obsidian <nome> [--graphs d] [--out dir]\n"); return 2; }
        const char* outdir = vopt(argc, argv, 3, "--out");
        gmb_brain_result_t r = gmb_brain_tool_graph_export_obsidian(dir, nm, outdir);
        if (r.text && r.text[0]) printf("%s\n", r.text);
        if (!r.ok && r.error) fprintf(stderr, "?%s\n", r.error);
        int rc = r.ok ? 0 : 1;
        gmb_brain_result_free(&r);
        return rc;
    }
    if (!strcmp(sub, "watch")) {
        const char* wiki = posn(argc, argv, 3, 0);
        if (!wiki) { fprintf(stderr, "?uso: gmb graph watch <wikiDir> [--graphs d] [--name n] [--interval S] [--once]\n"); return 2; }
        const char* nm = vopt(argc, argv, 3, "--name");
        if (!nm) nm = vopt(argc, argv, 3, "-n");
        char base[512];
        base_name(wiki, base, sizeof base);
        if (!nm || !*nm) nm = base;
        int interval = 10;
        const char* iv = vopt(argc, argv, 3, "--interval");
        if (iv && atoi(iv) > 0) interval = atoi(iv);
        int once = hasflag(argc, argv, 3, "--once");
        gmb_fs_mkdir_p(dir);
        printf("GRAPH WATCH: %s → %s/%s.json (ogni %ds%s)\n", wiki, dir, nm, interval,
               once ? ", una volta" : ", Ctrl-C per fermare");
        long last = -1;
        for (;;) {
            long sig = graph_mtime_sum(wiki);
            if (sig != last) {
                int64_t nn = gmb_graph_build(wiki, dir);
                if (nn >= 0) {
                    char from[4400], to[4600];
                    snprintf(from, sizeof from, "%s/%s.json", dir, base);
                    snprintf(to, sizeof to, "%s/%s.json", dir, nm);
                    if (strcmp(from, to) != 0) rename(from, to);
                    printf("[%s] grafo aggiornato: %lld nodi -> %s/%s.json\n",
                           now_stamp_str(), (long long)nn, dir, nm);
                } else {
                    fprintf(stderr, "?graph watch: build fallita (%s)\n", wiki);
                }
                last = sig;
            }
            if (once) return 0;
            sleep((unsigned)interval);
        }
    }
    if (!strcmp(sub, "unwatch")) {
        printf("GRAPH UNWATCH: il watcher gira in primo piano: fermalo con Ctrl-C, oppure installalo come\n"
               "  servizio (`gmb daemon install <nome> <programma>` + `gmb daemon stop <nome>`).\n");
        return 0;
    }
    fprintf(stderr, "?uso: gmb graph <build|update|status> <wikiDir> [--graphs d] [--name n]\n");
    return 2;
}

/* ── rag ─────────────────────────────────────────────────────────────────── */

static int rag_status(const char* db)
{
    char* cols = gmb_rag_list_collections(db);
    printf("RAG STATUS — %s\n", db);
    if (!cols) { printf("  (db non leggibile)\n"); return 1; }
    if (cols[0] == '[' && cols[1] == ']') { printf("  (nessuna collezione)\n"); free(cols); return 0; }
    /* il JSON è una lista di oggetti {name,…}: estrae i nomi in modo testuale (nessuna
     * dipendenza dal parser JSON per una stampa diagnostica). */
    const char* p = cols;
    while ((p = strstr(p, "\"name\":\"")) != NULL) {
        p += 8;
        const char* e = strchr(p, '"');
        if (!e) break;
        char nm[256];
        size_t l = (size_t)(e - p);
        if (l >= sizeof nm) l = sizeof nm - 1;
        memcpy(nm, p, l); nm[l] = 0;
        printf("  %-20s %d documenti\n", nm, gmb_rag_count(db, nm));
        p = e + 1;
    }
    free(cols);
    return 0;
}

int gmbadmin_rag(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "status";
    char ragdir[2048];
    const char* r = vopt(argc, argv, 3, "--rag");
    if (!r) r = vopt(argc, argv, 3, "-r");
    if (r && *r) snprintf(ragdir, sizeof ragdir, "%s", r);
    else gmb_paths_rag_dir(ragdir, sizeof ragdir);

    const char* wiki = posn(argc, argv, 3, 0);
    const char* name = vopt(argc, argv, 3, "--name");
    if (!name) name = vopt(argc, argv, 3, "-n");
    char nm[512];
    if (name && *name) snprintf(nm, sizeof nm, "%s", name);
    else if (wiki) base_name(wiki, nm, sizeof nm);
    else nm[0] = 0;

    char db[2600];
    if (nm[0]) snprintf(db, sizeof db, "%s/%s.db", ragdir, nm);
    else db[0] = 0;

    if (!strcmp(sub, "help") || !strcmp(sub, "-h") || !strcmp(sub, "--help")) {
        printf("uso: gmb rag build|update <wikiDir> [--rag d] [--name n] [--collection c]\n"
               "                          [--chunk N] [--overlap N] [--dry-run] [--prune|--no-prune]\n"
               "     gmb rag status [--rag d] [--name n]\n"
               "     gmb rag query \"<q>\" [--rag d] [--name n] [--topk N] [--collection c]\n");
        return 0;
    }
    if (!strcmp(sub, "status")) {
        if (!db[0]) { fprintf(stderr, "?rag status: serve --name (nessun nome deducibile)\n"); return 2; }
        return rag_status(db);
    }
    if (!strcmp(sub, "query")) {
        const char* q = wiki;
        if (!q) { fprintf(stderr, "?uso: gmb rag query \"<domanda>\" [--name n]\n"); return 2; }
        if (!db[0]) { fprintf(stderr, "?rag query: serve --name (nessun nome deducibile)\n"); return 2; }
        const char* coll = vopt(argc, argv, 3, "--collection");
        if (!coll) coll = vopt(argc, argv, 3, "-C");
        if (!coll) coll = "wiki";
        int topk = 5;
        const char* tk = vopt(argc, argv, 3, "--topk");
        if (tk) topk = atoi(tk);
        char* res = gmb_rag_search(db, coll, q, topk, NULL);
        if (!res) { fprintf(stderr, "?rag query: errore (db %s)\n", db); return 1; }
        printf("%s\n", res);
        free(res);
        return 0;
    }
    if (!strcmp(sub, "build") || !strcmp(sub, "update") || !strcmp(sub, "refresh")) {
        if (!wiki) { fprintf(stderr, "?uso: gmb rag build <wikiDir> [--name n] [--collection c]\n"); return 2; }
        const char* coll = vopt(argc, argv, 3, "--collection");
        if (!coll) coll = vopt(argc, argv, 3, "-C");
        if (!coll) coll = "wiki";
        int chunk = 2048, overlap = 256;
        const char* ck = vopt(argc, argv, 3, "--chunk");
        if (!ck) ck = vopt(argc, argv, 3, "-c");
        if (ck && atoi(ck) > 0) chunk = atoi(ck);
        const char* ov = vopt(argc, argv, 3, "--overlap");
        if (!ov) ov = vopt(argc, argv, 3, "-o");
        if (ov && atoi(ov) >= 0) overlap = atoi(ov);
        int dry = hasflag(argc, argv, 3, "--dry-run") || hasflag(argc, argv, 3, "-D");
        int prune = hasflag(argc, argv, 3, "--prune") || hasflag(argc, argv, 3, "-p");
        if (hasflag(argc, argv, 3, "--no-prune")) prune = 0;
        else if (!strcmp(sub, "update") || !strcmp(sub, "refresh")) prune = 1;
        if (!nm[0]) { fprintf(stderr, "?rag build: nome db non deducibile, usa --name\n"); return 2; }

        printf("%s RAG: %s -> %s (collection '%s', chunk %d/%d)%s\n",
               dry ? "DRY-RUN" : (!strcmp(sub, "build") ? "BUILD" : "UPDATE"),
               wiki, db, coll, chunk, overlap, prune ? " [prune]" : "");

        slist_t files;
        memset(&files, 0, sizeof files);
        walk(wiki, "", &files);
        if (files.n == 0) { fprintf(stderr, "?rag build: nessun documento in %s\n", wiki); return 1; }

        gmb_fs_mkdir_p(ragdir);
        int indexed = 0, updated = 0, skipped = 0, failed = 0;
        for (int i = 0; i < files.n; i++) {
            char full[4600];
            snprintf(full, sizeof full, "%s/%s", wiki, files.v[i]);
            char* text = read_file(full);
            if (!text) { failed++; continue; }
            if (dry) { printf("  %s\n", files.v[i]); free(text); continue; }
            char meta[1024];
            snprintf(meta, sizeof meta, "{\"path\":\"%s\"}", files.v[i]);
            int rc = gmb_rag_index_source(db, coll, files.v[i], text, meta, chunk, overlap);
            free(text);
            if (rc == 1) indexed++;
            else if (rc == 2) updated++;
            else if (rc == 0) skipped++;
            else failed++;
        }

        int removed = 0;
        if (prune && !dry) {
            char* known = gmb_rag_list_sources(db, coll);
            if (known) {
                const char* p = known;
                while ((p = strchr(p, '"')) != NULL) {
                    const char* s = p + 1;
                    const char* e = strchr(s, '"');
                    if (!e) break;
                    char src[2048];
                    size_t l = (size_t)(e - s);
                    if (l >= sizeof src) l = sizeof src - 1;
                    memcpy(src, s, l); src[l] = 0;
                    if (!sl_has(&files, src)) { gmb_rag_delete_source(db, coll, src); removed++; }
                    p = e + 1;
                }
                free(known);
            }
        }
        printf("[%s] indicizzati %d, aggiornati %d, skipped %d, rimossi %d, errori %d\n",
               failed ? "PARZIALE" : "OK", indexed, updated, skipped, removed, failed);
        sl_free(&files);
        if (failed && !indexed && !updated && !skipped) {
            fprintf(stderr, "?rag build: nessun documento indicizzato — manca il provider di embedding?\n"
                            "  (indicizzazione RAG richiede un modello: `gmb setup` / AI REGISTER)\n");
            return 1;
        }
        return 0;
    }
    fprintf(stderr, "?uso: gmb rag <build|update|status|query> …\n");
    return 2;
}
