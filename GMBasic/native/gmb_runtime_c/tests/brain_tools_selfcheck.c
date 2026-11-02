/* brain_tools_selfcheck.c — toolchain del Second Brain in C:
 * stats · verify · dedupe · extract · merge · convert · diff · doctor · export · sign.
 *
 * DB RAG sintetici (schema canonico) e grafi da vault temporanei: nessuna rete, nessun
 * provider di embedding (le operazioni testate non ri-embedano) e nessuna scrittura nella
 * config dell'utente (gmb_paths_test_override).
 */
#include "gmb_runtime.h"
#include "gmb_brain_tools.h"
#include "gmb_compress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sqlite3.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail = 1; } } while (0)

static void mkdirs(const char* p)
{
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char* q = tmp + 1; *q; q++)
        if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
    mkdir(tmp, 0755);
}

static void write_file(const char* path, const char* text)
{
    char dir[4096];
    snprintf(dir, sizeof dir, "%s", path);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; mkdirs(dir); }
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static long file_size(const char* p)
{
    struct stat st;
    return (stat(p, &st) == 0) ? (long)st.st_size : 0;
}

static void exec_sql(sqlite3* db, const char* sql)
{
    char* err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK)
        fprintf(stderr, "SQL: %s (%s)\n", err ? err : "?", sql);
    if (err) sqlite3_free(err);
}

/* DB vettoriale sintetico: schema canonico + N documenti con chunk/inclusioni fisse. */
static void make_db(const char* path, const char* const* sources, const char* const* texts,
                    int ndocs, int dup_chunk, int crumb)
{
    unlink(path);
    {   /* solo la CARTELLA del db (il path completo è un file) */
        char dir[4300];
        snprintf(dir, sizeof dir, "%s", path);
        char* sl = strrchr(dir, '/');
        if (sl) { *sl = 0; mkdirs(dir); }
    }
    sqlite3* db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { fprintf(stderr, "open %s\n", path); if (db) sqlite3_close(db); return; }
    exec_sql(db,
        "CREATE TABLE vec_collections (id INTEGER PRIMARY KEY, name TEXT UNIQUE, dimensions INTEGER, metric TEXT,"
        " model_version TEXT DEFAULT 'fixture-v1', model_digest TEXT, format TEXT DEFAULT 'fp32',"
        " created_at TEXT DEFAULT (datetime('now')));"
        "CREATE TABLE vec_documents (id INTEGER PRIMARY KEY, collection_id INTEGER, source_id TEXT, source_hash TEXT,"
        " status TEXT DEFAULT 'current', tenant_id TEXT, access_roles TEXT, metadata_json TEXT,"
        " created_at TEXT DEFAULT (datetime('now')), updated_at TEXT DEFAULT (datetime('now')),"
        " UNIQUE(collection_id, source_id));"
        "CREATE TABLE vec_chunks (id INTEGER PRIMARY KEY, document_id INTEGER, chunk_index INTEGER, text TEXT,"
        " token_count INTEGER, metadata_json TEXT, UNIQUE(document_id, chunk_index));"
        "CREATE TABLE vec_embeddings (chunk_id INTEGER, model_version TEXT, embedding_created_at TEXT DEFAULT (datetime('now')),"
        " dimensions INTEGER, embedding BLOB, PRIMARY KEY (chunk_id, model_version));"
        "CREATE TABLE fts_chunks (text, chunk_id UNINDEXED, collection_id UNINDEXED);"
        "INSERT INTO vec_collections(id,name,dimensions,metric) VALUES(1,'wiki',4,'cosine');");
    for (int i = 0; i < ndocs; i++) {
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db, "INSERT INTO vec_documents(id,collection_id,source_id,source_hash) VALUES(?1,1,?2,?3)", -1, &st, NULL) == SQLITE_OK) {
            char h[32];
            snprintf(h, sizeof h, "h%d", i + 1);
            sqlite3_bind_int(st, 1, i + 1);
            sqlite3_bind_text(st, 2, sources[i], -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 3, h, -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
        /* chunk 0 = testo del documento; chunk 1 = opzionale duplicato/crumb */
        for (int k = 0; k < 2; k++) {
            const char* txt = (k == 0) ? texts[i] : (k == 1 ? (crumb ? "x" : texts[i]) : NULL);
            if (!txt) continue;
            if (k == 1 && !dup_chunk && !crumb) continue;
            sqlite3_stmt* ci = NULL;
            if (sqlite3_prepare_v2(db, "INSERT INTO vec_chunks(document_id,chunk_index,text) VALUES(?1,?2,?3)", -1, &ci, NULL) == SQLITE_OK) {
                sqlite3_bind_int(ci, 1, i + 1);
                sqlite3_bind_int(ci, 2, k);
                sqlite3_bind_text(ci, 3, txt, -1, SQLITE_TRANSIENT);
                sqlite3_step(ci);
                sqlite3_finalize(ci);
            }
            long cid = (long)sqlite3_last_insert_rowid(db);
            sqlite3_stmt* ei = NULL;
            if (sqlite3_prepare_v2(db, "INSERT INTO vec_embeddings(chunk_id,model_version,dimensions,embedding)"
                                       " VALUES(?1,'fixture-v1',4,?2)", -1, &ei, NULL) == SQLITE_OK) {
                float v[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
                sqlite3_bind_int64(ei, 1, cid);
                sqlite3_bind_blob(ei, 2, v, sizeof v, SQLITE_TRANSIENT);
                sqlite3_step(ei);
                sqlite3_finalize(ei);
            }
            sqlite3_stmt* fi = NULL;
            if (sqlite3_prepare_v2(db, "INSERT INTO fts_chunks(text,chunk_id,collection_id) VALUES(?1,?2,1)", -1, &fi, NULL) == SQLITE_OK) {
                sqlite3_bind_text(fi, 1, txt, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(fi, 2, cid);
                sqlite3_step(fi);
                sqlite3_finalize(fi);
            }
        }
    }
    sqlite3_close(db);
}

static long db_scalar(const char* path, const char* sql)
{
    sqlite3* db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) { if (db) sqlite3_close(db); return -1; }
    sqlite3_stmt* st = NULL;
    long v = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        v = (long)sqlite3_column_int64(st, 0);
    if (st) sqlite3_finalize(st);
    sqlite3_close(db);
    return v;
}

/* Contenuto della prima entry che corrisponde a `name` dentro uno zip in memoria. */
typedef struct { const char* name; char* data; size_t len; int found; } entry_t;
static int zip_cb(const char* name, const unsigned char* content, size_t clen, void* ctx)
{
    entry_t* e = (entry_t*)ctx;
    if (strcmp(name, e->name) != 0) return 0;
    e->data = (char*)malloc(clen + 1);
    if (e->data) { memcpy(e->data, content, clen); e->data[clen] = 0; e->len = clen; e->found = 1; }
    return 0;
}

int main(void)
{
    char tmpl[] = "/tmp/gmb_brain_tools_sc.XXXXXX";
    char* root = mkdtemp(tmpl);
    CHECK(root != NULL, "mkdtemp");
    if (!root) return 1;

    char cfg[4200], ragd[4200], graphs[4200], brains[4200], bcache[4200], bexp[4200];
    snprintf(cfg, sizeof cfg, "%s/config", root);
    snprintf(ragd, sizeof ragd, "%s/rag", root);
    snprintf(graphs, sizeof graphs, "%s/graphs", root);
    snprintf(brains, sizeof brains, "%s/brains", root);
    snprintf(bcache, sizeof bcache, "%s/bcache", root);
    snprintf(bexp, sizeof bexp, "%s/bexp", root);
    gmb_paths_test_override("configDir", cfg);
    gmb_paths_test_override("ragDir", ragd);
    gmb_paths_test_override("graphsDir", graphs);
    gmb_paths_test_override("brainsDir", brains);
    gmb_paths_test_override("brainsCacheDir", bcache);
    gmb_paths_test_override("brainsExportsDir", bexp);

    /* ── dati: r1 (2 doc, 1 chunk duplicato, 1 crumb), r2 (1 doc) ── */
    const char* src1[2] = { "wiki/A.md", "wiki/B.md" };
    const char* txt1[2] = { "Alpha testo con un wikilink verso [[B]] e testo lungo da non essere un crumb",
                            "Beta collegato ad [[A]] con altro testo lungo per superare la soglia minima" };
    const char* src2[1] = { "wiki/C.md" };
    const char* txt2[1] = { "Gamma contenuto diverso con link verso A e testo lungo per il merge" };
    char db1[4300], db2[4300];
    snprintf(db1, sizeof db1, "%s/r1.db", ragd);
    snprintf(db2, sizeof db2, "%s/r2.db", ragd);
    make_db(db1, src1, txt1, 2, 1, 1);
    make_db(db2, src2, txt2, 1, 0, 0);
    CHECK(db_scalar(db1, "SELECT COUNT(*) FROM vec_documents") == 2, "setup r1: 2 documenti");
    CHECK(db_scalar(db1, "SELECT COUNT(*) FROM vec_chunks") == 4, "setup r1: 4 chunk (2 + dup + crumb)");

    CHECK(gmb_brain_reg_register("r1", db1, NULL, "rag", NULL, NULL, 0, NULL) == 1, "register r1");
    CHECK(gmb_brain_reg_register("r2", db2, NULL, "rag", NULL, NULL, 0, NULL) == 1, "register r2");

    /* ── STATS ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_stats("r1", 0);
        CHECK(r.ok && r.text && strstr(r.text, "BRAIN STATS") != NULL, "stats: testo");
        CHECK(r.text && strstr(r.text, "2 documenti") != NULL, "stats: documenti");
        gmb_brain_result_free(&r);
        r = gmb_brain_tool_stats("r1", 1);
        CHECK(r.ok && r.text && strstr(r.text, "\"documents\": 2") != NULL, "stats --json: documents");
        gmb_brain_result_free(&r);
        r = gmb_brain_tool_stats("ghost", 0);
        CHECK(!r.ok, "stats: cervello sconosciuto = errore");
        gmb_brain_result_free(&r);
    }

    /* ── VERIFY ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_verify("r1", 0);
        CHECK(r.ok && r.text && strstr(r.text, "[ok] rag.integrity") != NULL, "verify: integrità");
        CHECK(r.text && strstr(r.text, "[ok] rag.embeddings") != NULL, "verify: embeddings");
        gmb_brain_result_free(&r);
    }

    /* ── DEDUPE ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_dedupe("r1", 0, 40, 0);
        CHECK(r.ok && r.text && strstr(r.text, "duplicati") != NULL, "dedupe: report");
        CHECK(r.text && strstr(r.text, "usa --apply") != NULL, "dedupe: invito ad apply");
        gmb_brain_result_free(&r);
        CHECK(db_scalar(db1, "SELECT COUNT(*) FROM vec_chunks") == 4, "dedupe report non scrive");

        r = gmb_brain_tool_dedupe("r1", 1, 40, 0);
        CHECK(r.ok, "dedupe apply");
        gmb_brain_result_free(&r);
        long chunks = db_scalar(db1, "SELECT COUNT(*) FROM vec_chunks");
        CHECK(chunks == 2, "dedupe apply: resta 1 chunk per documento");
        CHECK(db_scalar(db1, "SELECT COUNT(*) FROM vec_embeddings") == 2, "dedupe: embedding coerenti");
        CHECK(db_scalar(db1, "SELECT COUNT(*) FROM fts_chunks") == 2, "dedupe: FTS coerente");
    }

    /* ── EXTRACT ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_extract("r1", "r1sub", "wiki/A", NULL, 1, 0);
        CHECK(r.ok, "extract");
        gmb_brain_result_free(&r);
        char sub[4300];
        snprintf(sub, sizeof sub, "%s/r1sub.db", ragd);
        CHECK(file_size(sub) > 0, "extract: db creato");
        CHECK(db_scalar(sub, "SELECT COUNT(*) FROM vec_documents") == 1, "extract: 1 documento filtrato");
        CHECK(db_scalar(sub, "SELECT COUNT(*) FROM vec_chunks") == 1, "extract: chunk copiati");
    }

    /* ── MERGE ── */
    {
        const char* names[2] = { "r1", "r2" };
        gmb_brain_result_t r = gmb_brain_tool_merge(names, 2, "rmerge", "skip", 0);
        CHECK(r.ok, "merge");
        CHECK(r.text && strstr(r.text, "registrato: rmerge") != NULL, "merge: registrazione");
        gmb_brain_result_free(&r);
        char mg[4300];
        snprintf(mg, sizeof mg, "%s/rmerge.db", ragd);
        CHECK(db_scalar(mg, "SELECT COUNT(*) FROM vec_documents") == 3, "merge: 3 documenti uniti");
        CHECK(db_scalar(mg, "SELECT COUNT(*) FROM vec_chunks") == 3, "merge: 3 chunk");
    }

    /* ── CONVERT rag → graph (wikilink A↔B) ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_convert("r1", "graph", "r1g", 0, 0, 0);
        CHECK(r.ok, "convert rag→graph");
        gmb_brain_result_free(&r);
        char gj[4300];
        snprintf(gj, sizeof gj, "%s/r1g.json", graphs);
        CHECK(file_size(gj) > 0, "convert: grafo scritto");
        char* js = NULL;
        FILE* f = fopen(gj, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            js = (char*)malloc((size_t)n + 1);
            if (js && fread(js, 1, (size_t)n, f) != (size_t)n) { free(js); js = NULL; }
            if (js) js[n] = 0;
            fclose(f);
        }
        CHECK(js && strstr(js, "\"edges\"") != NULL && strstr(js, "link") != NULL, "convert: edge wikilink");
        free(js);
        char stats[4300];
        snprintf(stats, sizeof stats, "%s/r1g.stats.json", graphs);
        CHECK(file_size(stats) > 0, "convert: stats.json");
        char* sj = NULL;
        f = fopen(stats, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            sj = (char*)malloc((size_t)n + 1);
            if (sj && fread(sj, 1, (size_t)n, f) != (size_t)n) { free(sj); sj = NULL; }
            if (sj) sj[n] = 0;
            fclose(f);
        }
        CHECK(sj && strstr(sj, "\"godNodes\"") != NULL, "stats: godNodes");
        CHECK(sj && strstr(sj, "\"surprisingConnections\"") != NULL, "stats: surprisingConnections");
        CHECK(sj && strstr(sj, "\"suggestedQuestions\"") != NULL, "stats: suggestedQuestions");
        free(sj);
    }

    /* ── DIFF ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_diff("r1", "r2", 0);
        CHECK(r.ok && r.text && strstr(r.text, "BRAIN DIFF") != NULL, "diff: testo");
        gmb_brain_result_free(&r);
        r = gmb_brain_tool_diff("r1", "r2", 1);
        CHECK(r.ok && r.text && strstr(r.text, "\"rag\":") != NULL, "diff --json");
        gmb_brain_result_free(&r);
    }

    /* ── DOCTOR ── */
    {
        gmb_brain_result_t r = gmb_brain_tool_doctor("r1");
        CHECK(r.ok, "doctor");
        CHECK(r.text && (strstr(r.text, "nessun problema") || strstr(r.text, "riverifica: OK")) != NULL,
              "doctor: esito");
        gmb_brain_result_free(&r);
    }

    /* ── EXPORT + manifest (roundtrip: unzip e verifica) ── */
    {
        char zip[4300];
        snprintf(zip, sizeof zip, "%s/r1.gmbrain.zip", root);
        gmb_brain_result_t r = gmb_brain_tool_export("r1", zip);
        CHECK(r.ok, "export");
        gmb_brain_result_free(&r);
        CHECK(file_size(zip) > 0, "export: zip creato");

        char out[4300];
        snprintf(out, sizeof out, "%s/unzip", root);
        mkdirs(out);
        CHECK(gmb_unzip_files(zip, out) == 0, "export: unzip");
        char err[512] = "";
        CHECK(gmb_brain_manifest_verify(out, err, sizeof err) == 0, "export: manifest verificato dopo unzip");
        char unzipped_db[4500];
        snprintf(unzipped_db, sizeof unzipped_db, "%s/rag/r1.db", out);
        CHECK(file_size(unzipped_db) > 0, "export: db RAG nello zip");
    }

    /* ── SIGN (vault markdown) ── */
    {
        char vault[4300];
        snprintf(vault, sizeof vault, "%s/vault-src", root);
        char a[4400], b2[4400];
        snprintf(a, sizeof a, "%s/A.md", vault);
        snprintf(b2, sizeof b2, "%s/B.md", vault);
        write_file(a, "# A\n[[B]]\n");
        write_file(b2, "# B\n[[A]]\n");
        gmb_brain_result_t r = gmb_brain_tool_sign("sig1", vault, NULL);
        CHECK(r.ok, "sign");
        CHECK(r.text && strstr(r.text, "firmato") != NULL, "sign: messaggio");
        gmb_brain_result_free(&r);
        char mpath[4400];
        snprintf(mpath, sizeof mpath, "%s/sig1/brain.manifest.json", brains);
        CHECK(file_size(mpath) > 0, "sign: manifest scritto");
        gmb_brain_result_t v = gmb_brain_tool_verify("sig1", 0);
        CHECK(v.ok, "sign: verify del firmato");
        gmb_brain_result_free(&v);
    }

    gmb_paths_test_override(NULL, NULL);
    {
        char clean[4300];
        snprintf(clean, sizeof clean, "rm -rf '%s'", root);
        if (system(clean) != 0) { /* best-effort */ }
    }

    if (g_fail) { fprintf(stderr, "brain_tools_selfcheck: FALLITO\n"); return 1; }
    printf("brain_tools_selfcheck: OK\n");
    return 0;
}
