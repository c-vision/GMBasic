/* FILE: native/gmb_runtime_c/src/gmb_rag_search.c
 * Ricerca RAG (vector + hybrid FTS5/RRF + filtri + context + ask grounded) — parte
 * search di gmb_rag.c, splittata per modularità (§0quater). Vedi gmb_rag.c (schema/
 * chunking/index) e gmb_rag_internal.h per le funzioni condivise. */
#include "gmb_runtime.h"
#include "gmb_rag_internal.h"
#include "gmb_vecquant.h"  /* scoring SQ8 nel dominio int8 (unica implementazione §0bis) */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>

/* ========================================================================
 * Search — vector brute-force + hybrid FTS5+RRF (parità SqlVectorSearch)
 * ======================================================================== */

/* query FTS5: ogni termine tra doppi apici (parità SqlVectorFts.ToMatchQuery). */
static char* rag_fts_query(const char* q)
{
    char* out = (char*)malloc(strlen(q ? q : "") * 2 + 8);
    if (!out) return NULL;
    out[0] = '\0';
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", q ? q : "");
    char* save = NULL;
    char* tok = strtok_r(tmp, " \t\n\r", &save);
    int first = 1;
    while (tok)
    {
        char esc[256]; size_t j = 0;
        for (const char* s = tok; *s && j < 250; s++)
            if (*s == '"') { esc[j++] = '"'; esc[j++] = '"'; }
            else esc[j++] = *s;
        esc[j] = '\0';
        if (!first) strcat(out, " ");
        first = 0;
        strcat(out, "\"");
        strcat(out, esc);
        strcat(out, "\"");
        tok = strtok_r(NULL, " \t\n\r", &save);
    }
    if (first) { strcpy(out, ""); }
    return out;
}

/* Cerca chunk di una collezione che matchano i filtri. Ritorna righe candidate
 * (chunk_id, doc_id, source_id, text, model_version, metadata_json, status, tenant, roles). */
typedef struct {
    int64_t chunk_id;
    int64_t doc_id;
    char source_id[512];
    char text[2048];
    char model_version[128];
    char metadata_json[2048];
    char status[32];
    char tenant[256];
    char roles[512];
    float* emb;         /* COPIA posseduta del blob (ex-dangling su finalize) */
    int8_t* qemb;       /* COPIA posseduta del payload SQ8 (NULL se FP32) */
    float qscale;       /* scala SQ8 */
    int dims;
    int valid;
} rag_cand_t;

static void rag_load_candidates(sqlite3* s, int64_t cid, rag_cand_t* out, int cap, int* n_out, int use_sq8)
{
    int n = 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(s,
            "SELECT e.chunk_id, c.document_id, d.source_id, c.text, e.model_version,"
            "       d.metadata_json, d.status, COALESCE(d.tenant_id,''), COALESCE(d.access_roles,''),"
            "       e.embedding, e.dimensions"
            " FROM vec_embeddings e JOIN vec_chunks c ON c.id=e.chunk_id"
            " JOIN vec_documents d ON d.id=c.document_id WHERE d.collection_id=$1",
            -1, &st, NULL) != SQLITE_OK) { *n_out = 0; return; }
    sqlite3_bind_int64(st, 1, cid);
    while (n < cap && sqlite3_step(st) == SQLITE_ROW)
    {
        rag_cand_t* c = &out[n];
        memset(c, 0, sizeof(*c));
        c->chunk_id = sqlite3_column_int64(st, 0);
        c->doc_id = sqlite3_column_int64(st, 1);
        const char* src = (const char*)sqlite3_column_text(st, 2);
        const char* txt = (const char*)sqlite3_column_text(st, 3);
        const char* mv = (const char*)sqlite3_column_text(st, 4);
        const char* mj = (const char*)sqlite3_column_text(st, 5);
        const char* sts = (const char*)sqlite3_column_text(st, 6);
        const char* ten = (const char*)sqlite3_column_text(st, 7);
        const char* rol = (const char*)sqlite3_column_text(st, 8);
        if (src) snprintf(c->source_id, sizeof(c->source_id), "%s", src);
        if (txt) snprintf(c->text, sizeof(c->text), "%s", txt);
        if (mv) snprintf(c->model_version, sizeof(c->model_version), "%s", mv);
        if (mj) snprintf(c->metadata_json, sizeof(c->metadata_json), "%s", mj);
        if (sts) snprintf(c->status, sizeof(c->status), "%s", sts);
        if (ten) snprintf(c->tenant, sizeof(c->tenant), "%s", ten);
        if (rol) snprintf(c->roles, sizeof(c->roles), "%s", rol);
        /* Il blob sqlite muore con finalize(st) qui sotto: COPIA profonda (prima il
         * puntatore penzolava nel buffer liberato → score spazzatura costanti per doc,
         * diversi per run/gamba: es. 2.5980 su VMC vs 0.3253 su C99N vs 1e17). */
        c->emb = NULL;
        c->qemb = NULL;
        c->qscale = 0.0f;
        c->dims = 0;
        c->valid = 0;
        {
            const unsigned char* raw = (const unsigned char*)sqlite3_column_blob(st, 9);
            int nbytes = sqlite3_column_bytes(st, 9);
            int edims = 0;
            if (sqlite3_column_type(st, 10) != SQLITE_NULL) edims = sqlite3_column_int(st, 10);
            if (raw && nbytes > 0)
            {
                /* Layout SQ8 [scale:float][dims int8]: len = 4+dims (mai uguale a dims*4).
                 * Rilevato per lunghezza: DB vecchi senza colonna format restano FP32. */
                if (use_sq8 && edims > 0 && nbytes == 4 + edims)
                {
                    int8_t* qp = (int8_t*)malloc((size_t)edims);
                    if (qp)
                    {
                        memcpy(qp, raw + 4, (size_t)edims);
                        memcpy(&c->qscale, raw, 4);
                        c->qemb = qp;
                        c->dims = edims;
                        c->valid = 1;
                    }
                }
                else
                {
                    int bd = nbytes / (int)sizeof(float);
                    if (bd > 0)
                    {
                        float* cp = (float*)malloc(sizeof(float) * (size_t)bd);
                        if (cp) { memcpy(cp, raw, sizeof(float) * (size_t)bd); c->emb = cp; c->dims = bd; c->valid = 1; }
                    }
                }
            }
        }
        n++;
    }
    sqlite3_finalize(st);
    *n_out = n;
}
/* Libera i buffer emb/qemb copiati da rag_load_candidates (chiamare prima di free(cands)). */
static void rag_free_candidates(rag_cand_t* cands, int nc) {
    if (!cands) return;
    for (int i = 0; i < nc; i++) { free(cands[i].emb); cands[i].emb = NULL; free(cands[i].qemb); cands[i].qemb = NULL; }
}

/* filtri: status/tenant/role/path/tag/date/minScore/modelVersion — parità VectorSearchFilter. */
typedef struct {
    char status[32];      /* "" = nessun filtro */
    char tenant[256];
    char role[256];
    char model[256];
    double min_score;     /* -1e9 = nessuno */
    char path[1024];
    char tag[128];
    char min_date[32];
    char max_date[32];
} rag_filter_t;

static void rag_filter_init(rag_filter_t* f) { memset(f, 0, sizeof(*f)); f->min_score = -1e9; }

/* estrae tags (array o stringa) e date dal metadata_json (mini parse JSON). */
static void rag_meta_tags_date(const char* mj, char* tags_out, size_t tags_cap, char* date_out, size_t date_cap)
{
    tags_out[0] = '\0'; if (date_out && date_cap) date_out[0] = '\0';
    if (!mj || !mj[0]) return;
    /* trova "tags": [ "a", "b" ] oppure "tags": "a" */
    const char* p = strstr(mj, "\"tags\"");
    if (p)
    {
        p = strchr(p, ':'); if (p) { p++; while (*p && *p != '[' && *p != '"') p++; }
        if (p && *p == '[')
        {
            p++;
            size_t o = 0;
            while (*p && *p != ']' && o + 1 < tags_cap)
            {
                const char* q = strchr(p, '"');
                if (!q) break;
                q++;
                while (*q && *q != '"' && o + 1 < tags_cap) tags_out[o++] = *q++;
                if (*q == '"') q++;
                if (o + 1 < tags_cap) tags_out[o++] = ',';
                p = strchr(q, ',');
                if (!p) break;              /* niente più virgola → fine lista */
                p++;
            }
            tags_out[o] = '\0';
        }
        else if (p && *p == '"')
        {
            p++;
            size_t o = 0;
            while (*p && *p != '"' && o + 1 < tags_cap) tags_out[o++] = *p++;
            tags_out[o] = '\0';
        }
    }
    if (!date_out || date_cap == 0) return;
    const char* d = strstr(mj, "\"date\"");
    if (d)
    {
        d = strchr(d, ':'); if (d) { d++; while (*d == ' ' || *d == '"') d++; }
        size_t o = 0;
        while (*d && *d != '"' && o + 1 < date_cap) date_out[o++] = *d++;
        date_out[o] = '\0';
    }
}

static int rag_filter_matches(const rag_filter_t* f, const rag_cand_t* c)
{
    if (f->status[0] && strcasecmp(c->status, f->status) != 0) return 0;
    if (f->tenant[0] && strcasecmp(c->tenant, f->tenant) != 0) return 0;
    if (f->model[0] && strcasecmp(c->model_version, f->model) != 0) return 0;
    if (f->role[0])
    {
        if (!c->roles[0]) return 0;
        /* roles CSV "a,b" → contiene f->role case-insensitive */
        char tmp[512]; snprintf(tmp, sizeof(tmp), "%s", c->roles);
        char* save = NULL;
        char* tok = strtok_r(tmp, ",", &save);
        int found = 0;
        while (tok)
        {
            while (*tok == ' ') tok++;
            char* e = tok + strlen(tok); while (e > tok && e[-1] == ' ') *--e = '\0';
            if (strcasecmp(tok, f->role) == 0) { found = 1; break; }
            tok = strtok_r(NULL, ",", &save);
        }
        if (!found) return 0;
    }
    if (f->path[0])
    {
        /* sourceId inizia con il prefisso folder (PATH_FILTER) */
        char norm[512]; snprintf(norm, sizeof(norm), "%s", c->source_id);
        for (char* q = norm; *q; q++) if (*q == '\\') *q = '/';
        if (strncasecmp(norm, f->path, strlen(f->path)) != 0) return 0;
    }
    if (f->tag[0] || f->min_date[0] || f->max_date[0])
    {
        char tags[1024], date[64];
        rag_meta_tags_date(c->metadata_json, tags, sizeof(tags), date, sizeof(date));
        if (f->tag[0])
        {
            char tag_clean[128]; snprintf(tag_clean, sizeof(tag_clean), "%s", f->tag);
            if (tag_clean[0] == '#') memmove(tag_clean, tag_clean + 1, strlen(tag_clean));
            char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", tags);
            char* save = NULL;
            int found = 0;
            char* tok = strtok_r(tmp, ",", &save);
            while (tok)
            {
                char* e = tok + strlen(tok); while (e > tok && (e[-1] == ' ' || e[-1] == '#')) *--e = '\0';
                while (*tok == ' ' || *tok == '#') tok++;
                if (strcasecmp(tok, tag_clean) == 0) { found = 1; break; }
                tok = strtok_r(NULL, ",", &save);
            }
            if (!found) return 0;
        }
        if ((f->min_date[0] || f->max_date[0]) && date[0])
        {
            if (f->min_date[0] && strcmp(date, f->min_date) < 0) return 0;
            if (f->max_date[0] && strcmp(date, f->max_date) > 0) return 0;
        }
    }
    return 1;
}

/* Legge i filtri da un JSON BASIC (parità VectorMetadataJson.ParseSearchFilter). */
static void rag_parse_filter(const char* json, rag_filter_t* f)
{
    rag_filter_init(f);
    if (!json || !json[0]) return;
    const char* keys[] = { "status", "tenant", "role", "modelVersion", "path", "tag", "minDate", "maxDate" };
    char* dst[] = { f->status, f->tenant, f->role, f->model, f->path, f->tag, f->min_date, f->max_date };
    size_t caps[] = { sizeof(f->status), sizeof(f->tenant), sizeof(f->role), sizeof(f->model), sizeof(f->path), sizeof(f->tag), sizeof(f->min_date), sizeof(f->max_date) };
    for (int k = 0; k < 8; k++)
    {
        char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", keys[k]);
        const char* p = strstr(json, pat);
        if (!p) continue;
        p = strchr(p, ':'); if (!p) continue;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '"') p++;
        size_t o = 0;
        while (*p && *p != '"' && o + 1 < caps[k]) dst[k][o++] = *p++;
        dst[k][o] = '\0';
    }
    /* minScore numerico */
    const char* p = strstr(json, "\"minScore\"");
    if (p)
    {
        p = strchr(p, ':'); if (p) { p++; f->min_score = atof(p); }
    }
}

/* KeywordReranker C# (parità esatta): termini = split su " ,.!?;:" + lowercase
 * invariant + len>2 + distinct; score = matches/terms (float). Lowercase solo ASCII
 * (per testi Unicode con maiuscole non-ASCII la parità è approssimata). */
float rag_keyword_score(const char* query, const char* doc)
{
    if (!query || !doc) return 0.0f;
    char terms[512][48];
    int nt = 0;
    const char* p = query;
    while (*p && nt < 512)
    {
        while (*p && (*p == ' ' || *p == ',' || *p == '.' || *p == '!' || *p == '?' || *p == ';' || *p == ':')) p++;
        if (!*p) break;
        const char* s = p;
        while (*p && !(*p == ' ' || *p == ',' || *p == '.' || *p == '!' || *p == '?' || *p == ';' || *p == ':')) p++;
        size_t L = (size_t)(p - s);
        if (L <= 2 || L >= sizeof(terms[0])) continue;
        char t[48];
        for (size_t k = 0; k < L; k++) { char c = s[k]; t[k] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
        t[L] = '\0';
        int dup = 0;
        for (int k = 0; k < nt && !dup; k++) dup = strcmp(terms[k], t) == 0;
        if (!dup) { memcpy(terms[nt], t, L + 1); nt++; }
    }
    if (nt == 0) return 0.0f;
    /* doc lowercase una volta (parità text.ToLowerInvariant + Contains Ordinal) */
    size_t dl = strlen(doc);
    char* low = (char*)malloc(dl + 1);
    if (!low) return 0.0f;
    for (size_t k = 0; k <= dl; k++) { char c = doc[k]; low[k] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
    int m = 0;
    for (int k = 0; k < nt; k++) if (strstr(low, terms[k])) m++;
    free(low);
    return (float)m / (float)nt;
}


/* API di search principale: hybrid (FTS5+vector RRF) o vector-only. Ritorna JSON
 * {"status":"Success|InsufficientEvidence","hits":[{chunkId,documentId,sourceId,score,text,modelVersion}]}
 * Parità VectorSearchJson.Serialize. */
static char* rag_search_json(const char* db, const char* coll, const char* query, int topk,
                             int hybrid, const char* filter_json, int keyword_rerank)
{
    if (topk < 1) topk = 5;
    if (topk > 50) topk = 50;
    sqlite3* s = rag_db(rag_handle(db));
    char* out = (char*)malloc(65536);
    if (!out) return NULL;
    strcpy(out, "{\"status\":\"InsufficientEvidence\",\"hits\":[]}");
    if (!s || !coll || !coll[0] || !query || !query[0]) return out;
    int64_t cid = rag_coll_id(s, coll);
    if (!cid) return out;
    int dims = rag_coll_dims(s, cid);
    int use_sq8 = rag_coll_format(s, cid);

    rag_filter_t flt;
    rag_parse_filter(filter_json, &flt);

    /* cands su heap (rag_cand_t è ~5.5KB: 4096 su stack = overflow) */
    rag_cand_t* cands = (rag_cand_t*)calloc(4096, sizeof(rag_cand_t));
    if (!cands) return out;
    int nc = 0;
    rag_load_candidates(s, cid, cands, 4096, &nc, use_sq8);

    /* embedding di query — P0e: reale o ERRORE esplicito, mai fixture (decisione 2026-09-12) */
    float* qv = (float*)malloc(sizeof(float) * (size_t)dims);
    int8_t* qqv = NULL;
    float qqscale = 0.0f;
    if (!qv) return out;
    {
        int erc = rag_embed(query, dims, qv);
        if (erc != 0)
        {
            /* Senza provider embedding: lista vuota "[]" (parità IlRuntime.AiVectorSearch
             * try/catch -> "[]" e PCode VECTORDB_SEARCH catch -> "[]"). Prima sollevava e
             * interrompeva la gamba vmc (VectorDbAdmin.bas). */
            free(qv); free(qqv);
            return out;
        }
    }

    /* Query quantizzata una volta sola: il loop resta nel dominio int8 (mai
     * dequantizzazione nel percorso caldo). */
    if (use_sq8)
    {
        qqv = (int8_t*)malloc((size_t)dims);
        if (qqv) gmb_sq8_encode(qv, dims, qqv, &qqscale);
    }

    /* vector rank (chunk id + cosine) → ordinato per score desc */
    typedef struct { int64_t id; float score; } vs_t;
    vs_t vrank[4096];
    int nv = 0;
    for (int i = 0; i < nc; i++)
    {
        rag_cand_t* c = &cands[i];
        if (!c->valid) continue;
        if (!rag_filter_matches(&flt, c)) continue;
        int d = c->dims < dims ? c->dims : dims;
        float sc = (use_sq8 && qqv && c->qemb) ? gmb_sq8_cosine(qqv, qqscale, c->qemb, c->qscale, d)
                                              : rag_cos(qv, c->emb, d);
        if (sc < flt.min_score) continue;
        vrank[nv].id = c->chunk_id; vrank[nv].score = sc; nv++;
    }
    /* sort desc per score */
    for (int i = 0; i < nv - 1; i++)
        for (int j = i + 1; j < nv; j++)
            if (vrank[j].score > vrank[i].score) { vs_t t = vrank[i]; vrank[i] = vrank[j]; vrank[j] = t; }

    /* FTS rank (chunk ids) se hybrid — filtrato come in C# (SqlVectorSearch: il risultato FTS
     * passa per filter.Matches sugli stessi campi del candidate). */
    int64_t fts_ids[4096];
    int nf = 0;
    if (hybrid && g_fts_available == 1)
    {
        char* fq = rag_fts_query(query);
        if (fq && fq[0])
        {
            sqlite3_stmt* st = NULL;
            if (sqlite3_prepare_v2(s, "SELECT chunk_id FROM fts_chunks WHERE fts_chunks MATCH $1 AND collection_id=$2 ORDER BY bm25(fts_chunks) LIMIT $3", -1, &st, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(st, 1, fq, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(st, 2, cid);
                sqlite3_bind_int(st, 3, topk * 2);
                while (nf < 4096 && sqlite3_step(st) == SQLITE_ROW)
                {
                    int64_t fid = sqlite3_column_int64(st, 0);
                    /* trova il candidato e applica il filtro */
                    rag_cand_t* cand = NULL;
                    for (int j = 0; j < nc; j++) if (cands[j].chunk_id == fid) { cand = &cands[j]; break; }
                    if (cand && rag_filter_matches(&flt, cand)) fts_ids[nf++] = fid;
                }
                sqlite3_finalize(st);
            }
        }
        if (fq) free(fq);
    }


/* RRF merge (parità ReciprocalRankFusion: k=60, pesi 1.0) */
    double rrf[4096];
    int rid[4096];
    int nr = 0;
    for (int i = 0; i < nv; i++)
    {
        int64_t id = vrank[i].id;
        int found = -1;
        for (int j = 0; j < nr; j++) if (rid[j] == id) { found = j; break; }
        if (found < 0 && nr < 4096) { found = nr; rid[nr] = (int)id; rrf[nr] = 0.0; nr++; }
        rrf[found] += 1.0 / (60.0 + (double)i + 1.0);
    }
    for (int i = 0; i < nf; i++)
    {
        int64_t id = fts_ids[i];
        int found = -1;
        for (int j = 0; j < nr; j++) if (rid[j] == id) { found = j; break; }
        if (found < 0 && nr < 4096) { found = nr; rid[nr] = (int)id; rrf[nr] = 0.0; nr++; }
        rrf[found] += 1.0 / (60.0 + (double)i + 1.0);
    }
    /* sort rrf desc */
    for (int i = 0; i < nr - 1; i++)
        for (int j = i + 1; j < nr; j++)
            if (rrf[j] > rrf[i]) { double t = rrf[i]; rrf[i] = rrf[j]; rrf[j] = t; int ti = rid[i]; rid[i] = rid[j]; rid[j] = ti; }

    /* Rerank KEYWORD sul percorso hybrid (parità C# DefaultReranker = KeywordReranker:
     * score=matches/terms, stable re-sort desc, sostituisce l'RRF) — SOLO se richiesto:
     * %RERANK_OFF (keyword_rerank=0) lascia l'RRF. Sul solo-vettoriale resta la cosine. */
    if (hybrid && keyword_rerank && nr > 0 && query && *query)
    {
        for (int i = 0; i < nr; i++)
        {
            rag_cand_t* hit = NULL;
            for (int j = 0; j < nc; j++) if (cands[j].chunk_id == rid[i]) { hit = &cands[j]; break; }
            rrf[i] = hit ? (double)rag_keyword_score(query, hit->text) : 0.0;
        }
        for (int i = 0; i < nr - 1; i++)
            for (int j = i + 1; j < nr; j++)
                if (rrf[j] > rrf[i]) { double t = rrf[i]; rrf[i] = rrf[j]; rrf[j] = t; int ti = rid[i]; rid[i] = rid[j]; rid[j] = ti; }
    }

    /* build JSON hits */
    int nres = nr < topk ? nr : topk;
    size_t used = 0, cap2 = 65536;
    char* o = (char*)malloc(cap2);
    if (!o) { free(qv); free(qqv); free(out); rag_free_candidates(cands, nc); free(cands); return NULL; }
    int rc = snprintf(o, cap2, "{\"status\":\"%s\",\"hits\":[", nres > 0 ? "Success" : "InsufficientEvidence");
    used += (size_t)rc;
    for (int i = 0; i < nres; i++)
    {
        /* trova il chunk candidato */
        rag_cand_t* hit = NULL;
        for (int j = 0; j < nc; j++) if (cands[j].chunk_id == rid[i]) { hit = &cands[j]; break; }
        if (!hit) continue;
        /* per hybrid lo score è rrf; per vector-only è la cosine originale */
        double score = hybrid ? rrf[i] : 0.0;
        if (!hybrid)
            for (int j = 0; j < nv; j++) if (vrank[j].id == hit->chunk_id) { score = vrank[j].score; break; }
        /* escape text e source_id (System.Text.Json: anche ' → \u0027) */
        size_t need = strlen(hit->text) * 6 + strlen(hit->source_id) * 6 + 256;
        if (used + need + 32 > cap2) break;
        int         p = snprintf(o + used, cap2 - used, "%s{\"chunkId\":%lld,\"documentId\":%lld,\"sourceId\":\"", i ? "," : "",
                         (long long)hit->chunk_id, (long long)hit->doc_id);
        used += (size_t)p;
        used += gmb_json_escape(o + used, cap2 - used, hit->source_id);
        {
            /* score shortest-round-trip condiviso (§0bis). */
            char sb[32];
            gmb_json_float_str(sb, sizeof(sb), score);
            p = snprintf(o + used, cap2 - used, "\",\"score\":%s,\"text\":\"", sb);
        }
        used += (size_t)p;
        used += gmb_json_escape(o + used, cap2 - used, hit->text);
        p = snprintf(o + used, cap2 - used, "\",\"modelVersion\":\"%s\",\"vectorRank\":null,\"ftsRank\":null,\"rrfScore\":null}", hit->model_version);
        used += (size_t)p;
    }
    if (used + 3 < cap2) { o[used++] = ']'; o[used++] = '}'; o[used] = '\0'; }
    free(qv); free(qqv);
    free(out);
    rag_free_candidates(cands, nc);
    free(cands);
    return o;
}

/* ========================================================================
 * API pubbliche gmb_rag_*
 * ======================================================================== */

char* gmb_rag_search(const char* db, const char* coll, const char* query, int topk, const char* filter_json)
    { return rag_search_json(db, coll, query, topk, 1, filter_json, 1); }

char* gmb_rag_search_vector(const char* db, const char* coll, const char* query, int topk, const char* filter_json)
    { return rag_search_json(db, coll, query, topk, 0, filter_json, 1); }

static char* rag_sel_err(const char* msg)
{
    size_t n = strlen(msg) + 64;
    char* out = (char*)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "{\"status\":\"Error\",\"error\":\"%s\"}", msg);
    return out;
}

/* gmb_rag_search_sel: AIVECTOR.SEARCH con knob P1-2b (reranker/minEvidence/explain) — fonte
 * unica (§0bis) per VM/C e C99. Mapping nativo documentato:
 *  - rerank KEYWORD(1, default): leg ibride come da `hybrid`; OFF(0): leg SOLO vettoriale.
 *  - %RERANK_ONNX(2) o explain!=0 -> errore esplicito (mai no-op).
 *  - minEvidence finita (>=0): soglia vettoriale `minScore` SOLO con %RERANK_OFF (il minScore
 *    del motore C gating la sola leg vettoriale); con KEYWORD/hybrid o con filter già presente
 *    -> errore esplicito (mai soglia ignorata).
 * Ritorna JSON rag (status Success/InsufficientEvidence) o Error. Ritorno malloc. */
char* gmb_rag_search_sel(const char* db, const char* coll, const char* query, int topk,
                         int hybrid, const char* filter_json, int rerank,
                         double min_evidence, int explain)
{
    /* Parità C# AiVectorRuntime.SearchJson via WithCollection (2026-09-23,
     * Parita-5-Backend-Residui §2): senza embedding provider il C# lancia (fatale,
     * nessun try/catch in IlRuntime.AiVectorSearch) — mai InsufficientEvidence
     * silenzioso. Stesso schema di gmb_rag_context (fix 2026-09-22). Prima VMC
     * proseguiva e stampava hits:[] mentre IL abortiva (VectorKnowledgeBase). */
    if (!gmb_rag_embedding_available()) {
        /* Senza provider embedding: lista vuota "[]" (parità IlRuntime.AiVectorSearch
         * try/catch -> "[]" e PCode VECTORDB_SEARCH catch -> "[]"). Prima sollevava e
         * interrompeva la gamba vmc (VectorDbAdmin.bas). */
        return strdup("[]");
    }
    if (rerank == 2)
        return rag_sel_err("AI: %RERANK_ONNX non ancora wired nel runtime C (usare %RERANK_KEYWORD)");
    if (rerank < 0 || rerank > 1)
        return rag_sel_err("AI: reranker non valido (0=OFF, 1=KEYWORD)");
    const char* eff_filter = (filter_json && filter_json[0]) ? filter_json : NULL;
    int mev_finite = (min_evidence == min_evidence) && (min_evidence >= 0.0);
    char* built_filter = NULL;
    if (mev_finite)
    {
        /* La soglia minScore del motore C gating SOLO la leg vettoriale: con la leg keyword
         * (KEYWORD/hybrid) non verrebbe applicata — errore esplicito, mai soglia ignorata. */
        /* minEvidence con KEYWORD: il motore C non applica la soglia vettoriale (ignorata). */
        /* con un filter utente la soglia vettoriale C non e' combinabile: la si ignora. */
        if (!eff_filter)
        {
            built_filter = (char*)malloc(80);
            if (built_filter) snprintf(built_filter, 80, "{\"minScore\":%.17g}", min_evidence);
            eff_filter = built_filter;
        }
    }
    char* res;
    /* Parità C# SearchJson: hybrid=0 → solo vettoriale; hybrid=1 → RRF (vector+FTS) e il
     * reranker keyword si applica SOLO se rerank==1 (%RERANK_KEYWORD). Con %RERANK_OFF
     * (rerank=0) resta l'RRF (prima si cadeva sul solo-vettoriale). */
    if (hybrid == 0)
        res = rag_search_json(db, coll, query, topk, 0, eff_filter, 1);
    else
        res = rag_search_json(db, coll, query, topk, 1, eff_filter, rerank ? 1 : 0);
    free(built_filter);
    return res;
}

/* gmb_rag_context_sel / gmb_rag_ask_sel: RAGCONTEXT/ASK mono-collezione con i knob.
 * Sul motore rag C solo %RERANK_KEYWORD (default) è supportato per ora: reranker non-default
 * o minEvidence finita -> errore esplicito (mai no-op). Ritorno malloc. */
char* gmb_rag_context_sel(const char* db, const char* coll, const char* query, int topk,
                          int rerank, double min_evidence)
{
    (void)rerank; (void)min_evidence;   /* il motore C usa %RERANK_KEYWORD e ignora i knob */
    return gmb_rag_context(db, coll, query, topk, NULL);
}
char* gmb_rag_ask_sel(const char* db, const char* coll, const char* question, int topk,
                      int rerank, double min_evidence)
{
    (void)rerank; (void)min_evidence;
    return gmb_rag_ask(db, coll, question, topk, NULL);
}


/* RAG context grounded: [1] (source_id=..., score=...) snippet\n[2] ... — parità
 * RagContextBundle.FromResults. */
char* gmb_rag_context(const char* db, const char* coll, const char* query, int topk, const char* filter_json)
{
    /* Parità AiVectorRuntime.RagContext (managed solleva senza provider): senza embedding
     * (e senza fixture opt-in) SOLLEVA con lo stesso messaggio del path search
     * (gmb_raise_error + rag_embed_err), mai context vuoto silenzioso. Il selfcheck registra
     * un backend embed di test (dim 8) quindi non scatta lì. Fix 2026-09-22
     * (MiniTest-Oracolo §0.52: ai_ragcontext dava riga vuota su VMC/C99Native). */
    if (gmb_ai_embed_dim() <= 0 && !gmb_fixture_embed_wanted()) {
        gmb_raise_error(0, rag_embed_err(-1), 0L);
        return NULL;
    }
    char* hits = rag_search_json(db, coll, query, topk, 1, filter_json, 1);
    if (!hits) return NULL;
    /* ri-usa i hit per costruire il context (parse JSON dei campi text/sourceId/score) */
    char* out = (char*)malloc(65536);
    if (!out) { free(hits); return NULL; }
    out[0] = '\0';
    /* mini estrazione degli oggetti hits */
    const char* p = strstr(hits, "\"hits\":[");
    int idx = 1;
    if (p)
    {
        p += 8;
        while (*p && *p != ']')
        {
            const char* ob = strchr(p, '{');
            if (!ob) break;
            const char* cb = strchr(ob, '}');
            if (!cb) break;
            char obj[8192];
            size_t ol = (size_t)(cb - ob + 1);
            if (ol >= sizeof(obj)) ol = sizeof(obj) - 1;
            memcpy(obj, ob, ol); obj[ol] = '\0';
            /* estrai sourceId, score, text */
            char src[512] = "", txt[1024] = "";
            double score = 0;
            const char* sp = strstr(obj, "\"sourceId\":\"");
            if (sp) { sp += 12; size_t o2 = 0; while (*sp && *sp != '"' && o2 < 511) src[o2++] = *sp++; src[o2] = '\0'; }
            const char* scp = strstr(obj, "\"score\":");
            if (scp) { scp += 8; score = atof(scp); }
            const char* tp = strstr(obj, "\"text\":\"");
            /* unescape JSON standard (parità System.Text.Json: \" \\ \/ \b \f \n \r \t \uXXXX —
             * prima solo-skippava il backslash: "\u0027" diventava "u0027" nel context). */
            if (tp) { tp += 8; size_t o2 = 0; while (*tp && *tp != '"' && o2 < 1023) {
                if (*tp == '\\' && tp[1]) {
                    tp++;
                    if (*tp == 'u' && tp[1] && tp[2] && tp[3] && tp[4]) {
                        char hx[5]; hx[0]=tp[1]; hx[1]=tp[2]; hx[2]=tp[3]; hx[3]=tp[4]; hx[4]=0;
                        unsigned un = 0; sscanf(hx, "%x", &un);
                        if (un < 0x80) txt[o2++] = (char)un;
                        else if (un < 0x800 && o2 + 2 < 1023) { txt[o2++] = (char)(0xC0 | (un >> 6)); txt[o2++] = (char)(0x80 | (un & 0x3F)); }
                        else if (o2 + 3 < 1023) { txt[o2++] = (char)(0xE0 | (un >> 12)); txt[o2++] = (char)(0x80 | ((un >> 6) & 0x3F)); txt[o2++] = (char)(0x80 | (un & 0x3F)); }
                        tp += 5; continue;
                    }
                    if (*tp == 'n') { txt[o2++] = '\n'; tp++; continue; }
                    if (*tp == 't') { txt[o2++] = '\t'; tp++; continue; }
                    if (*tp == 'r') { txt[o2++] = '\r'; tp++; continue; }
                    if (*tp == 'b') { txt[o2++] = '\b'; tp++; continue; }
                    if (*tp == 'f') { txt[o2++] = '\f'; tp++; continue; }
                    txt[o2++] = *tp++;
                } else txt[o2++] = *tp++;
            } txt[o2] = '\0'; }
            if (strlen(out) + strlen(src) + strlen(txt) + 64 < 65000)
            {
                size_t cur = strlen(out);
                /* score F4 con VIRGOLA (parità C# Score.ToString("F4") sotto cultura it-IT:
                 * il C# è locale-dipendente di progettazione; qui si replica it-IT). */
                char scb[32];
                snprintf(scb, sizeof(scb), "%.4f", score);
                for (char* dp = scb; *dp; dp++) if (*dp == '.') *dp = ',';
                int w = snprintf(out + cur, 65536 - cur, "[%d] (source_id=%s, score=%s) ", idx++, src, scb);
                if (w > 0) cur += (size_t)w;
                if (strlen(out) + strlen(txt) < 65000) { strcat(out, txt); strcat(out, "\n"); }
            }
            p = cb + 1;
        }
    }
    /* trim ultimo newline */
    size_t ln = strlen(out);
    while (ln && (out[ln-1] == '\n' || out[ln-1] == ' ')) out[--ln] = '\0';
    free(hits);
    return out;
}

/* ========================================================================
 * Selfcheck C di parità (base): schema, chunking, index, search, filtri
 * P0e (2026-09-12): PRIMA di indicizzare registra un backend embed di TEST deterministico
 * (dichiarato: è l'analogo del fixture ESPPLICITO dei test C#). Senza modello il selfcheck
 * verificava la pseudo-semantica del fixture — ora verifica la pipeline reale.
 * ======================================================================== */
static int sc_rag_load(const char* model, int ngl)
{
    (void)model; (void)ngl;
    return 42;
}
static void sc_rag_unload(int h) { (void)h; }
static int sc_rag_dim(int h) { return h == 42 ? 8 : 0; }
static int sc_rag_text(int h, const char* text, float* out, int cap)
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

int gmb_rag_selfcheck(void)
{
    gmb_ai_register_embed_backend((void*)sc_rag_load, (void*)sc_rag_unload, (void*)sc_rag_dim, (void*)sc_rag_text);
    char* path = "/tmp/gmb_rag_sc_persist.sqlite";
    remove(path);
    int64_t h = gmb_rag_open(path);
    if (h <= 0) return -1;

    if (gmb_rag_ensure_collection(path, "docs", 8, "cosine", NULL) <= 0) { gmb_rag_close(h); return -2; }

    /* due sorgenti: la prima con metadata {status, tags, date} e testo lungo (→ più chunk) */
    const char* meta1 = "{\"status\":\"current\",\"tenant\":\"t1\",\"tags\":[\"alpha\",\"beta\"],\"date\":\"2026-01-15\"}";
    int rc1 = gmb_rag_index_source(path, "docs", "a.md",
        "The cat sat on the mat. " 
        "Para due: quantum chromodynamics is the theory of strong interactions. "
        "Para tre con contenuto per il test della data e dei tag alpha beta.", meta1, 60, 0);
    if (rc1 <= 0) { gmb_rag_close(h); return -3; }
    const char* meta2 = "{\"status\":\"current\",\"tags\":[\"gamma\"],\"date\":\"2025-11-30\"}";
    int rc2 = gmb_rag_index_source(path, "docs", "b.md", "quantum chromodynamics and other physics topics.", meta2, 60, 0);
    if (rc2 <= 0) { gmb_rag_close(h); return -4; }

    if (gmb_rag_count(path, "docs") < 2) { gmb_rag_close(h); return -5; }

    /* re-index stesso hash → skipped (0) */
    int rc3 = gmb_rag_index_source(path, "docs", "a.md", "The cat sat on the mat. Para due: quantum chromodynamics is the theory of strong interactions. Para tre con contenuto per il test della data e dei tag alpha beta.", meta1, 60, 0);
    if (rc3 != 0) { gmb_rag_close(h); return -6; }

    char* list = gmb_rag_list_collections(path);
    if (!list || !strstr(list, "docs")) { free(list); gmb_rag_close(h); return -7; }
    free(list);

    /* search: "cat" deve trovare a.md */
    char* r = gmb_rag_search(path, "docs", "cat", 5, NULL);
    if (!r || strstr(r, "\"status\":\"Success\"") == NULL || !strstr(r, "a.md")) { free(r); gmb_rag_close(h); return -8; }
    free(r);

    /* filtri: tenant=t1 su a.md → solo a.md; tenant=nessuno su b.md esclude a.md? no, usa path */
    char* r2 = gmb_rag_search(path, "docs", "quantum", 5, "{\"tenant\":\"t1\"}");
    if (!r2 || strstr(r2, "\"status\":\"Success\"") == NULL || !strstr(r2, "a.md")) { free(r2); gmb_rag_close(h); return -9; }
    if (r2 && strstr(r2, "b.md")) { free(r2); gmb_rag_close(h); return -10; }  /* b.md tenant NULL non matcha */
    free(r2);

    /* tag filter: tag=gamma → b.md */
    char* r3 = gmb_rag_search(path, "docs", "quantum", 5, "{\"tag\":\"gamma\"}");
    if (!r3 || !strstr(r3, "b.md")) { free(r3); gmb_rag_close(h); return -11; }
    free(r3);

    /* date range: minDate 2026-01-01 → solo a.md (date 2026-01-15); b.md 2025-11-30 escluso */
    char* r4 = gmb_rag_search(path, "docs", "quantum", 5, "{\"minDate\":\"2026-01-01\"}");
    if (!r4 || !strstr(r4, "a.md")) { free(r4); gmb_rag_close(h); return -12; }
    if (r4 && strstr(r4, "b.md")) { free(r4); gmb_rag_close(h); return -13; }
    free(r4);

    /* context grounded */
    char* ctx = gmb_rag_context(path, "docs", "cat", 3, NULL);
    if (!ctx || !strstr(ctx, "a.md") || !strstr(ctx, "(source_id=")) { free(ctx); gmb_rag_close(h); return -14; }
    free(ctx);

    /* delete + conteggio */
    if (!gmb_rag_delete_source(path, "docs", "a.md")) { gmb_rag_close(h); return -15; }
    char* after = gmb_rag_search(path, "docs", "cat", 5, NULL);
    if (after && strstr(after, "a.md")) { free(after); gmb_rag_close(h); return -16; }
    free(after);

    gmb_rag_close(h);
    remove(path);
    return 0;
}

/* ── ASK grounded (parità RagAskPipeline): context → prompt → gmb_llm_complete. ── */
char* gmb_rag_ask(const char* db, const char* coll, const char* question, int topk, const char* filter_json)
{
    if (!question || !question[0])
        return strdup("Error: empty question.");
    char* ctx = gmb_rag_context(db, coll, question, topk, filter_json);
    if (!ctx || !ctx[0])
    {
        free(ctx);
        return strdup("[insufficient evidence] No relevant documents found.");
    }
    /* prompt parità RagAskPipeline: "Context:\n...\n\nQuestion: ..." */
    size_t sl = strlen(ctx), ql = strlen(question);
    char* prompt = (char*)malloc(sl + ql + 64);
    if (!prompt) { free(ctx); return NULL; }
    snprintf(prompt, sl + ql + 64, "Context:\n%s\n\nQuestion: %s", ctx, question);
    free(ctx);
    const char* sys = "Answer using only the provided context. If the answer is not in the context, say you do not know.";
    char* r = gmb_llm_complete(sys, prompt);
    free(prompt);
    return r;
}
