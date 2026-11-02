/* FILE: native/gmb_runtime_c/src/gmb_rag.c
 * RAG paritario col C# (SqlVectorSchema/SqlVectorIndexer/SqlVectorSearch/SqlVectorFts) —
 * §0bis: logica condivisibile tra VM/C vive nel runtime condiviso.
 *
 * Il C# (AiVectorRuntime via SQL OPEN su rag/<name>.db) usa lo schema:
 *   vec_collections (name, dimensions, metric, model_version, created_at)
 *   vec_documents  (collection_id, source_id, source_hash, status, tenant_id,
 *                   access_roles, metadata_json, created_at, updated_at)
 *   vec_chunks     (document_id, chunk_index, text, token_count, metadata_json)
 *   vec_embeddings (chunk_id, model_version, dimensions, embedding)  PK(chunk_id, model_version)
 *   fts_chunks     (FTS5: text, chunk_id UNINDEXED, collection_id UNINDEXED)
 *   vec_query_audit
 * Questo file implementa LO STESSO schema e la stessa pipeline in C, così C# e C possono
 * condividere gli stessi file rag/<name>.db (embedding blob float32 identici).
 *
 * API esposte (gmb_rag_*): nuova superficie paritaria. Le funzioni gmb_vecdb_* e gmb_aiv_*
 * legacy in gmb_vectordb.c restano finché i consumer (VM/C syscall, C99) non sono migrati.
 */

#include "gmb_runtime.h"
#include "gmb_commonlib.h"   /* gmb_sha256 (source_hash) */
#include "gmb_strbuf.h"      /* gmb_strbuf_t (accumulo chunking) */
#include "gmb_rag_internal.h"
#include "gmb_vecquant.h"  /* storage SQ8 (unica implementazione §0bis) */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>

#define GMB_RAG_CHUNK_DEFAULT 512
#define GMB_RAG_CHUNK_OVERLAP 64

/* ========================================================================
 * Handle registry (stesso pattern di gmb_vectordb.c: path → handle sqlite)
 * ======================================================================== */
#define GMB_RAG_MAX 8

typedef struct {
    int used;
    int external;      /* handle sqlite NON posseduto (area SQL OPEN): non va chiuso da gmb_rag */
    char path[1024];
    sqlite3* db;
} gmb_rag_db;

static gmb_rag_db g_rdb[GMB_RAG_MAX];
int g_fts_available = -1;   /* -1 non verificato, 0 no, 1 sì (condiviso con gmb_rag_search.c) */

sqlite3* rag_db(int64_t h)
{
    if (h < 1 || h >= GMB_RAG_MAX || !g_rdb[h].used) return NULL;
    return g_rdb[h].db;
}

/* ========================================================================
 * Schema C# allineato (SqlVectorSchema) — idempotente
 * ======================================================================== */
static const char* kRagSchema =
    "CREATE TABLE IF NOT EXISTS vec_collections ("
    "  id            INTEGER PRIMARY KEY,"
    "  name          TEXT NOT NULL UNIQUE,"
    "  dimensions    INTEGER NOT NULL,"
    "  metric        TEXT NOT NULL CHECK (metric IN ('cosine','l2')),"
    "  model_version TEXT NOT NULL DEFAULT 'fixture-v1',"
    "  model_digest  TEXT,"
    "  format        TEXT NOT NULL DEFAULT 'fp32' CHECK (format IN ('fp32','sq8')),"
    "  created_at    TEXT NOT NULL DEFAULT (datetime('now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS vec_documents ("
    "  id              INTEGER PRIMARY KEY,"
    "  collection_id   INTEGER NOT NULL REFERENCES vec_collections(id) ON DELETE CASCADE,"
    "  source_id       TEXT NOT NULL,"
    "  source_hash     TEXT NOT NULL,"
    "  status          TEXT NOT NULL DEFAULT 'current' CHECK (status IN ('draft','current','deprecated')),"
    "  tenant_id       TEXT,"
    "  access_roles    TEXT,"
    "  metadata_json   TEXT,"
    "  created_at      TEXT NOT NULL DEFAULT (datetime('now')),"
    "  updated_at      TEXT NOT NULL DEFAULT (datetime('now')),"
    "  UNIQUE(collection_id, source_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS vec_chunks ("
    "  id            INTEGER PRIMARY KEY,"
    "  document_id   INTEGER NOT NULL REFERENCES vec_documents(id) ON DELETE CASCADE,"
    "  chunk_index   INTEGER NOT NULL,"
    "  text          TEXT NOT NULL,"
    "  token_count   INTEGER,"
    "  metadata_json TEXT,"
    "  UNIQUE(document_id, chunk_index)"
    ");"
    "CREATE TABLE IF NOT EXISTS vec_embeddings ("
    "  chunk_id             INTEGER NOT NULL REFERENCES vec_chunks(id) ON DELETE CASCADE,"
    "  model_version        TEXT NOT NULL,"
    "  embedding_created_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  dimensions           INTEGER NOT NULL,"
    "  embedding            BLOB NOT NULL,"
    "  PRIMARY KEY (chunk_id, model_version)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_vec_documents_collection ON vec_documents(collection_id);"
    "CREATE INDEX IF NOT EXISTS idx_vec_documents_status ON vec_documents(status);"
    "CREATE TABLE IF NOT EXISTS vec_query_audit ("
    "  id              INTEGER PRIMARY KEY,"
    "  queried_at      TEXT NOT NULL DEFAULT (datetime('now')),"
    "  collection_id   INTEGER,"
    "  query_text      TEXT,"
    "  model_version   TEXT,"
    "  filters_json    TEXT,"
    "  top_k           INTEGER,"
    "  results_json    TEXT,"
    "  latency_ms      INTEGER,"
    "  status          TEXT,"
    "  is_shadow       INTEGER DEFAULT 0"
    ");";

/* Verifica FTS5 disponibile (una volta) e crea fts_chunks. */
static void rag_ensure_fts(sqlite3* db)
{
    if (g_fts_available == 0) return;
    if (g_fts_available < 0)
    {
        char* err = NULL;
        sqlite3_exec(db, "CREATE VIRTUAL TABLE IF NOT EXISTS __fts_probe USING fts5(x)", NULL, NULL, &err);
        if (err) { sqlite3_free(err); g_fts_available = 0; }
        else { sqlite3_exec(db, "DROP TABLE IF EXISTS __fts_probe", NULL, NULL, NULL); g_fts_available = 1; }
    }
    if (g_fts_available == 1)
    {
        sqlite3_exec(db,
            "CREATE VIRTUAL TABLE IF NOT EXISTS fts_chunks USING fts5("
            "  text, chunk_id UNINDEXED, collection_id UNINDEXED, tokenize='unicode61');",
            NULL, NULL, NULL);
    }
}

/* Migrazione embedding legacy: se vec_embeddings ha PK non-composita la ricrea (parità
 * SqlVectorSchema.MigrateEmbeddingsCompositeKey). Idempotente. */
static void rag_migrate(sqlite3* db)
{
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT sql FROM sqlite_master WHERE type='table' AND name='vec_embeddings'", -1, &st, NULL) != SQLITE_OK)
        return;
    int has_composite = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
    {
        const char* ddl = (const char*)sqlite3_column_text(st, 0);
        if (ddl && strstr(ddl, "PRIMARY KEY (chunk_id, model_version)") != NULL) has_composite = 1;
    }
    sqlite3_finalize(st);
    if (has_composite) return;
    sqlite3_exec(db,
        "CREATE TABLE vec_embeddings_migrated ("
        "  chunk_id INTEGER NOT NULL REFERENCES vec_chunks(id) ON DELETE CASCADE,"
        "  model_version TEXT NOT NULL,"
        "  embedding_created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "  dimensions INTEGER NOT NULL,"
        "  embedding BLOB NOT NULL,"
        "  PRIMARY KEY (chunk_id, model_version));"
        "INSERT INTO vec_embeddings_migrated (chunk_id, model_version, embedding_created_at, dimensions, embedding)"
        "  SELECT chunk_id, model_version, embedding_created_at, dimensions, embedding FROM vec_embeddings;"
        "DROP TABLE vec_embeddings;"
        "ALTER TABLE vec_embeddings_migrated RENAME TO vec_embeddings;",
        NULL, NULL, NULL);
    /* Migrazione formato storage SQ8: colonna format su DB creati prima (default fp32,
     * idempotente: ALTER fallisce in silenzio se la colonna esiste già). */
    {
        sqlite3_stmt* ci = NULL;
        int has_format = 0;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM pragma_table_info('vec_collections') WHERE name='format'", -1, &ci, NULL) == SQLITE_OK)
        {
            if (sqlite3_step(ci) == SQLITE_ROW && sqlite3_column_int(ci, 0) > 0) has_format = 1;
            sqlite3_finalize(ci);
        }
        if (!has_format)
            sqlite3_exec(db, "ALTER TABLE vec_collections ADD COLUMN format TEXT NOT NULL DEFAULT 'fp32'", NULL, NULL, NULL);
    }
}

/* ========================================================================
 * Open / close
 * ======================================================================== */

int64_t gmb_rag_open(const char* sqlite_path)
{
    int i;
    sqlite3* db = NULL;
    for (i = 1; i < GMB_RAG_MAX; i++)
        if (g_rdb[i].used && sqlite_path && strcmp(g_rdb[i].path, sqlite_path) == 0)
            return i;
    for (i = 1; i < GMB_RAG_MAX; i++)
        if (!g_rdb[i].used) break;
    if (i >= GMB_RAG_MAX) return 0;
    if (sqlite3_open(sqlite_path && sqlite_path[0] ? sqlite_path : ":memory:", &db) != SQLITE_OK)
    {
        if (db) sqlite3_close(db);
        return 0;
    }
    sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (sqlite3_exec(db, kRagSchema, NULL, NULL, NULL) != SQLITE_OK) { sqlite3_close(db); return 0; }
    rag_migrate(db);
    rag_ensure_fts(db);
    g_rdb[i].used = 1;
    g_rdb[i].external = 0;
    g_rdb[i].db = db;
    strncpy(g_rdb[i].path, sqlite_path ? sqlite_path : ":memory:", 1023);
    return i;
}

/* Registra un handle sqlite ESISTENTE (area SQL OPEN condivisa, parità con AiVectorRuntime
 * C# che risolve l'alias nel SqlDriver). Lo schema RAG viene assicurato sull'area; l'handle
 * NON è posseduto (external): gmb_rag_close non lo chiude. Ritorna l'handle registro. */
static int64_t rag_attach_external(sqlite3* db, const char* tag)
{
    int i;
    if (!db) return 0;
    for (i = 1; i < GMB_RAG_MAX; i++)
        if (g_rdb[i].used && g_rdb[i].external && g_rdb[i].db == db) return i;
    for (i = 1; i < GMB_RAG_MAX; i++)
        if (!g_rdb[i].used) break;
    if (i >= GMB_RAG_MAX) return 0;
    sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (sqlite3_exec(db, kRagSchema, NULL, NULL, NULL) != SQLITE_OK) return 0;
    rag_migrate(db);
    rag_ensure_fts(db);
    g_rdb[i].used = 1;
    g_rdb[i].external = 1;
    g_rdb[i].db = db;
    snprintf(g_rdb[i].path, sizeof(g_rdb[i].path), "@ext:%s", tag ? tag : "");
    return i;
}

int gmb_rag_close(int64_t h)
{
    if (h < 1 || h >= GMB_RAG_MAX || !g_rdb[h].used) return -1;
    if (!g_rdb[h].external) sqlite3_close(g_rdb[h].db);
    memset(&g_rdb[h], 0, sizeof(g_rdb[h]));
    return 0;
}

int64_t rag_handle(const char* db)   /* db = alias SQL OPEN, path file o ":memory:" */
{
    /* priorità all'alias aperto (parità AiVectorRuntime: prima FindAreaByAlias poi path) */
    if (db && db[0] && strcmp(db, ":memory:") != 0)
    {
        sqlite3* ext = gmb_sql_handle(db);
        if (ext) return rag_attach_external(ext, db);
    }
    char path[1024];
    if (!db || !db[0] || strcmp(db, ":memory:") == 0) snprintf(path, sizeof(path), "%s", ":memory:");
    else snprintf(path, sizeof(path), "%s", db);
    return gmb_rag_open(path);
}

/* ========================================================================
 * Helpers SQL
 * ======================================================================== */

int64_t rag_coll_id(sqlite3* db, const char* name)
{
    sqlite3_stmt* st = NULL;
    int64_t id = 0;
    if (sqlite3_prepare_v2(db, "SELECT id FROM vec_collections WHERE name=$1", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return id;
}

int rag_coll_dims(sqlite3* db, int64_t cid)
{
    sqlite3_stmt* st = NULL;
    int dims = 8;
    if (sqlite3_prepare_v2(db, "SELECT dimensions FROM vec_collections WHERE id=$1", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, cid);
        if (sqlite3_step(st) == SQLITE_ROW) dims = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return dims > 0 ? dims : 8;
}

/* 1 se la collezione usa storage SQ8, altrimenti 0 (fp32: default anche su DB migrati). */
int rag_coll_format(sqlite3* db, int64_t cid)
{
    sqlite3_stmt* st = NULL;
    int sq8 = 0;
    if (sqlite3_prepare_v2(db, "SELECT format FROM vec_collections WHERE id=$1", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, cid);
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* f = (const char*)sqlite3_column_text(st, 0);
            if (f && strcmp(f, "sq8") == 0) sq8 = 1;
        }
        sqlite3_finalize(st);
    }
    return sq8;
}

/* SHA256 hex LOWERCASE del testo (parità SqlVectorIndexer.ComputeHash = ToHexString().ToLower). */
static char* rag_hash_text(const char* text)
{
    size_t len = strlen(text ? text : "");
    unsigned char digest[32];
    gmb_sha256((const uint8_t*)(text ? text : ""), len, digest);
    static const char* hex = "0123456789abcdef";
    char* out = (char*)malloc(65);
    if (!out) return NULL;
    for (int i = 0; i < 32; i++)
    {
        out[i*2] = hex[digest[i] >> 4];
        out[i*2+1] = hex[digest[i] & 0x0f];
    }
    out[64] = '\0';
    return out;
}

/* Chunking paritario a TextChunker.Chunk: paragrafi (split "\n\n") + windowed overlap. */
static char** rag_chunk(const char* text, int* out_n, int max_chars, int overlap)
{
    *out_n = 0;
    if (!text) return NULL;
    if (max_chars <= 0) max_chars = GMB_RAG_CHUNK_DEFAULT;
    if (overlap < 0) overlap = GMB_RAG_CHUNK_OVERLAP;
    size_t len = strlen(text);
    if (len == 0) return NULL;

    char** chunks = NULL;
    int n = 0, cap = 0;

    /* copia normalizzando \r\n → \n (C# legge i file e lo split usa "\n\n") */
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    size_t ci = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (text[i] == '\r' && i + 1 < len && text[i + 1] == '\n') continue;
        copy[ci++] = text[i];
    }
    copy[ci] = '\0';

    char* pars[4096];
    int np = 0;
    char* p = copy;
    while (*p && np < 4096)
    {
        /* trova fine paragrafo: "\n\n" o fine stringa */
        char* end = strstr(p, "\n\n");
        size_t plen = end ? (size_t)(end - p) : strlen(p);
        /* trim attorno (parità TrimEntries) */
        size_t b = 0;
        while (b < plen && (p[b] == ' ' || p[b] == '\t')) b++;
        size_t e = plen;
        while (e > b && (p[e-1] == ' ' || p[e-1] == '\t')) e--;
        if (e > b)
        {
            char* par = (char*)malloc(e - b + 1);
            if (par) { memcpy(par, p + b, e - b); par[e - b] = '\0'; pars[np++] = par; }
        }
        if (!end) break;
        p = end + 2;
        while (*p == '\n') p++;   /* salta righe vuote consecutive */
    }
    free(copy);

    /* il C#: se dopo lo split non c'è nessun paragrafo → [text]; qui testo non vuoto e senza
     * "\n\n" produce già 1 paragrafo. Se np==0 (solo whitespace) ricadiamo su [text]. */
    if (np == 0)
    {
        char* par = (char*)malloc(len + 1);
        if (par) { memcpy(par, text, len); par[len] = '\0'; chunks = (char**)malloc(sizeof(char*)); if (chunks) { chunks[0] = par; n = 1; } else free(par); }
        *out_n = n;
        return chunks;
    }

    /* Accumulo greedy (parità TextChunker C# fix 2026-09-06): i paragrafi corti si
     * accumulano in un buffer finché aggiungere il successivo supera max_chars — prima ogni
     * paragrafo era un chunk (PDF OCR con \n\n sparsi → chunk-crumbs di rumore che inondavano
     * il ranking). Un paragrafo > max_chars resta windowed con overlap. */
    gmb_strbuf_t buf = gmb_strbuf_new();
    for (int pi = 0; pi < np; pi++)
    {
        size_t plen = strlen(pars[pi]);
        if (plen > (size_t)max_chars)
        {
            /* flush accumulo corrente */
            if (buf.length > 0)
            {
                if (n >= cap) { cap = cap ? cap * 2 : 16; char** nc = (char**)realloc(chunks, (size_t)cap * sizeof(char*)); if (!nc) break; chunks = nc; }
                chunks[n++] = strndup(buf.data, buf.length);
                buf.length = 0;
            }
            int step = (max_chars - overlap) > 0 ? (max_chars - overlap) : 1;
            for (size_t s = 0; s < plen; s += (size_t)step)
            {
                size_t l = plen - s;
                if (l > (size_t)max_chars) l = (size_t)max_chars;
                char* c = (char*)malloc(l + 1);
                if (!c) break;
                memcpy(c, pars[pi] + s, l);
                c[l] = '\0';
                if (n >= cap) { cap = cap ? cap * 2 : 16; char** nc = (char**)realloc(chunks, (size_t)cap * sizeof(char*)); if (!nc) { free(c); break; } chunks = nc; }
                chunks[n++] = c;
                if (s + l >= plen) break;
            }
            free(pars[pi]);
            continue;
        }
        size_t sep = buf.length > 0 ? 2 : 0;   /* "\n\n" */
        if (buf.length + sep + plen > (size_t)max_chars)
        {
            if (n >= cap) { cap = cap ? cap * 2 : 16; char** nc = (char**)realloc(chunks, (size_t)cap * sizeof(char*)); if (!nc) break; chunks = nc; }
            chunks[n++] = strndup(buf.data, buf.length);
            buf.length = 0;
            sep = 0;
        }
        if (buf.length > 0) gmb_strbuf_append(&buf, "\n\n", 2);
        gmb_strbuf_append(&buf, pars[pi], plen);
        free(pars[pi]);
    }
    if (buf.length > 0)
    {
        if (n >= cap) { cap = cap ? cap * 2 : 16; char** nc = (char**)realloc(chunks, (size_t)cap * sizeof(char*)); if (!nc) { gmb_strbuf_free(&buf); goto chunk_done; } chunks = nc; }
        chunks[n++] = strndup(buf.data, buf.length);
    }
chunk_done:
    gmb_strbuf_free(&buf);
    *out_n = n;
    return chunks;
}

static void rag_chunks_free(char** c, int n)
{
    for (int i = 0; i < n; i++) free(c[i]);
    free(c);
}

/* FTS5 index/delete (parità SqlVectorFts). */
static void rag_fts_index(sqlite3* db, int64_t chunk_id, int64_t coll_id, const char* text)
{
    if (g_fts_available != 1) return;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM fts_chunks WHERE rowid=$1", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, chunk_id);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    if (sqlite3_prepare_v2(db, "INSERT INTO fts_chunks(rowid,text,chunk_id,collection_id) VALUES($1,$2,$3,$4)", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, chunk_id);
        sqlite3_bind_text(st, 2, text, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 3, chunk_id);
        sqlite3_bind_int64(st, 4, coll_id);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

static void rag_fts_delete_source(sqlite3* db, int64_t coll_id, const char* source_id)
{
    if (g_fts_available != 1) return;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db,
            "DELETE FROM fts_chunks WHERE chunk_id IN ("
            " SELECT c.id FROM vec_chunks c JOIN vec_documents d ON d.id=c.document_id"
            " WHERE d.collection_id=$1 AND d.source_id=$2)", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, coll_id);
        sqlite3_bind_text(st, 2, source_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* ========================================================================
 * API pubblica — ensure collection / index text (con metadata JSON) / delete
 * ======================================================================== */

/* Parità C# AiVectorRuntime.WithCollection (P0a, decisione utente 2026-09-12) + todo
 * Parita-5-Backend-Residui §2 (2026-09-23): senza embedding provider l'operazione
 * vettoriale FALLISCE (il wrapper BASIC C# catching → -1). Prima ensure_collection
 * creava la riga SQLite anche senza provider (cid=1) mentre IL dava -1, e count dava
 * 0 mentre IL dava -1 (AiVectorFunctions, VectorKnowledgeBase). Condizione = stessa
 * di rag_embed: modello caricato (gmb_ai_embed_dim > 0) o fixture opt-in esplicito. */
int gmb_rag_embedding_available(void)
{
    if (gmb_ai_embed_dim() > 0) return 1;
    return gmb_fixture_embed_wanted();
}

/* Variante con formato storage ('fp32'|'sq8'; NULL = conserva l'esistente, mai reset
 * da chiamate senza intento). Su collezione esistente aggiorna il formato per le NUOVE
 * righe (le vecchie restano nel loro layout, rilevato per lunghezza: fp32 = dims*4 byte,
 * sq8 = 4+dims). */
int64_t gmb_rag_ensure_collection_fmt(const char* db, const char* name, int dims, const char* metric, const char* model_version, const char* format)
{
    if (!name || !name[0]) return 0;
    if (!gmb_rag_embedding_available()) return -1;
    sqlite3* s = rag_db(rag_handle(db));
    if (!s) return 0;
    int64_t cid = rag_coll_id(s, name);
    const char* fmt = (format && strcmp(format, "sq8") == 0) ? "sq8"
                    : (format && strcmp(format, "fp32") == 0) ? "fp32" : NULL;
    if (cid)
    {
        if (fmt)
        {
            sqlite3_stmt* up = NULL;
            if (sqlite3_prepare_v2(s, "UPDATE vec_collections SET format=$1 WHERE id=$2", -1, &up, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(up, 1, fmt, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(up, 2, cid);
                sqlite3_step(up);
                sqlite3_finalize(up);
            }
        }
        return cid;
    }
    if (dims <= 0) dims = 8;
    if (dims > GMB_RAG_MAXDIM) dims = GMB_RAG_MAXDIM;
    const char* met = (metric && metric[0]) ? metric : "cosine";
    const char* mv = (model_version && model_version[0]) ? model_version : "fixture-v1";
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(s, "INSERT INTO vec_collections(name,dimensions,metric,model_version,format) VALUES($1,$2,$3,$4,$5)", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, dims);
    sqlite3_bind_text(st, 3, met, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, mv, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, fmt ? fmt : "fp32", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return 0; }
    sqlite3_finalize(st);
    return rag_coll_id(s, name);
}

int64_t gmb_rag_ensure_collection(const char* db, const char* name, int dims, const char* metric, const char* model_version)
{
    return gmb_rag_ensure_collection_fmt(db, name, dims, metric, model_version, NULL);
}

/* Fixture embedding opt-in (decisione utente 2026-09-18): replica ESATTA del C#
 * FixtureEmbeddingProvider (FNV-1a su code-unit UTF-16 + sin + normalize L2) per parità
 * dev/test senza modello. MAI default: richiede GMB_FIXTURE_EMBED=1 E provider embedding
 * attivo di kind "fixture" (SETACTIVE esplicito = intento). Senza: P0e invariato. */
int gmb_fixture_embed_wanted(void) {
    /* Parità C# (IlRuntime P0, decisione 2026-09-12): il fixture embedding è legittimo SOLO
     * se scelto ESPLICITAMENTE (`AI SET ACTIVE %EMBEDDING, fixture-embedding` →
     * gmb_provider_embedding_explicit) o via opt-in di test (GMB_FIXTURE_EMBED=1, analogo di
     * GMBASIC_TEST_FIXTURE_EMBEDDING). Il fixture DEFAULT non basta: senza, l'operazione
     * vettoriale fallisce con errore esplicito (P0a/P0e). Prima si pretendeva sempre l'env →
     * FullRagPipeline (SETACTIVE esplicito) divergeva dal C#.
     * (2026-09-23, Parita-5-Backend-Residui §2 — ProviderConfig): si usa il PINNATO
     * (snapshot di SET ACTIVE), NON l'attivo corrente — C# EmbeddingProviderResolver.Default
     * non cambia su ROTATE. */
    const char* prov = gmb_provider_embedding_snapshot();
    if (!prov || !*prov) return 0;
    const char* k = gmb_provider_kind(prov);
    if (!(k && strcmp(k, "fixture") == 0)) return 0;
    if (gmb_provider_embedding_explicit()) return 1;
    const char* f = gmb_opt_env("GMB_FIXTURE_EMBED");
    return f && strcmp(f, "1") == 0;
}
int gmb_fixture_embed(const char* text, int dims, float* out) {
    if (!text || !out || dims <= 0 || dims > 4096) return -1;
    /* FNV-1a a 32 bit sulle CODE-UNIT UTF-16 (parità C#: foreach(char) su stringa .NET).
     * Decodifica UTF-8 → UTF-16 con surrogate per gli astrali; byte malformati → U+FFFD. */
    uint32_t hash = 2166136261u;
    const unsigned char* p = (const unsigned char*)text;
    while (*p) {
        uint32_t cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((uint32_t)(*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { cp = ((uint32_t)(*p & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { cp = ((uint32_t)(*p & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
        else { cp = 0xFFFD; p++; }
        if (cp < 0x10000) { hash ^= cp; hash *= 16777619u; }
        else { cp -= 0x10000; hash ^= (0xD800 + (cp >> 10)); hash *= 16777619u; hash ^= (0xDC00 + (cp & 0x3FF)); hash *= 16777619u; }
    }
    /* C#: seed=(int)hash; v[i]=(float)Sin(seed*(i+1)*0.131) — l'int-overflow wrappa
     * (unchecked): in C lo si rende esplicito in uint32 (stessi bit). */
    uint32_t useed = hash;
    double norm = 0.0;
    for (int i = 0; i < dims; i++) {
        int32_t s = (int32_t)(useed * (uint32_t)(i + 1));
        double v = sin((double)s * 0.131);
        out[i] = (float)v;
        /* parità C#: norm += vector[i]*vector[i] in FLOAT (poi widening a double). */
        float sq = out[i] * out[i];
        norm += (double)sq;
    }
    if (norm == 0.0) return 0;   /* parità C#: vettore nullo resta tale */
    double inv = 1.0 / sqrt(norm);
    for (int i = 0; i < dims; i++) out[i] = (float)((double)out[i] * inv);
    return 0;
}

/* P0e (todo Tokenizer-WordPiece, decisione utente 2026-09-12): embedding REALE o errore
 * esplicito — MAI degradazione al fixture FNV/sin (AGENTS §0bis.5). Codici: 0 ok,
 * -1 nessun modello caricato, -2 dims != dim del modello (prima: indice MISTO metà
 * vettori reali e metà hash nello stesso file, in silenzio). */
int rag_embed(const char* text, int dims, float* out)
{
    if (gmb_ai_embed_dim() <= 0) {
        /* Opt-in esplicito (decisione utente 2026-09-18, NON fallback silenzioso): con
         * GMB_FIXTURE_EMBED=1 e provider embedding attivo di kind "fixture", vettori
         * deterministici identici al C# FixtureEmbeddingProvider (parità sweep AI).
         * Senza flag, o senza SETACTIVE fixture: -1 come prima (P0e intatto). */
        if (gmb_fixture_embed_wanted() && gmb_fixture_embed(text ? text : "", dims, out) == 0)
            return 0;
        return -1;
    }
    if (gmb_ai_embed_text(text, out, dims) == 0)
        return 0;   /* success */
    /* fallisce se dims != dim del modello (o backend interno guasto): -2, no fallback */
    return -2;
}

const char* rag_embed_err(int rc)
{
    if (rc == -1) return "Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)";
    if (rc == -2) return "dimensione embedding richiesta != dimensione del modello attivo (ricrea la collection)";
    return "errore embedding sconosciuto";
}

/* Indica se l'embedding attivo è il fixture (nessun modello reale caricato). */
int gmb_rag_embedding_is_fixture(void) { return gmb_ai_embed_dim() <= 0 ? 1 : 0; }

/* Parsea metadata JSON (status/tenant/role). status default "current". */
typedef struct {
    char status[32];
    char tenant[256];
    char roles[512];
} rag_meta_t;

static void rag_parse_meta(const char* json, rag_meta_t* m)
{
    memset(m, 0, sizeof(*m));
    strcpy(m->status, "current");
    if (!json || !json[0]) return;
    /* mini-parse: cerca "status":"x", "tenant":"x", "role":"x" (JSONC semplice) */
    const char* p = json;
    while ((p = strstr(p, "\"status\"")) != NULL)
    {
        p = strchr(p, ':'); if (!p) break; p++;
        while (*p == ' ' || *p == '"') p++;
        char buf[128]; int i = 0;
        while (*p && *p != '"' && i < 127) buf[i++] = *p++;
        buf[i] = 0;
        if (i > 0) { strncpy(m->status, buf, sizeof(m->status)-1); m->status[sizeof(m->status)-1]=0; }
        break;
    }
    p = json;
    while ((p = strstr(p, "\"tenant\"")) != NULL)
    {
        p = strchr(p, ':'); if (!p) break; p++;
        while (*p == ' ' || *p == '"') p++;
        char buf[256]; int i = 0;
        while (*p && *p != '"' && i < 255) buf[i++] = *p++;
        buf[i] = 0;
        if (i > 0) { strncpy(m->tenant, buf, sizeof(m->tenant)-1); m->tenant[sizeof(m->tenant)-1]=0; }
        break;
    }
    p = json;
    while ((p = strstr(p, "\"role\"")) != NULL)
    {
        p = strchr(p, ':'); if (!p) break; p++;
        while (*p == ' ' || *p == '"') p++;
        char buf[512]; int i = 0;
        while (*p && *p != '"' && i < 511) buf[i++] = *p++;
        buf[i] = 0;
        if (i > 0) { strncpy(m->roles, buf, sizeof(m->roles)-1); m->roles[sizeof(m->roles)-1]=0; }
        break;
    }
}

/* Index text con source_id e metadata (parità SqlVectorIndexer.IndexText + chunking +
 * FTS5). source_id NON opzionale. Ritorna: 1 indexed, 2 updated, 0 skipped, -1 errore. */
int gmb_rag_index_source(const char* db, const char* coll, const char* source_id, const char* text, const char* metadata_json, int max_chars, int overlap)
{
    if (!coll || !coll[0] || !source_id || !source_id[0]) return -1;
    sqlite3* s = rag_db(rag_handle(db));
    if (!s) return -1;
    int64_t cid = gmb_rag_ensure_collection(db, coll, 0, "cosine", NULL);
    if (cid <= 0) return -1;   /* 0 = errore DB, -1 = senza provider (parità C# throw → -1); mai proseguire con cid invalido */
    int dims = rag_coll_dims(s, cid);
    int use_sq8 = rag_coll_format(s, cid);

    char* hash = rag_hash_text(text ? text : "");
    if (!hash) return -1;
    int outcome = 0;

    sqlite3_exec(s, "BEGIN", NULL, NULL, NULL);
    /* hash esistente → skip */
    {
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(s, "SELECT source_hash FROM vec_documents WHERE collection_id=$1 AND source_id=$2", -1, &st, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(st, 1, cid);
            sqlite3_bind_text(st, 2, source_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW)
            {
                const char* old = (const char*)sqlite3_column_text(st, 0);
                if (old && strcmp(old, hash) == 0) outcome = 0; /* skipped */
                else outcome = 2;                                /* updated */
            }
            else outcome = 1;                                    /* indexed */
            sqlite3_finalize(st);
        }
        else { sqlite3_exec(s, "ROLLBACK", NULL, NULL, NULL); free(hash); return -1; }
    }
    if (outcome == 0) { sqlite3_exec(s, "COMMIT", NULL, NULL, NULL); free(hash); return 0; }

    /* delete esistente (doc + chunks + embeddings + fts) */
    rag_fts_delete_source(s, cid, source_id);
    sqlite3_stmt* del = NULL;
    if (sqlite3_prepare_v2(s, "DELETE FROM vec_documents WHERE collection_id=$1 AND source_id=$2", -1, &del, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(del, 1, cid);
        sqlite3_bind_text(del, 2, source_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }

    rag_meta_t meta;
    rag_parse_meta(metadata_json, &meta);

    sqlite3_stmt* insd = NULL;
    if (sqlite3_prepare_v2(s,
            "INSERT INTO vec_documents(collection_id,source_id,source_hash,status,tenant_id,access_roles,metadata_json)"
            " VALUES($1,$2,$3,$4,$5,$6,$7)", -1, &insd, NULL) != SQLITE_OK)
    { sqlite3_exec(s, "ROLLBACK", NULL, NULL, NULL); free(hash); return -1; }
    sqlite3_bind_int64(insd, 1, cid);
    sqlite3_bind_text(insd, 2, source_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insd, 3, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insd, 4, meta.status, -1, SQLITE_TRANSIENT);
    if (meta.tenant[0]) sqlite3_bind_text(insd, 5, meta.tenant, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(insd, 5);
    if (meta.roles[0]) sqlite3_bind_text(insd, 6, meta.roles, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(insd, 6);
    if (metadata_json && metadata_json[0]) sqlite3_bind_text(insd, 7, metadata_json, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(insd, 7);
    if (sqlite3_step(insd) != SQLITE_DONE) { sqlite3_finalize(insd); sqlite3_exec(s, "ROLLBACK", NULL, NULL, NULL); free(hash); return -1; }
    sqlite3_finalize(insd);
    int64_t doc_id = sqlite3_last_insert_rowid(s);

    /* chunking + embedding + fts per chunk */
    int nchunks = 0;
    char** chunks = rag_chunk(text ? text : "", &nchunks, max_chars, overlap);
    if (!chunks || nchunks == 0)
    {
        /* un chunk singolo col testo intero (parità C#: if chunks.Count==0 → [text]) */
        chunks = (char**)malloc(sizeof(char*));
        if (chunks) { chunks[0] = strdup(text ? text : ""); nchunks = 1; }
    }
    if (!chunks) { sqlite3_exec(s, "ROLLBACK", NULL, NULL, NULL); free(hash); return -1; }

    for (int i = 0; i < nchunks; i++)
    {
        sqlite3_stmt* insc = NULL;
        if (sqlite3_prepare_v2(s, "INSERT INTO vec_chunks(document_id,chunk_index,text,token_count) VALUES($1,$2,$3,$4)", -1, &insc, NULL) != SQLITE_OK) break;
        sqlite3_bind_int64(insc, 1, doc_id);
        sqlite3_bind_int(insc, 2, i);
        sqlite3_bind_text(insc, 3, chunks[i], -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insc, 4, (int)strlen(chunks[i]));
        if (sqlite3_step(insc) != SQLITE_DONE) { sqlite3_finalize(insc); break; }
        sqlite3_finalize(insc);
        int64_t chunk_id = sqlite3_last_insert_rowid(s);
        float* vec = (float*)malloc(sizeof(float) * (size_t)dims);
        if (!vec) break;
        /* P0e: embedding reale o ROLLBACK con errore propagato — mai indice misto. */
        int erc = rag_embed(chunks[i], dims, vec);
        if (erc != 0)
        {
            free(vec);
            rag_chunks_free(chunks, nchunks);
            sqlite3_exec(s, "ROLLBACK", NULL, NULL, NULL);
            free(hash);
            return erc;   /* -1 nessun modello, -2 dims divergenti */
        }
        sqlite3_stmt* inse = NULL;
        if (sqlite3_prepare_v2(s,
                "INSERT INTO vec_embeddings(chunk_id,model_version,dimensions,embedding) VALUES($1,$2,$3,$4)"
                " ON CONFLICT(chunk_id,model_version) DO UPDATE SET dimensions=excluded.dimensions, embedding=excluded.embedding, embedding_created_at=datetime('now')",
                -1, &inse, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(inse, 1, chunk_id);
            sqlite3_bind_text(inse, 2, "fixture-v1", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(inse, 3, dims);
            if (use_sq8)
            {
                /* Storage SQ8: [scale:float][dims int8] — scoring resta nel dominio int8. */
                unsigned char* qb = (unsigned char*)malloc((size_t)4 + (size_t)dims);
                if (qb)
                {
                    float sc = 0.0f;
                    gmb_sq8_encode(vec, dims, (int8_t*)(qb + 4), &sc);
                    memcpy(qb, &sc, 4);
                    sqlite3_bind_blob(inse, 4, qb, 4 + dims, SQLITE_TRANSIENT);
                    free(qb);
                }
                else
                    sqlite3_bind_blob(inse, 4, vec, (int)(sizeof(float) * (size_t)dims), SQLITE_TRANSIENT);
            }
            else
                sqlite3_bind_blob(inse, 4, vec, (int)(sizeof(float) * (size_t)dims), SQLITE_TRANSIENT);
            sqlite3_step(inse);
            sqlite3_finalize(inse);
        }
        free(vec);
        rag_fts_index(s, chunk_id, cid, chunks[i]);
    }
    rag_chunks_free(chunks, nchunks);
    sqlite3_exec(s, "COMMIT", NULL, NULL, NULL);
    free(hash);
    return outcome;
}

/* Delete per source_id. Ritorna 1 se esisteva, 0 altrimenti. */
int gmb_rag_delete_source(const char* db, const char* coll, const char* source_id)
{
    if (!coll || !coll[0] || !source_id) return 0;
    sqlite3* s = rag_db(rag_handle(db));
    if (!s) return 0;
    int64_t cid = rag_coll_id(s, coll);
    if (!cid) return 0;
    rag_fts_delete_source(s, cid, source_id);
    sqlite3_stmt* del = NULL;
    int n = 0;
    if (sqlite3_prepare_v2(s, "DELETE FROM vec_documents WHERE collection_id=$1 AND source_id=$2", -1, &del, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(del, 1, cid);
        sqlite3_bind_text(del, 2, source_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(del) == SQLITE_DONE) n = sqlite3_changes(s) > 0 ? 1 : 0;
        sqlite3_finalize(del);
    }
    return n;
}

int gmb_rag_count(const char* db, const char* coll)
{
    /* Senza provider embedding attivo → 0 (non -1): parità IlRuntime.AiVectorCountEmbeddings
     * (catch -> 0) e PCode VECTORDB_COUNT (catch -> 0). VectorDbAdmin: il COUNT senza provider
     * dà 0 su tutte le gambe (prima il C dava -1). */
    if (!gmb_rag_embedding_available()) return 0;
    if (!coll || !coll[0]) return -1;
    sqlite3* s = rag_db(rag_handle(db));
    if (!s) return -1;
    int64_t cid = rag_coll_id(s, coll);
    if (!cid) return -1;
    sqlite3_stmt* st = NULL;
    int n = 0;
    if (sqlite3_prepare_v2(s, "SELECT COUNT(*) FROM vec_embeddings e JOIN vec_chunks c ON c.id=e.chunk_id JOIN vec_documents d ON d.id=c.document_id WHERE d.collection_id=$1", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, cid);
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return n;
}

/* PURGE DEPRECATED (parità SqlVectorIndexer.PurgeDeprecated): elimina i documenti con
 * status='deprecated' (e chunks/embeddings/fts via ON DELETE CASCADE) della collection.
 * Ritorna il numero di documenti eliminati, -1 su errore. */
int gmb_rag_purge_deprecated(const char* db, const char* coll)
{
    if (!coll || !coll[0]) return -1;
    sqlite3* s = rag_db(rag_handle(db));
    if (!s) return -1;
    int64_t cid = rag_coll_id(s, coll);
    if (!cid) return 0;   /* collection inesistente → niente da eliminare (parità C#) */
    /* FTS: elimina i chunk dei documenti deprecated (la DELETE su vec_documents non tocca fts) */
    sqlite3_stmt* ftsd = NULL;
    if (sqlite3_prepare_v2(s,
            "DELETE FROM fts_chunks WHERE chunk_id IN ("
            " SELECT c.id FROM vec_chunks c JOIN vec_documents d ON d.id=c.document_id"
            " WHERE d.collection_id=$1 AND d.status='deprecated')", -1, &ftsd, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(ftsd, 1, cid);
        sqlite3_step(ftsd);
        sqlite3_finalize(ftsd);
    }
    sqlite3_stmt* st = NULL;
    int n = -1;
    if (sqlite3_prepare_v2(s, "DELETE FROM vec_documents WHERE collection_id=$1 AND status='deprecated'", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, cid);
        if (sqlite3_step(st) == SQLITE_DONE) n = sqlite3_changes(s);
        sqlite3_finalize(st);
    }
    return n;
}

/* Lista collezioni JSON array (parità ListCollections). */
char* gmb_rag_list_collections(const char* db)
{
    /* Parità AiVectorRuntime.ListCollections C# (common/AI/Rag/AiVectorRuntime.cs):
     * ":memory:" apre una connessione SQLite NUOVA (vuota) e ritorna i NOMI separati da
     * newline (string.Join("\n", names)), NON un array JSON. Una :memory: fresca è sempre
     * vuota → stringa vuota. (Golden AivectorLoadDocs/VectorDbBasic: "Collezioni:" + riga
     * vuota; il runtime C ritornava sempre "[]" → divergenza vmc/c99n/c99 vs IL.) */
    if (db && strcmp(db, ":memory:") == 0)
        return strdup("");

    sqlite3* s = rag_db(rag_handle(db));
    sqlite3_stmt* st = NULL;
    char* out = (char*)malloc(16384);
    if (!out) return NULL;
    strcpy(out, "[");
    if (!s) { free(out); char* e = (char*)malloc(160); snprintf(e, 160, "ERROR: SQL database '%s' is not open.", db ? db : ""); return e; }
    int first = 1;
    /* parità AiVectorRuntime.ListCollections C# (su area/file): JSON array di oggetti
     * {name, dimensions, metric, modelVersion} */
    if (sqlite3_prepare_v2(s, "SELECT name, dimensions, metric, model_version FROM vec_collections ORDER BY name", -1, &st, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* n = (const char*)sqlite3_column_text(st, 0);
            int dims = sqlite3_column_int(st, 1);
            const char* met = (const char*)sqlite3_column_text(st, 2);
            const char* mv = (const char*)sqlite3_column_text(st, 3);
            if (!n) continue;
            if (!first) strcat(out, ",");
            first = 0;
            char piece[1400];
            char escn[512]; size_t j = 0;
            for (const char* q = n; *q && j < 500; q++)
                if (*q == '"' || *q == '\\') { escn[j++] = '\\'; escn[j++] = *q; }
                else escn[j++] = *q;
            escn[j] = '\0';
            snprintf(piece, sizeof(piece),
                "{\"name\":\"%s\",\"dimensions\":%d,\"metric\":\"%s\",\"modelVersion\":\"%s\"}",
                escn, dims, met ? met : "cosine", mv ? mv : "fixture-v1");
            strncat(out, piece, 16000 - strlen(out));
        }
        sqlite3_finalize(st);
    }
    strcat(out, "]");
    return out;
}

/* Sorgenti indicizzate in una collezione (JSON array di stringhe). Usata da `rag build
 * --prune` nel CLI C: la build confronta il set su disco con quello nel DB. */
char* gmb_rag_list_sources(const char* db, const char* coll)
{
    if (!coll || !coll[0]) return NULL;
    sqlite3* s = rag_db(rag_handle(db));
    if (!s) return NULL;
    int64_t cid = rag_coll_id(s, coll);
    if (cid <= 0) return strdup("[]");
    sqlite3_stmt* st = NULL;
    char* out = (char*)malloc(1 << 16);
    if (!out) return NULL;
    strcpy(out, "[");
    int first = 1;
    if (sqlite3_prepare_v2(s, "SELECT source_id FROM vec_documents WHERE collection_id=$1 ORDER BY id", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, cid);
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* n = (const char*)sqlite3_column_text(st, 0);
            if (!n) continue;
            if (!first) strcat(out, ",");
            first = 0;
            char esc[1024]; size_t j = 0;
            esc[j++] = '"';
            for (const char* q = n; *q && j < sizeof(esc) - 3; q++)
            {
                if (*q == '"' || *q == '\\') esc[j++] = '\\';
                esc[j++] = *q;
            }
            esc[j++] = '"'; esc[j] = '\0';
            if (strlen(out) + j + 4 < (1 << 16)) strcat(out, esc);
        }
        sqlite3_finalize(st);
    }
    strcat(out, "]");
    return out;
}

/* Candidati (chunk con embedding) di una collezione → per search. Ritorna numero righe. */
typedef struct {
    int64_t chunk_id, doc_id;
    float score;
    char source_id[512];
    char text[1024];
    char model_version[128];
    char metadata_json[1024];
    char status[32];
    char tenant[256];
    char roles[512];
} rag_hit_t;

/* Embedding di query: legacy — see rag_embed (P0e: i NUOVI call-site chiamano rag_embed e
 * propagano l'errore; questo wrapper ignora il codice ed è tenuto SOLO per i consumatori
 * storici non-migrati. NON usarlo in codice nuovo). */
void rag_embed_query(const char* q, int dims, float* out) { (void)rag_embed(q, dims, out); }

float rag_cos(const float* a, const float* b, int n)
{
    /* Parità ESATTA C# VectorMath.CosineSimilarity: prodotti in FLOAT (widening dopo),
     * divisione per le norme SEMPRE (anche con vettori già normalizzati: sposta 1 ulp). */
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < n; i++) {
        dot += (double)(a[i] * b[i]);
        na += (double)(a[i] * a[i]);
        nb += (double)(b[i] * b[i]);
    }
    if (na == 0.0 || nb == 0.0) return 0.0f;
    return (float)(dot / (sqrt(na) * sqrt(nb)));
}
