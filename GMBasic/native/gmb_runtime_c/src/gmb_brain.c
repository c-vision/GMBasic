/* FILE: native/gmb_runtime_c/src/gmb_brain.c
 * Second brain condiviso — porting di Basic.AI.MemoryBrain (common/AI/Rag/MemoryBrain.cs).
 * Memoria persistente su SQLite (<configDir>/brain.db — gmb_paths). P0e (decisione utente
 * 2026-09-12): embedding REALE (gmb_ai_embed_*) quando un modello è attivo; senza modello
 * NESSUN pseudo-vettore (niente più gmb_aiv_embed/fixture FNV-sin) — la memoria viene salvata
 * senza vettore e la ricerca usa SOLO il confronto lessicale DICHIARATO (brain_contains_ic),
 * che il messaggio d'errore nomina quando non trova nulla.
 * Le funzioni gmb_brain_* sono la fonte unica (§0bis) per VM/C e C99; il formato di
 * ricerca è "id|kind|testo" (parità MemoryBrain.Search).
 */
#include "gmb_runtime.h"
#include "gmb_brains.h"   /* gmb_brain_lookup: risoluzione unica dei cervelli */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define BRAIN_DIMS_DEFAULT 384

static void brain_db_path(char* out, size_t cap)
{
    gmb_paths_brain_db(out, cap);
}

/* DB RAG di un brain per-nome (parità BrainRegistry.RagDbPath C#). Ordine:
 *   1. RISOLUZIONE dei cervelli (vendor → exe → brainsDir → cache ~/.gmbasic/brains/<nome>/<ver>):
 *      è dove `AI BRAIN INSTALL` mette il cervello, e dove sta anche sul programma distribuito;
 *   2. <brainsDir>/<name>/rag/<name>.db (import classico);
 *   3. <ragDir>/<name>.db (legacy: i feed prima della cache).
 * 1 se esiste. */
int gmb_brain_rag_db(const char* name, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    if (!name || !*name || !out || !cap) return 0;
    char dir[4096];
    if (gmb_brain_lookup(name, NULL, dir, sizeof dir)) {
        snprintf(out, cap, "%s/rag/%s.db", dir, name);
        if (access(out, F_OK) == 0) return 1;
        /* cervello con un solo .db dentro rag/ (nome interno diverso dal nome logico) */
        char rag[4200];
        snprintf(rag, sizeof rag, "%s/rag", dir);
        DIR* d = opendir(rag);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != NULL) {
                size_t l = strlen(e->d_name);
                if (l > 3 && strcmp(e->d_name + l - 3, ".db") == 0) {
                    snprintf(out, cap, "%s/%s", rag, e->d_name);
                    closedir(d);
                    return 1;
                }
            }
            closedir(d);
        }
    }
    /* Ultimo passo: la cartella di BUILD configurata (`paths.ragDir`, default la CACHE utente
     * `~/.gmbasic/rag`). NON e' il vecchio `shared/rag`: i builder scrivono li', quindi un
     * cervello costruito in locale si trova. Nessuna scorciatoia su path hardcoded nel checkout. */
    {
        char rd[2048];
        gmb_paths_rag_dir(rd, sizeof rd);
        if (rd[0]) {
            snprintf(out, cap, "%s/%s.db", rd, name);
            if (access(out, F_OK) == 0) return 1;
        }
    }
    out[0] = 0;
    return 0;
}

/* Export JSON del grafo di un cervello (parità BrainRegistry.GraphJsonPath C#). Stesso ordine
 * del RAG: risoluzione (vendor → exe → brainsDir → cache <nome>/<versione>), poi la cartella di
 * BUILD configurata (`paths.graphsDir`, default ~/.gmbasic/graphs). 1 se esiste.
 *
 * PERCHÉ ESISTE: prima `sb_graph_path` cercava solo `<brainsDir piatto>/<nome>/graphs/` e
 * `<graphsDir>/<nome>.json` → un cervello INSTALLATO nella cache versionata non era
 * interrogabile come grafo dalle gambe C (stesso buco già corretto per il RAG). */
int gmb_brain_graph_json(const char* name, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    if (!name || !*name || !out || !cap) return 0;
    char dir[4096];
    if (gmb_brain_lookup(name, NULL, dir, sizeof dir)) {
        snprintf(out, cap, "%s/graphs/%s.json", dir, name);
        if (access(out, F_OK) == 0) return 1;
        /* cervello con un solo .json dentro graphs/ (nome interno diverso dal nome logico) */
        char gd[4200];
        snprintf(gd, sizeof gd, "%s/graphs", dir);
        DIR* d = opendir(gd);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != NULL) {
                size_t l = strlen(e->d_name);
                if (l > 5 && strcmp(e->d_name + l - 5, ".json") == 0 &&
                    strstr(e->d_name, ".stats.") == NULL && strstr(e->d_name, ".state.") == NULL) {
                    snprintf(out, cap, "%s/%s", gd, e->d_name);
                    closedir(d);
                    return 1;
                }
            }
            closedir(d);
        }
    }
    char gdir[2048];
    gmb_paths_graphs_dir(gdir, sizeof gdir);
    if (gdir[0]) {
        snprintf(out, cap, "%s/%s.json", gdir, name);
        if (access(out, F_OK) == 0) return 1;
    }
    out[0] = 0;
    return 0;
}

static int brain_rag_db_path(const char* name, char* out, size_t cap)
{
    return gmb_brain_rag_db(name, out, cap);
}

/* 1 se il DB ha una tabella `memories` leggibile dal runtime C (il formato supportato).
 * I DB vettoriali puri (vec_chunks/vec_embeddings, modello non-fixture) non sono leggibili
 * → il brain va saltato (non interrogabile dal C), come il C# salta i brain il cui provider
 * embedding non è attivo. */
static int brain_db_has_memories(const char* path)
{
    sqlite3* db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return 0; }
    sqlite3_stmt* st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='memories';", -1, &st, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(st) == SQLITE_ROW) ok = 1;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

static sqlite3* brain_open(void)
{
    char path[1024];
    brain_db_path(path, sizeof(path));
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    sqlite3* db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return NULL; }
    const char* schema =
        "CREATE TABLE IF NOT EXISTS memories ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " kind TEXT NOT NULL,"
        " text TEXT NOT NULL,"
        " source TEXT,"
        " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        " embedding TEXT);"
        "CREATE TABLE IF NOT EXISTS watched ("
        " path TEXT PRIMARY KEY,"
        " last_hash TEXT);"
        "CREATE TABLE IF NOT EXISTS links ("
        " from_path TEXT NOT NULL,"
        " to_name TEXT NOT NULL,"
        " PRIMARY KEY (from_path, to_name));";
    sqlite3_exec(db, schema, NULL, NULL, NULL);
    return db;
}

static char* brain_embed_json(const char* text, int dims, int* out_dims)
{
    if (dims <= 0) dims = BRAIN_DIMS_DEFAULT;
    /* P0e (decisione utente 2026-09-12): MEMORY usa il modello embedding REALE quando attivo;
     * senza modello il vettore NON esiste più (prima: gmb_aiv_embed installava l'hash
     * FNV/sin pseudo-semantico). Ritorna NULL: i chiamanti ricadono sul confronto lessicale
     * DICHIARATO (brain_contains_ic) e il messaggio d'errore lo nomina. */
    if (gmb_ai_embed_dim() <= 0)
    {
        *out_dims = 0;
        return NULL;
    }
    float* tmp = (float*)malloc(sizeof(float) * (size_t)dims);
    if (!tmp) { *out_dims = 0; return NULL; }
    if (gmb_ai_embed_text(text ? text : "", tmp, dims) != 0)
    {
        /* dims != dim del modello: anche qui niente pseudo-vettore */
        free(tmp);
        *out_dims = 0;
        return NULL;
    }
    size_t cap = (size_t)dims * 24 + 16;
    char* buf = (char*)malloc(cap);
    if (!buf) { free(tmp); *out_dims = 0; return NULL; }
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "[");
    for (int i = 0; i < dims; i++)
        off += (size_t)snprintf(buf + off, cap - off, i ? ",%.8g" : "%.8g", (double)tmp[i]);
    snprintf(buf + off, cap - off, "]");
    free(tmp);
    *out_dims = dims;
    return buf;
}

static int brain_parse_embed(const char* js, float* out, int cap)
{
    if (!js || !*js || js[0] != '[') return 0;
    const char* p = js + 1;
    int n = 0;
    while (*p && *p != ']' && n < cap)
    {
        out[n++] = (float)strtod(p, (char**)&p);
        while (*p && *p != ',' && *p != ']') p++;
        if (*p == ',') p++;
    }
    return n;
}

static double brain_cosine(const float* a, int an, const float* b, int bn)
{
    if (an <= 0 || an != bn) return 0;
    double dot = 0, na = 0, nb = 0;
    for (int i = 0; i < an; i++)
    {
        dot += (double)a[i] * b[i];
        na += (double)a[i] * a[i];
        nb += (double)b[i] * b[i];
    }
    if (na <= 0 || nb <= 0) return 0;
    return dot / (sqrt(na) * sqrt(nb));
}

static int brain_contains_ic(const char* hay, const char* needle)
{
    if (!hay || !needle || !*needle) return 0;
    size_t nl = strlen(needle);
    for (; *hay; hay++)
    {
        size_t i = 0;
        for (; i < nl && hay[i]; i++)
        {
            char a = hay[i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (i == nl) return 1;
    }
    return 0;
}

long gmb_brain_add(const char* text, const char* kind, const char* source, int dims)
{
    if (!text || !*text) return -1;
    int edims = 0;
    char* emb = brain_embed_json(text, dims, &edims);
    sqlite3* db = brain_open();
    if (!db) { free(emb); return -1; }
    long id = -1;
    sqlite3_stmt* st = NULL;
    /* NOTA (2026-09-30): sqlite3_prepare_v2 compila SOLO la prima istruzione, quindi il
     * `SELECT last_insert_rowid()` che seguiva nell'unico prepare NON veniva eseguito e
     * `sqlite3_step` restituiva SQLITE_DONE → id SEMPRE -1 pur con l'inserimento riuscito
     * (bug pre-esistente, scoperto portando la rotta /api/brain/note: la risposta C# porta
     * l'id vero). Parità C# `MemoryBrain.Add` → `ExecuteScalar()` = last_insert_rowid. */
    if (sqlite3_prepare_v2(db,
            "INSERT INTO memories(kind,text,source,embedding) VALUES(?1,?2,?3,?4);",
            -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, kind && *kind ? kind : "Note", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, text, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, source && *source ? source : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, emb ? emb : NULL, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_DONE) id = (long)sqlite3_last_insert_rowid(db);
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    free(emb);
    return id;
}

int gmb_brain_forget(long id)
{
    sqlite3* db = brain_open();
    if (!db) return 0;
    int n = 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM memories WHERE id=?1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, id);
        if (sqlite3_step(st) == SQLITE_DONE) n = sqlite3_changes(db);
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return n > 0 ? 1 : 0;
}

/* Scoring mem0 (parità C# MemoryRanker, che NON usa vettori: la query è ignorata):
 * score = 0.4*recency + 0.3*frequency + 0.3*importance; recency = exp(-0.1*giorni da
 * created_at UTC); frequency = COUNT(*) WHERE text esatto; importance = min(1,len/200 +
 * 0.3 se contiene preferisce/decisione/importante/sempre, case-insensitive).
 * Output "{id}|{kind}|{text}|{score:F4-it}|{freq}|{recency:F4-it}" (F4 con virgola it-IT).
 * Prima: cosine su embedding (mai presenti senza modello) + fallback lessicale 0.5, e
 * shape a 3 campi — mai uguale al C#. Lowercase/len solo ASCII (approssimazione notata). */
static double brain_mem_importance(const char* text) {
    if (!text || !*text) return 0.0;
    size_t L = strlen(text);
    double base = (double)L / 200.0;
    if (base > 1.0) base = 1.0;
    char* low = (char*)malloc(L + 1);
    if (!low) return base;
    for (size_t k = 0; k <= L; k++) { char c = text[k]; low[k] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
    if (strstr(low, "preferisce") || strstr(low, "decisione") || strstr(low, "importante") || strstr(low, "sempre")) base += 0.3;
    free(low);
    return base > 1.0 ? 1.0 : base;
}
static double brain_mem_recency(sqlite3* db, const char* created_at) {
    (void)db;
    int Y = 0, Mo = 0, D = 0, h = 0, mi = 0, s = 0;
    if (!created_at || sscanf(created_at, "%d-%d-%d %d:%d:%d", &Y, &Mo, &D, &h, &mi, &s) != 6) return 1.0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = Y - 1900; tmv.tm_mon = Mo - 1; tmv.tm_mday = D;
    tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = s;
    time_t t = timegm(&tmv);
    if (t == (time_t)-1) return 1.0;
    double days = (double)(time(NULL) - t) / 86400.0;
    if (days < 0) days = 0;
    return exp(-0.1 * days);
}
static int brain_mem_frequency(sqlite3* db, const char* text) {
    sqlite3_stmt* st = NULL;
    int n = 1;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM memories WHERE text=$1;", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, text ? text : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) { n = sqlite3_column_int(st, 0); if (n < 1) n = 1; }
        sqlite3_finalize(st);
    }
    return n;
}
char* gmb_brain_search(const char* query, int topK, int dims)
{
    if (topK <= 0) topK = 5;
    if (!query) query = "";
    (void)dims;
    sqlite3* db = brain_open();
    if (!db) return strdup("");
    /* Parità MemoryBrain.Search (C#): score = cosine(queryEmb, rowEmb) se entrambi presenti,
     * altrimenti fallback lessicale (text contiene query → 0.5, altrimenti 0); filtra > 0,
     * ordina desc, take topK; output "id|kind|text". Prima usava un modello "mem0"
     * (0.4*recency+0.3*freq+0.3*importance, query ignorata, output a 6 campi) → divergente. */
    typedef struct { long id; char kind[32]; char text[4096]; double score; } brain_row_t;
    brain_row_t* rows = (brain_row_t*)calloc(256, sizeof(brain_row_t));
    if (!rows) { sqlite3_close(db); return strdup(""); }
    int nrows = 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id,kind,text,embedding FROM memories;", -1, &st, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(st) == SQLITE_ROW && nrows < 256)
        {
            const char* k = (const char*)sqlite3_column_text(st, 1);
            const char* t = (const char*)sqlite3_column_text(st, 2);
            const char* emb = (const char*)sqlite3_column_text(st, 3);
            double sc = 0.0;
            /* Senza modello di embedding (caso C) qn=0 → fallback lessicale. */
            if (emb && *emb) sc = 0.0; /* embeddings presenti ma query non embeddata: fallback */
            /* Parità C# `row.Text.Contains(query, OrdinalIgnoreCase)`: con query VUOTA
             * Contains("") è TRUE → 0.5 per ogni riga (serve a /api/brain/list e a
             * AI.MEMORYSEARCH ""). brain_contains_ic respinge il needle vuoto (0), quindi
             * il caso vuoto va trattato qui. Tutti a 0.5 → l'ordinamento stabile li lascia
             * nell'ordine di inserimento, come OrderByDescending in C#. */
            if (query && *query) { if (brain_contains_ic(t ? t : "", query)) sc = 0.5; }
            else sc = 0.5;
            if (sc > 0) {
                rows[nrows].id = sqlite3_column_int64(st, 0);
                snprintf(rows[nrows].kind, sizeof(rows[nrows].kind), "%s", k ? k : "");
                snprintf(rows[nrows].text, sizeof(rows[nrows].text), "%s", t ? t : "");
                rows[nrows].score = sc;
                nrows++;
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);

    for (int i = 0; i < nrows; i++)
        for (int j = i + 1; j < nrows; j++)
            if (rows[j].score > rows[i].score)
            {
                brain_row_t tmp = rows[i];
                rows[i] = rows[j]; rows[j] = tmp;
            }

    size_t cap = 1;
    for (int i = 0; i < nrows && i < topK; i++)
        cap += strlen(rows[i].kind) + strlen(rows[i].text) + 32;
    char* out = (char*)malloc(cap);
    out[0] = '\0';
    int printed = 0;
    for (int i = 0; i < nrows && i < topK; i++)
    {
        if (printed) strncat(out, "\n", cap - strlen(out) - 1);
        snprintf(out + strlen(out), cap - strlen(out), "%lld|%s|%s",
                 (long long)rows[i].id, rows[i].kind, rows[i].text);
        printed++;
    }
    free(rows);
    return out;
}

long gmb_brain_import_file(const char* path, int dims)
{
    if (!path || !*path) return -1;
    const char* dot = strrchr(path, '.');
    if (!dot) return -1;
    int is_text = (strcasecmp(dot, ".md") == 0 || strcasecmp(dot, ".txt") == 0);
    int is_img = (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 ||
                  strcasecmp(dot, ".jpeg") == 0 || strcasecmp(dot, ".gif") == 0 ||
                  strcasecmp(dot, ".webp") == 0 || strcasecmp(dot, ".bmp") == 0);
    if (!is_text && !is_img) return -1;
    if (access(path, R_OK) != 0) return -1;
    char* content = (char*)malloc(65536);
    if (!content) return -1;
    if (is_text)
    {
        FILE* f = fopen(path, "rb");
        if (!f) { free(content); return -1; }
        size_t n = fread(content, 1, 65535, f);
        fclose(f);
        content[n] = '\0';
        long id = gmb_brain_add(content, "Note", path, dims);
        free(content);
        return id;
    }
    /* immagine: OCR se il backend visione è registrato, altrimenti skip (parità TryOcr). */
    free(content);
    const char* text = gmb_ai_ocr_read(path, "Extract all text from this image.", 512);
    if (text && text[0] && text[0] != '[') return gmb_brain_add(text, "Note", path, dims);
    return -1;
}

/* keyword evidence in [0,1]: frazione di termini distinti della query presenti nel testo
   (confronto case-insensitive su word boundary). Parità di comportamento del secondo stadio
   KeywordReranker gestito (non di codice). */
static double brain_kw_evidence(const char* query, const char* text)
{
    if (!query || !*query || !text) return 0.0;
    char q[512];
    size_t i = 0, n = strlen(query);
    while (i < n && i + 1 < sizeof(q)) { q[i] = (char)tolower((unsigned char)query[i]); i++; }
    q[i] = '\0';
    int total = 0, hit = 0;
    const char* p = q;
    while (*p)
    {
        while (*p && !(isalnum((unsigned char)*p))) p++;
        if (!*p) break;
        const char* start = p;
        while (*p && isalnum((unsigned char)*p)) p++;
        total++;
        /* word match nel testo (case-insensitive) */
        const char* t = text;
        int found = 0;
        size_t wl = (size_t)(p - start);
        while (*t && !found)
        {
            const char* ts = t;
            while (*ts && !(isalnum((unsigned char)*ts))) ts++;
            const char* ws = ts;
            while (*ts && isalnum((unsigned char)*ts)) ts++;
            size_t tl = (size_t)(ts - ws);
            if (tl == wl && strncasecmp(ws, start, wl) == 0) found = 1;
            t = ts;
        }
        if (found) hit++;
    }
    return total > 0 ? (double)hit / (double)total : 0.0;
}

char* gmb_brain_search_ex(const char* query, int topK, int dims, int rerank,
                          double min_evidence, int explain, char** err, int* insufficient)
{
    if (err) *err = NULL;
    if (insufficient) *insufficient = 0;
    if (rerank == 2)
    {
        if (err) *err = "AI: %RERANK_ONNX non ancora wired nel runtime C (usare %RERANK_KEYWORD)";
        return NULL;
    }
    if (rerank < 0 || rerank > 1)
    {
        if (err) *err = "AI: reranker non valido (0=OFF, 1=KEYWORD)";
        return NULL;
    }
    if (explain != 0)
    {
        if (err) *err = "AI: explain non supportato sul merge multi-brain (usare AIVECTOR.SEARCH single-collection)";
        return NULL;
    }
    if (!query || !*query) return strdup("");
    if (topK <= 0) topK = 5;

    int qd = 0;
    char* qemb = brain_embed_json(query, dims, &qd);
    float* qf = qd > 0 ? (float*)malloc(sizeof(float) * (size_t)qd) : NULL;
    int qn = qf ? brain_parse_embed(qemb, qf, qd) : 0;

    sqlite3* db = brain_open();
    if (!db) { free(qemb); free(qf); return strdup(""); }
    typedef struct { long id; char kind[32]; char text[4096]; float emb[BRAIN_DIMS_DEFAULT]; int en; double score; double ev; } brow_t;
    brow_t* rows = (brow_t*)calloc(256, sizeof(brow_t));
    if (!rows) { free(qemb); free(qf); sqlite3_close(db); return strdup(""); }
    int nrows = 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id,kind,text,embedding FROM memories;", -1, &st, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(st) == SQLITE_ROW && nrows < 256)
        {
            rows[nrows].id = sqlite3_column_int64(st, 0);
            const char* k = (const char*)sqlite3_column_text(st, 1);
            const char* t = (const char*)sqlite3_column_text(st, 2);
            const char* e = (const char*)sqlite3_column_text(st, 3);
            snprintf(rows[nrows].kind, sizeof(rows[nrows].kind), "%s", k ? k : "");
            snprintf(rows[nrows].text, sizeof(rows[nrows].text), "%s", t ? t : "");
            rows[nrows].en = e ? brain_parse_embed(e, rows[nrows].emb, BRAIN_DIMS_DEFAULT) : 0;
            double sc = 0;
            if (qn > 0 && rows[nrows].en == qn) sc = brain_cosine(qf, qn, rows[nrows].emb, rows[nrows].en);
            else if (brain_contains_ic(rows[nrows].text, query)) sc = 0.5;
            rows[nrows].score = sc;
            rows[nrows].ev = rerank == 1 ? brain_kw_evidence(query, rows[nrows].text) : -1.0;
            nrows++;
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    free(qemb); free(qf);

    for (int i = 0; i < nrows; i++)
        for (int j = i + 1; j < nrows; j++)
            if (rows[j].score > rows[i].score)
            {
                brow_t tmp = rows[i];
                rows[i] = rows[j]; rows[j] = tmp;
            }

    if (rerank == 1)
    {
        int win = topK * 3;
        if (win > nrows) win = nrows;
        /* soglia attiva solo con valore reale >= 0 (evidence in [0,1]); NaN/negativo = off */
        int finite = (min_evidence == min_evidence) && (min_evidence >= 0.0);
        int kept = 0;
        for (int i = 0; i < win; i++)
        {
            if (rows[i].score <= 0) break;
            if (finite && rows[i].ev < min_evidence) continue;
            rows[kept++] = rows[i];
        }
        for (int i = 0; i < kept; i++)
            for (int j = i + 1; j < kept; j++)
                if (rows[j].ev > rows[i].ev)
                {
                    brow_t tmp = rows[i];
                    rows[i] = rows[j]; rows[j] = tmp;
                }
        if (finite && kept == 0 && insufficient) *insufficient = 1;
        if (kept > topK) kept = topK;
        nrows = kept;
    }
    else
    {
        /* OFF: nessuna seconda passata keyword; evidence -1. */
        int kept = 0;
        for (int i = 0; i < nrows && i < topK && rows[i].score > 0; i++) rows[kept++] = rows[i];
        nrows = kept;
    }

    size_t cap = 1;
    for (int i = 0; i < nrows; i++)
        cap += strlen(rows[i].kind) + strlen(rows[i].text) + 48;
    char* out = (char*)malloc(cap);
    out[0] = '\0';
    for (int i = 0; i < nrows; i++)
    {
        if (i) strncat(out, "\n", cap - strlen(out) - 1);
        snprintf(out + strlen(out), cap - strlen(out), "%lld|%s|%.6g|%.6g|%s",
                 (long long)rows[i].id, rows[i].kind,
                 rows[i].score, rows[i].ev, rows[i].text);
    }
    free(rows);
    return out;
}

static void brain_json_reserve(char** o, size_t* l, size_t* c, size_t need)
{
    if (*l + need + 1 > *c) { *c = (*l + need + 1) * 2; *o = (char*)realloc(*o, *c); }
}
static void brain_json_append(char** o, size_t* l, size_t* c, const char* s)
{
    size_t n = strlen(s);
    brain_json_reserve(o, l, c, n);
    memcpy(*o + *l, s, n + 1); *l += n;
}
static void brain_json_escape(char** o, size_t* l, size_t* c, const char* s)
{
    size_t n = s ? strlen(s) : 0;
    brain_json_reserve(o, l, c, n * 2 + 3);
    (*o)[(*l)++] = '"';
    for (size_t i = 0; i < n; i++)
    {
        char ch = s[i];
        if (ch == '"' || ch == '\\') { (*o)[(*l)++] = '\\'; (*o)[(*l)++] = ch; }
        else if (ch == '\n') { (*o)[(*l)++] = '\\'; (*o)[(*l)++] = 'n'; }
        else (*o)[(*l)++] = ch;
    }
    (*o)[(*l)++] = '"';
    (*o)[*l] = '\0';
}

/* gmb_ai_sbm_json_multi: SEARCHMULTI multi-brain (parità MultiBrainSearch C#).
 * brains_csv = lista CSV di nomi brain (o NULL → i COLLEGATI da BRAINUSED); collection =
 * collezione RAG ("wiki"). Per ogni brain risolve il DB (import/legacy) e lo interroga con
 * il motore vettoriale condiviso (gmb_rag_search_sel, FTS+vettore). Ritorna il PRIMO risultato
 * con hits; se nessun brain è interrogabile → InsufficientEvidence "nessun brain interrogabile".
 * Fonte unica (§0bis) per VM/C e C99. Ritorno malloc. */
char* gmb_ai_sbm_json_multi(const char* query, int topK, const char* collection,
                            const char* brains_csv, int rerank, double min_evidence, int explain)
{
    /* explain != 0: il merge multi-brain non lo supporta (parità MultiBrainSearch C# e
     * VM/C gmb_builtin.c:15795). Ritorna la stringa d'errore col prefisso ERROR: (stessa
     * convenzione del risultato single-brain/VM/C) → il chiamante la stampa direttamente.
     * Senza questo guard C99/C99Native cadevano su InsufficientEvidence (minitest
     * gen_AI_SearchMulti). Fonte unica §0bis: il check vive qui, VM/C lo anticipa soltanto. */
    if (explain != 0)
        return strdup("ERROR: SearchMulti: explain non supportato sul merge multi-brain (usare AIVECTOR.SEARCH single-collection).");
    char* names = NULL;
    if (brains_csv && *brains_csv) names = strdup(brains_csv);
    else names = gmb_brain_used_csv();
    if (!names || !*names)
    {
        free(names);
        return strdup("{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessun brain collegato (AI BRAIN USE)\"}");
    }
    if (topK <= 0) topK = 5;
    char* save = NULL;
    char* out = NULL;
    /* Un cervello chiesto e NON disponibile non si salta in silenzio: si annota e, se non
     * resta niente da interrogare, l'errore lo dice (prima: "nessun brain interrogabile", senza
     * dire quale mancasse). */
    char missing[512] = "";
    for (char* nm = strtok_r(names, ",", &save); nm; nm = strtok_r(NULL, ",", &save))
    {
        while (*nm == ' ' || *nm == '\t') nm++;
        char* end = nm + strlen(nm) - 1;
        while (end > nm && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }
        if (!*nm) continue;
        char dbp[1024];
        if (!brain_rag_db_path(nm, dbp, sizeof(dbp)))
        {
            size_t ml = strlen(missing);
            snprintf(missing + ml, sizeof missing - ml, "%s%s", ml ? ", " : "", nm);
            continue;
        }
        /* Senza provider embedding il motore vettoriale ABORTISCE (gmb_raise_error, exit 6)
         * invece di ritornare: per il merge multi-brain (parità MultiBrainSearch che fa
         * try/catch per brain) il brain va SALTATO, non fatto abortire il processo. */
        if (gmb_ai_embed_dim() <= 0)
            continue;
        char* r = gmb_rag_search_sel(dbp, collection && *collection ? collection : "wiki",
                                     query ? query : "", topK, 1, NULL, rerank, min_evidence, explain);
        if (!r) continue;
        /* Salta i brain in ERRORE (es. modello embedding mancante) e quelli senza hits:
         * parità MultiBrainSearch C# (try/catch per brain → skip). Usa il PRIMO brain con hits. */
        if (strstr(r, "\"status\":\"Error\"") != NULL) { free(r); continue; }
        if (strstr(r, "\"hits\":[]") == NULL)
        {
            free(names);
            return r;
        }
        free(r);
    }
    free(names);
    if (missing[0])
    {
        char msg[1024];
        snprintf(msg, sizeof msg,
                 "{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"cervelli non disponibili: %s"
                 " (non installati e non trovati in vendor/exe/brainsDir/cache: usare AI BRAIN INSTALL \\\"%s\\\")\"}",
                 missing, missing);
        return strdup(msg);
    }
    return strdup("{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessun brain interrogabile\"}");
}

/* gmb_ai_sbm_json: output JSON di SEARCHMULTI (brain singolo condiviso "vm"). Fonte unica
 * (§0bis) usata da VM/C e dagli emitter C99/C99Native. Status: Success/Error/InsufficientEvidence. */
char* gmb_ai_sbm_json(const char* query, int topK, int dims, int rerank, double min_evidence, int explain)
{
    /* Parità MultiBrainSearch C#: SEARCHMULTI interroga i brain COLLEGATI (BRAINUSED). Il C ha
     * UN brain condiviso ("vm"); i brain per-nome di brains.json (es. airline/finance) non sono
     * interrogabili dal C. Quindi: nessun brain collegato → "nessun brain collegato (AI BRAIN
     * USE)"; collegati ma nessuno è il condiviso → "nessun brain interrogabile" (prima il C
     * interrogava comunque il brain condiviso e ritornava Success con hits vuoti). */
    {
        char* used = gmb_brain_used_csv();
        if (used)
        {
            int empty = (*used == '\0');
            int has_shared = (strstr(used, "vm") != NULL);
            free(used);
            if (empty)
                return strdup("{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessun brain collegato (AI BRAIN USE)\"}");
            if (!has_shared)
                return strdup("{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessun brain interrogabile\"}");
        }
    }
    char* err = NULL;
    int insuf = 0;
    char* lines = gmb_brain_search_ex(query, topK, dims, rerank, min_evidence, explain, &err, &insuf);
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { free(lines); return NULL; }
    if (err)
    {
        brain_json_reserve(&out, &len, &cap, strlen(err) + 64);
        snprintf(out, cap, "{\"status\":\"Error\",\"hits\":[],\"error\":\"%s\"}", err);
        free(lines);
        return out;
    }
    if (insuf)
    {
        free(lines);
        return out ? strcpy(out, "{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessuna evidenza sufficiente dopo il rerank\"}") : out;
    }
    brain_json_append(&out, &len, &cap, "{\"status\":\"Success\",\"hits\":[");
    int first = 1;
    char* save = NULL;
    for (char* line = lines ? strtok_r(lines, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save))
    {
        if (!*line) continue;
        char* p1 = strchr(line, '|'); if (!p1) continue;
        *p1 = 0; const char* id = line;
        char* p2 = strchr(p1 + 1, '|'); if (!p2) continue;
        *p2 = 0; const char* kind = p1 + 1;
        char* p3 = strchr(p2 + 1, '|'); if (!p3) continue;
        *p3 = 0; const char* score = p2 + 1;
        char* p4 = strchr(p3 + 1, '|'); if (!p4) continue;
        *p4 = 0; const char* ev = p3 + 1;
        const char* text = p4 + 1;
        if (!first) brain_json_append(&out, &len, &cap, ",");
        first = 0;
        char num[160];
        snprintf(num, sizeof(num), "{\"chunkId\":%s,\"sourceId\":", id);
        brain_json_append(&out, &len, &cap, num);
        brain_json_escape(&out, &len, &cap, id);
        brain_json_append(&out, &len, &cap, ",\"text\":");
        brain_json_escape(&out, &len, &cap, text);
        brain_json_append(&out, &len, &cap, ",\"kind\":");
        brain_json_escape(&out, &len, &cap, kind);
        brain_json_append(&out, &len, &cap, ",\"brain\":\"vm\",\"score\":");
        brain_json_append(&out, &len, &cap, score);
        brain_json_append(&out, &len, &cap, ",\"rrf\":0");
        if (rerank == 1)
        {
            brain_json_append(&out, &len, &cap, ",\"evidence\":");
            brain_json_append(&out, &len, &cap, ev);
        }
        brain_json_append(&out, &len, &cap, "}");
    }
    brain_json_append(&out, &len, &cap, "]}");
    free(lines);
    return out;
}

/* gmb_ai_sbm_ctx: contesto text di RAGCONTEXTMULTI/ASKMULTI (brain singolo condiviso).
   Su error/insufficiente ritorna il JSON d'errore (identificabile dal prefisso {"status":). */
char* gmb_ai_sbm_ctx(const char* query, int topK, int dims, int rerank, double min_evidence)
{
    char* err = NULL;
    int insuf = 0;
    char* lines = gmb_brain_search_ex(query, topK, dims, rerank, min_evidence, 0, &err, &insuf);
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { free(lines); return NULL; }
    if (err)
    {
        brain_json_reserve(&out, &len, &cap, strlen(err) + 64);
        snprintf(out, cap, "{\"status\":\"Error\",\"error\":\"%s\"}", err);
        free(lines);
        return out;
    }
    if (insuf)
    {
        free(lines);
        /* Messaggio canonico del pipeline C# (RagAskPipeline.InsufficientMessage): ASKMULTI lo
         * usa come risposta diretta (nessun LLM) — parità IL/PCode. */
        return out ? strcpy(out, "[insufficient evidence] No relevant documents found.") : out;
    }
    char* save = NULL;
    for (char* line = lines ? strtok_r(lines, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save))
    {
        char* p = line;
        int field = 0;
        char* text = line;
        for (; *p; p++) if (*p == '|' && ++field == 4) { text = p + 1; break; }
        size_t need = strlen(text) + 2;
        brain_json_reserve(&out, &len, &cap, need);
        if (len) out[len++] = '\n';
        memcpy(out + len, text, strlen(text)); len += strlen(text);
        out[len] = 0;
    }
    free(lines);
    /* Contesto vuoto = nessun documento rilevante → messaggio canonico (parità IL/PCode). */
    if (len == 0 && out) strcpy(out, "[insufficient evidence] No relevant documents found.");
    return out;
}

int gmb_brain_add_watch(const char* path)
{
    if (!path || !*path) return 0;
    sqlite3* db = brain_open();
    if (!db) return 0;
    int ok = 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO watched(path,last_hash) VALUES(?1,'');", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

char* gmb_brain_watched_csv(void)
{
    sqlite3* db = brain_open();
    if (!db) return strdup("");
    char out[8192];
    out[0] = '\0';
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT path FROM watched;", -1, &st, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* p = (const char*)sqlite3_column_text(st, 0);
            if (strlen(out) + strlen(p ? p : "") + 2 < sizeof(out))
            {
                if (out[0]) strcat(out, "\n");
                strcat(out, p ? p : "");
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return strdup(out);
}

static void brain_hash_file(const char* path, char* out, size_t cap)
{
    FILE* f = fopen(path, "rb");
    if (!f) { out[0] = '\0'; return; }
    char buf[4096];
    size_t n;
    uint32_t h = 2166136261u;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        for (size_t i = 0; i < n; i++) { h ^= (unsigned char)buf[i]; h *= 16777619u; }
    fclose(f);
    snprintf(out, cap, "%08x", h);
}

static const char* brain_last_hash(sqlite3* db, const char* file)
{
    static char buf[64];
    buf[0] = '\0';
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT last_hash FROM watched WHERE path=?1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, file, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* v = (const char*)sqlite3_column_text(st, 0);
            snprintf(buf, sizeof(buf), "%s", v ? v : "");
        }
        sqlite3_finalize(st);
    }
    return buf;
}

static void brain_set_last_hash(sqlite3* db, const char* file, const char* hash)
{
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE watched SET last_hash=?1 WHERE path=?2;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, file, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

int gmb_brain_refresh(int dims)
{
    sqlite3* db = brain_open();
    if (!db) return 0;
    char* watched = gmb_brain_watched_csv();
    int count = 0;
    char* line = watched;
    while (line && *line)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        DIR* d = opendir(line);
        if (d)
        {
            struct dirent* e;
            char full[1200];
            while ((e = readdir(d)))
            {
                if (e->d_name[0] == '.') continue;
                snprintf(full, sizeof(full), "%s/%s", line, e->d_name);
                if (e->d_type == DT_DIR) continue;
                const char* dot = strrchr(e->d_name, '.');
                if (!dot) continue;
                int is_text = (strcasecmp(dot, ".md") == 0 || strcasecmp(dot, ".txt") == 0);
                int is_img = (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 ||
                              strcasecmp(dot, ".jpeg") == 0 || strcasecmp(dot, ".gif") == 0 ||
                              strcasecmp(dot, ".webp") == 0 || strcasecmp(dot, ".bmp") == 0);
                if (!is_text && !is_img) continue;
                char hash[16];
                brain_hash_file(full, hash, sizeof(hash));
                const char* last = brain_last_hash(db, full);
                if (strcmp(hash, last) == 0) continue;
                if (is_text)
                {
                    FILE* f = fopen(full, "rb");
                    if (f)
                    {
                        char* content = (char*)malloc(65536);
                        size_t n = fread(content, 1, 65535, f);
                        fclose(f);
                        if (content) { content[n] = '\0'; if (gmb_brain_add(content, "Note", full, dims) > 0) count++; free(content); }
                    }
                }
                else
                {
                    const char* txt = gmb_ai_ocr_read(full, "Extract all text from this image.", 512);
                    if (txt && txt[0] && txt[0] != '[') { if (gmb_brain_add(txt, "Note", full, dims) > 0) count++; }
                    else continue; /* nessun OCR: non marcare indicizzato */
                }
                brain_set_last_hash(db, full, hash);
            }
            closedir(d);
        }
        line = nl ? nl + 1 : NULL;
    }
    free(watched);
    sqlite3_close(db);
    return count;
}

/* ── MEMORYRANK / MEMORYCONSOLIDATE (port di Basic.AI.Rag.MemoryRanker).
   Score mem0: 0.4*recency + 0.3*frequency + 0.3*importance.
   Format ritorno (parita' SearchRanked): "id|kind|text|score|freq|recency" per riga. ── */
static double brain_importance(const char* text)
{
    if (!text || !*text) return 0.0;
    size_t n = strlen(text);
    double s = fmin(1.0, (double)n / 200.0);
    char low[4096];
    if (n < sizeof(low)) { for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)text[i]); low[n] = 0; }
    else { snprintf(low, sizeof(low), "%s", text); }
    if (strstr(low, "preferisce") || strstr(low, "decisione") ||
        strstr(low, "importante") || strstr(low, "sempre")) s += 0.3;
    return fmin(1.0, s);
}

static long long brain_epoch_ms(const char* created)
{
    /* sqlite datetime('now') -> "YYYY-MM-DD HH:MM:SS" UTC */
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    if (sscanf(created, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &se) < 3) return 0;
    struct tm tm = {0};
    tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
    tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
    return (long long)timegm(&tm) * 1000LL;
}

static int brain_freq_of(sqlite3* db, const char* text)
{
    sqlite3_stmt* st = NULL;
    int n = 1;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM memories WHERE text=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, text, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return n > 0 ? n : 1;
}

typedef struct { long long id; double score; int freq; double rec; char* kind; char* text; } brain_mem_t;

char* gmb_memory_rank(const char* query, int topK)
{
    (void)query;
    if (topK <= 0) topK = 5;
    sqlite3* db = brain_open();
    if (!db) return NULL;
    brain_mem_t rows[512];
    int nrows = 0;
    sqlite3_stmt* st = NULL;
    long long now_ms = (long long)time(NULL) * 1000LL;
    if (sqlite3_prepare_v2(db, "SELECT id, kind, text, created_at FROM memories;", -1, &st, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(st) == SQLITE_ROW && nrows < 512)
        {
            const char* kind = (const char*)sqlite3_column_text(st, 1);
            const char* text = (const char*)sqlite3_column_text(st, 2);
            const char* created = (const char*)sqlite3_column_text(st, 3);
            if (!text) continue;
            brain_mem_t* r = &rows[nrows];
            r->id = sqlite3_column_int64(st, 0);
            r->kind = strdup(kind ? kind : "");
            r->text = strdup(text);
            int freq = brain_freq_of(db, text);
            double days = 0;
            if (created) { long long cr = brain_epoch_ms(created); if (cr > 0) days = (now_ms - cr) / 86400000.0; }
            if (days < 0) days = 0;
            r->freq = freq;
            r->rec = exp(-0.1 * days);
            r->score = 0.4 * r->rec + 0.3 * (double)freq + 0.3 * brain_importance(text);
            nrows++;
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    /* ordina per score desc */
    for (int i = 0; i < nrows; i++)
        for (int j = i + 1; j < nrows; j++)
            if (rows[j].score > rows[i].score) { brain_mem_t t = rows[i]; rows[i] = rows[j]; rows[j] = t; }
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    if (nrows > topK) nrows = topK;
    for (int i = 0; i < nrows; i++)
    {
        /* F4 con virgola it-IT (parità Score.ToString("F4"); stesso fix in gmb_brain_search). */
        char scb[32], rcb[32];
        snprintf(scb, sizeof(scb), "%.4f", rows[i].score);
        snprintf(rcb, sizeof(rcb), "%.4f", rows[i].rec);
        for (char* dp = scb; *dp; dp++) if (*dp == '.') *dp = ',';
        for (char* dp = rcb; *dp; dp++) if (*dp == '.') *dp = ',';
        char line[6144];
        int w = snprintf(line, sizeof(line), "%lld|%s|%s|%s|%d|%s",
            rows[i].id, rows[i].kind ? rows[i].kind : "", rows[i].text ? rows[i].text : "",
            scb, rows[i].freq, rcb);
        if (len + (size_t)w + 2 > cap) { cap = (len + (size_t)w + 2) * 2; char* r = realloc(out, cap); if (!r) break; out = r; }
        memcpy(out + len, line, (size_t)w); len += (size_t)w;
        out[len++] = '\n';
        out[len] = 0;
    }
    if (len > 0 && out[len - 1] == '\n') out[len - 1] = 0;
    for (int i = 0; i < nrows; i++) { free(rows[i].kind); free(rows[i].text); }
    return out;
}

int gmb_memory_consolidate(void)
{
    /* Contratto superficie BASIC (guida AI-Namespace-Manual): ritorna 1 in caso di
     * successo, 0 se il db non è utilizzabile. NON il conteggio dei duplicati
     * (parità C#: Consolidate() poi sempre 1.0; VMC che ignora il ritorno e dà 1.0). */
    sqlite3* db = brain_open();
    if (!db) return 0;
    sqlite3_stmt* st = NULL;
    long long first[512];
    char (*norm1)[4096] = (char (*)[4096])malloc(512 * 4096);
    if (!norm1) { sqlite3_close(db); return 0; }
    int nseen = 0, ok = 0;
    if (sqlite3_prepare_v2(db, "SELECT id, text FROM memories ORDER BY id;", -1, &st, NULL) == SQLITE_OK)
    {
        ok = 1;
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            long long id = sqlite3_column_int64(st, 0);
            const char* text = (const char*)sqlite3_column_text(st, 1);
            if (!text) continue;
            char norm[4096];
            size_t wi = 0, inSp = 0;
            for (size_t i = 0; text[i] && wi + 1 < sizeof(norm); i++)
            {
                char ch = (char)tolower((unsigned char)text[i]);
                if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') { if (!inSp && wi > 0) norm[wi++] = ' '; inSp = 1; }
                else { norm[wi++] = ch; inSp = 0; }
            }
            if (wi > 0 && norm[wi - 1] == ' ') wi--;
            norm[wi] = 0;
            int dup = 0;
            for (int i = 0; i < nseen && !dup; i++)
                if (strcmp(norm1[i], norm) == 0) dup = 1;
            if (dup)
            {
                sqlite3_stmt* dl = NULL;
                if (sqlite3_prepare_v2(db, "DELETE FROM memories WHERE id=$1;", -1, &dl, NULL) == SQLITE_OK)
                {
                    sqlite3_bind_int64(dl, 1, id);
                    sqlite3_step(dl);
                    sqlite3_finalize(dl);
                }
            }
            else if (nseen < 512) { first[nseen] = id; snprintf(norm1[nseen], sizeof(norm1[nseen]), "%s", norm); nseen++; }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    free(norm1);
    return ok;
}
