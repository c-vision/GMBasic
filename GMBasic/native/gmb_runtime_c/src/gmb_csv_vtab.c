/* FILE: native/gmb_runtime_c/src/gmb_csv_vtab.c
 * Estensione SQLite: virtual table read-WRITE `csv` per leggere e MODIFICARE file CSV in SQL.
 *
 *   CREATE VIRTUAL TABLE t USING csv(name='file.csv'[, header=1][, delim=',']);
 *   INSERT INTO t VALUES (...); UPDATE t SET ... WHERE ...; DELETE FROM t WHERE ...;
 *
 * Ogni INSERT/UPDATE/DELETE riscrive il file (RFC4180) in modo atomico (tmp + rename).
 * Semantica: se header!=0 (default) la prima riga dà i nomi colonna; ogni cella è TEXT;
 * parse RFC4180 (quote, "" interno, newline dentro quote). Righe con numero di campi diverso
 * da ncol vengono paddate (NULL) o troncate. Campi > 4095 byte troncati (v1).
 *
 * Sorgente UNICO (§0bis): compilato come MODULE caricabile (host C#) o staticamente nel runtime
 * C condiviso (VM/C, C99, C99Native) via gmb_sql_register_csv. */
#ifdef GMB_CSV_AS_EXTENSION
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#else
#include <sqlite3.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>   /* getpid (nome tmp) */
#include <math.h>

#define CSV_FIELD_MAX 4096

typedef struct csv_vtab {
    sqlite3_vtab base;
    char path[4096];
    char delim;
    int header;
    int ncol;
    char** names;          /* ncol (owned) */
    sqlite3_int64 nrow;    /* slot (incluse righe cancellate = tombstone) */
    char*** data;          /* nrow × ncol: stringhe owned (NULL = NULL/assente) */
    unsigned char* alive;  /* 1 = riga viva, 0 = tombstone (rowid stabile) */
    sqlite3_int64 cap;     /* capacità slot di data/alive */
} csv_vtab;

typedef struct { sqlite3_vtab_cursor base; sqlite3_int64 row; } csv_cursor;

/* ---------- tokenizer RFC4180 → record (righe di campi owned) ---------- */
typedef struct { char** f; int n; } csv_rec;

static void recs_free(csv_rec* recs, int nrec)
{
    int i, j;
    if (!recs) return;
    for (i = 0; i < nrec; i++) { for (j = 0; j < recs[i].n; j++) free(recs[i].f[j]); free(recs[i].f); }
    free(recs);
}
static int csv_split(const char* raw, size_t len, char delim, csv_rec** out_recs, int* out_nrec)
{
    csv_rec* recs = NULL;
    int nrec = 0, cap = 0;
    size_t i = 0;
    char field[CSV_FIELD_MAX];
    int fl = 0, in_q = 0;
    char** cur = NULL;
    int ncur = 0, ccur = 0;
    while (i <= len) {
        char c = (i < len) ? raw[i] : '\n';
        int end_field = 0, end_rec = 0;
        if (i == len) { end_field = 1; end_rec = 1; }
        else if (in_q) {
            if (c == '"') { if (i + 1 < len && raw[i + 1] == '"') { if (fl < CSV_FIELD_MAX - 1) field[fl++] = '"'; i += 2; continue; } in_q = 0; i++; continue; }
            if (fl < CSV_FIELD_MAX - 1) field[fl++] = c;
            i++; continue;
        }
        else if (c == '"' && fl == 0) { in_q = 1; i++; continue; }
        else if (c == delim) end_field = 1;
        else if (c == '\n') { end_field = 1; end_rec = 1; }
        else if (c == '\r') { i++; continue; }
        else { if (fl < CSV_FIELD_MAX - 1) field[fl++] = c; i++; continue; }
        field[fl] = '\0';
        {
            char* cp = strdup(field);
            if (!cp) { recs_free(recs, nrec); free(cur); return -1; }
            if (ncur >= ccur) { ccur = ccur ? ccur * 2 : 8; cur = (char**)realloc(cur, (size_t)ccur * sizeof(char*)); if (!cur) { free(cp); recs_free(recs, nrec); return -1; } }
            cur[ncur++] = cp;
        }
        fl = 0;
        if (end_rec) {
            if (!(i == len && ncur == 1 && cur[0][0] == '\0')) {
                if (nrec >= cap) { cap = cap ? cap * 2 : 16; recs = (csv_rec*)realloc(recs, (size_t)cap * sizeof(csv_rec)); if (!recs) { free(cur); return -1; } }
                recs[nrec].f = (char**)malloc((size_t)ncur * sizeof(char*));
                if (!recs[nrec].f) { free(cur); recs_free(recs, nrec); return -1; }
                memcpy(recs[nrec].f, cur, (size_t)ncur * sizeof(char*));
                recs[nrec].n = ncur; nrec++;
            }
            ncur = 0;
        }
        i++;
    }
    free(cur);
    *out_recs = recs; *out_nrec = nrec;
    return 0;
}

/* ---------- scrittura file (RFC4180) ---------- */
static void fput_field(FILE* f, const char* s, char delim)
{
    int need_q = 0;
    const char* p;
    if (!s) return;                       /* NULL → campo vuoto */
    for (p = s; *p; p++) if (*p == delim || *p == '"' || *p == '\n' || *p == '\r') { need_q = 1; break; }
    if (!need_q) { fputs(s, f); return; }
    fputc('"', f);
    for (p = s; *p; p++) { if (*p == '"') fputc('"', f); fputc(*p, f); }
    fputc('"', f);
}
static int csv_rewrite_file(csv_vtab* v)
{
    char tmp[4096];
    FILE* f;
    snprintf(tmp, sizeof(tmp), "%s.tmp%ld", v->path, (long)getpid());
    f = fopen(tmp, "wb");
    if (!f) return SQLITE_ERROR;
    if (v->header) {
        int c;
        for (c = 0; c < v->ncol; c++) { if (c) fputc(v->delim, f); fput_field(f, v->names[c], v->delim); }
        fputc('\n', f);
    }
    for (sqlite3_int64 r = 0; r < v->nrow; r++) {
        int c;
        if (!v->alive[r]) continue;
        for (c = 0; c < v->ncol; c++) { if (c) fputc(v->delim, f); fput_field(f, v->data[r][c], v->delim); }
        fputc('\n', f);
    }
    if (fclose(f) != 0) { remove(tmp); return SQLITE_ERROR; }
    if (rename(tmp, v->path) != 0) { remove(tmp); return SQLITE_ERROR; }
    return SQLITE_OK;
}

/* ---------- connect (parse arg + carica griglia) ---------- */
static int csv_connect_impl(sqlite3* db, void* aux, int argc, const char* const* argv,
                            sqlite3_vtab** ppVtab, char** pzErr)
{
    csv_vtab* v;
    char delim = ',';
    int header = 1;
    const char* fname = NULL;
    csv_rec* recs = NULL;
    int nrec = 0, r;
    FILE* f;
    long sz;
    char* raw;
    size_t rd;
    int first_data;
    (void)aux;

    v = (csv_vtab*)calloc(1, sizeof(csv_vtab));
    if (!v) return SQLITE_NOMEM;
    for (r = 3; r < argc; r++) {
        const char* a = argv[r];
        if (r == 3 && strchr(a, '=') == NULL) { fname = a; continue; }
        if (strncmp(a, "name=", 5) == 0) fname = a + 5;
        else if (strncmp(a, "header=", 7) == 0) { const char* hv = a + 7; if (*hv == '\'' || *hv == '"') hv++; header = (*hv == '0') ? 0 : 1; }
        else if (strncmp(a, "delim=", 6) == 0) { const char* dv = a + 6; if (*dv == '\'' || *dv == '"') dv++; delim = *dv ? *dv : ','; }
    }
    if (!fname || !*fname) { if (pzErr) *pzErr = sqlite3_mprintf("csv: manca il file (USING csv(name='...'))"); free(v); return SQLITE_ERROR; }
    {   /* strip quote opzionali attorno al valore */
        size_t fl = strlen(fname);
        if (fl >= 2 && (fname[0] == '\'' || fname[0] == '"') && fname[fl-1] == fname[0]) { memcpy(v->path, fname+1, fl-2); v->path[fl-2] = 0; }
        else { strncpy(v->path, fname, sizeof(v->path)-1); }
    }
    v->delim = delim; v->header = header;

    f = fopen(v->path, "rb");
    if (!f) { if (pzErr) *pzErr = sqlite3_mprintf("csv: impossibile aprire '%s'", v->path); free(v); return SQLITE_ERROR; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) sz = 0;
    raw = (char*)malloc((size_t)sz + 1);
    if (!raw) { fclose(f); free(v); return SQLITE_NOMEM; }
    rd = fread(raw, 1, (size_t)sz, f); raw[rd] = 0; fclose(f);

    if (csv_split(raw, rd, delim, &recs, &nrec) != 0) { free(raw); free(v); if (pzErr) *pzErr = sqlite3_mprintf("csv: OOM"); return SQLITE_NOMEM; }
    free(raw);

    v->ncol = (nrec > 0) ? recs[0].n : 1;
    if (v->ncol < 1) v->ncol = 1;
    first_data = header ? 1 : 0;
    v->names = (char**)calloc((size_t)v->ncol, sizeof(char*));
    if (!v->names) { recs_free(recs, nrec); free(v); return SQLITE_NOMEM; }
    for (r = 0; r < v->ncol; r++) {
        if (header && nrec > 0 && r < recs[0].n) v->names[r] = strdup(recs[0].f[r]);
        else { char nb[32]; snprintf(nb, sizeof(nb), "c%d", r); v->names[r] = strdup(nb); }
        if (!v->names[r]) { for (int j = 0; j < r; j++) free(v->names[j]); free(v->names); recs_free(recs, nrec); free(v); return SQLITE_NOMEM; }
    }
    v->nrow = (nrec - first_data > 0) ? nrec - first_data : 0;
    v->cap = v->nrow ? v->nrow : 1;
    v->data = (char***)calloc((size_t)v->cap, sizeof(char**));
    v->alive = (unsigned char*)calloc((size_t)v->cap, 1);
    if (!v->data || !v->alive) { for (r = 0; r < v->ncol; r++) free(v->names[r]); free(v->names); recs_free(recs, nrec); free(v); return SQLITE_NOMEM; }
    for (sqlite3_int64 i = 0; i < v->nrow; i++) {
        v->alive[i] = 1;
        v->data[i] = (char**)calloc((size_t)v->ncol, sizeof(char*));
        for (int c = 0; c < v->ncol; c++)
            if (c < recs[i + first_data].n) v->data[i][c] = strdup(recs[i + first_data].f[c]);
    }
    recs_free(recs, nrec);

    {
        char sql[512]; size_t used; int c;
        snprintf(sql, sizeof(sql), "CREATE TABLE x(");
        used = strlen(sql);
        for (c = 0; c < v->ncol; c++) {
            if (c) sql[used++] = ',';
            sql[used++] = '"';
            for (const char* q = v->names[c]; *q && used < sizeof(sql)-8; q++) { if (*q == '"') { sql[used++]='"'; sql[used++]='"'; } else sql[used++] = *q; }
            sql[used++] = '"';
            memcpy(sql + used, " NUMERIC", 8); used += 8;
        }
        sql[used++] = ')'; sql[used] = 0;
        if (sqlite3_declare_vtab(db, sql) != SQLITE_OK) { if (pzErr) *pzErr = sqlite3_mprintf("csv: declare_vtab fallito"); return SQLITE_ERROR; }
    }
    *ppVtab = &v->base;
    return SQLITE_OK;
}

static int csv_disconnect(sqlite3_vtab* p)
{
    csv_vtab* v = (csv_vtab*)p;
    for (sqlite3_int64 i = 0; i < v->nrow; i++) { for (int c = 0; c < v->ncol; c++) free(v->data[i][c]); free(v->data[i]); }
    free(v->data); free(v->alive);
    for (int c = 0; c < v->ncol; c++) free(v->names[c]);
    free(v->names);
    free(v);
    return SQLITE_OK;
}

static int csv_open(sqlite3_vtab* p, sqlite3_vtab_cursor** pp) { csv_cursor* c = (csv_cursor*)calloc(1, sizeof(csv_cursor)); (void)p; if (!c) return SQLITE_NOMEM; *pp = &c->base; return SQLITE_OK; }
static int csv_close(sqlite3_vtab_cursor* c) { free(c); return SQLITE_OK; }
static void csv_advance(csv_cursor* c)
{
    csv_vtab* v = (csv_vtab*)c->base.pVtab;
    while (c->row < v->nrow && !v->alive[c->row]) c->row++;
}
static int csv_filter(sqlite3_vtab_cursor* c, int a, const char* b, int d, sqlite3_value** e) { (void)a; (void)b; (void)d; (void)e; ((csv_cursor*)c)->row = 0; csv_advance((csv_cursor*)c); return SQLITE_OK; }
static int csv_next(sqlite3_vtab_cursor* c) { ((csv_cursor*)c)->row++; csv_advance((csv_cursor*)c); return SQLITE_OK; }
static int csv_eof(sqlite3_vtab_cursor* c) { return ((csv_cursor*)c)->row >= ((csv_vtab*)c->pVtab)->nrow; }
static int csv_is_int(const char* p)
{
    const char* q = p;
    if (*q == '-' || *q == '+') q++;
    if (!*q) return 0;
    if (q[0] == '0' && q[1]) return 0;   /* zeri iniziali → testo (es. ID "007") */
    for (; *q; q++) if (*q < '0' || *q > '9') return 0;
    return 1;
}
static int csv_is_num(const char* p)
{
    char* end; double d;
    if (!*p) return 0;
    d = strtod(p, &end);
    if (end == p || *end) return 0;
    return isfinite(d);
}
static int csv_column(sqlite3_vtab_cursor* c, sqlite3_context* ctx, int col)
{
    csv_vtab* v = (csv_vtab*)c->pVtab;
    sqlite3_int64 r = ((csv_cursor*)c)->row;
    char* p = (col >= 0 && col < v->ncol && r < v->nrow) ? v->data[r][col] : NULL;
    if (!p) { sqlite3_result_null(ctx); return SQLITE_OK; }
    if (csv_is_int(p)) sqlite3_result_int64(ctx, strtoll(p, NULL, 10));
    else if (csv_is_num(p)) sqlite3_result_double(ctx, strtod(p, NULL));
    else sqlite3_result_text(ctx, p, -1, SQLITE_TRANSIENT);
    return SQLITE_OK;
}
static int csv_rowid(sqlite3_vtab_cursor* c, sqlite3_int64* id) { *id = ((csv_cursor*)c)->row + 1; return SQLITE_OK; }

/* INSERT (old==NULL) / UPDATE / DELETE: modifica la griglia e riscrive il file. */
static int csv_update(sqlite3_vtab* p, int argc, sqlite3_value** argv, sqlite3_int64* pRowid)
{
    csv_vtab* v = (csv_vtab*)p;
    int c;
    if (argc == 1) {                                   /* DELETE (tombstone: rowid stabile) */
        sqlite3_int64 rid = sqlite3_value_int64(argv[0]);
        if (rid < 1 || rid > v->nrow) return SQLITE_ERROR;
        v->alive[rid-1] = 0;
        *pRowid = rid;
    } else if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {   /* INSERT */
        if (v->nrow >= v->cap) { v->cap = v->cap ? v->cap * 2 : 8; v->data = (char***)realloc(v->data, (size_t)v->cap * sizeof(char**)); v->alive = (unsigned char*)realloc(v->alive, (size_t)v->cap); if (!v->data || !v->alive) return SQLITE_NOMEM; }
        v->alive[v->nrow] = 1;
        v->data[v->nrow] = (char**)calloc((size_t)v->ncol, sizeof(char*));
        for (c = 0; c < v->ncol && c + 2 < argc; c++) {
            if (sqlite3_value_type(argv[c+2]) != SQLITE_NULL) { const unsigned char* t = sqlite3_value_text(argv[c+2]); v->data[v->nrow][c] = strdup(t ? (const char*)t : ""); }
        }
        v->nrow++;
        *pRowid = v->nrow;
    } else {                                           /* UPDATE */
        sqlite3_int64 rid = sqlite3_value_int64(argv[0]);
        if (rid < 1 || rid > v->nrow) return SQLITE_ERROR;
        for (c = 0; c < v->ncol && c + 2 < argc; c++) {
            free(v->data[rid-1][c]); v->data[rid-1][c] = NULL;
            if (sqlite3_value_type(argv[c+2]) != SQLITE_NULL) { const unsigned char* t = sqlite3_value_text(argv[c+2]); v->data[rid-1][c] = strdup(t ? (const char*)t : ""); }
        }
        *pRowid = rid;
    }
    return csv_rewrite_file(v);
}

static int csv_bestindex(sqlite3_vtab* p, sqlite3_index_info* info) { (void)p; info->estimatedCost = 1000.0; return SQLITE_OK; }

static sqlite3_module csv_module = {
    0,                /* iVersion */
    csv_connect_impl, /* xCreate */
    csv_connect_impl, /* xConnect */
    csv_bestindex,    /* xBestIndex */
    csv_disconnect,   /* xDisconnect */
    csv_disconnect,   /* xDestroy */
    csv_open,         /* xOpen */
    csv_close,        /* xClose */
    csv_filter,       /* xFilter */
    csv_next,         /* xNext */
    csv_eof,          /* xEof */
    csv_column,       /* xColumn */
    csv_rowid,        /* xRowid */
    csv_update,       /* xUpdate (INSERT/UPDATE/DELETE → file) */
    0, 0, 0, 0, 0, 0, 0, 0, 0    /* rename/... non supportati */
};

#ifdef GMB_CSV_AS_EXTENSION
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_gmbcsvext_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi)
{
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;
    return sqlite3_create_module(db, "csv", &csv_module, 0);
}
#else
int gmb_sql_register_csv(sqlite3* db) { return sqlite3_create_module(db, "csv", &csv_module, 0); }
#endif
