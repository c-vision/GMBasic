/* FILE: native/gmb_runtime_c/src/gmb_vectordb.c
 * VectorDB community — P0c/P0e (todo Tokenizer-WordPiece, decisione utente 2026-09-12):
 * le superfici legacy gmb_vecdb_* e gmb_aiv_* SONO FACADE sulle gmb_rag_* (schema esteso
 * vec_documents + vec_chunks + vec_embeddings per chunk + fts_chunks, §0bis UNA sola
 * implementazione). Include GenTest (R3) — generazione testset sintetici. */
#include "gmb_community_db.h"
#include "gmb_runtime.h"
#include "gmb_rag_internal.h"   /* rag_embed (fonte unica §0bis — P0e: reale o errore) */
#include "gmb_commonlib.h"      /* gmb_sha256 (R3 GenTest) */
#include "gmb_sql_iface.h"
#include <sqlite3.h>
#include <sys/stat.h>           /* mkdir (R3 GenTest) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <dirent.h>
#include <strings.h>
#include <time.h>

#define GMB_VEC_MAX 8
#define GMB_VEC_MAXDIM 256

typedef struct {
    int used;
    char path[1024];
    sqlite3* db;
} gmb_vec_db;

static gmb_vec_db g_vdb[GMB_VEC_MAX];

static const char* kSchema =
    "CREATE TABLE IF NOT EXISTS vec_collections ("
    "  id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE,"
    "  dimensions INTEGER NOT NULL,"
    "  metric TEXT NOT NULL CHECK (metric IN ('cosine','l2')),"
    "  model_version TEXT NOT NULL DEFAULT 'fixture-v1'"
    ");"
    "CREATE TABLE IF NOT EXISTS vec_documents ("
    "  id INTEGER PRIMARY KEY,"
    "  collection_id INTEGER NOT NULL REFERENCES vec_collections(id) ON DELETE CASCADE,"
    "  source_id TEXT NOT NULL, text TEXT NOT NULL,"
    "  UNIQUE(collection_id, source_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS vec_embeddings ("
    "  document_id INTEGER PRIMARY KEY REFERENCES vec_documents(id) ON DELETE CASCADE,"
    "  dimensions INTEGER NOT NULL, embedding BLOB NOT NULL"
    ");";

/* P0e (decisione utente 2026-09-12): fixture_embed ELIMINATO — era un hash FNV-1a + sin
 * normalizzato (pseudo-semantica stabile ma priva di significato: gatto/micio distano come
 * gatto/gravitational). Ogni superficie che indicizza/cerca ora passa da rag_embed
 * (gmb_rag.c, fonte unica §0bis) e fallisce con errore esplicito senza modello.
 * Include "gmb_rag_internal.h" per il prototipo. */

static float cosine(const float* a, const float* b, int n)
{
    double d = 0.0;
    int i;
    for (i = 0; i < n; i++) d += (double)a[i] * (double)b[i];
    return (float)d;
}

static sqlite3* vec_db(int64_t h)
{
    if (h < 1 || h >= GMB_VEC_MAX || !g_vdb[h].used) return NULL;
    return g_vdb[h].db;
}

int64_t gmb_vecdb_open(const char* sqlite_path)
{
    int i;
    sqlite3* db = NULL;
    for (i = 1; i < GMB_VEC_MAX; i++) {
        if (g_vdb[i].used && sqlite_path && strcmp(g_vdb[i].path, sqlite_path) == 0)
            return i;
    }
    for (i = 1; i < GMB_VEC_MAX; i++)
        if (!g_vdb[i].used) break;
    if (i >= GMB_VEC_MAX) return 0;
    if (sqlite3_open(sqlite_path && sqlite_path[0] ? sqlite_path : ":memory:", &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (sqlite3_exec(db, kSchema, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }
    g_vdb[i].used = 1;
    g_vdb[i].db = db;
    strncpy(g_vdb[i].path, sqlite_path ? sqlite_path : ":memory:", 1023);
    return i;
}

int gmb_vecdb_close(int64_t h)
{
    if (h < 1 || h >= GMB_VEC_MAX || !g_vdb[h].used) return -1;
    sqlite3_close(g_vdb[h].db);
    memset(&g_vdb[h], 0, sizeof(g_vdb[h]));
    return 0;
}

static int64_t coll_id(sqlite3* db, const char* name)
{
    sqlite3_stmt* st = NULL;
    int64_t id = 0;
    if (sqlite3_prepare_v2(db, "SELECT id FROM vec_collections WHERE name=?1", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return id;
}

int64_t gmb_vecdb_ensure_collection(int64_t h, const char* name, int dims)
{
    sqlite3* db = vec_db(h);
    sqlite3_stmt* st = NULL;
    int64_t id;
    if (!db || !name || !name[0]) return 0;
    if (dims <= 0) dims = 8;
    /* Cap 256 (todo Tokenizer-WordPiece §9): errore esplicito, mai clamp silenzioso. */
    if (dims > GMB_VEC_MAXDIM) return 0;
    id = coll_id(db, name);
    if (id) return id;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO vec_collections(name,dimensions,metric) VALUES(?1,?2,'cosine')",
            -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, dims);
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return 0; }
    sqlite3_finalize(st);
    return coll_id(db, name);
}

int gmb_vecdb_index_text(int64_t h, const char* coll, const char* source_id, const char* text)
{
    sqlite3* db = vec_db(h);
    int64_t cid, did;
    int dims = 8;
    float vec[GMB_VEC_MAXDIM];
    sqlite3_stmt* st = NULL;
    if (!db || !coll) return -1;
    cid = gmb_vecdb_ensure_collection(h, coll, 0);
    if (!cid) return -1;
    if (sqlite3_prepare_v2(db, "SELECT dimensions FROM vec_collections WHERE id=?1", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, cid);
        if (sqlite3_step(st) == SQLITE_ROW) dims = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
        st = NULL;
    }
    if (dims < 1) dims = 8;
    if (dims > GMB_VEC_MAXDIM) return -1;   /* cap 256: errore esplicito, mai clamp silenzioso (§9) */
    /* P0e: embedding reale o errore propagato (-1 no modello, -2 dims) — mai fixture */
    {
        int erc = rag_embed(text ? text : "", dims, vec);
        if (erc != 0) return erc;
    }
    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    if (sqlite3_prepare_v2(db,
            "INSERT INTO vec_documents(collection_id,source_id,text) VALUES(?1,?2,?3) "
            "ON CONFLICT(collection_id,source_id) DO UPDATE SET text=excluded.text",
            -1, &st, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int64(st, 1, cid);
    sqlite3_bind_text(st, 2, source_id ? source_id : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, text ? text : "", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_finalize(st);
    did = sqlite3_last_insert_rowid(db);
    if (sqlite3_prepare_v2(db, "SELECT id FROM vec_documents WHERE collection_id=?1 AND source_id=?2",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, cid);
        sqlite3_bind_text(st, 2, source_id ? source_id : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) did = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    if (sqlite3_prepare_v2(db,
            "INSERT INTO vec_embeddings(document_id,dimensions,embedding) VALUES(?1,?2,?3) "
            "ON CONFLICT(document_id) DO UPDATE SET embedding=excluded.embedding",
            -1, &st, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_bind_int64(st, 1, did);
    sqlite3_bind_int(st, 2, dims);
    sqlite3_bind_blob(st, 3, vec, dims * (int)sizeof(float), SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    return 0;
}

int gmb_vecdb_delete_text(int64_t h, const char* coll, const char* source_id)
{
    sqlite3* db = vec_db(h);
    sqlite3_stmt* st = NULL;
    int64_t cid;
    if (!db || !coll) return -1;
    cid = coll_id(db, coll);
    if (!cid) return 0;
    if (sqlite3_prepare_v2(db,
            "DELETE FROM vec_documents WHERE collection_id=?1 AND source_id=?2",
            -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, cid);
    sqlite3_bind_text(st, 2, source_id ? source_id : "", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return -1; }
    sqlite3_finalize(st);
    return 0;
}

int gmb_vecdb_count(int64_t h, const char* coll)
{
    sqlite3* db = vec_db(h);
    sqlite3_stmt* st = NULL;
    int n = 0;
    int64_t cid;
    if (!db) return 0;
    cid = coll_id(db, coll);
    if (!cid) return 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM vec_documents WHERE collection_id=?1", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_int64(st, 1, cid);
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return n;
}

char* gmb_vecdb_search(int64_t h, const char* coll, const char* query, int topk)
{
    sqlite3* db = vec_db(h);
    sqlite3_stmt* st = NULL;
    int64_t cid;
    int dims = 8, i, nres = 0;
    float qv[GMB_VEC_MAXDIM];
    typedef struct { float score; char text[512]; } hit_t;
    hit_t hits[32];
    char* out;
    size_t cap = 4096, used = 0;
    if (topk < 1) topk = 5;
    if (topk > 32) topk = 32;
    out = (char*)malloc(cap);
    if (!out) return NULL;
    out[0] = '['; out[1] = ']'; out[2] = 0;
    if (!db) return out;
    cid = coll_id(db, coll);
    if (!cid) return out;
    if (sqlite3_prepare_v2(db, "SELECT dimensions FROM vec_collections WHERE id=?1", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, cid);
        if (sqlite3_step(st) == SQLITE_ROW) dims = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    if (dims < 1) dims = 8;
    if (dims > GMB_VEC_MAXDIM)
    {
        free(out);
        char* err = (char*)malloc(96);
        if (err) snprintf(err, 96, "[{\"score\":0,\"text\":\"ERROR: collection dimensions %d > cap %d\"}]", dims, GMB_VEC_MAXDIM);
        return err ? err : NULL;
    }
    /* P0e: embedding reale della query o errore esplicito — la search senza modello non
     * restituisce più hit pseudo-ordinati. */
    {
        int erc = rag_embed(query, dims, qv);
        if (erc != 0)
        {
            /* Nessun errore occulto: senza provider embedding la search SOLLEVA (parità
             * IlRuntime/AiVectorRuntime, che lanciano l'eccezione), non ritorna un JSON-err
             * silenzioso con exit 0. */
            free(out);
            gmb_raise_error(0, rag_embed_err(erc), 0L);
            return NULL;
        }
    }
    if (sqlite3_prepare_v2(db,
            "SELECT d.text, e.embedding, e.dimensions FROM vec_documents d "
            "JOIN vec_embeddings e ON e.document_id=d.id WHERE d.collection_id=?1",
            -1, &st, NULL) != SQLITE_OK)
        return out;
    sqlite3_bind_int64(st, 1, cid);
    memset(hits, 0, sizeof(hits));
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(st, 0);
        const float* blob = (const float*)sqlite3_column_blob(st, 1);
        int bd = sqlite3_column_bytes(st, 1) / (int)sizeof(float);
        float sc;
        int k, ins;
        if (!blob || bd < 1) continue;
        if (bd > dims) bd = dims;
        sc = cosine(qv, blob, bd);
        ins = nres < topk ? nres : topk - 1;
        if (nres < topk) nres++;
        else if (sc <= hits[topk - 1].score) continue;
        for (k = nres - 1; k > 0 && sc > hits[k - 1].score; k--) hits[k] = hits[k - 1];
        hits[k].score = sc;
        strncpy(hits[k].text, text ? text : "", 511);
        (void)ins;
    }
    sqlite3_finalize(st);
    used = 0;
    out[used++] = '[';
    for (i = 0; i < nres; i++) {
        char piece[640];
        int n = snprintf(piece, sizeof(piece), "%s{\"score\":%.4f,\"text\":\"", i ? "," : "", hits[i].score);
        if (used + (size_t)n + strlen(hits[i].text) + 4 >= cap) break;
        memcpy(out + used, piece, (size_t)n); used += (size_t)n;
        memcpy(out + used, hits[i].text, strlen(hits[i].text)); used += strlen(hits[i].text);
        out[used++] = '"'; out[used++] = '}';
    }
    out[used++] = ']';
    out[used] = 0;
    return out;
}

/* ── Backend embed di TEST per il selfcheck (deterministico, NON in produzione) ── */
static int sc_test_ctx = -1;
static int sc_test_load(const char* model, int ngl);
static void sc_test_unload(int h);
static int sc_test_dim(int h);
static int sc_test_text(int h, const char* text, float* out, int cap);

int gmb_vecdb_selfcheck(void)
{
    /* P0e (2026-09-12): il selfcheck ora verifica la semantica REALE.
     * (1) senza modello → le superfici di scrittura/ricerca devono FALLIRE (non scrivere
     *     pseudo-vettori); (2) con un backend embed di TEST esplicito (deterministico,
     *     dichiarato: identità progettata per il test, non fixture di produzione) tutto
     *     l'ordine vec_collections/vec_documents persiste e count/delete funzionano. */
    {
        /* FASE A — nessun modello: il runtime in questo processo non ha embed backend. */
        int64_t h, id;
        char* json;
        h = gmb_vecdb_open(":memory:");
        if (h <= 0) return -1;
        id = gmb_vecdb_ensure_collection(h, "docs", 8);
        if (id <= 0) return -2;
        if (gmb_vecdb_index_text(h, "docs", "s1", "the cat sat on the mat") != -1) return -3;   /* no modello → -1 */
        if (gmb_vecdb_count(h, "docs") != 0) return -5;   /* niente scritture silenziose */
        json = gmb_vecdb_search(h, "docs", "the cat sat on the mat", 2);
        if (!json || strstr(json, "ERROR") == NULL) { free(json); return -6; }   /* errore esplicito, non [] */
        free(json);
        if (gmb_vecdb_ensure_collection(h, "big", 400) != 0) return -41;   /* cap 256: errore esplicito (§9), non clamp */
        gmb_vecdb_close(h);
    }
    {
        /* FASE B — backend embed di TEST (deterministico, dichiara dims=8): la pipeline
         * indicizza/cerca con vettori VERI (qui: funzione test nel selfcheck, parità del
         * fixture esplicito dei test C#). */
        int64_t h, h2, id;
        char* json;
        /* backend deterministico: primo byte = hash del testo, il resto costante — abbastanza
         * perché "x" e "x" coincidano e "x" e "y" differiscano (il minimo per il test). */
        gmb_ai_register_embed_backend(
            (void*)sc_test_load, (void*)sc_test_unload, (void*)sc_test_dim, (void*)sc_test_text);
        /* P0e: rag_embed richiede un backend ATTIVO (g_emb_handle >= 0) — il selfcheck registrava
         * il backend di test senza MAI caricarlo → dim()==0 → rag_embed -1 → -16. Bug pre-esistente
         * del selfcheck dopo P0e, scoperto rieseguendolo per il §9. */
        if (!gmb_ai_embed_load("selfcheck-test", 0)) return -13;
        char vpath[256];
        snprintf(vpath, sizeof(vpath), "/tmp/gmb_vec_sc.sqlite");
        remove(vpath);
        h = gmb_vecdb_open(vpath);
        if (h <= 0) return -14;
        id = gmb_vecdb_ensure_collection(h, "docs", 8);
        if (id <= 0) return -15;
        if (gmb_vecdb_index_text(h, "docs", "s1", "persist me") != 0) return -16;
        gmb_vecdb_close(h);
        h2 = gmb_vecdb_open(vpath);
        if (h2 <= 0) return -17;
        if (gmb_vecdb_count(h2, "docs") != 1) return -18;
        gmb_vecdb_close(h2);
        remove(vpath);
        /* superficie gmb_aiv_* via gmb_rag_* */
        if (!gmb_aiv_ensure_collection(":memory:", "docs", 8)) return -8;
        if (!gmb_aiv_index_source(":memory:", "docs", "d1", "the cat sat on the mat")) return -9;
        if (gmb_aiv_count(":memory:", "docs") < 1) return -10;
        json = gmb_aiv_search(":memory:", "docs", "the cat sat on the mat", 1);
        if (!json || !strstr(json, "cat")) { free(json); return -11; }
        free(json);
        if (!gmb_aiv_delete_text(":memory:", "docs", "d1")) return -19;
        if (gmb_aiv_count(":memory:", "docs") != 0) return -20;
        if (!gmb_aiv_index_source(":memory:", "docs", "d2", "remove me")) return -22;
        if (!gmb_aiv_remove_source(":memory:", "docs", "d2")) return -23;
        if (gmb_aiv_count(":memory:", "docs") != 0) return -24;
        json = gmb_aiv_list_collections(":memory:");
        if (!json || !strstr(json, "docs")) { free(json); return -25; }
        free(json);
    }
    return 0;
}

/* ── Backend embed di TEST per il selfcheck (deterministico, NON in produzione) ── */
static int sc_test_load(const char* model, int ngl)
{
    (void)model; (void)ngl;
    sc_test_ctx = 42;   /* "modello caricato" */
    return 42;
}
static void sc_test_unload(int h) { (void)h; sc_test_ctx = -1; }
static int sc_test_dim(int h) { return h == 42 ? 8 : 0; }
static int sc_test_text(int h, const char* text, float* out, int cap)
{
    if (h != 42 || !text || !out || cap < 8) return 1;
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++) { hash ^= *p; hash *= 16777619u; }
    double norm = 0.0;
    for (int i = 0; i < 8; i++)
    {
        out[i] = (float)sin((double)hash * (i + 1) * 0.131);
        norm += (double)out[i] * (double)out[i];
    }
    norm = sqrt(norm);
    if (norm >= 1e-12) for (int i = 0; i < 8; i++) out[i] = (float)(out[i] / norm);
    return 0;
}

/* --- gmb_aiv_*: P0c (2026-09-12) — FACADE sulle gmb_rag_* (schema esteso C#, fonte unica §0bis).
 * Prima: handle dedicato su gmb_vecdb_* e schema legacy ridotto (vec_documents senza
 * chunk/FTS/audit) — i .db scritti dal lato C erano incompatibili col C# che li leggeva come
 * semantici. Ora ogni gmb_aiv_* inoltra alla controparte gmb_rag_* sullo STESSO file. --- */

int gmb_aiv_ensure_collection(const char* db, const char* name, int dims)
{
    /* Parità C# AiVectorRuntime.EnsureCollection (2026-09-23, Parita-5-Backend-Residui §2):
     * ritorna l'id collezione su successo e -1 su errore (il C# lancia → wrapper -1).
     * Prima mappava a 1/0: senza provider dava 0 mentre IL dava -1 (e il C99/C99Native,
     * che chiamano questa, divergevano). Nota truthiness: -1 è truthy in C — i call-site
     * con `!` (selfcheck FASE B) girano con backend embed caricato, quindi cid > 0. */
    int64_t cid = gmb_rag_ensure_collection(db, name, dims, "cosine", NULL);
    if (cid <= 0) return -1;
    return (int)cid;
}

/* Variante con formato storage ('fp32'|'sq8', default fp32): inoltra a _fmt. */
int gmb_aiv_ensure_collection_fmt(const char* db, const char* name, int dims, const char* format)
{
    int64_t cid = gmb_rag_ensure_collection_fmt(db, name, dims, "cosine", NULL, format);
    if (cid <= 0) return -1;
    return (int)cid;
}

int gmb_aiv_index_text(const char* db, const char* coll, const char* text)
{
    /* P0c: la superficie gmb_aiv_* a 3 parametri non ha source_id — ne genera uno deterministico
     * dal contenuto (hash troncato), così il re-index dello stesso testo è idempotente (ON
     * CONFLICT per source_id) invece di duplicare righe come faceva il d<N> a contatore. */
    static char src[128];
    const char* t = text ? text : "";
    uint32_t h = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)t; *p; p++) { h ^= *p; h *= 16777619u; }
    snprintf(src, sizeof(src), "d%08x", h);
    /* P0e: -1/-2 da gmb_rag_index_source (via rag_embed) si propagano com'erano (0/1 booleano) */
    int r = gmb_rag_index_source(db, coll, src, t, NULL, 0, 0);
    if (r > 0)
    {
        gmb_agent_insert_trigger_fire(db, coll, src, t);
        return 1;
    }
    return r == 0 ? 1 : 0;   /* skipped (stesso hash) → successo senza trigger (parità C# outcome==0) */
}

/* P0c (todo Tokenizer-WordPiece §9): INDEXTEXT/INDEX META con source_id ESPLICITO (era perso:
 * le emissioni C99/C99Native passavano a gmb_aiv_index_text che genera un source hash da solo)
 * e metadata JSON (era scartato/as-text). Stesse regole di ritorno di gmb_aiv_index_text:
 * 1 indicizzato/aggiornato (+trigger), 1 skipped (hash invariato), 0 errore. */
int gmb_aiv_index_source(const char* db, const char* coll, const char* source_id, const char* text)
{
    const char* t = text ? text : "";
    const char* sid = (source_id && source_id[0]) ? source_id : "d0";
    int r = gmb_rag_index_source(db, coll, sid, t, NULL, 0, 0);
    /* Mappa ai codici C# IndexTextOutcome (Indexed=0, Updated=1, Skipped=2; errore -1):
     * gmb_rag_index_source usa 0=skipped/1=indexed/2=updated → invertito. */
    if (r > 0)
    {
        gmb_agent_insert_trigger_fire(db, coll, sid, t);
        if (r == 1) return 0;   /* Indexed */
        if (r == 2) return 1;   /* Updated */
        return r;
    }
    if (r == 0) return 2;       /* Skipped */
    return -1;                  /* errore */
}

int gmb_aiv_index_meta(const char* db, const char* coll, const char* source_id, const char* text, const char* metadata)
{
    const char* t = text ? text : "";
    const char* sid = (source_id && source_id[0]) ? source_id : "d0";
    const char* m = (metadata && metadata[0]) ? metadata : NULL;
    int r = gmb_rag_index_source(db, coll, sid, t, m, 0, 0);
    /* Mappa i codici di gmb_rag_index_source (0=skipped, 1=indexed, 2=updated) ai codici C#
     * IndexTextOutcome (Indexed=0, Updated=1, Skipped=2): prima ritornava 1/0 → INDEXMETA
     * divergeva da IL/PCode (che ritornano l'enum). errore → -1 (parità catch C#). */
    if (r > 0)
    {
        gmb_agent_insert_trigger_fire(db, coll, sid, t);
        if (r == 1) return 0;   /* Indexed */
        if (r == 2) return 1;   /* Updated */
        return r;
    }
    if (r == 0) return 2;       /* Skipped */
    return -1;                  /* errore */
}

int gmb_aiv_delete_text(const char* db, const char* coll, const char* source_id)
{
    return gmb_rag_delete_source(db, coll, source_id) > 0 ? 1 : 0;
}

int gmb_aiv_remove_source(const char* db, const char* coll, const char* source_id)
{
    return gmb_aiv_delete_text(db, coll, source_id);
}

int gmb_aiv_count(const char* db, const char* coll)
{
    return gmb_rag_count(db, coll);
}

int gmb_aiv_purge_deprecated(const char* db, const char* coll)
{
    return gmb_rag_purge_deprecated(db, coll);
}

char* gmb_aiv_search(const char* db, const char* coll, const char* query, int topK)
{
    return gmb_rag_search(db, coll, query, topK, NULL);
}

char* gmb_aiv_ask(const char* db, const char* coll, const char* question, int topK)
{
    char* ctx = gmb_aiv_search(db, coll, question, topK);
    char* out = (char*)malloc(8192);
    if (!out) { free(ctx); return NULL; }
    snprintf(out, 8192, "answer(ctx: %s)", ctx ? ctx : "");
    free(ctx);
    return out;
}

char* gmb_aiv_list_collections(const char* db)
{
    return gmb_rag_list_collections(db);
}

char* gmb_vecdb_list_collections(int64_t h)
{
    sqlite3* db = vec_db(h);
    sqlite3_stmt* st = NULL;
    char* out = (char*)malloc(4096);
    int first = 1;
    if (!out) return NULL;
    strcpy(out, "[");
    if (!db) { strcat(out, "]"); return out; }
    if (sqlite3_prepare_v2(db, "SELECT name FROM vec_collections ORDER BY name", -1, &st, NULL) != SQLITE_OK) {
        strcat(out, "]");
        return out;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* n = (const char*)sqlite3_column_text(st, 0);
        if (!n) continue;
        if (!first) strcat(out, ",");
        first = 0;
        strcat(out, "\"");
        strncat(out, n, 200);
        strcat(out, "\"");
    }
    sqlite3_finalize(st);
    strcat(out, "]");
    return out;
}

int gmb_community_db_selfcheck(void)
{
    int a = gmb_rdd_selfcheck();
    int b = gmb_sql_iface.selfcheck ? gmb_sql_iface.selfcheck() : 0;
    int c = gmb_vecdb_selfcheck();
    if (a != 0) return a;
    if (b != 0) return b - 20;
    if (c != 0) return c - 40;
    return 0;
}

/* AI RAG LOAD DOCUMENTS — porting di AiVectorRuntime.LoadDocuments:
 * cammina la cartella, filtra per pattern di estensione (default *.txt,*.html,*.md),
 * indicizza ogni file con sourceId = basename. Ritorna il conteggio. */
static int aiv_ext_match(const char* name, const char* pattern)
{
    if (!pattern || !*pattern) return 1;
    const char* dot = strrchr(name, '.');
    if (!dot) return 0;
    char ext[64];
    snprintf(ext, sizeof(ext), "*.%s", dot + 1);
    /* pattern è "*.txt,*.html,*.md" — split su ',' e match case-insensitive */
    char pat[512];
    strncpy(pat, pattern, sizeof(pat) - 1); pat[sizeof(pat) - 1] = '\0';
    char* tok = strtok(pat, ",");
    while (tok)
    {
        while (*tok == ' ' || *tok == '\t') tok++;
        char* end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        if (strcasecmp(tok, ext) == 0) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

int gmb_aiv_load_documents(const char* db, const char* coll, const char* path, const char* pattern)
{
    /* Parità con l'oracolo (IlRuntime.AiVectorLoadDocuments: try/catch -> -1): path assente o
     * illeggibile è un ERRORE esplicito (-1), non un "caricati 0 documenti". Prima il C tornava 0
     * e la gamba VM/C divergeva da IL/PCode (-1) — riprodotto da gen_AI_LoadDocuments.
     * (2026-09-23, Parita-5-Backend-Residui §2 — AgentIngestionDemo): ANCHE senza provider è
     * -1: il C# IndexTextWithMetadata lancia (WithCollection) e l'eccezione esce da
     * LoadDocuments → catch → -1. Il vecchio commento qui sotto ("l'indicizzazione fallita
     * NON è un errore, ritorna 0") descriveva un comportamento C# inesistente. */
    if (!gmb_rag_embedding_available()) return -1;
    if (!path || !*path) return -1;
    DIR* d = opendir(path);
    if (!d)
    {
        /* `path` è un FILE (non una directory): caricalo direttamente (parità C#
         * AiVectorRuntime.LoadDocuments: Directory.Exists false → carica il file). Prima il C
         * gestiva solo le directory → un path-file dava -1 (divergente). */
        FILE* ff = fopen(path, "rb");
        if (!ff) return -1;
        char fbuf[65536];
        size_t fn = fread(fbuf, 1, sizeof(fbuf) - 1, ff);
        fclose(ff);
        if (!fn) return 0;
        fbuf[fn] = '\0';
        const char* base = strrchr(path, '/');
        base = base ? base + 1 : path;
        /* Parità C# (IndexTextWithMetadata lancia al primo fallimento → catch → -1):
         * errore di indicizzazione = -1 (non 0); skipped/ok = 0/1. */
        { int ir = gmb_rag_index_source(db, coll, base, fbuf, NULL, 0, 0);
          if (ir < 0) return -1;
          return ir > 0 ? 1 : 0; }
    }
    const char* pat = (pattern && *pattern) ? pattern : "*.txt,*.html,*.md";
    int count = 0;
    struct dirent* e;
    char full[1100];
    while ((e = readdir(d)))
    {
        if (e->d_name[0] == '.') continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (e->d_type == DT_DIR)
        {
            { int sub = gmb_aiv_load_documents(db, coll, full, pat);
              if (sub > 0) count += sub; }
            continue;
        }
        if (!aiv_ext_match(e->d_name, pat)) continue;
        FILE* f = fopen(full, "rb");
        if (!f) continue;
        char buf[65536];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        if (!n) continue;
        buf[n] = '\0';
        /* P0c: indicizza via gmb_rag_index_source (source_id = path del file, schema esteso).
         * Parità C# (lancia al primo fallimento): errore = abort -1 immediato. */
        { int ir = gmb_rag_index_source(db, coll, e->d_name, buf, NULL, 0, 0);
          if (ir < 0) { closedir(d); return -1; }
          if (ir > 0) count++; }
    }
    closedir(d);
    return count;
}

/* AI.EMBED / AIVECTOR.EMBED(text, dims) — P0e (decisione utente 2026-09-12): embedding REALE
 * dal modello attivo (gmb_ai_embed_*), o NULL + errore esplicito (rag_embed_err).
 * Il vecchio commento «gmb_aiv_embed ha fallback deterministico, nessun backend richiesto»
 * documentava esattamente il difetto: AI.EMBED non poteva fallire per costruzione e serviva
 * l'hash FNV/sin. dims<=0 → dimensione del MODELLO (non più default 384 fittizio). */
static char g_aiv_embed_err[256];

double* gmb_aiv_embed(const char* text, int dims, gmb_int_t* outn)
{
    if (!outn) return NULL;
    *outn = 0;
    int mdim = gmb_ai_embed_dim();
    if (mdim <= 0)
    {
        /* Opt-in fixture (2026-09-18, parità C# FixtureEmbeddingProvider): stesse condizioni
         * di rag_embed (flag + provider fixture). Senza: errore esplicito (P0e intatto). */
        if (dims <= 0 || dims > GMB_VEC_MAXDIM) return NULL;
        if (gmb_fixture_embed_wanted()) {
            float* tmp = (float*)malloc(sizeof(float) * (size_t)dims);
            if (!tmp) return NULL;
            if (gmb_fixture_embed(text ? text : "", dims, tmp) != 0) { free(tmp); return NULL; }
            double* r = (double*)malloc(sizeof(double) * (size_t)dims);
            if (!r) { free(tmp); return NULL; }
            for (int i = 0; i < dims; i++) r[i] = (double)tmp[i];
            free(tmp);
            *outn = dims;
            return r;
        }
        snprintf(g_aiv_embed_err, sizeof(g_aiv_embed_err), "%s",
                 rag_embed_err(-1));
        return NULL;
    }
    if (dims <= 0 || dims > mdim) dims = mdim;
    float* tmp = (float*)malloc(sizeof(float) * (size_t)dims);
    if (!tmp) return NULL;
    if (gmb_ai_embed_text(text ? text : "", tmp, dims) != 0)
    {
        free(tmp);
        snprintf(g_aiv_embed_err, sizeof(g_aiv_embed_err),
                 "[AI embed ERROR: dimensione richiesta %d != dimensione modello %d]", dims, mdim);
        return NULL;
    }
    double* r = (double*)malloc(sizeof(double) * (size_t)dims);
    if (!r) { free(tmp); return NULL; }
    for (int i = 0; i < dims; i++) r[i] = (double)tmp[i];
    free(tmp);
    *outn = dims;
    return r;
}

/* ── R3 GenTest: genera testset sintetico da collection. ── */
char* gmb_vectordb_gentest(const char* db, const char* coll, const char* n_str, const char* types)
{
    if (!db || !coll) return strdup("ERROR: invalid arguments.");
    int n = 5;
    if (n_str && n_str[0]) { char* end = NULL; long v = strtol(n_str, &end, 10); if (end && end != n_str && v > 0) n = (int)v; }
    if (n > 50) n = 50;
    char* tcopy = strdup(types && types[0] ? types : "simple");
    if (!tcopy) return strdup("ERROR: out of memory.");

    /* Open vector DB */
    int64_t h = rag_handle(db);
    sqlite3* sql = NULL;
    for (int i = 1; i < 64; i++) if (g_vdb[i].used && g_vdb[i].db) { sql = g_vdb[i].db; break; }
    if (!sql) { free(tcopy); return strdup("ERROR: cannot open vector DB."); }

    int64_t cid = rag_coll_id(sql, coll);
    if (cid <= 0) { free(tcopy); return strdup("ERROR: collection not found."); }

    /* Sample random chunks */
    sqlite3_stmt* st = NULL;
    int sample_n = n * 3;
    if (sample_n > 100) sample_n = 100;
    if (sqlite3_prepare_v2(sql,
        "SELECT c.id, c.text FROM vec_chunks c "
        "JOIN vec_documents d ON c.document_id = d.id "
        "WHERE d.collection_id = ?1 AND d.status = 'current' "
        "ORDER BY RANDOM() LIMIT ?2", -1, &st, NULL) != SQLITE_OK)
    { free(tcopy); return strdup("ERROR: SQL prepare failed."); }
    sqlite3_bind_int64(st, 1, cid);
    sqlite3_bind_int(st, 2, sample_n);

    /* Collect chunks in a simple array */
    enum { MAX_CHUNKS = 64 };
    int64_t chunk_ids[MAX_CHUNKS];
    char chunk_texts[MAX_CHUNKS][4096];
    int chunk_count = 0;
    while (sqlite3_step(st) == SQLITE_ROW && chunk_count < MAX_CHUNKS)
    {
        chunk_ids[chunk_count] = sqlite3_column_int64(st, 0);
        const char* txt = (const char*)sqlite3_column_text(st, 1);
        snprintf(chunk_texts[chunk_count], 4096, "%s", txt ? txt : "");
        chunk_count++;
    }
    sqlite3_finalize(st);

    if (chunk_count == 0) { free(tcopy); return strdup("ERROR: no chunks found."); }

    /* Parse types */
    int nt = 0;
    const char* type_arr[16];
    char* tok = strtok(tcopy, ",");
    while (tok && nt < 16)
    {
        while (*tok == ' ' || *tok == '\t') tok++;
        char* end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = 0;
        type_arr[nt++] = tok;
        tok = strtok(NULL, ",");
    }
    if (nt == 0) { type_arr[0] = "simple"; nt = 1; }

    /* Build golden set JSON */
    char out[65536];
    int pos = snprintf(out, sizeof(out),
        "{\n"
        "  \"schema_version\": \"1.0\",\n"
        "  \"dataset_version\": \"1.0\",\n"
        "  \"source\": \"ai-vectordb-gentest-c\",\n"
        "  \"collection\": \"%s\",\n"
        "  \"cases\": [\n", coll);

    int generated = 0;
    for (int i = 0; generated < n && i < chunk_count; i++)
    {
        const char* type = type_arr[generated % nt];
        char question[4096], answer[4096];
        snprintf(question, sizeof(question), "What is in the text? (chunk %lld)", (long long)chunk_ids[i]);
        snprintf(answer, sizeof(answer), "%s", chunk_texts[i]);

        /* Try LLM call */
        char prompt[8192];
        snprintf(prompt, sizeof(prompt),
            "Generate a %s question that can be answered from the text below.\n"
            "Text: %s\n\nReturn ONLY JSON with question and expected_answer fields.",
            type, chunk_texts[i]);
        char* llm_resp = gmb_llm_complete(
            "You are a test generator. Return only valid JSON.", prompt);
        if (llm_resp)
        {
            char* js = strstr(llm_resp, "{");
            char* je = strrchr(llm_resp, '}');
            if (js && je && je > js)
            {
                *je = 0;
                char qfield[8192], afield[8192];
                snprintf(qfield, sizeof(qfield), "");
                snprintf(afield, sizeof(afield), "");
                sscanf(js + 1, "\"question\":\"%[^\"]", qfield);
                char* apos = strstr(js, "\"expected_answer\"");
                if (apos) sscanf(apos, "\"expected_answer\":\"%[^\"]", afield);
                if (qfield[0] && afield[0])
                {
                    snprintf(question, sizeof(question), "%s", qfield);
                    snprintf(answer, sizeof(answer), "%s", afield);
                }
            }
            free(llm_resp);
        }

        pos += snprintf(out + pos, sizeof(out) - (size_t)pos,
            "%s    {\n"
            "      \"id\": \"%s_%d\",\n"
            "      \"type\": \"%s\",\n"
            "      \"question\": \"%s\",\n"
            "      \"expected_answer\": \"%s\",\n"
            "      \"ideal_chunks\": [%lld]\n"
            "    }",
            generated > 0 ? ",\n" : "",
            type, generated, type, question, answer,
            (long long)chunk_ids[i]);
        generated++;
    }

    pos += snprintf(out + pos, sizeof(out) - (size_t)pos,
        "\n  ]\n}\n");

    /* Compute SHA256 hash */
    uint8_t digest[32];
    gmb_sha256((const uint8_t*)out, (size_t)pos, digest);
    char hash[65];
    for (int i = 0; i < 32; i++) snprintf(hash + i * 2, 3, "%02x", digest[i]);

    /* Write golden set file */
    /* Build safe filename: replace non-alphanumeric with _ */
    char safe_coll[128];
    int si = 0;
    for (int ci = 0; coll[ci] && si < 120; ci++) {
        char c = coll[ci];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
            safe_coll[si++] = c;
        else if (si > 0 && safe_coll[si-1] != '_')
            safe_coll[si++] = '_';
    }
    safe_coll[si] = 0;
    if (si == 0) snprintf(safe_coll, sizeof(safe_coll), "collection");

    char fname[1024];
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    snprintf(fname, sizeof(fname), "samples/Fixtures/rag-eval/gentest_%s_%04d%02d%02d%02d%02d%02d.json",
        safe_coll, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "samples/Fixtures/rag-eval");
    mkdir(dir, 0755);

    FILE* fp = fopen(fname, "w");
    if (fp) { fputs(out, fp); fclose(fp); }

    char* result = (char*)malloc(2048);
    if (result) snprintf(result, 2048,
        "Golden set saved: %s (%d cases, sha256=%s)",
        fname, generated, hash);

    free(tcopy);
    return result ? result : strdup("ERROR: out of memory.");
}
