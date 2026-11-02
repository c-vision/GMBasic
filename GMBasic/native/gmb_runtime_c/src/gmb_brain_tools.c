/* strcasestr/strdup con le estensioni POSIX/Darwin */
#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1
/* gmb_brain_tools.c — Second Brain: registry/manifest + operazioni di gestione (V1 + export/sign).
 *
 * Sostituisce il toolchain C# (`Basic.AI.Knowledge.BrainToolchain*`): stats, verify, create,
 * register, buildvault, optimize, rename, backup, export, sign, diff, doctor.
 * Usa sqlite3 per i conteggi RAG e le API grafo del runtime (`gmb_graph_build`).
 */
#include "gmb_brain_tools.h"
#include "gmb_brain_tools_internal.h"

#include "gmb_runtime.h"
#include "gmb_fsutil.h"
#include "gmb_commonlib.h"
#include "gmb_compress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>

/* ── esiti ───────────────────────────────────────────────────────────────── */

void gmb_brain_result_free(gmb_brain_result_t* r)
{
    if (!r) return;
    free(r->text);
    free(r->error);
    r->text = NULL;
    r->error = NULL;
    r->ok = 0;
}

static char* vformat(const char* fmt, va_list ap)
{
    va_list c1;
    va_copy(c1, ap);
    int n = vsnprintf(NULL, 0, fmt, c1);
    va_end(c1);
    if (n < 0) return NULL;
    char* s = (char*)malloc((size_t)n + 1);
    if (!s) return NULL;
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    return s;
}

gmb_brain_result_t br_ok(const char* fmt, ...)
{
    gmb_brain_result_t r;
    r.ok = 1; r.error = NULL;
    va_list ap;
    va_start(ap, fmt);
    r.text = vformat(fmt, ap);
    va_end(ap);
    return r;
}

gmb_brain_result_t br_fail(const char* fmt, ...)
{
    gmb_brain_result_t r;
    r.ok = 0; r.text = NULL;
    va_list ap;
    va_start(ap, fmt);
    r.error = vformat(fmt, ap);
    va_end(ap);
    return r;
}

/* ── helper generici ─────────────────────────────────────────────────────── */

char* br_read_file(const char* path)
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

int br_write_file(const char* path, const char* text)
{
    char dir[2048];
    snprintf(dir, sizeof dir, "%s", path);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; gmb_fs_mkdir_p(dir); }
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t l = text ? strlen(text) : 0;
    int ok = (l == 0) || (fwrite(text, 1, l, f) == l);
    fclose(f);
    return ok;
}

long br_file_size(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    return (long)st.st_size;
}

void br_now_utc_iso(char* out, size_t cap)
{
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

void br_now_local_stamp(char* out, size_t cap)
{
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(out, cap, "%Y-%m-%d %H:%M:%S", &tmv);
}

/* Conta i .md sotto `dir` (ricorsivo), fino a `limit` (0 = nessun limite). */
long br_count_md(const char* dir, long limit)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    long n = 0;
    char** subs = NULL;
    int nsubs = 0, capsubs = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (nsubs == capsubs) {
                capsubs = capsubs ? capsubs * 2 : 16;
                char** nb = (char**)realloc(subs, (size_t)capsubs * sizeof(char*));
                if (!nb) break;
                subs = nb;
            }
            subs[nsubs++] = strdup(p);
        } else if (S_ISREG(st.st_mode)) {
            size_t l = strlen(e->d_name);
            if (l > 3 && !strcasecmp(e->d_name + l - 3, ".md")) {
                n++;
                if (limit > 0 && n >= limit) break;
            }
        }
    }
    closedir(d);
    for (int i = 0; i < nsubs; i++) {
        if (limit > 0 && n >= limit) { free(subs[i]); continue; }
        n += br_count_md(subs[i], limit > 0 ? limit - n : 0);
        free(subs[i]);
    }
    free(subs);
    return n;
}

/* ── SQLite helper ───────────────────────────────────────────────────────── */

sqlite3* br_sql_open(const char* path, int readonly)
{
    sqlite3* db = NULL;
    int flags = readonly ? SQLITE_OPEN_READONLY
                         : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    if (sqlite3_open_v2(path, &db, flags, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    return db;
}

long br_sql_scalar(sqlite3* db, const char* sql)
{
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    long v = 0;
    if (sqlite3_step(st) == SQLITE_ROW) v = (long)sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

char* br_sql_text(sqlite3* db, const char* sql)
{
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
    char* out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char* t = (const char*)sqlite3_column_text(st, 0);
        if (t) out = strdup(t);
    }
    sqlite3_finalize(st);
    return out;
}

int br_sql_exec(sqlite3* db, const char* sql)
{
    char* err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 1 : 0;
}

/* ── STATS ───────────────────────────────────────────────────────────────── */

gmb_brain_result_t gmb_brain_tool_stats(const char* name, int as_json)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info))
        return br_fail("cervello '%s' non registrato (BRAIN LIST)", name);

    long db_bytes = 0, doc_count = 0, chunk_count = 0;
    int nodes = 0, edges = 0, dangling = 0, communities = 0;
    char p[2048];

    if (gmb_brain_reg_rag_db(name, p, sizeof p) && access(p, R_OK) == 0) {
        db_bytes = br_file_size(p);
        sqlite3* db = br_sql_open(p, 1);
        if (!db) return br_fail("db RAG illeggibile (%s)", p);
        doc_count = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_documents");
        chunk_count = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_chunks");
        sqlite3_close(db);
    }
    if (gmb_brain_reg_graph_json(name, p, sizeof p) && access(p, R_OK) == 0) {
        char* js = br_read_file(p);
        if (!js) return br_fail("grafo illeggibile (%s)", p);
        gmb_val_t g = gmb_json_parse(js);
        free(js);
        gmb_val_t n = gmb_val_dict_get(g, "nodes");
        gmb_val_t e = gmb_val_dict_get(g, "edges");
        gmb_val_t d = gmb_val_dict_get(g, "dangling");
        nodes = (int)gmb_list_count(gmb_val_foreach_list(n));
        edges = (int)gmb_list_count(gmb_val_foreach_list(e));
        dangling = (int)gmb_list_count(gmb_val_foreach_list(d));
        char stats[2400];
        snprintf(stats, sizeof stats, "%s", p);
        char* dot = strrchr(stats, '.');
        if (dot) snprintf(dot, 16, ".stats.json");
        char* sj = br_read_file(stats);
        if (sj) {
            gmb_val_t s = gmb_json_parse(sj);
            free(sj);
            gmb_val_t c = gmb_val_dict_get(s, "communities");
            if (c.kind == GMB_TYPE_INT) communities = (int)c.v_int;
        }
    }

    char root[1024];
    int has_root = gmb_brain_reg_root(name, root, sizeof root);

    if (as_json) {
        char avail_arr[64];
        if (!strcmp(info.available, "rag,graph")) snprintf(avail_arr, sizeof avail_arr, "\"rag\", \"graph\"");
        else if (info.available[0]) snprintf(avail_arr, sizeof avail_arr, "\"%s\"", info.available);
        else snprintf(avail_arr, sizeof avail_arr, "");
        char root_json[1100];
        if (has_root) { root_json[0] = '"'; strcpy(root_json + 1, root); strcat(root_json, "\""); }
        else snprintf(root_json, sizeof root_json, "null");
        char cs_json[220];
        if (info.checksum[0]) { cs_json[0] = '"'; snprintf(cs_json + 1, sizeof cs_json - 2, "%s", info.checksum); strcat(cs_json, "\""); }
        else snprintf(cs_json, sizeof cs_json, "null");
        char vp_json[1100];
        if (info.vault_path[0]) { vp_json[0] = '"'; strcpy(vp_json + 1, info.vault_path); strcat(vp_json, "\""); }
        else snprintf(vp_json, sizeof vp_json, "null");
        char* out = (char*)malloc(16384);
        if (!out) return br_fail("memoria esaurita");
        snprintf(out, 16384,
                 "{\n  \"name\": \"%s\",\n  \"mode\": \"%s\",\n  \"available\": [%s],\n"
                 "  \"attached\": %s,\n  \"docs\": %ld,\n"
                 "  \"rag\": {\"present\": %s, \"bytes\": %ld, \"documents\": %ld, \"chunks\": %ld},\n"
                 "  \"graph\": {\"present\": %s, \"nodes\": %d, \"edges\": %d, \"dangling\": %d, \"communities\": %d},\n"
                 "  \"checksum\": %s,\n  \"vaultPath\": %s,\n  \"brainRoot\": %s\n}",
                 info.name, info.mode, avail_arr,
                 info.attached ? "true" : "false", info.docs,
                 db_bytes > 0 ? "true" : "false", db_bytes, doc_count, chunk_count,
                 nodes > 0 ? "true" : "false", nodes, edges, dangling, communities,
                 cs_json, vp_json, root_json);
        gmb_brain_result_t r;
        r.ok = 1; r.error = NULL; r.text = out;
        return r;
    }

    char* out = NULL;
    size_t cap = 4096;
    out = (char*)malloc(cap);
    if (!out) return br_fail("memoria esaurita");
    size_t off = 0;
    char avail_disp[128];
    if (!strcmp(info.available, "rag,graph")) snprintf(avail_disp, sizeof avail_disp, "rag, graph");
    else snprintf(avail_disp, sizeof avail_disp, "%s", info.available);
    off += (size_t)snprintf(out + off, cap - off,
        "BRAIN STATS — %s\n  mode:       %s   available: [%s]\n  attached:   %s\n  docs:       %ld\n",
        info.name, info.mode, avail_disp, info.attached ? "true" : "false",
        info.docs > 0 ? info.docs : doc_count);
    if (db_bytes > 0)
        off += (size_t)snprintf(out + off, cap - off, "  rag:        %ld documenti, %ld chunk, %.1f MB\n",
                                doc_count, chunk_count, (double)db_bytes / 1048576.0);
    else off += (size_t)snprintf(out + off, cap - off, "  rag:        assente\n");
    if (nodes > 0)
        off += (size_t)snprintf(out + off, cap - off, "  graph:      %d nodi, %d edge, %d dangling, %d community\n",
                                nodes, edges, dangling, communities);
    else off += (size_t)snprintf(out + off, cap - off, "  graph:      assente\n");
    off += (size_t)snprintf(out + off, cap - off, "  checksum:   %s\n  root:       %s",
                            info.checksum[0] ? info.checksum : "-", has_root ? root : "-");
    gmb_brain_result_t r;
    r.ok = 1; r.error = NULL; r.text = out;
    return r;
}

/* ── VERIFY ──────────────────────────────────────────────────────────────── */

typedef struct { char name[64]; int ok; char detail[512]; } br_check_t;

static void add_check(br_check_t* c, int* n, const char* name, int ok, const char* fmt, ...)
{
    if (*n >= 32) return;
    br_check_t* x = &c[(*n)++];
    snprintf(x->name, sizeof x->name, "%s", name);
    x->ok = ok;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(x->detail, sizeof x->detail, fmt, ap);
    va_end(ap);
}

gmb_brain_result_t gmb_brain_tool_verify(const char* name, int as_json)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info))
        return br_fail("cervello '%s' non registrato (BRAIN LIST)", name);

    br_check_t checks[32];
    int n = 0;
    char p[2048];
    int has_rag = gmb_brain_reg_rag_db(name, p, sizeof p) && access(p, R_OK) == 0;
    char graph_path[2048];
    int has_graph = gmb_brain_reg_graph_json(name, graph_path, sizeof graph_path) && access(graph_path, R_OK) == 0;

    /* 1) manifest + checksum */
    char root[1024];
    if (gmb_brain_reg_root(name, root, sizeof root)) {
        char err[512] = "";
        int mrc = gmb_brain_manifest_verify(root, err, sizeof err);
        if (mrc == 1) add_check(checks, &n, "manifest", 1, "assente (brain costruito localmente: esportabile con BRAIN BACKUP)");
        else if (mrc == 0) add_check(checks, &n, "manifest.checksum", 1, "file verificati");
        else add_check(checks, &n, "manifest.checksum", 0, "%s", err);
    }

    /* 2) RAG */
    if (has_rag) {
        sqlite3* db = br_sql_open(p, 1);
        if (!db) add_check(checks, &n, "rag.open", 0, "db non apribile");
        else {
            char* integ = br_sql_text(db, "PRAGMA integrity_check");
            add_check(checks, &n, "rag.integrity", integ && !strcasecmp(integ, "ok"), "%s", integ ? integ : "");
            free(integ);
            long docs = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_documents");
            long chunks = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_chunks");
            add_check(checks, &n, "rag.contents", chunks > 0, "%ld documenti, %ld chunk", docs, chunks);
            long emb = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_embeddings");
            add_check(checks, &n, "rag.embeddings", chunks == 0 || emb > 0, "%ld embedding (chunk attesi %ld)", emb, chunks);
            long fts = br_sql_scalar(db, "SELECT COUNT(*) FROM fts_chunks");
            add_check(checks, &n, "rag.fts", 1, "%ld righe FTS5", fts);
            sqlite3_close(db);
        }
    }

    /* 3) GRAPH */
    if (has_graph) {
        char* js = br_read_file(graph_path);
        if (!js) add_check(checks, &n, "graph.open", 0, "file non leggibile");
        else {
            gmb_val_t g = gmb_json_parse(js);
            free(js);
            int nodes = (int)gmb_list_count(gmb_val_foreach_list(gmb_val_dict_get(g, "nodes")));
            int edges = (int)gmb_list_count(gmb_val_foreach_list(gmb_val_dict_get(g, "edges")));
            int dangling = (int)gmb_list_count(gmb_val_foreach_list(gmb_val_dict_get(g, "dangling")));
            add_check(checks, &n, "graph.json", nodes > 0, "%d nodi, %d edge, %d dangling", nodes, edges, dangling);
            double ratio = edges > 0 ? (double)dangling / (double)edges : 0.0;
            add_check(checks, &n, "graph.dangling", ratio < 0.5, "rapporto dangling %.0f%% (soglia 50%%)", ratio * 100.0);
        }
    }

    if (!has_rag && !has_graph)
        add_check(checks, &n, "artefatti", 0,
                  "né db RAG né grafo presenti su disco: il cervello è registrato ma vuoto (ricostruire con BRAIN CREATE o RAG/GRAPH BUILD)");

    int failed = 0;
    for (int i = 0; i < n; i++) if (!checks[i].ok) failed++;

    char* out = (char*)malloc(16384);
    if (!out) return br_fail("memoria esaurita");
    size_t off = 0;
    if (as_json) {
        off += (size_t)snprintf(out + off, 16384 - off, "{\n  \"name\": \"%s\",\n  \"ok\": %s,\n  \"checks\": [",
                                name, failed == 0 ? "true" : "false");
        for (int i = 0; i < n; i++) {
            char esc[1100]; size_t j = 0;
            for (const char* q = checks[i].detail; *q && j < sizeof esc - 3; q++) {
                if (*q == '"' || *q == '\\') esc[j++] = '\\';
                if (*q == '\n') { esc[j++] = '\\'; esc[j++] = 'n'; continue; }
                esc[j++] = *q;
            }
            esc[j] = 0;
            off += (size_t)snprintf(out + off, 16384 - off,
                "%s\n    {\"name\": \"%s\", \"ok\": %s, \"detail\": \"%s\"}",
                i ? "," : "", checks[i].name, checks[i].ok ? "true" : "false", esc);
        }
        off += (size_t)snprintf(out + off, 16384 - off, "\n  ]\n}");
    } else {
        off += (size_t)snprintf(out + off, 16384 - off, "BRAIN VERIFY — %s: %s",
                                name, failed == 0 ? "OK" : "");
        if (failed) off += (size_t)snprintf(out + off, 16384 - off, "%d PROBLEMI", failed);
        off += (size_t)snprintf(out + off, 16384 - off, "\n");
        for (int i = 0; i < n; i++)
            off += (size_t)snprintf(out + off, 16384 - off, "  [%s] %s: %s\n",
                                    checks[i].ok ? "ok" : "KO", checks[i].name, checks[i].detail);
        if (off && out[off - 1] == '\n') out[off - 1] = 0;
    }
    gmb_brain_result_t r;
    r.ok = failed == 0;
    r.text = out;
    r.error = r.ok ? NULL : strdup("verifica fallita");
    if (r.ok && as_json) { /* il testo JSON resta */ }
    return r;
}

/* ── REGISTER ────────────────────────────────────────────────────────────── */

gmb_brain_result_t gmb_brain_tool_register(const char* name, const char* mode, const char* source, int activate)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    char avail[64] = "";
    if (!gmb_brain_reg_available(name, avail, sizeof avail)) {
        /* `--source` esplicito: un file .db/.json (o una cartella con rag/*.db, graphs/*.json)
         * È l'artefatto del cervello (parità BrainRegistry.Register(source: …)). */
        struct stat st;
        if (source && *source && stat(source, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                size_t l = strlen(source);
                if (l > 3 && !strcasecmp(source + l - 3, ".db")) snprintf(avail, sizeof avail, "rag");
                else if (l > 5 && !strcasecmp(source + l - 5, ".json")) snprintf(avail, sizeof avail, "graph");
            } else if (S_ISDIR(st.st_mode)) {
                char p[2400];
                snprintf(p, sizeof p, "%s/%s.db", source, name);
                int rag = access(p, R_OK) == 0;
                snprintf(p, sizeof p, "%s/rag/%s.db", source, name);
                rag = rag || access(p, R_OK) == 0;
                snprintf(p, sizeof p, "%s/%s.json", source, name);
                int graph = access(p, R_OK) == 0;
                snprintf(p, sizeof p, "%s/graphs/%s.json", source, name);
                graph = graph || access(p, R_OK) == 0;
                if (rag && graph) snprintf(avail, sizeof avail, "rag,graph");
                else if (rag) snprintf(avail, sizeof avail, "rag");
                else if (graph) snprintf(avail, sizeof avail, "graph");
            }
        }
    }
    if (!avail[0])
        return br_fail("cervello '%s': nessun artefatto su disco (rag/%s.db o graphs/%s.json): "
                       "costruiscilo con `gmb rag build`/`gmb graph build` o `BRAIN CREATE`", name, name, name);
    char inferred[16];
    if (mode && *mode) snprintf(inferred, sizeof inferred, "%s", mode);
    else if (!strcmp(avail, "rag,graph")) snprintf(inferred, sizeof inferred, "both");
    else snprintf(inferred, sizeof inferred, "%s", avail);
    if (strcmp(inferred, "rag") && strcmp(inferred, "graph") && strcmp(inferred, "both"))
        return br_fail("--mode attesi: rag|graph|both");
    if (!gmb_brain_reg_register(name, source, NULL, inferred, NULL, NULL, 0, NULL))
        return br_fail("scrittura del registry fallita");

    char avail_disp[128];
    if (!strcmp(avail, "rag,graph")) snprintf(avail_disp, sizeof avail_disp, "rag, graph");
    else snprintf(avail_disp, sizeof avail_disp, "%s", avail);
    gmb_brain_result_t r = br_ok("registrato: %s (mode %s, available [%s])", name, inferred, avail_disp);
    if (activate) {
        int ok = gmb_brain_use(name);
        char* t = (char*)malloc(strlen(r.text) + 64);
        if (t) {
            sprintf(t, "%s%s", r.text, ok ? " — collegato" : " — collegamento NON riuscito (verifica gli artefatti)");
            free(r.text);
            r.text = t;
        }
    }
    return r;
}

/* ── CREATE ──────────────────────────────────────────────────────────────── */

gmb_brain_result_t gmb_brain_tool_create(const char* name, const char* from_dir, const char* mode,
                                         int do_register, int activate, int dry_run)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    struct stat st;
    if (!from_dir || !*from_dir || stat(from_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return br_fail("cartella di origine inesistente: '%s'", from_dir ? from_dir : "");
    const char* m = (mode && *mode) ? mode : "both";
    if (strcasecmp(m, "rag") && strcasecmp(m, "graph") && strcasecmp(m, "both"))
        return br_fail("--mode attesi: rag|graph|both");
    int do_rag = !strcasecmp(m, "rag") || !strcasecmp(m, "both");
    int do_graph = !strcasecmp(m, "graph") || !strcasecmp(m, "both");

    char* out = (char*)malloc(8192);
    if (!out) return br_fail("memoria esaurita");
    size_t off = 0;

    char ragdir[1024], graphs[1024];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    gmb_paths_graphs_dir(graphs, sizeof graphs);

    if (do_rag) {
        char db[2200];
        snprintf(db, sizeof db, "%s/%s.db", ragdir, name);
        if (dry_run) {
            off += (size_t)snprintf(out + off, 8192 - off, "[dry-run] RAG: %s → %s\n", from_dir, db);
        } else {
            gmb_fs_mkdir_p(ragdir);
            int idx = 0, upd = 0, sk = 0, rem = 0, fail = 0;
            int total = gmb_rag_build_dir(from_dir, db, "wiki", 2048, 256, 0, &idx, &upd, &sk, &rem, &fail);
            if (total < 0) { free(out); return br_fail("CREATE rag impossibile: cartella non indicizzabile (%s)", from_dir); }
            if (fail && !idx && !upd && !sk) {
                free(out);
                return br_fail("CREATE rag impossibile: nessun documento indicizzato (manca il provider di embedding?)");
            }
            off += (size_t)snprintf(out + off, 8192 - off, "RAG: %d indicizzati, %d aggiornati, %d invariati → %s\n",
                                    idx, upd, sk, db);
        }
    }
    if (do_graph) {
        char target[2400];
        snprintf(target, sizeof target, "%s/%s.json", graphs, name);
        if (dry_run) {
            off += (size_t)snprintf(out + off, 8192 - off, "[dry-run] GRAPH: %s → %s\n", from_dir, target);
        } else {
            gmb_fs_mkdir_p(graphs);
            int64_t nn = gmb_graph_build(from_dir, graphs);
            if (nn < 0) { free(out); return br_fail("CREATE graph impossibile: %s", from_dir); }
            /* gmb_graph_build scrive <basename>.json */
            char base[512];
            snprintf(base, sizeof base, "%s", from_dir);
            size_t bl = strlen(base);
            while (bl > 1 && base[bl - 1] == '/') base[--bl] = 0;
            char* sl = strrchr(base, '/');
            char src[2400];
            snprintf(src, sizeof src, "%s/%s.json", graphs, sl ? sl + 1 : base);
            if (strcmp(src, target) != 0) rename(src, target);
            br_graph_write_artifacts(name, graphs);
            long edges = br_graph_edge_count(target);
            long dangling = br_graph_dangling_count(target);
            off += (size_t)snprintf(out + off, 8192 - off, "GRAPH: %lld nodi, %ld edge, %ld dangling → %s\n",
                                    (long long)nn, edges, dangling, target);
        }
    }
    if (!dry_run && do_register) {
        if (gmb_brain_reg_register(name, from_dir, from_dir, m, NULL, NULL, 0, NULL))
            off += (size_t)snprintf(out + off, 8192 - off, "registrato: %s (mode %s)\n", name, m);
        if (activate && gmb_brain_use(name))
            off += (size_t)snprintf(out + off, 8192 - off, "attivato: %s\n", name);
    }
    if (off && out[off - 1] == '\n') out[off - 1] = 0;
    gmb_brain_result_t r;
    r.ok = 1; r.error = NULL; r.text = out;
    return r;
}

/* ── BUILDVAULT ──────────────────────────────────────────────────────────── */

/* Pagine `.md` per namespace dal registry canonico (shared/spec/registry). Il catalogo C#
 * ha anche i nomi/tipi dei parametri; qui si emettono nome, kind e arietà — quanto serve al
 * RAG per rispondere "quale funzione esiste e quanti argomenti prende". */
gmb_brain_result_t gmb_brain_tool_buildvault(const char* out_dir)
{
    char dir[2048];
    if (out_dir && *out_dir) snprintf(dir, sizeof dir, "%s", out_dir);
    else snprintf(dir, sizeof dir, "/tmp/gmbasic-brainvault-pages");
    char brain[2200];
    snprintf(brain, sizeof brain, "%s/brain", dir);
    gmb_fs_mkdir_p(brain);

    char root[1024];
    char reg[2048];
    if (gmb_paths_repo_root(root, sizeof root) && root[0]) {
        snprintf(reg, sizeof reg, "%s/shared/spec/registry/canonical.registry.json", root);
        if (access(reg, R_OK) != 0) snprintf(reg, sizeof reg, "%s/../shared/spec/registry/canonical.registry.json", root);
    } else snprintf(reg, sizeof reg, "shared/spec/registry/canonical.registry.json");

    char* js = br_read_file(reg);
    if (!js) return br_fail("registry canonico non trovato (%s)", reg);
    gmb_val_t r = gmb_json_parse(js);
    free(js);
    if (r.kind != GMB_TYPE_OBJECT) return br_fail("registry canonico non leggibile (%s)", reg);
    gmb_list_t procs = gmb_val_foreach_list(gmb_val_dict_get(r, "procedures"));
    gmb_int_t np = gmb_list_count(procs);
    if (np <= 0) { gmb_list_free(&procs); return br_fail("nessuna procedura nel registry canonico"); }

    /* raggruppa per namespace (ordine alfabetico) */
    char namespaces[512][64];
    int nns = 0;
    for (gmb_int_t i = 0; i < np && nns < 512; i++) {
        gmb_val_t p = gmb_list_get(procs, i);
        gmb_val_t nsv = gmb_val_dict_get(p, "ns");
        if (nsv.kind != GMB_TYPE_STRING || !nsv.v_string.data) continue;
        const char* ns = nsv.v_string.data;
        int found = 0;
        for (int k = 0; k < nns; k++) if (!strcasecmp(namespaces[k], ns)) { found = 1; break; }
        if (!found) snprintf(namespaces[nns++], sizeof namespaces[0], "%s", ns);
    }
    for (int i = 0; i < nns; i++)
        for (int j = i + 1; j < nns; j++)
            if (strcasecmp(namespaces[j], namespaces[i]) < 0) { char t[64]; memcpy(t, namespaces[i], 64); memcpy(namespaces[i], namespaces[j], 64); memcpy(namespaces[j], t, 64); }

    int written = 0, indexed_files = 0;
    for (int k = 0; k < nns; k++) {
        const char* ns = namespaces[k];
        int count = 0;
        for (gmb_int_t i = 0; i < np; i++) {
            gmb_val_t p = gmb_list_get(procs, i);
            gmb_val_t nsv = gmb_val_dict_get(p, "ns");
            if (nsv.kind == GMB_TYPE_STRING && nsv.v_string.data && !strcasecmp(nsv.v_string.data, ns)) count++;
        }
        size_t cap = (size_t)count * 160 + 2048;
        char* page = (char*)malloc(cap);
        if (!page) continue;
        size_t off = (size_t)snprintf(page, cap,
            "---\ntitle: %s\nnamespace: %s\ntype: stdlib\nfunctions: %d\n---\n\n"
            "# Namespace %s — %d funzioni GMBasic\n\n"
            "| Funzione | Arità | Tipo |\n|---|---|---|\n", ns, ns, count, ns, count);
        for (gmb_int_t i = 0; i < np; i++) {
            gmb_val_t p = gmb_list_get(procs, i);
            gmb_val_t nsv = gmb_val_dict_get(p, "ns");
            if (nsv.kind != GMB_TYPE_STRING || !nsv.v_string.data || strcasecmp(nsv.v_string.data, ns)) continue;
            gmb_val_t nm = gmb_val_dict_get(p, "name");
            gmb_val_t kind = gmb_val_dict_get(p, "kindName");
            gmb_val_t isfn = gmb_val_dict_get(p, "fn");
            gmb_val_t amin = gmb_val_dict_get(p, "arityMin");
            gmb_val_t amax = gmb_val_dict_get(p, "arityMax");
            long lo = amin.kind == GMB_TYPE_INT ? (long)amin.v_int : 0;
            long hi = amax.kind == GMB_TYPE_INT ? (long)amax.v_int : lo;
            char arity[32];
            if (hi <= 0) snprintf(arity, sizeof arity, "0");
            else if (hi == lo) snprintf(arity, sizeof arity, "%ld", lo);
            else snprintf(arity, sizeof arity, "%ld-%ld", lo, hi);
            off += (size_t)snprintf(page + off, cap - off, "| `%s.%s()` | `%s` | %s |\n",
                                    ns,
                                    (nm.kind == GMB_TYPE_STRING && nm.v_string.data) ? nm.v_string.data : "?",
                                    arity,
                                    (isfn.kind == GMB_TYPE_BOOL && isfn.v_bool) ? "funzione" : "comando");
            (void)kind;
        }
        off += (size_t)snprintf(page + off, cap - off,
            "\nUsa questa pagina per generare codice GMBasic corretto: quando ti chiedono di scrivere\n"
            "codice che usa queste funzioni, consulta il nome esatto e l'arità.\n");
        char path[2400];
        snprintf(path, sizeof path, "%s/%s.md", brain, ns);
        if (br_write_file(path, page)) { written++; indexed_files++; }
        free(page);
    }
    gmb_list_free(&procs);
    if (written == 0) return br_fail("nessuna pagina generata (registry vuoto?)");

    /* indice RAG del brainvault */
    char ragdir[1024], db[2200];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    snprintf(db, sizeof db, "%s/brainvault.db", ragdir);
    int idx = 0, upd = 0, sk = 0, rem = 0, fail = 0;
    gmb_fs_mkdir_p(ragdir);
    gmb_rag_build_dir(dir, db, "wiki", 2048, 256, 0, &idx, &upd, &sk, &rem, &fail);
    if (idx + upd == 0) {
        /* Le pagine sono scritte (utili), ma senza provider embedding il brain non esiste:
         * errore ESPLICITO, come `gmb rag build` (mai un successo silenzioso). */
        return br_fail("BRAIN BUILDVAULT: %d pagine scritte in %s, ma 0 indicizzate — serve un provider di "
                       "embedding reale (configura i provider: `gmb setup` / AI SET ACTIVE %EMBEDDING)", written, dir);
    }
    gmb_brain_reg_register("brainvault", db, NULL, "rag", NULL, NULL, 0, NULL);
    return br_ok("BRAIN BUILDVAULT: %d pagine in %s, %d indicizzate → %s", written, dir, idx + upd, db);
}

/* ── OPTIMIZE ────────────────────────────────────────────────────────────── */

gmb_brain_result_t gmb_brain_tool_optimize(const char* name, int dry_run, int reindex_fts)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info)) return br_fail("cervello '%s' non registrato (BRAIN LIST)", name);
    char* out = (char*)malloc(8192);
    if (!out) return br_fail("memoria esaurita");
    size_t off = 0;
    char p[2048];

    if (gmb_brain_reg_rag_db(name, p, sizeof p) && access(p, R_OK) == 0) {
        long before = br_file_size(p);
        if (dry_run) {
            off += (size_t)snprintf(out + off, 8192 - off,
                "[dry-run] RAG %s: VACUUM + PRAGMA optimize%s (attuale %.1f MB)\n",
                p, reindex_fts ? " + FTS5 rebuild" : " + FTS5 optimize", (double)before / 1048576.0);
        } else {
            sqlite3* db = br_sql_open(p, 0);
            if (db) {
                br_sql_exec(db, "PRAGMA optimize");
                br_sql_exec(db, reindex_fts ? "INSERT INTO fts_chunks(fts_chunks) VALUES('rebuild')"
                                            : "INSERT INTO fts_chunks(fts_chunks) VALUES('optimize')");
                sqlite3_close(db);
                off += (size_t)snprintf(out + off, 8192 - off, "RAG: FTS5 %s\n", reindex_fts ? "rebuild" : "optimize");
                db = br_sql_open(p, 0);
                if (db) { br_sql_exec(db, "VACUUM"); sqlite3_close(db); }
                long after = br_file_size(p);
                off += (size_t)snprintf(out + off, 8192 - off, "RAG: %.1f MB → %.1f MB\n",
                                        (double)before / 1048576.0, (double)after / 1048576.0);
            } else {
                off += (size_t)snprintf(out + off, 8192 - off, "RAG: db non apribile (%s)\n", p);
            }
        }
    } else off += (size_t)snprintf(out + off, 8192 - off, "RAG: assente (niente da ottimizzare)\n");

    char gdir[1024];
    gmb_paths_graphs_dir(gdir, sizeof gdir);
    if (gmb_brain_reg_graph_json(name, p, sizeof p) && access(p, R_OK) == 0) {
        if (dry_run) {
            off += (size_t)snprintf(out + off, 8192 - off, "[dry-run] GRAPH %s: ricalcolo export (pagerank/community)\n", p);
        } else {
            br_graph_write_artifacts(name, gdir);
            off += (size_t)snprintf(out + off, 8192 - off, "GRAPH: export ricalcolati (stats/mermaid/graphml/html)\n");
        }
    } else off += (size_t)snprintf(out + off, 8192 - off, "GRAPH: assente (niente da ottimizzare)\n");

    if (off && out[off - 1] == '\n') out[off - 1] = 0;
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = out;
    return r;
}

/* ── RENAME ──────────────────────────────────────────────────────────────── */

typedef struct { char from[2200]; char to[2200]; int is_dir; } br_move_t;

gmb_brain_result_t gmb_brain_tool_rename(const char* name, const char* new_name, int dry_run)
{
    if (!name || !*name || !new_name || !*new_name) return br_fail("servono nome attuale e nuovo nome");
    if (!strcasecmp(name, new_name)) return br_fail("il nuovo nome è identico a quello attuale");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info)) return br_fail("cervello '%s' non registrato (BRAIN LIST)", name);
    gmb_brain_rec_t other;
    char avail[64] = "";
    if (gmb_brain_reg_info(new_name, &other) || gmb_brain_reg_available(new_name, avail, sizeof avail))
        return br_fail("il nome '%s' è già in uso (cervello registrato o artefatti presenti)", new_name);

    br_move_t moves[32];
    int nm = 0;
    char ragdir[1024], graphs[1024], brains[1024];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    gmb_paths_graphs_dir(graphs, sizeof graphs);
    gmb_paths_brains_dir(brains, sizeof brains);

    static const char* rag_suffix[] = { ".db", ".db-wal", ".db-shm", ".state.json", NULL };
    for (int i = 0; rag_suffix[i] && nm < 32; i++) {
        char from[2200], to[2200];
        snprintf(from, sizeof from, "%s/%s%s", ragdir, name, rag_suffix[i]);
        if (access(from, F_OK) != 0) continue;
        snprintf(to, sizeof to, "%s/%s%s", ragdir, new_name, rag_suffix[i]);
        snprintf(moves[nm].from, sizeof moves[0].from, "%s", from);
        snprintf(moves[nm].to, sizeof moves[0].to, "%s", to);
        moves[nm].is_dir = 0;
        nm++;
    }
    static const char* g_ext[] = { "json", "stats.json", "mermaid", "graphml", "html", "report.md", NULL };
    for (int i = 0; g_ext[i] && nm < 32; i++) {
        char from[2200], to[2200];
        snprintf(from, sizeof from, "%s/%s.%s", graphs, name, g_ext[i]);
        if (access(from, F_OK) != 0) continue;
        snprintf(to, sizeof to, "%s/%s.%s", graphs, new_name, g_ext[i]);
        snprintf(moves[nm].from, sizeof moves[0].from, "%s", from);
        snprintf(moves[nm].to, sizeof moves[0].to, "%s", to);
        moves[nm].is_dir = 0;
        nm++;
    }
    {
        char from[2200], to[2200];
        snprintf(from, sizeof from, "%s/%s", brains, name);
        if (access(from, F_OK) == 0 && nm < 32) {
            snprintf(to, sizeof to, "%s/%s", brains, new_name);
            snprintf(moves[nm].from, sizeof moves[0].from, "%s", from);
            snprintf(moves[nm].to, sizeof moves[0].to, "%s", to);
            moves[nm].is_dir = 1;
            nm++;
        }
    }
    if (nm == 0) return br_fail("cervello '%s': nessun artefatto da rinominare", name);

    char* out = (char*)malloc(16384);
    if (!out) return br_fail("memoria esaurita");
    size_t off = 0;
    if (dry_run) {
        off += (size_t)snprintf(out + off, 16384 - off, "[dry-run] RENAME %s → %s (%d elementi):\n", name, new_name, nm);
        for (int i = 0; i < nm; i++)
            off += (size_t)snprintf(out + off, 16384 - off, "  %s → %s\n", moves[i].from, moves[i].to);
        off += (size_t)snprintf(out + off, 16384 - off, "  registry: register('%s') + unregister('%s')", new_name, name);
        gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = out;
        return r;
    }
    off += (size_t)snprintf(out + off, 16384 - off, "RENAME %s → %s:\n", name, new_name);
    for (int i = 0; i < nm; i++) {
        int rc = moves[i].is_dir ? rename(moves[i].from, moves[i].to) : rename(moves[i].from, moves[i].to);
        if (rc != 0) {
            free(out);
            return br_fail("rinomina interrotta su '%s'", moves[i].from);
        }
        off += (size_t)snprintf(out + off, 16384 - off, "  %s → %s\n", moves[i].from, moves[i].to);
    }
    /* dir import: artefatti interni nominati col nome del cervello */
    char newdir[2200];
    snprintf(newdir, sizeof newdir, "%s/%s", brains, new_name);
    if (access(newdir, F_OK) == 0) {
        static const char* inner[] = { "rag/%s.db", "rag/%s.state.json", "graphs/%s.json", NULL };
        for (int i = 0; inner[i]; i++) {
            char from[2400], to[2400], relfrom[128], relto[128];
            snprintf(relfrom, sizeof relfrom, inner[i], name);
            snprintf(relto, sizeof relto, inner[i], new_name);
            snprintf(from, sizeof from, "%s/%s", newdir, relfrom);
            snprintf(to, sizeof to, "%s/%s", newdir, relto);
            if (access(from, F_OK) == 0 && access(to, F_OK) != 0) rename(from, to);
        }
    }
    gmb_brain_reg_register(new_name, info.source[0] ? info.source : NULL,
                           info.vault_path[0] ? info.vault_path : NULL,
                           info.mode[0] ? info.mode : NULL, info.checksum[0] ? info.checksum : NULL,
                           info.version[0] ? info.version : NULL, info.docs,
                           info.signed_by[0] ? info.signed_by : NULL);
    gmb_brain_reg_unregister(name);
    int reattached = 0;
    if (info.attached) {
        gmb_brain_detach(name);
        reattached = gmb_brain_use(new_name);
    }
    off += (size_t)snprintf(out + off, 16384 - off, "registry: '%s' → '%s' (mode %s)%s",
                            name, new_name, info.mode, reattached ? ", ri-collegato" : "");
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = out;
    return r;
}

/* ── EXPORT / BACKUP / SIGN ──────────────────────────────────────────────── */

/* Emissione del manifest identica a System.Text.Json (camelCase, indentato 2 spazi, null
 * inclusi): il checksum è auto-riferito e deve combaciare byte per byte col C#. */
/* Serializzazione IDENTICA a System.Text.Json (WriteIndented, camelCase, null inclusi):
 * oggetti/array NON vuoti vanno su più righe con 2 spazi per livello. Il checksum è
 * auto-riferito: se il testo non combacia, un .gmbrain prodotto dal C# non si verificherebbe. */
static void json_emit_string_array(sbuf_t* b, const char* const* items, int n, const char* indent)
{
    if (n <= 0) { sb_put(b, "[]"); return; }
    sb_put(b, "[\n");
    for (int i = 0; i < n; i++) {
        sb_put(b, indent);
        sb_put(b, "  ");
        sb_esc(b, items[i]);
        sb_put(b, i + 1 < n ? ",\n" : "\n");
    }
    sb_put(b, indent);
    sb_put(b, "]");
}

static void json_emit_string_dict(sbuf_t* b, const char* const* keys, const char* const* vals, int n, const char* indent)
{
    if (n <= 0) { sb_put(b, "{}"); return; }
    sb_put(b, "{\n");
    for (int i = 0; i < n; i++) {
        sb_put(b, indent);
        sb_put(b, "  ");
        sb_esc(b, keys[i]);
        sb_put(b, ": ");
        sb_esc(b, vals[i]);
        sb_put(b, i + 1 < n ? ",\n" : "\n");
    }
    sb_put(b, indent);
    sb_put(b, "}");
}

static char* manifest_json(const char* name, const char* version, const char* mode,
                           const char* created_at, long docs,
                           const char* const* avail, int navail,
                           const char* const* fkeys, const char* const* fvals, int nfiles,
                           const char* signed_by, const char* source,
                           const char* const* pkeys, const char* const* pvals, int nprobe,
                           int with_checksum, const char* checksum)
{
    sbuf_t b;
    sb_init(&b);
    sb_put(&b, "{\n  \"format\": \"gmbrain\",\n  \"formatVersionValue\": 1,\n  \"name\": ");
    sb_esc(&b, name);
    sb_put(&b, ",\n  \"version\": ");
    if (version && *version) sb_esc(&b, version); else sb_put(&b, "null");
    sb_put(&b, ",\n  \"mode\": ");
    if (mode && *mode) sb_esc(&b, mode); else sb_put(&b, "null");
    sb_put(&b, ",\n  \"createdAt\": ");
    if (created_at && *created_at) sb_esc(&b, created_at); else sb_put(&b, "null");
    char t[96];
    snprintf(t, sizeof t, ",\n  \"docs\": %ld,\n  \"parsers\": [],\n  \"available\": ", docs);
    sb_put(&b, t);
    json_emit_string_array(&b, avail, navail, "  ");
    sb_put(&b, ",\n  \"files\": ");
    json_emit_string_dict(&b, fkeys, fvals, nfiles, "  ");
    sb_put(&b, ",\n");
    if (with_checksum) {
        sb_put(&b, "  \"checksum\": ");
        if (checksum && *checksum) sb_esc(&b, checksum); else sb_put(&b, "null");
        sb_put(&b, ",\n");
    }
    sb_put(&b, "  \"signedBy\": ");
    if (signed_by && *signed_by) sb_esc(&b, signed_by); else sb_put(&b, "null");
    sb_put(&b, ",\n  \"source\": ");
    if (source && *source) sb_esc(&b, source); else sb_put(&b, "null");
    sb_put(&b, ",\n  \"probe\": ");
    json_emit_string_dict(&b, pkeys, pvals, nprobe, "  ");
    sb_put(&b, "\n}");
    return b.p;
}

static char* sha256_prefixed(const char* path)
{
    return gmb_brain_manifest_hash_file(path);
}

/* Copia file (binario). 1 = ok. */
static int copy_file(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in) return 0;
    char d[2200];
    snprintf(d, sizeof d, "%s", dst);
    char* sl = strrchr(d, '/');
    if (sl) { *sl = 0; gmb_fs_mkdir_p(d); }
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[65536];
    size_t r;
    int ok = 1;
    while ((r = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, r, out) != r) { ok = 0; break; }
    fclose(in);
    fclose(out);
    return ok;
}

gmb_brain_result_t gmb_brain_tool_export(const char* name, const char* out_zip)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    char avail_csv[64] = "";
    if (!gmb_brain_reg_available(name, avail_csv, sizeof avail_csv))
        return br_fail("cervello '%s' non esportabile (nessun artefatto rag/graph)", name);

    char ragdb[2048], graphjson[2048];
    int has_rag = gmb_brain_reg_rag_db(name, ragdb, sizeof ragdb) && access(ragdb, R_OK) == 0;
    int has_graph = gmb_brain_reg_graph_json(name, graphjson, sizeof graphjson) && access(graphjson, R_OK) == 0;

    char staging[2400], tmpdir[2400];
    const char* tmp = getenv("TMPDIR");
    snprintf(tmpdir, sizeof tmpdir, "%s", (tmp && *tmp) ? tmp : "/tmp");
    snprintf(staging, sizeof staging, "%s/gmbrain-export-%ld", tmpdir, (long)getpid());
    gmb_fs_rmtree(staging);
    gmb_fs_mkdir_p(staging);
    char ragstage[2400], graphstage[2400];
    snprintf(ragstage, sizeof ragstage, "%s/rag", staging);
    snprintf(graphstage, sizeof graphstage, "%s/graphs", staging);
    gmb_fs_mkdir_p(ragstage);
    gmb_fs_mkdir_p(graphstage);

    char* fkeys[64];
    char* fvals[64];
    int nfiles = 0;

    if (has_rag) {
        char dst[2400], rel[256];
        snprintf(dst, sizeof dst, "%s/%s.db", ragstage, name);
        if (copy_file(ragdb, dst) && nfiles < 64) {
            snprintf(rel, sizeof rel, "rag/%s.db", name);
            char* h = sha256_prefixed(dst);
            fkeys[nfiles] = strdup(rel);
            fvals[nfiles] = h ? h : strdup("");
            nfiles++;
        }
        char state[2200];
        snprintf(state, sizeof state, "%s", ragdb);
        char* dot = strrchr(state, '.');
        if (dot) {
            snprintf(dot, sizeof state - (size_t)(dot - state), ".state.json");
            if (access(state, R_OK) == 0 && nfiles < 64) {
                char sdst[2400];
                snprintf(sdst, sizeof sdst, "%s/%s.state.json", ragstage, name);
                if (copy_file(state, sdst)) {
                    snprintf(rel, sizeof rel, "rag/%s.state.json", name);
                    char* h = sha256_prefixed(sdst);
                    fkeys[nfiles] = strdup(rel);
                    fvals[nfiles] = h ? h : strdup("");
                    nfiles++;
                }
            }
        }
    }
    if (has_graph) {
        static const char* exts[] = { "json", "graphml", "mermaid", "stats.json", "state.json", NULL };
        char gdir[2048];
        snprintf(gdir, sizeof gdir, "%s", graphjson);
        char* sl = strrchr(gdir, '/');
        if (sl) *sl = 0;
        for (int i = 0; exts[i] && nfiles < 64; i++) {
            char src[2400], dst[2400], rel[256];
            snprintf(src, sizeof src, "%s/%s.%s", gdir, name, exts[i]);
            if (access(src, R_OK) != 0) continue;
            snprintf(dst, sizeof dst, "%s/%s.%s", graphstage, name, exts[i]);
            if (!copy_file(src, dst)) continue;
            snprintf(rel, sizeof rel, "graphs/%s.%s", name, exts[i]);
            char* h = sha256_prefixed(dst);
            fkeys[nfiles] = strdup(rel);
            fvals[nfiles] = h ? h : strdup("");
            nfiles++;
        }
    }

    char created[64];
    br_now_utc_iso(created, sizeof created);
    /* docs: numero di documenti nel RAG (o .md nella radice, per i cervelli a solo grafo) */
    long docs = 0;
    if (has_rag) {
        sqlite3* db = br_sql_open(ragdb, 1);
        if (db) { docs = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_documents"); sqlite3_close(db); }
    } else {
        char root[1024];
        if (gmb_brain_reg_root(name, root, sizeof root)) docs = br_count_md(root, 0);
    }
    const char* mode = has_rag && has_graph ? "both" : has_rag ? "rag" : "graph";
    const char* avail[2];
    int navail = 0;
    if (has_rag) avail[navail++] = "rag";
    if (has_graph) avail[navail++] = "graph";

    char* no_cs = manifest_json(name, NULL, mode, created, docs, avail, navail,
                                (const char* const*)fkeys, (const char* const*)fvals, nfiles,
                                "gmb-export", NULL, NULL, NULL, 0, 0, NULL);
    if (!no_cs) return br_fail("memoria esaurita");
    unsigned char dig[32];
    gmb_sha256((const uint8_t*)no_cs, strlen(no_cs), dig);
    free(no_cs);
    char hex[80];
    snprintf(hex, sizeof hex, "sha256:");
    static const char* HX = "0123456789ABCDEF";
    for (int i = 0; i < 32; i++) { hex[7 + 2 * i] = HX[dig[i] >> 4]; hex[8 + 2 * i] = HX[dig[i] & 15]; }
    hex[7 + 64] = 0;

    char* with_cs = manifest_json(name, NULL, mode, created, docs, avail, navail,
                                  (const char* const*)fkeys, (const char* const*)fvals, nfiles,
                                  "gmb-export", NULL, NULL, NULL, 0, 1, hex);
    for (int i = 0; i < nfiles; i++) { free(fkeys[i]); free(fvals[i]); }
    if (!with_cs) { gmb_fs_rmtree(staging); return br_fail("memoria esaurita"); }
    char mpath[2500];
    snprintf(mpath, sizeof mpath, "%s/brain.manifest.json", staging);
    int ok = br_write_file(mpath, with_cs);
    free(with_cs);
    if (!ok) { gmb_fs_rmtree(staging); return br_fail("scrittura del manifest fallita"); }

    char zip[2400];
    if (out_zip && *out_zip) snprintf(zip, sizeof zip, "%s", out_zip);
    else {
        char exp[1024];
        gmb_paths_brains_exports_dir(exp, sizeof exp);
        gmb_fs_mkdir_p(exp);
        char stamp[32];
        time_t t = time(NULL);
        struct tm tmv;
        localtime_r(&t, &tmv);
        strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tmv);
        snprintf(zip, sizeof zip, "%s/%s-%s.gmbrain.zip", exp, name, stamp);
    }
    char** list = NULL;
    int n = 0;
    static const char* subs[] = { "", "rag", "graphs", NULL };
    for (int si = 0; subs[si]; si++) {
        char d[2500];
        if (subs[si][0]) snprintf(d, sizeof d, "%s/%s", staging, subs[si]);
        else snprintf(d, sizeof d, "%s", staging);
        DIR* dh = opendir(d);
        if (!dh) continue;
        struct dirent* e;
        while ((e = readdir(dh))) {
            if (e->d_name[0] == '.') continue;
            char full[2600];
            if (subs[si][0]) snprintf(full, sizeof full, "%s/%s/%s", staging, subs[si], e->d_name);
            else snprintf(full, sizeof full, "%s/%s", staging, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            list = (char**)realloc(list, (size_t)(n + 1) * sizeof(char*));
            list[n++] = strdup(full);
        }
        closedir(dh);
    }
    if (n == 0) { gmb_fs_rmtree(staging); return br_fail("nessun file da esportare"); }
    int rc = gmb_zip_files((const char* const*)list, n, zip);
    for (int i = 0; i < n; i++) free(list[i]);
    free(list);
    gmb_fs_rmtree(staging);
    if (rc != 0) return br_fail("creazione dello zip fallita (%s)", zip);
    return br_ok("esportato %s — available: [%s]", zip,
                 has_rag && has_graph ? "rag, graph" : avail_csv);
}

gmb_brain_result_t gmb_brain_tool_backup(const char* name, const char* out_zip)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    char zip[2400];
    if (!out_zip || !*out_zip) {
        char exp[1024];
        gmb_paths_brains_exports_dir(exp, sizeof exp);
        gmb_fs_mkdir_p(exp);
        char stamp[32];
        time_t t = time(NULL);
        struct tm tmv;
        localtime_r(&t, &tmv);
        strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tmv);
        snprintf(zip, sizeof zip, "%s/%s-%s.gmbrain.zip", exp, name, stamp);
    } else snprintf(zip, sizeof zip, "%s", out_zip);
    gmb_brain_result_t r = gmb_brain_tool_export(name, zip);
    if (!r.ok) return r;
    free(r.text);
    char avail[64] = "";
    gmb_brain_reg_available(name, avail, sizeof avail);
    return br_ok("backup: %s — available: [%s]", zip,
                 !strcmp(avail, "rag,graph") ? "rag, graph" : avail);
}

/* probe minimi per SIGN: grafo dal vault, vectordb interrogabile */
static int probe_graph_dir(const char* dir, char* report, size_t cap)
{
    long md = br_count_md(dir, 200);
    /* grafo già pronto? */
    DIR* d = opendir(dir);
    int ready = 0;
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            size_t l = strlen(e->d_name);
            if (l > 5 && (!strcasecmp(e->d_name + l - 5, ".json") || !strcasecmp(e->d_name + l - 8, ".graphml"))) { ready = 1; break; }
        }
        closedir(d);
    }
    if (md == 0 && !ready) { snprintf(report, cap, "fail: 0 .md e nessun grafo json/graphml pronto"); return 0; }
    if (ready) { snprintf(report, cap, "ok: grafo pronto"); return 1; }
    char tmp[1200];
    const char* t = getenv("TMPDIR");
    snprintf(tmp, sizeof tmp, "%s/gmbsign-%ld", (t && *t) ? t : "/tmp", (long)getpid());
    gmb_fs_rmtree(tmp);
    gmb_fs_mkdir_p(tmp);
    int64_t nn = gmb_graph_build(dir, tmp);
    gmb_fs_rmtree(tmp);
    if (nn >= 1) { snprintf(report, cap, "ok: %lld nodi", (long long)nn); return 1; }
    snprintf(report, cap, "fail: 0 nodi (nessun .md con contenuto riconosciuto)");
    return 0;
}

static int probe_vectordb_dir(const char* dir, char* report, size_t cap, char* db_out, size_t dbcap)
{
    DIR* d = opendir(dir);
    if (!d) { snprintf(report, cap, "fail: cartella non leggibile"); return 0; }
    struct dirent* e;
    int found = 0, ok = 0;
    long docs = 0, chunks = 0;
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l < 4) continue;
        if (strcasecmp(e->d_name + l - 3, ".db") && strcasecmp(e->d_name + l - 7, ".sqlite")) continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        found++;
        sqlite3* db = br_sql_open(p, 1);
        if (!db) continue;
        long ncoll = br_sql_scalar(db, "SELECT COUNT(*) FROM vec_collections");
        if (ncoll > 0) {
            ok = 1;
            docs += br_sql_scalar(db, "SELECT COUNT(*) FROM vec_documents");
            chunks += br_sql_scalar(db, "SELECT COUNT(*) FROM vec_chunks");
            if (db_out && dbcap) snprintf(db_out, dbcap, "%s", p);
        }
        sqlite3_close(db);
    }
    closedir(d);
    if (!found) { snprintf(report, cap, "fail: nessun .db/.sqlite"); return 0; }
    if (!ok) { snprintf(report, cap, "fail: nessun db con tabelle vec_*"); return 0; }
    snprintf(report, cap, "ok: %ld documenti, %ld chunk", docs, chunks);
    return 1;
}

gmb_brain_result_t gmb_brain_tool_sign(const char* name, const char* dir, const char* out_zip)
{
    if (!name || !*name || !dir || !*dir) return br_fail("uso: BRAIN SIGN <nome> <cartella> [--out zip]");
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return br_fail("cartella non trovata: %s", dir);

    char graph_report[256] = "", rag_report[256] = "";
    int graph_ok = probe_graph_dir(dir, graph_report, sizeof graph_report);
    char db_src[2048] = "";
    int rag_ok = probe_vectordb_dir(dir, rag_report, sizeof rag_report, db_src, sizeof db_src);
    if (!graph_ok && !rag_ok)
        return br_fail("il cervello '%s' NON è valido (nessun probe passato):\n  graph: %s\n  vectordb: %s",
                       name, graph_report, rag_report);

    long docs = br_count_md(dir, 0);
    const char* mode = graph_ok && rag_ok ? "both" : graph_ok ? "graph" : "rag";
    const char* avail[2];
    int navail = 0;
    if (graph_ok) avail[navail++] = "graph";
    if (rag_ok) avail[navail++] = "rag";

    char brains[1024], dest[2200];
    gmb_paths_brains_dir(brains, sizeof brains);
    snprintf(dest, sizeof dest, "%s/%s", brains, name);
    gmb_fs_mkdir_p(dest);

    char* fkeys[16];
    char* fvals[16];
    int nfiles = 0;

    if (rag_ok && db_src[0]) {
        char dst[2400], rel[256];
        snprintf(dst, sizeof dst, "%s/rag/%s.db", dest, name);
        if (copy_file(db_src, dst)) {
            snprintf(rel, sizeof rel, "rag/%s.db", name);
            char* h = sha256_prefixed(dst);
            fkeys[nfiles] = strdup(rel);
            fvals[nfiles] = h ? h : strdup("");
            nfiles++;
        }
    }
    if (graph_ok) {
        char gj[2400], gdir2[2400];
        snprintf(gj, sizeof gj, "%s/graphs/%s.json", dest, name);
        snprintf(gdir2, sizeof gdir2, "%s/graphs", dest);
        gmb_fs_mkdir_p(gdir2);
        char ready[4096] = "";
        DIR* d = opendir(dir);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d))) {
                size_t l = strlen(e->d_name);
                if (l > 5 && !strcasecmp(e->d_name + l - 5, ".json")) {
                    snprintf(ready, sizeof ready, "%s/%s", dir, e->d_name);
                    break;
                }
            }
            closedir(d);
        }
        int made = 0;
        if (ready[0] && copy_file(ready, gj)) made = 1;
        else {
            int64_t nn = gmb_graph_build(dir, gdir2);
            if (nn >= 0) {
                char base[512];
                snprintf(base, sizeof base, "%s", dir);
                size_t bl = strlen(base);
                while (bl > 1 && base[bl - 1] == '/') base[--bl] = 0;
                char* sl = strrchr(base, '/');
                char src[2400];
                snprintf(src, sizeof src, "%s/%s.json", gdir2, sl ? sl + 1 : base);
                if (strcmp(src, gj) != 0) rename(src, gj);
                made = (access(gj, R_OK) == 0);
            }
        }
        if (made) {
            br_graph_write_artifacts(name, gdir2);
            char* h = sha256_prefixed(gj);
            char rel[256];
            snprintf(rel, sizeof rel, "graphs/%s.json", name);
            fkeys[nfiles] = strdup(rel);
            fvals[nfiles] = h ? h : strdup("");
            nfiles++;
        }
    }

    const char* pk[2] = { "graph", "vectordb" };
    const char* pv[2] = { graph_report, rag_report };
    char created[64];
    br_now_utc_iso(created, sizeof created);
    char* no_cs = manifest_json(name, NULL, mode, created, docs, avail, navail,
                                (const char* const*)fkeys, (const char* const*)fvals, nfiles,
                                "gmb-sign", "user-provided", pk, pv, 2, 0, NULL);
    if (!no_cs) {
        for (int i = 0; i < nfiles; i++) { free(fkeys[i]); free(fvals[i]); }
        return br_fail("memoria esaurita");
    }
    unsigned char dig[32];
    gmb_sha256((const uint8_t*)no_cs, strlen(no_cs), dig);
    free(no_cs);
    char hex[80];
    snprintf(hex, sizeof hex, "sha256:");
    static const char* HX = "0123456789ABCDEF";
    for (int i = 0; i < 32; i++) { hex[7 + 2 * i] = HX[dig[i] >> 4]; hex[8 + 2 * i] = HX[dig[i] & 15]; }
    hex[7 + 64] = 0;
    char* manifest = manifest_json(name, NULL, mode, created, docs, avail, navail,
                                   (const char* const*)fkeys, (const char* const*)fvals, nfiles,
                                   "gmb-sign", "user-provided", pk, pv, 2, 1, hex);
    for (int i = 0; i < nfiles; i++) { free(fkeys[i]); free(fvals[i]); }
    if (!manifest) return br_fail("memoria esaurita");
    char mpath[2400];
    snprintf(mpath, sizeof mpath, "%s/brain.manifest.json", dest);
    int ok = br_write_file(mpath, manifest);
    free(manifest);
    if (!ok) return br_fail("scrittura del manifest fallita (%s)", mpath);

    gmb_brain_reg_register(name, dir, dest, mode, hex, NULL, docs, "gmb-sign");

    char* out = (char*)malloc(4096);
    if (!out) return br_fail("memoria esaurita");
    size_t off = (size_t)snprintf(out, 4096,
        "[OK] brain '%s' firmato — available: [%s]\n  probe.graph: %s\n  probe.vectordb: %s",
        name, graph_ok && rag_ok ? "graph, rag" : graph_ok ? "graph" : "rag", graph_report, rag_report);
    if (out_zip && *out_zip) {
        gmb_brain_result_t e = gmb_brain_tool_export(name, out_zip);
        if (e.ok) {
            off += (size_t)snprintf(out + off, 4096 - off, "\n[OK] zip: %s", out_zip);
            free(e.text);
        } else {
            off += (size_t)snprintf(out + off, 4096 - off, "\n?export zip fallito: %s", e.error ? e.error : "");
            free(e.error);
        }
    }
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = out;
    return r;
}
