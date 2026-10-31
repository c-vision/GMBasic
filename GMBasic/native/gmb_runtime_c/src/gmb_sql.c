/* FILE: native/gmb_runtime_c/src/gmb_sql.c
 * SQL community: wrapper sqlite3 (SQL OPEN/EXEC/QUERY). Spec: common/Databases/Rdd/SqlDriver.cs */
#include "gmb_runtime.h"
#include "gmb_rdd_db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <math.h>

/* Virtual table `csv` (scan diretto di file CSV): registrazione STATICA dal runtime condiviso
 * (sorgente unico native/gmb_sql_ext/gmb_csv_vtab.c, §0bis). Così VM/C, C99 e C99Native la
 * ottengono senza dipendere dal load-extension di sqlite3 (omesso nella sqlite di sistema macOS).
 * I backend C# la caricano come estensione caricabile dello stesso sorgente. */
int gmb_sql_register_csv(sqlite3* db);
int gmb_sql_register_parquet(sqlite3* db);

#define GMB_SQL_MAX 32
#define GMB_SQL_RS_MAX 16

typedef struct {
    int used;
    char alias[32];
    sqlite3* db;
    char fv[8][64];   /* nomi delle virtual table file-backed (csv/parquet) creati su questa connessione */
    int nfv;
} gmb_sql_db;

/* Result set materializzato con allocazione DINAMICA (nessun cap riga/cella/colonna):
 * prima era `calloc(4096 * cols, 512)` per ogni QUERY — fino a ~134MB eager — e scartava
 * silenziosamente oltre 4096 righe o 511 caratteri per cella (divergenza da IL/PCode, che
 * materializzano tutto). Ora il testo delle celle cresce quanto serve (cella seguita da
 * '\0', offset per cella) e i nomi colonna sono dinamici. */
typedef struct {
    int used;
    int rows, cols;
    int64_t cursor;  /* SQL.FETCH: riga corrente (-1 = prima del primo FETCH) */
    char* cells;  size_t cells_len, cells_cap;   /* testo celle (cella k = row*cols+col) */
    int*  offs;   int    offs_cap;               /* offs[k] = offset inizio cella k */
    char* ctypes; size_t ctypes_cap;             /* tipo SQLite per cella (parità tipo IL/C#) */
    char* names;  size_t names_len, names_cap;   /* nomi colonne concatenati, '\0'-terminati */
    int*  noffs;  int    noffs_cap;              /* noffs[c] = offset inizio nome colonna c */
} gmb_sql_rs;

/* Crescita geometrica di un buffer byte/int. Ritorna 1 su successo, 0 su OOM. */
static int sql_reserve(char** p, size_t* cap, size_t need)
{
    size_t nc;
    char* np;
    if (*cap >= need) return 1;
    nc = *cap ? *cap : 256;
    while (nc < need) nc *= 2;
    np = (char*)realloc(*p, nc);
    if (!np) return 0;
    *p = np; *cap = nc;
    return 1;
}
static int sql_reserve_ints(int** p, int* cap, size_t need)
{
    int nc;
    int* np;
    if ((size_t)*cap >= need) return 1;
    nc = *cap ? *cap : 64;
    while ((size_t)nc < need) nc *= 2;
    np = (int*)realloc(*p, (size_t)nc * sizeof(int));
    if (!np) return 0;
    *p = np; *cap = nc;
    return 1;
}

static gmb_sql_db g_db[GMB_SQL_MAX];
static gmb_sql_rs g_rs[GMB_SQL_RS_MAX];
static char g_err[512];

static void seterr(const char* m)
{
    strncpy(g_err, m ? m : "", sizeof(g_err) - 1);
    g_err[sizeof(g_err) - 1] = 0;
}

const char* gmb_sql_last_error(void) { return g_err; }

static void gmb_sql_register_udfs(sqlite3* db);
static gmb_sql_db* find_alias(const char* alias);

/* ── Coarse-park: discriminante "scrittura PESANTE su virtual table file-backed". ──
 * Le scritture che riscrivono un file intero (CSV/Parquet: CREATE/INSERT/UPDATE/DELETE)
 * sono le uniche operazioni DB che vale la pena eseguire fuori dal worker GPM (park);
 * le letture e le scritture su tabelle normali restano sincrone (parcheggiare il leggero
 * è più lento). Il classificatore NON esegue nulla: decide solo guardando il testo SQL. */
static int sql_ci_eq(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b) { if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0; a++; b++; }
    return *a == 0 && *b == 0;
}
static const char* sql_skip(const char* p)
{
    for (;;) {
        while (*p && (unsigned char)*p <= ' ') p++;
        if (p[0] == '-' && p[1] == '-') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0] == '*' && p[1] == '/')) p++; if (*p) p += 2; continue; }
        break;
    }
    return p;
}
static int sql_token(const char** pp, char* out, size_t cap)
{
    const char* p = sql_skip(*pp); size_t i = 0;
    if (*p == '"' || *p == '`' || *p == '[') { char q = (*p == '[') ? ']' : *p; p++; while (*p && *p != q && i < cap - 1) out[i++] = *p++; if (*p == q) p++; }
    else while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$') && i < cap - 1) out[i++] = *p++;
    out[i] = 0; *pp = p; return (int)i;
}
static int sql_ci_find(const char* hay, const char* needle)
{
    size_t nl = strlen(needle);
    if (!hay || nl == 0) return 0;
    for (; *hay; hay++) { size_t k = 0; while (k < nl && hay[k] && tolower((unsigned char)hay[k]) == tolower((unsigned char)needle[k])) k++; if (k == nl) return 1; }
    return 0;
}
static int sql_is_file_vtab(gmb_sql_db* d, const char* name)
{
    int i;
    if (!d || !name || !name[0]) return 0;
    for (i = 0; i < d->nfv; i++) if (sql_ci_eq(d->fv[i], name)) return 1;
    return 0;
}
/* Registra una vtab file-backed dopo un CREATE VIRTUAL TABLE ... USING csv|parquet riuscito. */
static void gmb_sql_track_vtab(gmb_sql_db* d, const char* sql)
{
    const char* p = sql; char t[6][64]; int n = 0;
    if (!d || !sql) return;
    while (n < 6 && sql_token(&p, t[n], sizeof(t[n])) > 0) n++;
    if (n >= 4 && sql_ci_eq(t[0], "create") && sql_ci_eq(t[1], "virtual") && sql_ci_eq(t[2], "table")
        && (sql_ci_find(sql, "using csv") || sql_ci_find(sql, "using parquet")) && d->nfv < 8) {
        snprintf(d->fv[d->nfv], sizeof(d->fv[d->nfv]), "%s", t[3]);
        d->nfv++;
    }
}
/* 1 = scrittura pesante da eseguire fuori dal worker (park); 0 = sincrona. */
int gmb_sql_stmt_is_heavy_write(const char* alias, const char* sql)
{
    gmb_sql_db* d = find_alias(alias);
    const char* p = sql; char t[6][64]; int n = 0;
    if (!d || !sql) return 0;
    while (n < 6 && sql_token(&p, t[n], sizeof(t[n])) > 0) n++;
    if (n == 0) return 0;
    if (sql_ci_eq(t[0], "create"))
        return (n >= 4 && sql_ci_eq(t[1], "virtual") && sql_ci_eq(t[2], "table")
                && (sql_ci_find(sql, "using csv") || sql_ci_find(sql, "using parquet")));
    if ((sql_ci_eq(t[0], "insert") || sql_ci_eq(t[0], "replace")) && n >= 3 && sql_ci_eq(t[1], "into")) return sql_is_file_vtab(d, t[2]);
    if (sql_ci_eq(t[0], "update") && n >= 2) return sql_is_file_vtab(d, t[1]);
    if (sql_ci_eq(t[0], "delete") && n >= 3 && sql_ci_eq(t[1], "from")) return sql_is_file_vtab(d, t[2]);
    if ((sql_ci_eq(t[0], "drop") || sql_ci_eq(t[0], "alter")) && n >= 3 && sql_ci_eq(t[1], "table")) return sql_is_file_vtab(d, t[2]);
    return 0;
}

/* ── Hook di park (coarse): registrato dalla GPM del backend (C99) per eseguire le
 * scritture pesanti fuori dal worker. NULL = nessun park (esecuzione sincrona). ── */
typedef int (*gmb_sql_park_fn)(const char*, const char*, const char* const*, int);
static gmb_sql_park_fn g_sql_park_hook = NULL;
void gmb_sql_set_park_hook(gmb_sql_park_fn fn) { g_sql_park_hook = fn; }

static gmb_sql_db* find_alias(const char* alias)
{
    int i;
    if (!alias) return NULL;
    for (i = 0; i < GMB_SQL_MAX; i++)
        if (g_db[i].used && strcmp(g_db[i].alias, alias) == 0) return &g_db[i];
    return NULL;
}

sqlite3* gmb_sql_handle(const char* alias)
{
    gmb_sql_db* d = find_alias(alias);
    return d ? d->db : NULL;
}

int gmb_sql_open(const char* path, const char* alias)
{
    int i, slot = -1;
    sqlite3* db = NULL;
    if (!alias || !alias[0]) { seterr("alias required"); return -1; }
    gmb_sql_close(alias);
    for (i = 0; i < GMB_SQL_MAX; i++)
        if (!g_db[i].used) { slot = i; break; }
    if (slot < 0) { seterr("too many SQL connections"); return -1; }
    if (sqlite3_open(path && path[0] ? path : ":memory:", &db) != SQLITE_OK) {
        seterr(db ? sqlite3_errmsg(db) : "sqlite3_open failed");
        if (db) sqlite3_close(db);
        return -1;
    }
    g_db[slot].used = 1;
    g_db[slot].db = db;
    g_db[slot].nfv = 0;
    strncpy(g_db[slot].alias, alias, 31);
    g_db[slot].alias[31] = 0;
    g_err[0] = 0;
    gmb_sql_register_udfs(db);
    gmb_sql_register_csv(db);   /* virtual table `csv` (scan diretto; sorgente condiviso) */
    gmb_sql_register_parquet(db);   /* virtual table `parquet` (reader Parquet nativo) */
    return 0;
}

/* --- UDF temporali (parità C# SqlTimeUdf): TIMEBUCKET scalare + FIRST_TS/LAST_TS.
 * Semantica bloccata (stessa nei due mondi, vedi guida SQL):
 * - TIMEBUCKET(ts, secs) = FLOOR(ts/secs)*secs. INTEGER+INTEGER → INTEGER (floor vera,
 *   anche con ts negativi); altrimenti REAL. ts non numerico, secs NULL/<=0 → NULL.
 * - FIRST_TS(x, ts)/LAST_TS(x, ts) = x della riga con ts minimo/massimo (a pari ts vince
 *   la prima riga incontrata). Righe con ts NULL/non-numerico o x NULL saltate (come gli
 *   aggregati SQL standard); insieme vuoto → NULL. Il tipo di x è preservato. --- */
static void sqlfn_timebucket(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
    int tts, tsecs;
    if (argc != 2) { sqlite3_result_null(ctx); return; }
    tts = sqlite3_value_type(argv[0]);
    tsecs = sqlite3_value_type(argv[1]);
    if (tts != SQLITE_INTEGER && tts != SQLITE_FLOAT) { sqlite3_result_null(ctx); return; }
    if (tsecs != SQLITE_INTEGER && tsecs != SQLITE_FLOAT) { sqlite3_result_null(ctx); return; }
    if (tts == SQLITE_INTEGER && tsecs == SQLITE_INTEGER) {
        sqlite3_int64 ts = sqlite3_value_int64(argv[0]);
        sqlite3_int64 s = sqlite3_value_int64(argv[1]);
        sqlite3_int64 r;
        if (s <= 0) { sqlite3_result_null(ctx); return; }
        r = ts % s;
        if (r < 0) r += s;
        sqlite3_result_int64(ctx, ts - r);
    } else {
        double ts = sqlite3_value_double(argv[0]);
        double s = sqlite3_value_double(argv[1]);
        if (!(s > 0.0)) { sqlite3_result_null(ctx); return; }
        sqlite3_result_double(ctx, floor(ts / s) * s);
    }
}

typedef struct {
    int has;
    double ts;
    int vtype;
    sqlite3_int64 vi;
    double vr;
    char* vt;
    int vlen;
} fl_acc;

static void sqlfn_firstlast_step(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
    fl_acc* a = (fl_acc*)sqlite3_aggregate_context(ctx, sizeof(fl_acc));
    int want_first = sqlite3_user_data(ctx) ? 1 : 0;
    double ts;
    int xt;
    if (!a || argc != 2) return;
    if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER &&
        sqlite3_value_type(argv[1]) != SQLITE_FLOAT) return;  /* ts NULL/non-numerico: skip */
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) return;   /* x NULL: skip */
    ts = sqlite3_value_double(argv[1]);
    if (a->has && (want_first ? !(ts < a->ts) : !(ts > a->ts))) return;
    xt = sqlite3_value_type(argv[0]);
    if (xt == SQLITE_INTEGER) {
        a->vi = sqlite3_value_int64(argv[0]);
    } else if (xt == SQLITE_FLOAT) {
        a->vr = sqlite3_value_double(argv[0]);
    } else {
        /* TEXT/BLOB (anche vuoti: malloc(1) garantisce "" / x'' invece di NULL). */
        const void* p = (xt == SQLITE_BLOB) ? sqlite3_value_blob(argv[0])
                                            : (const void*)sqlite3_value_text(argv[0]);
        int n = sqlite3_value_bytes(argv[0]);
        char* cp = (char*)malloc((size_t)(n > 0 ? n : 0) + 1);
        if (!cp) return;   /* OOM: tiene il best precedente */
        if (p && n > 0) memcpy(cp, p, (size_t)n);
        cp[n > 0 ? n : 0] = 0;
        free(a->vt);
        a->vt = cp;
        a->vlen = n > 0 ? n : 0;
    }
    a->vtype = xt;
    a->ts = ts;
    a->has = 1;
}

static void sqlfn_firstlast_final(sqlite3_context* ctx)
{
    fl_acc* a = (fl_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || !a->has) { sqlite3_result_null(ctx); return; }
    if (a->vtype == SQLITE_INTEGER) sqlite3_result_int64(ctx, a->vi);
    else if (a->vtype == SQLITE_FLOAT) sqlite3_result_double(ctx, a->vr);
    else if (a->vtype == SQLITE_BLOB && a->vt) sqlite3_result_blob(ctx, a->vt, a->vlen, SQLITE_TRANSIENT);
    else if (a->vt) sqlite3_result_text(ctx, a->vt, a->vlen, SQLITE_TRANSIENT);
    else sqlite3_result_null(ctx);
    free(a->vt);
    a->vt = NULL;
}

/* --- Aggregati statistici (gap vs Polars/DuckDB/PostgreSQL; SQLite non li ha):
 * VAR_POP/VAR_SAMP/VARIANCE/STDDEV_POP/STDDEV_SAMP/STDDEV/MEDIAN. Var/Std via Welford
 * (stabile, ordine d'iterazione → identico tra C e C#); MEDIAN colleziona i valori
 * (user_data != NULL) e ordina in final. NULL/non-numerici saltati; insieme vuoto → NULL;
 * VAR_SAMP/STDDEV_SAMP richiedono n>=2. Semantica bloccata, identica al mirror C#. --- */
typedef struct {
    sqlite3_int64 n;
    double mean, m2;
    double* vals;   /* solo per MEDIAN/MODE/QUANTILE */
    size_t nv, cap;
    double qpar;    /* QUANTILE: p (0..1) */
    int qhas;
} stats_acc;

static void sqlfn_stats_step(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, sizeof(stats_acc));
    int vt;
    double x, d;
    if (!a || argc < 1) return;
    /* QUANTILE(x, p): cattura p una volta (costante per query). */
    if (argc >= 2 && !a->qhas) {
        int pt = sqlite3_value_type(argv[1]);
        if (pt == SQLITE_INTEGER || pt == SQLITE_FLOAT) { a->qpar = sqlite3_value_double(argv[1]); a->qhas = 1; }
    }
    vt = sqlite3_value_type(argv[0]);
    if (vt != SQLITE_INTEGER && vt != SQLITE_FLOAT) return;   /* skip NULL/non-numerico */
    x = sqlite3_value_double(argv[0]);
    a->n++;
    d = x - a->mean;
    a->mean += d / (double)a->n;
    {
        /* `volatile` impedisce la FMA contraction (clang -ffp-contract=on): senza, `m2 += d*(x-mean)`
         * diventa una fma e diverge di 1 ULP dal mirror C# (che fa mul+add separati). */
        volatile double prod = d * (x - a->mean);
        a->m2 += prod;
    }
    if (sqlite3_user_data(ctx)) {   /* raccolta valori (MEDIAN) */
        if (a->nv >= a->cap) {
            size_t nc = a->cap ? a->cap * 2 : 64;
            double* np = (double*)realloc(a->vals, nc * sizeof(double));
            if (np) { a->vals = np; a->cap = nc; }
        }
        if (a->nv < a->cap) a->vals[a->nv++] = x;
    }
}

static void sqlfn_var_pop(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || a->n < 1) { sqlite3_result_null(ctx); return; }
    sqlite3_result_double(ctx, a->m2 / (double)a->n);
}
static void sqlfn_var_samp(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || a->n < 2) { sqlite3_result_null(ctx); return; }
    sqlite3_result_double(ctx, a->m2 / (double)(a->n - 1));
}
static void sqlfn_std_pop(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || a->n < 1) { sqlite3_result_null(ctx); return; }
    sqlite3_result_double(ctx, sqrt(a->m2 / (double)a->n));
}
static void sqlfn_std_samp(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || a->n < 2) { sqlite3_result_null(ctx); return; }
    sqlite3_result_double(ctx, sqrt(a->m2 / (double)(a->n - 1)));
}
static int sqlfn_cmp_double(const void* p, const void* q)
{
    double x = *(const double*)p, y = *(const double*)q;
    return x < y ? -1 : (x > y ? 1 : 0);
}
static void sqlfn_median(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || a->n < 1 || !a->vals) { sqlite3_result_null(ctx); free(a ? a->vals : NULL); return; }
    qsort(a->vals, a->nv, sizeof(double), sqlfn_cmp_double);
    if (a->nv % 2 == 1) sqlite3_result_double(ctx, a->vals[a->nv / 2]);
    else sqlite3_result_double(ctx, (a->vals[a->nv / 2 - 1] + a->vals[a->nv / 2]) / 2.0);
    free(a->vals);
    a->vals = NULL;
}
/* MODE: valore più frequente; a parità (stesso conteggio) il PIÙ PICCOLO (ordine crescente →
 * il primo massimo vince); insieme vuoto → NULL. */
static void sqlfn_mode(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    size_t i = 0;
    sqlite3_int64 best_c = 0;
    double best_v = 0.0;
    if (!a || a->n < 1 || !a->vals) { sqlite3_result_null(ctx); free(a ? a->vals : NULL); return; }
    qsort(a->vals, a->nv, sizeof(double), sqlfn_cmp_double);
    while (i < a->nv) {
        size_t j = i + 1;
        sqlite3_int64 c;
        while (j < a->nv && a->vals[j] == a->vals[i]) j++;
        c = (sqlite3_int64)(j - i);
        if (c > best_c) { best_c = c; best_v = a->vals[i]; }
        i = j;
    }
    sqlite3_result_double(ctx, best_v);
    free(a->vals);
    a->vals = NULL;
}
/* QUANTILE(x, p): p in [0,1], interpolazione lineare tra gli ordini (come numpy/pandas).
 * p fuori [0,1] o non fornito → NULL; insieme vuoto → NULL. */
static void sqlfn_quantile(sqlite3_context* ctx)
{
    stats_acc* a = (stats_acc*)sqlite3_aggregate_context(ctx, 0);
    if (!a || a->n < 1 || !a->vals || !a->qhas || a->qpar < 0.0 || a->qpar > 1.0) {
        sqlite3_result_null(ctx); free(a ? a->vals : NULL); return;
    }
    qsort(a->vals, a->nv, sizeof(double), sqlfn_cmp_double);
    if (a->nv == 1) { sqlite3_result_double(ctx, a->vals[0]); free(a->vals); a->vals = NULL; return; }
    {
        double pos = a->qpar * (double)(a->nv - 1);
        size_t lo = (size_t)floor(pos);
        size_t hi = lo + 1 < a->nv ? lo + 1 : lo;
        double frac = pos - (double)lo;
        volatile double prod = (a->vals[hi] - a->vals[lo]) * frac;   /* no FMA (parità C#) */
        sqlite3_result_double(ctx, a->vals[lo] + prod);
    }
    free(a->vals);
    a->vals = NULL;
}

/* --- Covarianza/correlazione (aggregati a 2 argomenti). Sxy/Sxx/Syy dalle somme. --- */
typedef struct { sqlite3_int64 n; double sy, sx, syy, sxx, sxy; } corr_acc;

static void sqlfn_corr_step(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
    corr_acc* a = (corr_acc*)sqlite3_aggregate_context(ctx, sizeof(corr_acc));
    int t0, t1;
    double y, x;
    if (!a || argc < 2) return;
    t0 = sqlite3_value_type(argv[0]);
    t1 = sqlite3_value_type(argv[1]);
    if ((t0 != SQLITE_INTEGER && t0 != SQLITE_FLOAT) || (t1 != SQLITE_INTEGER && t1 != SQLITE_FLOAT)) return;
    y = sqlite3_value_double(argv[0]);
    x = sqlite3_value_double(argv[1]);
    a->n++;
    a->sy += y; a->sx += x;
    {
        volatile double yy = y * y, xx = x * x, xy = y * x;   /* no FMA (parità C#) */
        a->syy += yy; a->sxx += xx; a->sxy += xy;
    }
}
static int sqlfn_corr_sums(sqlite3_context* ctx, corr_acc** out)
{
    corr_acc* a = (corr_acc*)sqlite3_aggregate_context(ctx, 0);
    *out = a;
    return a ? 1 : 0;
}
static void sqlfn_covar_pop(sqlite3_context* ctx)
{
    corr_acc* a;
    if (!sqlfn_corr_sums(ctx, &a) || a->n < 1) { sqlite3_result_null(ctx); return; }
    {
        volatile double v3 = a->sy * a->sx;   /* no FMA */
        sqlite3_result_double(ctx, (a->sxy - v3 / (double)a->n) / (double)a->n);
    }
}
static void sqlfn_covar_samp(sqlite3_context* ctx)
{
    corr_acc* a;
    if (!sqlfn_corr_sums(ctx, &a) || a->n < 2) { sqlite3_result_null(ctx); return; }
    {
        volatile double v3 = a->sy * a->sx;   /* no FMA */
        sqlite3_result_double(ctx, (a->sxy - v3 / (double)a->n) / (double)(a->n - 1));
    }
}
static void sqlfn_corr(sqlite3_context* ctx)
{
    corr_acc* a;
    double syy, sxx, sxy;
    if (!sqlfn_corr_sums(ctx, &a) || a->n < 2) { sqlite3_result_null(ctx); return; }
    {
        volatile double v1 = a->sy * a->sy, v2 = a->sx * a->sx, v3 = a->sy * a->sx;   /* no FMA */
        syy = a->syy - v1 / (double)a->n;
        sxx = a->sxx - v2 / (double)a->n;
        sxy = a->sxy - v3 / (double)a->n;
    }
    if (syy <= 0.0 || sxx <= 0.0) { sqlite3_result_null(ctx); return; }  /* varianza nulla → r indefinito */
    sqlite3_result_double(ctx, sxy / sqrt(syy * sxx));
}

static void gmb_sql_register_udfs(sqlite3* db)
{
    if (!db) return;
    sqlite3_create_function(db, "TIMEBUCKET", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                            NULL, sqlfn_timebucket, NULL, NULL);
    /* Aggregati statistici (xFunc NULL; user_data != NULL solo per MEDIAN). */
    sqlite3_create_function(db, "VAR_POP", 1, SQLITE_UTF8, NULL, NULL, sqlfn_stats_step, sqlfn_var_pop);
    sqlite3_create_function(db, "VAR_SAMP", 1, SQLITE_UTF8, NULL, NULL, sqlfn_stats_step, sqlfn_var_samp);
    sqlite3_create_function(db, "VARIANCE", 1, SQLITE_UTF8, NULL, NULL, sqlfn_stats_step, sqlfn_var_samp);
    sqlite3_create_function(db, "STDDEV_POP", 1, SQLITE_UTF8, NULL, NULL, sqlfn_stats_step, sqlfn_std_pop);
    sqlite3_create_function(db, "STDDEV_SAMP", 1, SQLITE_UTF8, NULL, NULL, sqlfn_stats_step, sqlfn_std_samp);
    sqlite3_create_function(db, "STDDEV", 1, SQLITE_UTF8, NULL, NULL, sqlfn_stats_step, sqlfn_std_samp);
    sqlite3_create_function(db, "MEDIAN", 1, SQLITE_UTF8, (void*)1, NULL, sqlfn_stats_step, sqlfn_median);
    sqlite3_create_function(db, "MODE", 1, SQLITE_UTF8, (void*)1, NULL, sqlfn_stats_step, sqlfn_mode);
    sqlite3_create_function(db, "QUANTILE", 2, SQLITE_UTF8, (void*)1, NULL, sqlfn_stats_step, sqlfn_quantile);
    sqlite3_create_function(db, "COVAR_POP", 2, SQLITE_UTF8, NULL, NULL, sqlfn_corr_step, sqlfn_covar_pop);
    sqlite3_create_function(db, "COVAR_SAMP", 2, SQLITE_UTF8, NULL, NULL, sqlfn_corr_step, sqlfn_covar_samp);
    sqlite3_create_function(db, "CORR", 2, SQLITE_UTF8, NULL, NULL, sqlfn_corr_step, sqlfn_corr);
    /* Aggregati temporali: xFunc NULL, xStep/xFinal valorizzati (l'ordine conta: prima NULL). */
    sqlite3_create_function(db, "FIRST_TS", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                            (void*)1, NULL, sqlfn_firstlast_step, sqlfn_firstlast_final);
    sqlite3_create_function(db, "LAST_TS", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                            NULL, NULL, sqlfn_firstlast_step, sqlfn_firstlast_final);
}

int gmb_sql_close(const char* alias)
{
    gmb_sql_db* d = find_alias(alias);
    if (!d) return 0;
    sqlite3_close(d->db);
    memset(d, 0, sizeof(*d));
    return 0;
}

int gmb_sql_exec_raw(const char* alias, const char* sql)
{
    gmb_sql_db* d = find_alias(alias);
    char* errmsg = NULL;
    int rc;
    if (!d) { seterr("unknown SQL alias"); return -1; }
    rc = sqlite3_exec(d->db, sql ? sql : "", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        seterr(errmsg ? errmsg : sqlite3_errmsg(d->db));
        sqlite3_free(errmsg);
        return -1;
    }
    gmb_sql_track_vtab(d, sql);
    g_err[0] = 0;
    return 0;
}

int gmb_sql_begin(const char* alias) { return gmb_sql_exec(alias, "BEGIN"); }
int gmb_sql_commit(const char* alias) { return gmb_sql_exec(alias, "COMMIT"); }
int gmb_sql_rollback(const char* alias) { return gmb_sql_exec(alias, "ROLLBACK"); }

int64_t gmb_sql_changes(const char* alias)
{
    gmb_sql_db* d = find_alias(alias);
    if (!d) { seterr("unknown SQL alias"); return -1; }
    return (int64_t)sqlite3_changes(d->db);
}

int64_t gmb_sql_last_rowid(const char* alias)
{
    gmb_sql_db* d = find_alias(alias);
    if (!d) { seterr("unknown SQL alias"); return -1; }
    return sqlite3_last_insert_rowid(d->db);
}

int gmb_sql_attach(const char* alias, const char* path, const char* schema)
{
    char sql[1600];
    const char* sch = (schema && schema[0]) ? schema : "aux";
    if (!path || strchr(path, '\'') || strchr(sch, '"')) {
        seterr("invalid ATTACH path/schema");
        return -1;
    }
    snprintf(sql, sizeof(sql), "ATTACH DATABASE '%s' AS \"%s\"", path, sch);
    return gmb_sql_exec(alias, sql);
}

int gmb_sql_detach(const char* alias, const char* schema)
{
    char sql[160];
    const char* sch = (schema && schema[0]) ? schema : "aux";
    if (strchr(sch, '"')) { seterr("invalid DETACH schema"); return -1; }
    snprintf(sql, sizeof(sql), "DETACH DATABASE \"%s\"", sch);
    return gmb_sql_exec(alias, sql);
}

int gmb_sql_table_count(const char* alias)
{
    int64_t rs = gmb_sql_query(alias,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
    int n;
    if (rs <= 0) return -1;
    n = gmb_sql_row_count(rs);
    gmb_sql_close_rs(rs);
    return n;
}

int gmb_sql_table_name(const char* alias, int idx, char* buf, int cap)
{
    int64_t rs = gmb_sql_query(alias,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
    int rc;
    if (rs <= 0) return -1;
    rc = gmb_sql_fetch(rs, idx, 0, buf, cap);
    gmb_sql_close_rs(rs);
    return rc;
}

/* Materializza lo statement (già preparato, parametri già legati) nel result set.
 * Fa la finalize internamente; ritorna lo slot rs o 0 su errore. */
static int64_t sql_materialize(sqlite3_stmt* st)
{
    int slot = -1, i, cols;
    gmb_sql_rs* rs;
    for (i = 1; i < GMB_SQL_RS_MAX; i++)
        if (!g_rs[i].used) { slot = i; break; }
    if (slot < 0) { seterr("too many result sets"); sqlite3_finalize(st); return 0; }
    cols = sqlite3_column_count(st);
    if (cols < 0) cols = 0;
    rs = &g_rs[slot];
    memset(rs, 0, sizeof(*rs));
    rs->used = 1;
    rs->cols = cols;
    rs->cursor = -1;
    /* nomi colonna (dinamici, nessun cap) */
    if (cols > 0) {
        if (!sql_reserve_ints(&rs->noffs, &rs->noffs_cap, (size_t)cols)) { gmb_sql_close_rs(slot); sqlite3_finalize(st); return 0; }
        for (i = 0; i < cols; i++) {
            const char* nm = sqlite3_column_name(st, i);
            size_t nl = strlen(nm ? nm : "");
            if (!sql_reserve(&rs->names, &rs->names_cap, rs->names_len + nl + 1)) { gmb_sql_close_rs(slot); sqlite3_finalize(st); return 0; }
            rs->noffs[i] = (int)rs->names_len;
            memcpy(rs->names + rs->names_len, nm ? nm : "", nl);
            rs->names_len += nl;
            rs->names[rs->names_len++] = 0;
        }
    }
    if (cols < 1) {
        sqlite3_finalize(st);
        g_err[0] = 0;
        return slot;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (!sql_reserve_ints(&rs->offs, &rs->offs_cap, (size_t)(rs->rows + 1) * (size_t)cols)) { gmb_sql_close_rs(slot); sqlite3_finalize(st); return 0; }
        if (!sql_reserve(&rs->ctypes, &rs->ctypes_cap, (size_t)(rs->rows + 1) * (size_t)cols)) { gmb_sql_close_rs(slot); sqlite3_finalize(st); return 0; }
        for (i = 0; i < cols; i++) {
            const unsigned char* t = sqlite3_column_text(st, i);
            const char* s = t ? (const char*)t : "";
            size_t sl = strlen(s);
            if (!sql_reserve(&rs->cells, &rs->cells_cap, rs->cells_len + sl + 1)) { gmb_sql_close_rs(slot); sqlite3_finalize(st); return 0; }
            rs->offs[(size_t)rs->rows * (size_t)cols + (size_t)i] = (int)rs->cells_len;
            rs->ctypes[(size_t)rs->rows * (size_t)cols + (size_t)i] = (char)sqlite3_column_type(st, i);
            memcpy(rs->cells + rs->cells_len, s, sl);
            rs->cells_len += sl;
            rs->cells[rs->cells_len++] = 0;
        }
        rs->rows++;
    }
    sqlite3_finalize(st);
    g_err[0] = 0;
    return slot;
}

int64_t gmb_sql_query(const char* alias, const char* sql)
{
    gmb_sql_db* d = find_alias(alias);
    sqlite3_stmt* st = NULL;
    if (!d) { seterr("unknown SQL alias"); return 0; }
    if (sqlite3_prepare_v2(d->db, sql ? sql : "", -1, &st, NULL) != SQLITE_OK) {
        seterr(sqlite3_errmsg(d->db));
        return 0;
    }
    return sql_materialize(st);
}

int64_t gmb_sql_query_params(const char* alias, const char* sql, const char* const* params, int n)
{
    gmb_sql_db* d = find_alias(alias);
    sqlite3_stmt* st = NULL;
    int i;
    if (!d) { seterr("unknown SQL alias"); return 0; }
    if (sqlite3_prepare_v2(d->db, sql ? sql : "", -1, &st, NULL) != SQLITE_OK) {
        seterr(sqlite3_errmsg(d->db));
        return 0;
    }
    for (i = 0; i < n && params; i++)
        sqlite3_bind_text(st, i + 1, params[i] ? params[i] : "", -1, SQLITE_TRANSIENT);
    return sql_materialize(st);
}

int gmb_sql_exec_params_raw(const char* alias, const char* sql, const char* const* params, int n)
{
    gmb_sql_db* d = find_alias(alias);
    sqlite3_stmt* st = NULL;
    int i, rc;
    if (!d) { seterr("unknown SQL alias"); return -1; }
    /* Nessun parametro → esecuzione MULTI-statement (parità IL/PCode: `SQL EXEC a, "s1; s2"`
     * esegue TUTTI gli statement). Con parametri si resta prepare/step (uno statement, i `?` non sono
     * assegnabili in modo non ambiguo su più statement). Prima il runtime C usava prepare/step anche
     * senza parametri → su VM/C/C99 eseguiva solo il PRIMO statement (divergenza). */
    if (n <= 0) return gmb_sql_exec_raw(alias, sql);
    if (sqlite3_prepare_v2(d->db, sql ? sql : "", -1, &st, NULL) != SQLITE_OK) {
        seterr(sqlite3_errmsg(d->db));
        return -1;
    }
    for (i = 0; i < n && params; i++)
        sqlite3_bind_text(st, i + 1, params[i] ? params[i] : "", -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) { seterr(sqlite3_errmsg(d->db)); return -1; }
    gmb_sql_track_vtab(d, sql);
    g_err[0] = 0;
    return 0;
}

/* SQL.FETCH(rs): avanza il cursore; 1 se la riga esiste (parità IL SqlDriver.Fetch). */
int gmb_sql_fetch_next(int64_t rs)
{
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return 0;
    g_rs[rs].cursor++;
    return g_rs[rs].cursor < g_rs[rs].rows ? 1 : 0;
}

/* Puntatore al testo della cella (row,col) NUL-terminato, o NULL se fuori range/non usado. */
static const char* sql_cell_ptr(int64_t rs, int row, int col)
{
    gmb_sql_rs* r;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return NULL;
    r = &g_rs[rs];
    if (row < 0 || row >= r->rows || col < 0 || col >= r->cols) return NULL;
    return r->cells + r->offs[(size_t)row * (size_t)r->cols + (size_t)col];
}

/* Valore tipizzato di una cella dal suo tipo SQLite (parità IL SqlResultSetField/C# reader):
 * INTEGER → int, FLOAT → double, TEXT/BLOB → stringa, NULL → stringa vuota. Prima si indovinava
 * dal testo (una colonna TEXT di sole cifre tornava numero su VM/C/C99 ma stringa su IL/PCode). */
static gmb_val_t sql_val_of(int t, const char* c)
{
    if (t == SQLITE_INTEGER) return gmb_make_val_int((gmb_int_t)strtoll(c, NULL, 10));
    if (t == SQLITE_FLOAT) return gmb_make_val_double(strtod(c, NULL));
    return gmb_make_val_string(c ? c : "");
}

/* rs("col") sulla riga corrente: valore TIPIZZATO come IL SqlResultSetField. Nessun
 * troncamento: usa direttamente la cella dinamica (prima copiava in `char[512]`). */
gmb_val_t gmb_sql_current_val(int64_t rs, const char* col)
{
    int i;
    const char* c;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return gmb_make_val_string("");
    if (g_rs[rs].cursor < 0 || g_rs[rs].cursor >= g_rs[rs].rows) return gmb_make_val_string("");
    if (!col) return gmb_make_val_string("");
    for (i = 0; i < g_rs[rs].cols; i++)
        if (strcasecmp(g_rs[rs].names + g_rs[rs].noffs[i], col) == 0) break;
    if (i >= g_rs[rs].cols) return gmb_make_val_string("");
    c = sql_cell_ptr(rs, (int)g_rs[rs].cursor, i);
    if (!c) return gmb_make_val_string("");
    return sql_val_of((int)(unsigned char)g_rs[rs].ctypes[(size_t)g_rs[rs].cursor * (size_t)g_rs[rs].cols + (size_t)i], c);
}

/* rs(i) per indice di colonna sulla riga corrente (parità IL SqlResultSetFieldByIndex). */
gmb_val_t gmb_sql_current_val_idx(int64_t rs, int col)
{
    const char* c;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return gmb_make_val_string("");
    if (g_rs[rs].cursor < 0 || g_rs[rs].cursor >= g_rs[rs].rows) return gmb_make_val_string("");
    c = sql_cell_ptr(rs, (int)g_rs[rs].cursor, col);
    if (!c) return gmb_make_val_string("");
    return sql_val_of((int)(unsigned char)g_rs[rs].ctypes[(size_t)g_rs[rs].cursor * (size_t)g_rs[rs].cols + (size_t)col], c);
}

/* Valore tipizzato di una cella (row 0-based, col) — per SQL.COL (intera colonna). */
gmb_val_t gmb_sql_val_at(int64_t rs, int row, int col)
{
    const char* c;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return gmb_make_val_string("");
    if (row < 0 || row >= g_rs[rs].rows || col < 0 || col >= g_rs[rs].cols) return gmb_make_val_string("");
    c = sql_cell_ptr(rs, row, col);
    if (!c) return gmb_make_val_string("");
    return sql_val_of((int)(unsigned char)g_rs[rs].ctypes[(size_t)row * (size_t)g_rs[rs].cols + (size_t)col], c);
}

/* G10: SQL.COL(rs, col): intera colonna come LISTA di valori tipizzati (backend C99). */
gmb_list_t gmb_sql_col_list(int64_t rs, const char* col)
{
    gmb_list_t l = gmb_list_new();
    int i, ci = -1;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used || !col) return l;
    for (i = 0; i < g_rs[rs].cols; i++)
        if (strcasecmp(g_rs[rs].names + g_rs[rs].noffs[i], col) == 0) { ci = i; break; }
    if (ci < 0) return l;
    for (i = 0; i < g_rs[rs].rows; i++) {
        gmb_string_t s = gmb_val_to_string(gmb_sql_val_at(rs, i, ci));
        gmb_list_add(&l, gmb_make_val_string(s.data));
    }
    return l;
}

/* G10: SQL.COL(rs, col) come array C di stringhe (alloc + out-len; backend emitter C#). */
gmb_string_t* gmb_sql_col_stralloc(int64_t rs, const char* col, gmb_int_t* out_len)
{
    int i, ci = -1, n;
    gmb_string_t* arr;
    if (out_len) *out_len = 0;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used || !col) return NULL;
    for (i = 0; i < g_rs[rs].cols; i++)
        if (strcasecmp(g_rs[rs].names + g_rs[rs].noffs[i], col) == 0) { ci = i; break; }
    if (ci < 0) return NULL;
    n = g_rs[rs].rows;
    arr = (gmb_string_t*)calloc(n ? (size_t)n : 1, sizeof(gmb_string_t));
    if (!arr) return NULL;
    for (i = 0; i < n; i++) arr[i] = gmb_val_to_string(gmb_sql_val_at(rs, i, ci));
    if (out_len) *out_len = (gmb_int_t)n;
    return arr;
}

int gmb_sql_row_count(int64_t rs)
{
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return -1;
    return g_rs[rs].rows;
}

int gmb_sql_col_count(int64_t rs)
{
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return -1;
    return g_rs[rs].cols;
}

int gmb_sql_fetch(int64_t rs, int row, int col, char* buf, int cap)
{
    const char* c;
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    c = sql_cell_ptr(rs, row, col);
    if (!c) return -1;
    strncpy(buf, c, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

int gmb_sql_fetch_by_name(int64_t rs, int row, const char* col, char* buf, int cap)
{
    int i;
    if (!col) return -1;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return -1;
    for (i = 0; i < g_rs[rs].cols; i++)
        if (strcasecmp(g_rs[rs].names + g_rs[rs].noffs[i], col) == 0)
            return gmb_sql_fetch(rs, row, i, buf, cap);
    return -1;
}

int gmb_sql_col_name(int64_t rs, int col, char* buf, int cap)
{
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return -1;
    if (col < 0 || col >= g_rs[rs].cols) return -1;
    strncpy(buf, g_rs[rs].names + g_rs[rs].noffs[col], (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

/* --- Ergonomia DataFrame (G10): DESCRIBE / TOCSV / FROMCSV ---------------------- */

/* Scrive un campo CSV (quoting se contiene , " CR LF). */
static void sql_csv_field(FILE* f, const char* s)
{
    int q = 0; const char* p;
    if (!s) s = "";
    for (p = s; *p; p++) if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') { q = 1; break; }
    if (!q) { fputs(s, f); return; }
    fputc('"', f);
    for (p = s; *p; p++) { if (*p == '"') fputc('"', f); fputc(*p, f); }
    fputc('"', f);
}

/* SQL.DESCRIBE(rs): schema come JSON {"rows":n,"columns":[{"name","type"}...]}. 0 ok. */
int gmb_sql_describe(int64_t rs, char* buf, int cap)
{
    int i, o;
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    if (rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return -1;
    o = snprintf(buf, (size_t)cap, "{\"rows\":%d,\"columns\":[", g_rs[rs].rows);
    for (i = 0; i < g_rs[rs].cols; i++) {
        int t = (unsigned char)g_rs[rs].ctypes[i];
        const char* ty = (t == SQLITE_INTEGER) ? "int" : (t == SQLITE_FLOAT) ? "float" : "text";
        int room = (o < cap) ? cap - o : 0;
        o += snprintf(buf + o, (size_t)room, "%s{\"name\":\"%s\",\"type\":\"%s\"}",
                      i ? "," : "", g_rs[rs].names + g_rs[rs].noffs[i], ty);
    }
    { int room = (o < cap) ? cap - o : 0; snprintf(buf + o, (size_t)room, "]}"); }
    return 0;
}

/* SQL.TOCSV(rs, path): scrive header + tutte le righe in CSV. 0 ok, -1 err. */
int gmb_sql_to_csv(int64_t rs, const char* path)
{
    FILE* f; int row, col;
    if (!path || rs < 1 || rs >= GMB_SQL_RS_MAX || !g_rs[rs].used) return -1;
    f = fopen(path, "wb");
    if (!f) return -1;
    for (col = 0; col < g_rs[rs].cols; col++) { if (col) fputc(',', f); sql_csv_field(f, g_rs[rs].names + g_rs[rs].noffs[col]); }
    fputc('\n', f);
    for (row = 0; row < g_rs[rs].rows; row++) {
        for (col = 0; col < g_rs[rs].cols; col++) { if (col) fputc(',', f); sql_csv_field(f, sql_cell_ptr(rs, row, col)); }
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

/* SQL.FROMCSV(alias, table, path): crea la tabella dal CSV (header=nomi colonne, tutte TEXT).
 * Parse manuale (quote/escape) identico a SqlDriver.SqlCsvSplit (C#) → parità 4 backend,
 * senza dipendere dal vtab `csv` (che su C99Native non popolava la tabella). 0 ok. */
static char* sql_str_dup(const char* s)
{
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Split di una riga CSV in campi; ritorna n campi (>0) con i duplicati in *out, -1 su OOM. */
static int csv_split_line(const char* line, char*** out)
{
    int cap = 8, n = 0, q = 0;
    size_t sl = strlen(line), bl = 0, i;
    char** v = (char**)malloc(sizeof(char*) * (size_t)cap);
    char* sb = (char*)malloc(sl + 1);
    if (!v || !sb) { free(v); free(sb); return -1; }
    for (i = 0; i < sl; i++) {
        char ch = line[i];
        if (q) {
            if (ch == '"') { if (i + 1 < sl && line[i + 1] == '"') { sb[bl++] = '"'; i++; } else q = 0; }
            else sb[bl++] = ch;
        } else if (ch == '"') q = 1;
        else if (ch == ',') {
            sb[bl] = 0;
            if (n == cap) { cap *= 2; v = (char**)realloc(v, sizeof(char*) * (size_t)cap); }
            v[n++] = sql_str_dup(sb); bl = 0;
        } else sb[bl++] = ch;
    }
    sb[bl] = 0;
    if (n == cap) { cap++; v = (char**)realloc(v, sizeof(char*) * (size_t)cap); }
    v[n++] = sql_str_dup(sb);
    free(sb);
    *out = v;
    return n;
}

static void csv_free_fields(char** v, int n)
{
    int i;
    if (!v) return;
    for (i = 0; i < n; i++) free(v[i]);
    free(v);
}

int gmb_sql_from_csv(const char* alias, const char* table, const char* path)
{
    char sql[65536];
    char line[65536];
    FILE* f;
    int first = 1;
    if (!alias || !table || !*table || !path) return -1;
    f = fopen(path, "rb");
    if (!f) { seterr("from_csv: file non apribile"); return -1; }
    while (fgets(line, sizeof(line), f)) {
        size_t ll = strlen(line), rem;
        char** fld = NULL;
        char* p;
        int nf, i, w;
        while (ll && (line[ll - 1] == '\n' || line[ll - 1] == '\r')) line[--ll] = 0;
        if (ll == 0) continue;
        nf = csv_split_line(line, &fld);
        if (nf <= 0) continue;
        p = sql; rem = sizeof(sql);
        if (first) {
            first = 0;
            w = snprintf(p, rem, "CREATE TABLE IF NOT EXISTS \"%s\" (", table);
            p += w; rem -= (size_t)w;
            for (i = 0; i < nf; i++) {
                char esc[512];
                int e = 0;
                const char* c;
                for (c = fld[i]; *c && e < 500; c++) { if (*c == '"') esc[e++] = '"'; esc[e++] = *c; }
                esc[e] = 0;
                w = snprintf(p, rem, "%s\"%s\" TEXT", i ? ", " : "", esc);
                p += w; rem -= (size_t)w;
            }
            snprintf(p, rem, ")");
            csv_free_fields(fld, nf);
            if (gmb_sql_exec(alias, sql) != 0) { fclose(f); return -1; }
            continue;
        }
        w = snprintf(p, rem, "INSERT INTO \"%s\" VALUES (", table);
        p += w; rem -= (size_t)w;
        for (i = 0; i < nf; i++) { w = snprintf(p, rem, "%s?", i ? "," : ""); p += w; rem -= (size_t)w; }
        snprintf(p, rem, ")");
        i = gmb_sql_exec_params_raw(alias, sql, (const char* const*)fld, nf);
        csv_free_fields(fld, nf);
        if (i != 0) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

int gmb_sql_close_rs(int64_t rs)
{
    if (rs < 1 || rs >= GMB_SQL_RS_MAX) return -1;
    free(g_rs[rs].cells);
    free(g_rs[rs].offs);
    free(g_rs[rs].ctypes);
    free(g_rs[rs].names);
    free(g_rs[rs].noffs);
    memset(&g_rs[rs], 0, sizeof(g_rs[rs]));
    return 0;
}

static int sql_ident_ok(const char* n)
{
    if (!n || !n[0]) return 0;
    for (; *n; n++) {
        if ((*n >= 'A' && *n <= 'Z') || (*n >= 'a' && *n <= 'z') || (*n >= '0' && *n <= '9') || *n == '_')
            continue;
        return 0;
    }
    return 1;
}

int gmb_sql_savepoint(const char* alias, const char* name)
{
    char sql[96];
    if (!sql_ident_ok(name)) { seterr("invalid savepoint name"); return -1; }
    snprintf(sql, sizeof(sql), "SAVEPOINT \"%s\"", name);
    return gmb_sql_exec(alias, sql);
}

int gmb_sql_release(const char* alias, const char* name)
{
    char sql[96];
    if (!sql_ident_ok(name)) { seterr("invalid savepoint name"); return -1; }
    snprintf(sql, sizeof(sql), "RELEASE SAVEPOINT \"%s\"", name);
    return gmb_sql_exec(alias, sql);
}

int gmb_sql_rollback_to(const char* alias, const char* name)
{
    char sql[96];
    if (!sql_ident_ok(name)) { seterr("invalid savepoint name"); return -1; }
    snprintf(sql, sizeof(sql), "ROLLBACK TO SAVEPOINT \"%s\"", name);
    return gmb_sql_exec(alias, sql);
}

int gmb_sql_exec_scalar(const char* alias, const char* sql, char* buf, int cap)
{
    int64_t rs;
    int rc;
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    rs = gmb_sql_query(alias, sql);
    if (rs <= 0) return -1;
    rc = gmb_sql_fetch(rs, 0, 0, buf, cap);
    gmb_sql_close_rs(rs);
    return rc;
}

int gmb_sql_selfcheck(void)
{
    int64_t rs;
    char buf[64];
    if (gmb_sql_open(":memory:", "T") != 0) return -1;
    if (gmb_sql_exec("T", "CREATE TABLE p (id INTEGER, name TEXT)") != 0) return -2;
    if (gmb_sql_table_count("T") < 1) return -20;
    if (gmb_sql_table_name("T", 0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "p") != 0) return -21;
    if (gmb_sql_exec("T", "INSERT INTO p VALUES (1,'ALICE'),(2,'BOB')") != 0) return -3;
    rs = gmb_sql_query("T", "SELECT name FROM p ORDER BY id");
    if (rs <= 0) return -4;
    if (gmb_sql_row_count(rs) != 2) return -5;
    if (gmb_sql_fetch(rs, 0, 0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "ALICE") != 0) return -6;
    if (gmb_sql_fetch_by_name(rs, 1, "name", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "BOB") != 0) return -15;
    if (gmb_sql_col_name(rs, 0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "name") != 0) return -7;
    if (gmb_sql_begin("T") != 0 || gmb_sql_exec("T", "INSERT INTO p VALUES (3,'CAROL')") != 0) return -8;
    if (gmb_sql_rollback("T") != 0) return -9;
    gmb_sql_close_rs(rs);
    rs = gmb_sql_query("T", "SELECT COUNT(*) FROM p");
    if (rs <= 0) return -10;
    if (gmb_sql_fetch(rs, 0, 0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "2") != 0) return -11;
    gmb_sql_close_rs(rs);
    if (gmb_sql_exec("T", "INSERT INTO p VALUES (3,'CAROL')") != 0) return -12;
    if (gmb_sql_changes("T") != 1) return -13;
    if (gmb_sql_last_rowid("T") < 1) return -14;
    {
        sqlite3* aux = NULL;
        char apath[256];
        snprintf(apath, sizeof(apath), "/tmp/gmb_sql_att.sqlite");
        remove(apath);
        if (sqlite3_open(apath, &aux) != SQLITE_OK) return -16;
        sqlite3_exec(aux, "CREATE TABLE q (n TEXT); INSERT INTO q VALUES ('ZED');", NULL, NULL, NULL);
        sqlite3_close(aux);
        if (gmb_sql_attach("T", apath, "aux") != 0) return -17;
        rs = gmb_sql_query("T", "SELECT n FROM aux.q");
        if (rs <= 0) return -18;
        if (gmb_sql_fetch(rs, 0, 0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "ZED") != 0) return -19;
        gmb_sql_close_rs(rs);
        gmb_sql_detach("T", "aux");
        remove(apath);
    }
    if (gmb_sql_begin("T") != 0) return -22;
    if (gmb_sql_savepoint("T", "sp1") != 0) return -23;
    if (gmb_sql_exec("T", "INSERT INTO p VALUES (4,'DAN')") != 0) return -24;
    if (gmb_sql_rollback_to("T", "sp1") != 0) return -25;
    if (gmb_sql_release("T", "sp1") != 0) return -26;
    if (gmb_sql_commit("T") != 0) return -27;
    if (gmb_sql_exec_scalar("T", "SELECT COUNT(*) FROM p", buf, (int)sizeof(buf)) != 0) return -28;
    if (strcmp(buf, "3") != 0) return -29;
    /* UDF temporali (parità SqlTimeUdf): TIMEBUCKET + FIRST_TS/LAST_TS su tick OHLCV. */
    if (gmb_sql_exec("T", "CREATE TABLE ticks(ts INTEGER, price INTEGER, vol INTEGER)") != 0) return -30;
    if (gmb_sql_exec("T", "INSERT INTO ticks VALUES (0,10,100),(60,12,200),(120,11,150),"
                          "(300,20,50),(360,18,75),(420,22,125),(-60,9,10)") != 0) return -31;
    rs = gmb_sql_query("T", "SELECT TIMEBUCKET(ts,300) AS b, FIRST_TS(price,ts) AS o,"
                            " MAX(price) AS h, MIN(price) AS l, LAST_TS(price,ts) AS c,"
                            " SUM(vol) AS v FROM ticks GROUP BY b ORDER BY b");
    if (rs <= 0) return -32;
    if (gmb_sql_row_count(rs) != 3) return -33;
    {
        const char* exp[3][6] = {
            { "-300", "9", "9", "9", "9", "10" },
            { "0", "10", "12", "10", "11", "450" },
            { "300", "20", "22", "18", "22", "250" },
        };
        const char* cols[6] = { "b", "o", "h", "l", "c", "v" };
        int r, k;
        for (r = 0; r < 3; r++)
            for (k = 0; k < 6; k++) {
                if (gmb_sql_fetch_by_name(rs, r, cols[k], buf, (int)sizeof(buf)) != 0 ||
                    strcmp(buf, exp[r][k]) != 0) return -34 - r * 10 - k;
            }
    }
    gmb_sql_close_rs(rs);
    if (gmb_sql_exec_scalar("T", "SELECT TIMEBUCKET(-1,300)", buf, (int)sizeof(buf)) != 0 ||
        strcmp(buf, "-300") != 0) return -35;
    if (gmb_sql_exec_scalar("T", "SELECT TIMEBUCKET(7,0)", buf, (int)sizeof(buf)) != 0 ||
        strcmp(buf, "") != 0) return -36;
    if (gmb_sql_exec_scalar("T", "SELECT FIRST_TS(price,ts) FROM ticks WHERE 0", buf,
                            (int)sizeof(buf)) != 0 || strcmp(buf, "") != 0) return -37;
    /* Resultsets oltre i vecchi cap (erano 4096 righe / 511 char, silenziosi):
     * 5000 righe con una cella lunga 800 caratteri devono sopravvivere intere. */
    if (gmb_sql_exec("T", "CREATE TABLE big(n INTEGER, s TEXT)") != 0) return -38;
    if (gmb_sql_exec("T", "WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM c WHERE x<5000) "
                          "INSERT INTO big(n,s) SELECT x, printf('%0800d', x) FROM c") != 0) return -39;
    rs = gmb_sql_query("T", "SELECT n, s FROM big ORDER BY n");
    if (rs <= 0) return -40;
    if (gmb_sql_row_count(rs) != 5000) return -41;
    {
        char big2[1024];
        if (gmb_sql_fetch(rs, 4999, 0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "5000") != 0) return -42;
        if (gmb_sql_fetch(rs, 4999, 1, big2, (int)sizeof(big2)) != 0 || strlen(big2) != 800) return -43;
        if (big2[0] != '0' || big2[797] != '0' || big2[798] != '0' || big2[799] != '0') return -44;
    }
    gmb_sql_close_rs(rs);
    /* Aggregati statistici (Welford; set classico 2,4,4,4,5,5,7,9: media 5, var_pop 4). */
    if (gmb_sql_exec("T", "CREATE TABLE sv(x REAL)") != 0) return -45;
    if (gmb_sql_exec("T", "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)") != 0) return -46;
    {
        struct { const char* sql; double want; int isnull; } chk[] = {
            { "SELECT VAR_POP(x) FROM sv",        4.0, 0 },
            { "SELECT STDDEV_POP(x) FROM sv",     2.0, 0 },
            { "SELECT MEDIAN(x) FROM sv",         4.5, 0 },
            { "SELECT VAR_SAMP(x) FROM sv",       32.0 / 7.0, 0 },
            { "SELECT VARIANCE(x) FROM sv",       32.0 / 7.0, 0 },
            { "SELECT STDDEV_SAMP(x) FROM sv",    sqrt(32.0 / 7.0), 0 },
            { "SELECT STDDEV(x) FROM sv",         sqrt(32.0 / 7.0), 0 },
            { "SELECT VAR_POP(x) FROM sv WHERE 0", 0.0, 1 },   /* insieme vuoto -> NULL */
            { "SELECT VAR_SAMP(x) FROM sv WHERE x=2", 0.0, 1 },/* n<2 -> NULL */
            { "SELECT MEDIAN(x) FROM sv WHERE x IN (2,9)", 5.5, 0 },
        };
        size_t ci;
        for (ci = 0; ci < sizeof(chk) / sizeof(chk[0]); ci++) {
            if (gmb_sql_exec_scalar("T", chk[ci].sql, buf, (int)sizeof(buf)) != 0) return -47 - (int)ci;
            if (chk[ci].isnull) { if (buf[0] != 0) return -60 - (int)ci; continue; }
            if (fabs(strtod(buf, NULL) - chk[ci].want) > 1e-9) return -47 - (int)ci;
        }
    }
    /* MODE / QUANTILE (set 2,4,4,4,5,5,7,9): moda 4; quantile 0.5 -> 4.5, 1.0 -> 9, 1.5 -> NULL. */
    if (gmb_sql_exec_scalar("T", "SELECT MODE(x) FROM sv", buf, (int)sizeof(buf)) != 0 ||
        fabs(strtod(buf, NULL) - 4.0) > 1e-12) return -80;
    if (gmb_sql_exec_scalar("T", "SELECT QUANTILE(x,0.5) FROM sv", buf, (int)sizeof(buf)) != 0 ||
        fabs(strtod(buf, NULL) - 4.5) > 1e-12) return -81;
    if (gmb_sql_exec_scalar("T", "SELECT QUANTILE(x,1.0) FROM sv", buf, (int)sizeof(buf)) != 0 ||
        fabs(strtod(buf, NULL) - 9.0) > 1e-12) return -82;
    if (gmb_sql_exec_scalar("T", "SELECT QUANTILE(x,1.5) FROM sv", buf, (int)sizeof(buf)) != 0 ||
        buf[0] != 0) return -83;
    /* CORR/COVAR: y = 2x+1 (correlazione perfetta) -> CORR 1; COVAR_SAMP(y,x)=2*VAR_SAMP(x). */
    if (gmb_sql_exec("T", "CREATE TABLE xy(x REAL, y REAL)") != 0) return -84;
    if (gmb_sql_exec("T", "INSERT INTO xy VALUES (2,5),(4,9),(4,9),(4,9),(5,11),(5,11),(7,15),(9,19)") != 0) return -85;
    if (gmb_sql_exec_scalar("T", "SELECT CORR(y,x) FROM xy", buf, (int)sizeof(buf)) != 0 ||
        fabs(strtod(buf, NULL) - 1.0) > 1e-9) return -86;
    if (gmb_sql_exec_scalar("T", "SELECT COVAR_SAMP(y,x) FROM xy", buf, (int)sizeof(buf)) != 0 ||
        fabs(strtod(buf, NULL) - 2.0 * (32.0 / 7.0)) > 1e-9) return -87;
    if (gmb_sql_exec_scalar("T", "SELECT COVAR_POP(y,x) FROM xy", buf, (int)sizeof(buf)) != 0 ||
        fabs(strtod(buf, NULL) - 2.0 * 4.0) > 1e-9) return -88;
    if (gmb_sql_exec_scalar("T", "SELECT CORR(y,x) FROM xy WHERE 0", buf, (int)sizeof(buf)) != 0 ||
        buf[0] != 0) return -89;
    gmb_sql_close("T");
    return 0;
}

/* ── Wrapper pubblici (usati dal codice emesso/C# via runtime): se lo statement è una
 * scrittura pesante e la GPM ha registrato un hook, la esegue fuori dal worker (park);
 * altrimenti esecuzione sincrona. Le letture/leggere restano sincrone. ── */
int gmb_sql_exec(const char* alias, const char* sql)
{
    if (g_sql_park_hook && gmb_sql_stmt_is_heavy_write(alias, sql))
        return g_sql_park_hook(alias, sql, NULL, 0);
    return gmb_sql_exec_raw(alias, sql);
}
int gmb_sql_exec_params(const char* alias, const char* sql, const char* const* params, int n)
{
    if (g_sql_park_hook && n > 0 && gmb_sql_stmt_is_heavy_write(alias, sql))
        return g_sql_park_hook(alias, sql, params, n);
    return gmb_sql_exec_params_raw(alias, sql, params, n);
}
