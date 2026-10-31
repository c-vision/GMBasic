/* FILE: native/gmb_runtime_c/src/gmb_rdd.c
 * DataSession + driver ARRAY/CSV/SQL (community). Spec: common/Databases/Rdd. */
#include "gmb_community_db.h"
#include "gmb_runtime.h"
#include "gmb_rdd_rowbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include <sqlite3.h>

#define GMB_RDD_MAX_AREA 255
#define GMB_RDD_MAX_FIELDS 32
#define GMB_RDD_CELL 256
#define GMB_RDD_MAX_ROWS 8192

typedef enum { GMB_RDD_NONE = 0, GMB_RDD_ARRAY, GMB_RDD_CSV, GMB_RDD_SDF, GMB_RDD_SQL, GMB_RDD_DBF } gmb_rdd_kind;

typedef struct {
    int used;
    gmb_rdd_kind kind;
    char alias[32];
    char path[1024];
    int eof, bof, recno, lastrec, nfields, found;
    char fields[GMB_RDD_MAX_FIELDS][32];
    int fld_len[GMB_RDD_MAX_FIELDS];
    char* cells; /* lastrec * nfields * CELL */
    unsigned char* del; /* 1 = soft-deleted, lunghezza cap_rows */
    int cap_rows;
    int set_deleted; /* SET DELETED ON */
    char filt_field[32];
    char filt_value[GMB_RDD_CELL];
    char order_field[32];
    int* ord; /* permutazione 0-based, NULL = ordine fisico */
    int nord;
    int locked;      /* lock attivo sull'area (RLOCK/FLOCK) — UNLOCK lo azzera */
    sqlite3* sql;
} gmb_rdd_area;

static gmb_rdd_area g_areas[GMB_RDD_MAX_AREA + 1];
static int g_cur = 1;
/* SET LOCK TIMEOUT: stato globale (il runtime C# ha Locks.LockTimeoutMs nello stesso ruolo). */
static int gmb_rdd_lock_timeout_ms = 0;
static int gmb_rdd_reprocess_ms = 0;

static gmb_rdd_area* cur(void)
{
    if (g_cur < 1 || g_cur > GMB_RDD_MAX_AREA) return NULL;
    return &g_areas[g_cur];
}

static char* cell(gmb_rdd_area* a, int rec0, int fi)
{
    if (!a->cells || rec0 < 0 || rec0 >= a->lastrec || fi < 0 || fi >= a->nfields) return NULL;
    return a->cells + ((size_t)rec0 * (size_t)a->nfields + (size_t)fi) * GMB_RDD_CELL;
}

static void grow_rows(gmb_rdd_area* a, int need)
{
    int cap, old;
    char* p;
    unsigned char* d;
    if (need <= a->cap_rows) return;
    cap = a->cap_rows < 8 ? 8 : a->cap_rows;
    while (cap < need) cap *= 2;
    if (cap > GMB_RDD_MAX_ROWS) cap = GMB_RDD_MAX_ROWS;
    old = a->cap_rows;
    p = (char*)realloc(a->cells, (size_t)cap * (size_t)(a->nfields > 0 ? a->nfields : 1) * GMB_RDD_CELL);
    if (!p) return;
    if (!a->cells)
        memset(p, 0, (size_t)cap * (size_t)(a->nfields > 0 ? a->nfields : 1) * GMB_RDD_CELL);
    else
        memset(p + (size_t)old * (size_t)(a->nfields > 0 ? a->nfields : 1) * GMB_RDD_CELL, 0,
               (size_t)(cap - old) * (size_t)(a->nfields > 0 ? a->nfields : 1) * GMB_RDD_CELL);
    a->cells = p;
    d = (unsigned char*)realloc(a->del, (size_t)cap);
    if (d) {
        if (!a->del) memset(d, 0, (size_t)cap);
        else memset(d + old, 0, (size_t)(cap - old));
        a->del = d;
    }
    a->cap_rows = cap;
}

static int split_csv(const char* csv, char out[][GMB_RDD_CELL], int maxn)
{
    int n = 0;
    const char* p = csv ? csv : "";
    while (*p && n < maxn) {
        int i = 0, quoted = 0;
        if (*p == '"') { quoted = 1; p++; }
        while (*p && i < GMB_RDD_CELL - 1) {
            if (quoted) {
                if (*p == '"' && p[1] == '"') { out[n][i++] = '"'; p += 2; continue; }
                if (*p == '"') { p++; break; }
            } else if (*p == ',') break;
            out[n][i++] = *p++;
        }
        out[n][i] = 0;
        if (*p == ',') p++;
        n++;
    }
    return n;
}

static void sync_flags(gmb_rdd_area* a)
{
    if (!a->used || a->lastrec <= 0) {
        a->eof = 1; a->bof = 1; a->recno = 0; a->found = 0;
        return;
    }
    if (a->recno < 1) { a->recno = 1; a->bof = 1; } else a->bof = 0;
    if (a->recno > a->lastrec) { a->recno = a->lastrec + 1; a->eof = 1; a->found = 0; }
    else { a->eof = 0; }
}

static int field_eq(gmb_rdd_area* a, int rec1, const char* field, const char* value)
{
    int i;
    char* c;
    if (!field || !field[0]) return 1;
    for (i = 0; i < a->nfields; i++)
        if (strcasecmp(a->fields[i], field) == 0) {
            c = cell(a, rec1 - 1, i);
            return c && strcmp(c, value ? value : "") == 0;
        }
    return 0;
}

static int row_visible(gmb_rdd_area* a, int rec1)
{
    if (rec1 < 1 || rec1 > a->lastrec) return 0;
    if (a->set_deleted && a->del && a->del[rec1 - 1]) return 0;
    if (a->filt_field[0] && !field_eq(a, rec1, a->filt_field, a->filt_value)) return 0;
    return 1;
}

static int next_visible(gmb_rdd_area* a, int from, int dir)
{
    int r;
    for (r = from; r >= 1 && r <= a->lastrec; r += dir)
        if (row_visible(a, r)) return r;
    return 0;
}

static gmb_rdd_area* g_sort_area;

static int order_field_idx(gmb_rdd_area* a)
{
    int i;
    if (!a->order_field[0]) return -1;
    for (i = 0; i < a->nfields; i++)
        if (strcasecmp(a->fields[i], a->order_field) == 0) return i;
    return -1;
}

static int cmp_ord(const void* pa, const void* pb)
{
    int ia = *(const int*)pa, ib = *(const int*)pb, fi;
    const char *ca, *cb;
    if (!g_sort_area) return 0;
    fi = order_field_idx(g_sort_area);
    if (fi < 0) return ia - ib;
    ca = cell(g_sort_area, ia, fi);
    cb = cell(g_sort_area, ib, fi);
    return strcmp(ca ? ca : "", cb ? cb : "");
}

static void rebuild_order(gmb_rdd_area* a)
{
    int i;
    if (!a->order_field[0] || a->lastrec < 1) {
        free(a->ord);
        a->ord = NULL;
        a->nord = 0;
        return;
    }
    a->ord = (int*)realloc(a->ord, (size_t)a->lastrec * sizeof(int));
    if (!a->ord) { a->nord = 0; return; }
    for (i = 0; i < a->lastrec; i++) a->ord[i] = i;
    g_sort_area = a;
    qsort(a->ord, (size_t)a->lastrec, sizeof(int), cmp_ord);
    a->nord = a->lastrec;
}

static int log_of(gmb_rdd_area* a, int rec1)
{
    int i;
    if (!a->ord || a->nord < 1) return rec1;
    for (i = 0; i < a->nord; i++)
        if (a->ord[i] + 1 == rec1) return i + 1;
    return rec1;
}

static int next_visible_ord(gmb_rdd_area* a, int from_log, int dir)
{
    int k;
    if (!a->ord || a->nord < 1) return next_visible(a, from_log, dir);
    for (k = from_log; k >= 1 && k <= a->nord; k += dir) {
        int rec1 = a->ord[k - 1] + 1;
        if (row_visible(a, rec1)) return rec1;
    }
    return 0;
}

static int find_empty(void)
{
    int i;
    for (i = 1; i <= GMB_RDD_MAX_AREA; i++)
        if (!g_areas[i].used) return i;
    return 0;
}

int gmb_rdd_select(int area)
{
    if (area < 1) area = find_empty();
    if (area < 1 || area > GMB_RDD_MAX_AREA) return 0;
    g_cur = area;
    return g_cur;
}

int gmb_rdd_select_alias(const char* alias)
{
    int i;
    if (!alias || !alias[0]) return 0;
    for (i = 1; i <= GMB_RDD_MAX_AREA; i++)
        if (g_areas[i].used && strcasecmp(g_areas[i].alias, alias) == 0) {
            g_cur = i;
            return i;
        }
    return 0;
}

/* SELECTED(): numero (1-based) dell'area DBF/ISAM/RDD correntemente selezionata, 0 se
 * nessuna. Stessa semantica di IlRuntime.DbfSelected / del PCode (work-area 1-based):
 * prima la VM/C ritornava -1/0 (un flag) via gmb_dbf_selected, quindi SELECTED() dava -1
 * invece di 1 (§ MiniTests/dbf/dbf_state.bas). */
int gmb_rdd_selected_area(void)
{
    return (g_cur >= 1 && g_cur <= GMB_RDD_MAX_AREA && g_areas[g_cur].used) ? g_cur : 0;
}

int gmb_rdd_used(void)
{
    gmb_rdd_area* a = cur();
    return a && a->used ? 1 : 0;
}

/* Numero GREZZO dell'area selezionata (g_cur), anche quando l'area non è un cell-store
 * RDD (caso ISAM): è la chiave del row-buffer. */
int gmb_rdd_current_area(void) { return (g_cur >= 1 && g_cur <= GMB_RDD_MAX_AREA) ? g_cur : 0; }

static void csv_write_field(FILE* f, const char* s)
{
    const char* p = s ? s : "";
    int q = 0;
    const char* t;
    for (t = p; *t; t++)
        if (*t == ',' || *t == '"' || *t == '\n' || *t == '\r') { q = 1; break; }
    if (!q) { fputs(p, f); return; }
    fputc('"', f);
    for (; *p; p++) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

static int save_csv(gmb_rdd_area* a)
{
    FILE* f;
    int r, c;
    if (!a->path[0]) return -1;
    f = fopen(a->path, "w");
    if (!f) return -1;
    for (c = 0; c < a->nfields; c++) {
        if (c) fputc(',', f);
        csv_write_field(f, a->fields[c]);
    }
    fputc('\n', f);
    for (r = 0; r < a->lastrec; r++) {
        for (c = 0; c < a->nfields; c++) {
            char* cellv = a->cells + ((size_t)r * (size_t)a->nfields + (size_t)c) * GMB_RDD_CELL;
            if (c) fputc(',', f);
            csv_write_field(f, cellv);
        }
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

static int save_sdf(gmb_rdd_area* a)
{
    FILE* f;
    int r;
    if (!a->path[0]) return -1;
    f = fopen(a->path, "w");
    if (!f) return -1;
    for (r = 0; r < a->lastrec; r++) {
        char* c = a->cells + (size_t)r * (size_t)(a->nfields > 0 ? a->nfields : 1) * GMB_RDD_CELL;
        fputs(c ? c : "", f);
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

static int load_sdf(gmb_rdd_area* a, const char* path)
{
    FILE* f = fopen(path, "r");
    char line[GMB_RDD_CELL];
    a->nfields = 1;
    strncpy(a->fields[0], "FLD1", 31);
    if (!f) {
        a->recno = 0;
        sync_flags(a);
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        char* c;
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
        grow_rows(a, a->lastrec + 1);
        if (a->lastrec >= a->cap_rows) { fclose(f); return -1; }
        c = a->cells + (size_t)a->lastrec * (size_t)a->nfields * GMB_RDD_CELL;
        strncpy(c, line, GMB_RDD_CELL - 1);
        a->lastrec++;
    }
    fclose(f);
    a->recno = a->lastrec > 0 ? 1 : 0;
    sync_flags(a);
    return 0;
}

static void close_area(gmb_rdd_area* a)
{
    if (!a) return;
    gmb_rdd_rowbuf_forget((int)(a - g_areas));
    if (a->kind == GMB_RDD_CSV && a->path[0])
        save_csv(a);
    else if (a->kind == GMB_RDD_SDF && a->path[0])
        save_sdf(a);
    if (a->sql) { sqlite3_close(a->sql); a->sql = NULL; }
    free(a->cells);
    free(a->del);
    free(a->ord);
    memset(a, 0, sizeof(*a));
}

int gmb_rdd_close(int area)
{
    if (area <= 0) area = g_cur;
    if (area < 1 || area > GMB_RDD_MAX_AREA) return -1;
    close_area(&g_areas[area]);
    return 0;
}

int gmb_rdd_set_fields(const char* names_csv)
{
    gmb_rdd_area* a = cur();
    char tmp[GMB_RDD_MAX_FIELDS][GMB_RDD_CELL];
    int n, i;
    if (!a || !a->used) return -1;
    gmb_rdd_rowbuf_forget((int)(a - g_areas));
    n = split_csv(names_csv, tmp, GMB_RDD_MAX_FIELDS);
    a->nfields = n;
    for (i = 0; i < n; i++) {
        strncpy(a->fields[i], tmp[i], 31);
        a->fields[i][31] = 0;
    }
    free(a->cells);
    a->cells = NULL;
    free(a->del);
    a->del = NULL;
    free(a->ord);
    a->ord = NULL;
    a->nord = 0;
    a->cap_rows = 0;
    a->lastrec = 0;
    a->recno = 0;
    sync_flags(a);
    return 0;
}

/* Lettore dBase III/III+ (version 0x03/0x83): header + field descriptors + record.
 * Ogni campo viene estratto (trim) e copiato nel modello celle (parita' load_csv). */
static int load_dbf(gmb_rdd_area* a, const char* path)
{
    FILE* f = fopen(path, "rb");
    unsigned char hdr[32], fd[32];
    int i, rec_count, hsize, rsize, nfields;
    if (!f) { a->recno = 0; sync_flags(a); return 0; }
    if (fread(hdr, 1, 32, f) != 32) { fclose(f); return 0; }
    if (hdr[0] != 0x03 && hdr[0] != 0x83 && hdr[0] != 0x30) { fclose(f); return 0; }
    rec_count = (int)((uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24));
    hsize = (int)((uint16_t)hdr[8] | ((uint16_t)hdr[9] << 8));
    rsize = (int)((uint16_t)hdr[10] | ((uint16_t)hdr[11] << 8));
    nfields = (hsize - 33) / 32;
    if (nfields <= 0 || nfields > GMB_RDD_MAX_FIELDS || rsize <= 0) { fclose(f); return 0; }
    a->nfields = 0;
    for (i = 0; i < nfields; i++)
    {
        size_t nlen;
        if (fread(fd, 1, 32, f) != 32) break;
        nlen = 0;
        while (nlen < 11 && fd[nlen]) nlen++;
        if (nlen == 0) break;
        memcpy(a->fields[a->nfields], fd, nlen);
        a->fields[a->nfields][nlen] = '\0';
        a->fld_len[a->nfields] = (int)fd[16];
        if (a->fld_len[a->nfields] <= 0) a->fld_len[a->nfields] = 1;
        a->nfields++;
    }
    {
        unsigned char term = 0;
        if (fread(&term, 1, 1, f) != 1 || term != 0x0D) { fclose(f); return 0; }
    }
    for (i = 0; i < rec_count; i++)
    {
        unsigned char flag = 0;
        unsigned char* raw;
        int off = 0, fi;
        if (fread(&flag, 1, 1, f) != 1) break;
        raw = (unsigned char*)malloc((size_t)rsize + 1);
        if (!raw) break;
        /* rsize include il flag deleted (1 byte): i dati utili sono rsize-1 byte. */
        if (fread(raw, 1, (size_t)(rsize - 1), f) != (size_t)(rsize - 1)) { free(raw); break; }
        grow_rows(a, a->lastrec + 1);
        if (a->lastrec >= a->cap_rows) { free(raw); break; }
        for (fi = 0; fi < a->nfields; fi++)
        {
            int flen = a->fld_len[fi];
            char* c = a->cells + ((size_t)a->lastrec * (size_t)a->nfields + (size_t)fi) * GMB_RDD_CELL;
            int k = 0, j;
            for (j = 0; j < flen && off + j < rsize; j++)
            {
                char ch = (char)raw[off + j];
                if (ch != ' ' && ch != '\0' && k < GMB_RDD_CELL - 1) c[k++] = ch;
            }
            c[k] = '\0';
            off += flen;
        }
        if (flag == 0x2A && a->del && a->lastrec >= 0) a->del[a->lastrec] = 1;
        a->lastrec++;
        free(raw);
    }
    fclose(f);
    a->recno = a->lastrec > 0 ? 1 : 0;
    sync_flags(a);
    return a->lastrec > 0 ? 0 : 0;
}

static int load_csv(gmb_rdd_area* a, const char* path)
{
    FILE* f = fopen(path, "r");
    char line[4096];
    int first = 1;
    if (!f) {
        a->recno = 0;
        sync_flags(a);
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        char cols[GMB_RDD_MAX_FIELDS][GMB_RDD_CELL];
        int n, i;
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
        if (!line[0]) continue;
        n = split_csv(line, cols, GMB_RDD_MAX_FIELDS);
        if (first) {
            a->nfields = n;
            for (i = 0; i < n; i++) {
                strncpy(a->fields[i], cols[i], 31);
                a->fields[i][31] = 0;
            }
            first = 0;
            continue;
        }
        grow_rows(a, a->lastrec + 1);
        if (a->lastrec >= a->cap_rows) { fclose(f); return -1; }
        for (i = 0; i < a->nfields; i++) {
            char* c = a->cells + ((size_t)a->lastrec * (size_t)a->nfields + (size_t)i) * GMB_RDD_CELL;
            strncpy(c, i < n ? cols[i] : "", GMB_RDD_CELL - 1);
        }
        a->lastrec++;
    }
    fclose(f);
    a->recno = a->lastrec > 0 ? 1 : 0;
    sync_flags(a);
    return 0;
}

static int load_sql_table(gmb_rdd_area* a)
{
    char sql[256];
    sqlite3_stmt* st = NULL;
    int rc, i, ncol;
    snprintf(sql, sizeof(sql), "SELECT * FROM \"%s\"", a->alias);
    rc = sqlite3_prepare_v2(a->sql, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) {
        snprintf(sql, sizeof(sql), "SELECT * FROM sqlite_master WHERE type='table' LIMIT 1");
        return -1;
    }
    ncol = sqlite3_column_count(st);
    if (ncol > GMB_RDD_MAX_FIELDS) ncol = GMB_RDD_MAX_FIELDS;
    a->nfields = ncol;
    for (i = 0; i < ncol; i++) {
        const char* nm = sqlite3_column_name(st, i);
        strncpy(a->fields[i], nm ? nm : "", 31);
        a->fields[i][31] = 0;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        grow_rows(a, a->lastrec + 1);
        if (a->lastrec >= a->cap_rows) break;
        for (i = 0; i < ncol; i++) {
            const unsigned char* t = sqlite3_column_text(st, i);
            char* c = a->cells + ((size_t)a->lastrec * (size_t)a->nfields + (size_t)i) * GMB_RDD_CELL;
            strncpy(c, t ? (const char*)t : "", GMB_RDD_CELL - 1);
        }
        a->lastrec++;
    }
    sqlite3_finalize(st);
    a->recno = a->lastrec > 0 ? 1 : 0;
    sync_flags(a);
    return 0;
}

int gmb_rdd_use(const char* path, const char* alias, const char* driver)
{
    int area = find_empty();
    gmb_rdd_area* a;
    char drv[16];
    int i;
    if (!area) return 0;
    g_cur = area;
    a = &g_areas[area];
    close_area(a);
    a->used = 1;
    strncpy(a->alias, alias ? alias : "", 31);
    strncpy(a->path, path ? path : "", 1023);
    drv[0] = 0;
    if (driver) {
        for (i = 0; driver[i] && i < 15; i++) drv[i] = (char)toupper((unsigned char)driver[i]);
        drv[i] = 0;
    }
    if (!drv[0]) {
        /* Senza driver esplicito lo si deduce dal FILE: un .dbf e' un DBF. Prima ogni
         * "RDD USE file.dbf" senza driver diventava ARRAY, quindi VALTYPE() rispondeva
         * "ARRAY" invece di "DBF" (§ MiniTests/dbf/rdd_fields.bas). */
        size_t pl = strlen(a->path);
        if (pl >= 4 && strcasecmp(a->path + pl - 4, ".dbf") == 0) { drv[0] = 'D'; drv[1] = 'B'; drv[2] = 'F'; drv[3] = 0; }
        /* .csv/.tsv → CSV (parità IL: un CSV senza VIA è un DelimitedTextDriver, NON un
         * ARRAY vuoto — senza, l'area CSV non si popolava e il loop saltava, bug
         * CustomerOrdersReport.bas). */
        else if (pl >= 4 && (strcasecmp(a->path + pl - 4, ".csv") == 0
                             || strcasecmp(a->path + pl - 4, ".tsv") == 0))
        { drv[0] = 'C'; drv[1] = 'S'; drv[2] = 'V'; drv[3] = 0; }
        /* Parità DriverRegistry C#: .delim → DELIM, .sdf → SDF. Senza, un RDD USE per estensione
         * su questi file cadeva su ARRAY (vuoto) → nessuna riga (TextDriversDemo.bas §4-5). */
        else if (pl >= 6 && strcasecmp(a->path + pl - 6, ".delim") == 0)
        { drv[0] = 'D'; drv[1] = 'E'; drv[2] = 'L'; drv[3] = 'I'; drv[4] = 'M'; drv[5] = 0; }
        else if (pl >= 4 && strcasecmp(a->path + pl - 4, ".sdf") == 0)
        { drv[0] = 'S'; drv[1] = 'D'; drv[2] = 'F'; drv[3] = 0; }
    }
    if (!drv[0] || strcmp(drv, "ARRAY") == 0) {
        a->kind = GMB_RDD_ARRAY;
        a->nfields = 0;
        sync_flags(a);
        return area;
    }
    if (strcmp(drv, "CSV") == 0 || strcmp(drv, "DELIM") == 0) {
        a->kind = GMB_RDD_CSV;
        if (load_csv(a, path) != 0) { close_area(a); return 0; }
        return area;
    }
    if (strcmp(drv, "SDF") == 0) {
        a->kind = GMB_RDD_SDF;
        if (load_sdf(a, path) != 0) { close_area(a); return 0; }
        return area;
    }
    if (strcmp(drv, "DBF") == 0) {
        a->kind = GMB_RDD_DBF;
        if (load_dbf(a, path) != 0) { close_area(a); return 0; }
        return area;
    }
    if (strcmp(drv, "SQL") == 0 || strcmp(drv, "SQLITE") == 0) {
        a->kind = GMB_RDD_SQL;
        if (sqlite3_open(path && path[0] ? path : ":memory:", &a->sql) != SQLITE_OK) {
            close_area(a);
            return 0;
        }
        load_sql_table(a);
        return area;
    }
    close_area(a);
    return 0;
}

int gmb_rdd_gotop(void)
{
    gmb_rdd_area* a = cur();
    int r;
    if (!a || !a->used) return -1;
    if (a->ord && a->nord > 0)
        r = next_visible_ord(a, 1, 1);
    else
        r = next_visible(a, 1, 1);
    a->recno = r ? r : (a->lastrec > 0 ? a->lastrec + 1 : 0);
    sync_flags(a);
    if (!r) a->found = 0;
    return 0;
}

int gmb_rdd_gobottom(void)
{
    gmb_rdd_area* a = cur();
    int r;
    if (!a || !a->used) return -1;
    if (a->ord && a->nord > 0)
        r = next_visible_ord(a, a->nord, -1);
    else
        r = next_visible(a, a->lastrec, -1);
    a->recno = r ? r : (a->lastrec > 0 ? a->lastrec + 1 : 0);
    sync_flags(a);
    if (!r) a->found = 0;
    return 0;
}

int gmb_rdd_go(int recno)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used) return -1;
    a->recno = recno;
    sync_flags(a);
    return 0;
}

int gmb_rdd_skip(int n)
{
    gmb_rdd_area* a = cur();
    int dir, left, r;
    if (!a || !a->used) return -1;
    if (n == 0) { sync_flags(a); return 0; }
    dir = n > 0 ? 1 : -1;
    left = n > 0 ? n : -n;
    r = a->recno;
    while (left > 0) {
        int nxt;
        if (a->ord && a->nord > 0) {
            int log = log_of(a, r);
            nxt = next_visible_ord(a, log + dir, dir);
        } else {
            nxt = next_visible(a, r + dir, dir);
        }
        if (!nxt) {
            if (dir > 0) {
                a->recno = a->lastrec + 1;
                sync_flags(a);
            } else {
                int first = next_visible(a, 1, 1);
                a->recno = first ? first : 1;
                sync_flags(a);
                a->bof = 1;
            }
            return 0;
        }
        r = nxt;
        left--;
    }
    a->recno = r;
    sync_flags(a);
    return 0;
}

int gmb_rdd_eof(void) { gmb_rdd_area* a = cur(); return a && a->used ? a->eof : 1; }
int gmb_rdd_bof(void) { gmb_rdd_area* a = cur(); return a && a->used ? a->bof : 1; }
int gmb_rdd_recno(void) { gmb_rdd_area* a = cur(); return a && a->used ? a->recno : 0; }
/* 1 se l'area RDD corrente e' un file DBF (RDD USE x.dbf): in IL DBF.RECNO() legge il suo driver DBF (record corrente); per CSV/SDF/ARRAY vale 0. */
int gmb_rdd_cur_is_dbf(void) { gmb_rdd_area* a = cur(); return a && a->used && a->kind == GMB_RDD_DBF; }
int gmb_rdd_lastrec(void) { gmb_rdd_area* a = cur(); return a && a->used ? a->lastrec : 0; }
int gmb_rdd_field_count(void) { gmb_rdd_area* a = cur(); return a && a->used ? a->nfields : 0; }

int gmb_rdd_field_name(int idx, char* buf, int cap)
{
    gmb_rdd_area* a = cur();
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    if (!a || !a->used || idx < 0 || idx >= a->nfields) return -1;
    strncpy(buf, a->fields[idx], (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

/* Raw (senza buffer) sul cell-store RDD: lettura campo `idx` (0-based) del record corrente. */
int gmb_rdd_raw_read(int idx, char* buf, int cap)
{
    gmb_rdd_area* a = cur();
    char* c;
    if (!buf || cap <= 0) return -1;
    if (!a || !a->used || a->eof || a->recno < 1) return -1;
    c = cell(a, a->recno - 1, idx);
    if (!c) return -1;
    strncpy(buf, c, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

/* Pubblico: con row-buffer attivo sull'area si legge DAL BUFFER (ADR §6); altrimenti
 * cell-store RDD, poi motore ISAM via hook (l'area ISAM non ha celle RDD). */
/* Convenzione Clipper 1-based: un indice di campo < 0 (colonna, cioè ordinale < 1) è un
 * ERRORE, non un accesso silenzioso a vuoto. Parità IL/PCode (§ RddRequireValidField). */
static void gmb_rdd_bad_field(void)
{
    gmb_raise_error(5, "RDD: indice campo non valido (convenzione Clipper 1-based)", 0L);
}

int gmb_rdd_get_field(int idx, char* buf, int cap)
{
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    if (idx < 0) { gmb_rdd_bad_field(); return -1; }
    if (gmb_rdd_rb_read(gmb_rdd_current_area(), idx, buf, cap)) return 0;
    /* Area RDD vince: mai cadere sull'ISAM quando il cell-store è attivo. */
    if (gmb_rdd_used()) return gmb_rdd_raw_read(idx, buf, cap);
    return gmb_rdd_engine_isam_read(idx, buf, cap);
}

int gmb_rdd_get_field_by_name(const char* name, char* buf, int cap)
{
    int i = gmb_rdd_field_index(name);   /* cell-store + fallback hook ISAM */
    if (i < 0) return -1;
    return gmb_rdd_get_field(i, buf, cap);
}

/* Getter RDD TIPIZZATO (gmb_val_t): numero se la cella è un numero, altrimenti stringa.
 * Serve al C99Native per l'assegnazione `v = RDD.GETFIELD(...)` (target numerico)
 * senza l'errore "gmb_double_t = gmb_string_t": parità con IL (RddTypedValue) e VM-C (bout
 * is_num). Il consumo passa dalle coercizioni val già esistenti (val_to_double/string). */
gmb_val_t gmb_rdd_get_field_val(int idx)
{
    char buf[GMB_RDD_CELL]; buf[0] = 0;
    gmb_rdd_get_field(idx, buf, sizeof(buf));
    /* Driver TESTO (CSV/DELIM/SDF): il campo è dichiarato STRINGA → val STRINGA (parità IL
     * RddTypedValue, che converte a double SOLO per campi NUMERICI typati). Senza, "1" → double
     * → PRINT " 1 " (padding Clipper) vs IL "1" (TextDriversDemo.bas §1). */
    { gmb_rdd_area* a = cur(); if (a && (a->kind == GMB_RDD_CSV || a->kind == GMB_RDD_SDF)) return gmb_make_val_string(buf); }
    { char* e = NULL; double d = strtod(buf, &e); if (buf[0] && e && *e == 0) return gmb_make_val_double(d); }
    return gmb_make_val_string(buf);
}
/* True se l'area corrente e' un driver TESTO (CSV/DELIM/SDF): i suoi campi sono STRINGHE
 * (parità IL RddTypedValue), quindi i getter non devono convertire a numero. */
int gmb_rdd_area_is_text(void)
{
    gmb_rdd_area* a = cur();
    return a && a->used && (a->kind == GMB_RDD_CSV || a->kind == GMB_RDD_SDF) ? 1 : 0;
}

gmb_val_t gmb_rdd_get_field_val_by_name(const char* name)
{
    char buf[GMB_RDD_CELL]; buf[0] = 0;
    gmb_rdd_get_field_by_name(name, buf, sizeof(buf));
    { char* e = NULL; double d = strtod(buf, &e); if (buf[0] && e && *e == 0) return gmb_make_val_double(d); }
    return gmb_make_val_string(buf);
}

/* Late-binding campo nudo Clipper (parità IL IlRuntime.DbfLateBindField, che ritorna STRINGA):
 * sempre stringa VERBATIM della cella, così PRINT produce "100" (non " 100 " del numero canonico
 * gmb_val_to_string). Usato solo da ensure_dbf_field_var ramo RDD, non da RDD.GETFIELD. */
gmb_val_t gmb_rdd_get_field_str_name(const char* name)
{
    char buf[GMB_RDD_CELL]; buf[0] = 0;
    gmb_rdd_get_field_by_name(name, buf, sizeof(buf));
    return gmb_make_val_string(buf);
}
gmb_val_t gmb_rdd_oldval_val(int idx)
{
    const char* ov = gmb_rdd_oldval(idx);
    { char* e = NULL; double d = strtod(ov, &e); if (ov[0] && e && *e == 0) return gmb_make_val_double(d); }
    return gmb_make_val_string(ov);
}

int gmb_rdd_append(const char* values_csv)
{
    gmb_rdd_area* a = cur();
    char cols[GMB_RDD_MAX_FIELDS][GMB_RDD_CELL];
    int n, i;
    if (!a || !a->used) return -1;
    n = split_csv(values_csv, cols, GMB_RDD_MAX_FIELDS);
    if (a->nfields == 0) {
        a->nfields = n;
        for (i = 0; i < n; i++) snprintf(a->fields[i], 32, "F%d", i + 1);
    }
    grow_rows(a, a->lastrec + 1);
    if (a->lastrec >= a->cap_rows) return -1;
    for (i = 0; i < a->nfields; i++) {
        char* c = a->cells + ((size_t)a->lastrec * (size_t)a->nfields + (size_t)i) * GMB_RDD_CELL;
        strncpy(c, i < n ? cols[i] : "", GMB_RDD_CELL - 1);
    }
    if (a->del) a->del[a->lastrec] = 0;
    a->lastrec++;
    a->recno = a->lastrec;
    rebuild_order(a);
    sync_flags(a);
    return 0;
}

/* Raw (senza buffer) sul cell-store RDD: scrittura campo `idx` (0-based) del record corrente. */
int gmb_rdd_raw_write(int idx, const char* value)
{
    gmb_rdd_area* a = cur();
    char* c;
    if (!a || !a->used || a->eof || a->recno < 1) return -1;
    c = cell(a, a->recno - 1, idx);
    if (!c) return -1;
    strncpy(c, value ? value : "", GMB_RDD_CELL - 1);
    return 0;
}

/* Pubblico: con row-buffer attivo la scrittura è DEVIATA nel buffer e il motore non viene
 * toccato finché TABLEUPDATE non applica (ADR §6); poi cell-store RDD, poi ISAM via hook
 * (il ramo ISAM prima viveva inline nel layer VM/C — migrato qui per §0bis). */
int gmb_rdd_replace_field(int idx, const char* value)
{
    /* Su un'area SENZA record corrente (vuota) un REPLACE è un NO-OP, anche con indice campo
     * non valido — parità IL (RDD.REPLACE / RDD REPLACE ... ALL su area ARRAY vuota → 0, non
     * errore; RDD_Omonime.bas §3). L'errore "campo non valido" scatta solo su un record reale. */
    if (idx < 0) { if (!gmb_rdd_used() || gmb_rdd_recno() <= 0) return 0; gmb_rdd_bad_field(); return -1; }
    if (gmb_rdd_rb_write(gmb_rdd_current_area(), idx, value)) return 0;
    if (gmb_rdd_used()) return gmb_rdd_raw_write(idx, value);
    return gmb_rdd_engine_isam_write_field(idx, value);
}

/* Pubblico: RDD REPLACE campo WITH valore [ALL|REST|NEXT n|RECORD n|FOR <cond>|WHILE <cond>].
 * Scope applicato sul cell-store RDD (parità VM/C gmb_rdd_scope_apply, §0bis): unica sorgente
 * condivisa tra VM/C e C99Native. scope: -1=corrente, 0=FOR, 1=WHILE, 2=NEXT n, 3=ALL,
 * 4=RECORD n, 5=REST. cond = testo "campo OP letterale" (gmb_rdd_condition_match).
 * Ritorna il numero di record toccati, o -1 se la condizione non è valutabile. */
int gmb_rdd_replace_scoped(int idx, const char* value, int scope, int n, const char* cond)
{
    /* NIENTE check anticipato di idx: su un'area VUOTA (0 record) un REPLACE con scope
     * (ALL/FOR/...) è un NO-OP (parità IL: RDD REPLACE Nome WITH ... ALL su area ARRAY vuota
     * → OK). L'errore "campo non valido" scatta solo se si tocca davvero un record
     * (gmb_rdd_replace_field lo solleva al primo record). Vedi RDD_ScopeCommands.bas §4. */
    if (scope < 0) return gmb_rdd_replace_field(idx, value) == 0 ? 1 : 0;   /* record corrente */
    if (scope == 0 || scope == 1)   /* FOR / WHILE */
    {
        int done = 0, m;
        if (scope == 0) gmb_rdd_gotop();
        while (!gmb_rdd_eof())
        {
            m = gmb_rdd_condition_match(cond);
            if (m < 0) return -1;
            if (!m) { if (scope == 1) break; else { gmb_rdd_skip(1); continue; } }
            if (gmb_rdd_replace_field(idx, value) == 0) done++;
            gmb_rdd_skip(1);
        }
        return done;
    }
    if (scope == 3 || scope == 5 || scope == 2)   /* ALL / REST / NEXT n */
    {
        int done = 0;
        if (scope != 5) gmb_rdd_gotop();
        if (scope == 2 && n < 1) n = 1;
        while (!gmb_rdd_eof())
        {
            if (scope == 2 && done >= n) break;
            if (gmb_rdd_replace_field(idx, value) == 0) done++;
            gmb_rdd_skip(1);
        }
        return done;
    }
    if (scope == 4)   /* RECORD n */
    {
        if (gmb_rdd_go(n) != 0) return -1;
        return gmb_rdd_replace_field(idx, value) == 0 ? 1 : 0;
    }
    return -1;
}

/* Raw accessors usati dal row-buffer (gmb_rdd_rowbuf.c) sul cell-store RDD. */
int gmb_rdd_raw_field_count(void)
{
    gmb_rdd_area* a = cur();
    return (a && a->used) ? a->nfields : 0;
}

int gmb_rdd_raw_field_name(int ord0, char* buf, int cap)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used || ord0 < 0 || ord0 >= a->nfields || !buf || cap <= 0) return -1;
    strncpy(buf, a->fields[ord0], (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

int gmb_rdd_raw_recno(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used || a->eof || a->recno < 1) return 0;
    return a->recno;
}

int gmb_rdd_raw_deleted(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used || a->recno < 1 || a->recno > a->lastrec || !a->del) return 0;
    return a->del[a->recno - 1] ? 1 : 0;
}

int gmb_rdd_delete(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used || a->eof || a->recno < 1 || a->lastrec < 1) return -1;
    grow_rows(a, a->lastrec);
    if (!a->del) return -1;
    a->del[a->recno - 1] = 1;
    if (a->set_deleted) return gmb_rdd_skip(1);
    return 0;
}

int gmb_rdd_recall(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used || a->eof || a->recno < 1) return -1;
    if (a->del) a->del[a->recno - 1] = 0;
    return 0;
}

/* RECALL(scope [, expr]) → NUMERO di record recallati (convenzione del mondo gestito: come
 * IlRuntime, che ritorna il conteggio — § MiniTests/dbf/rdd_fields.bas: atteso 2 con scope
 * "ALL" su 2 record). La variante a zero argomenti sopra recalla solo il record corrente e
 * ritorna 0 = ok (convenzione C): qui si aggiunge quella con scope, che è cosa serve alla
 * VM/C per non divergere. */
int gmb_rdd_recall_scoped(const char* scope)
{
    gmb_rdd_area* a = cur();
    int n, r;
    if (!a || !a->used) return -1;
    /* scope CURRENT: solo il record corrente (parity IlRuntime.ParseFunctionScope → RecordN/Current). */
    if (scope && strcasecmp(scope, "CURRENT") == 0) {
        int idx = a->recno - 1;
        if (a->del && idx >= 0 && idx < a->lastrec) { a->del[idx] = 0; return 1; }
        return 0;
    }
    if (!scope || !*scope || strcasecmp(scope, "ALL") == 0) n = a->lastrec;
    else n = a->lastrec;   /* NEXT/REST/WHILE: la VM/C non ha ancora il predicato → tutto */
    if (n < 0) n = 0;
    if (n > a->lastrec) n = a->lastrec;
    for (r = 0; r < n; r++) if (a->del) a->del[r] = 0;
    return n;
}

int gmb_rdd_pack(void)
{
    gmb_rdd_area* a = cur();
    int r, w, bytes;
    if (!a || !a->used) return -1;
    if (!a->del) return 0;
    bytes = a->nfields > 0 ? a->nfields * GMB_RDD_CELL : GMB_RDD_CELL;
    w = 0;
    for (r = 0; r < a->lastrec; r++) {
        if (a->del[r]) continue;
        if (w != r && a->cells)
            memcpy(a->cells + (size_t)w * (size_t)bytes,
                   a->cells + (size_t)r * (size_t)bytes, (size_t)bytes);
        a->del[w] = 0;
        w++;
    }
    a->lastrec = w;
    a->recno = w > 0 ? 1 : 0;
    rebuild_order(a);
    sync_flags(a);
    return 0;
}

int gmb_rdd_deleted(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used || a->eof || a->recno < 1 || !a->del) return 0;
    return a->del[a->recno - 1] ? 1 : 0;
}

int gmb_rdd_set_deleted(int on)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used) return -1;
    a->set_deleted = on ? 1 : 0;
    return 0;
}

int gmb_rdd_set_filter(const char* field, const char* value)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used) return -1;
    if (!field || !field[0]) {
        a->filt_field[0] = 0;
        a->filt_value[0] = 0;
        return 0;
    }
    strncpy(a->filt_field, field, 31);
    a->filt_field[31] = 0;
    strncpy(a->filt_value, value ? value : "", GMB_RDD_CELL - 1);
    a->filt_value[GMB_RDD_CELL - 1] = 0;
    return gmb_rdd_gotop();
}

int gmb_rdd_clear_filter(void)
{
    return gmb_rdd_set_filter(NULL, NULL);
}

int gmb_rdd_zap(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used) return -1;
    a->lastrec = 0;
    a->recno = 0;
    if (a->del && a->cap_rows) memset(a->del, 0, (size_t)a->cap_rows);
    rebuild_order(a);
    sync_flags(a);
    return 0;
}

int gmb_rdd_found(void)
{
    gmb_rdd_area* a = cur();
    return a && a->used ? a->found : 0;
}

int gmb_rdd_alias(char* buf, int cap)
{
    gmb_rdd_area* a = cur();
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    if (!a || !a->used) return -1;
    strncpy(buf, a->alias, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

int gmb_rdd_driver(char* buf, int cap)
{
    gmb_rdd_area* a = cur();
    const char* n = "ARRAY";
    if (!buf || cap <= 0) return -1;
    buf[0] = 0;
    if (!a || !a->used) return -1;
    if (a->kind == GMB_RDD_CSV) n = "CSV";
    else if (a->kind == GMB_RDD_SDF) n = "SDF";
    else if (a->kind == GMB_RDD_SQL) n = "SQL";
    else if (a->kind == GMB_RDD_DBF) n = "DBF";
    strncpy(buf, n, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 0;
}

int gmb_rdd_locate(const char* field, const char* value)
{
    gmb_rdd_area* a = cur();
    int rec, save;
    char buf[GMB_RDD_CELL];
    if (!a || !a->used || !field) return -1;
    save = a->recno;
    for (rec = 1; rec <= a->lastrec; rec++) {
        if (!row_visible(a, rec)) continue;
        a->recno = rec;
        sync_flags(a);
        if (gmb_rdd_get_field_by_name(field, buf, (int)sizeof(buf)) == 0
            && strcmp(buf, value ? value : "") == 0) {
            a->found = 1;
            return 1;
        }
    }
    a->recno = save;
    sync_flags(a);
    a->found = 0;
    return 0;
}

int gmb_rdd_set_order(const char* field)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used) return -1;
    if (!field || !field[0]) a->order_field[0] = 0;
    else {
        strncpy(a->order_field, field, 31);
        a->order_field[31] = 0;
    }
    rebuild_order(a);
    return gmb_rdd_gotop();
}

int gmb_rdd_seek(const char* value)
{
    gmb_rdd_area* a = cur();
    const char* field;
    if (!a || !a->used) return -1;
    field = a->order_field[0] ? a->order_field : (a->nfields > 0 ? a->fields[0] : NULL);
    if (!field) { a->found = 0; return 0; }
    return gmb_rdd_locate(field, value);
}

int gmb_rdd_close_all(void)
{
    int i;
    for (i = 1; i <= GMB_RDD_MAX_AREA; i++)
        if (g_areas[i].used) close_area(&g_areas[i]);
    g_cur = 1;
    return 0;
}

int gmb_rdd_copy_to(const char* path, const char* driver)
{
    gmb_rdd_area* a = cur();
    char drv[16];
    int i, r, c;
    FILE* f;
    if (!a || !a->used || !path || !path[0]) return -1;
    drv[0] = 0;
    if (driver) {
        for (i = 0; driver[i] && i < 15; i++) drv[i] = (char)toupper((unsigned char)driver[i]);
        drv[i] = 0;
    }
    f = fopen(path, "w");
    if (!f) return -1;
    if (strcmp(drv, "SDF") == 0) {
        for (r = 1; r <= a->lastrec; r++) {
            char* cellv;
            if (!row_visible(a, r)) continue;
            cellv = cell(a, r - 1, 0);
            fputs(cellv ? cellv : "", f);
            fputc('\n', f);
        }
        fclose(f);
        return 0;
    }
    for (c = 0; c < a->nfields; c++) {
        if (c) fputc(',', f);
        csv_write_field(f, a->fields[c]);
    }
    fputc('\n', f);
    for (r = 1; r <= a->lastrec; r++) {
        if (!row_visible(a, r)) continue;
        for (c = 0; c < a->nfields; c++) {
            char* cellv = cell(a, r - 1, c);
            if (c) fputc(',', f);
            csv_write_field(f, cellv ? cellv : "");
        }
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

int gmb_rdd_selfcheck(void)
{
    char buf[64];
    char csvpath[256];
    char dbpath[256];
    FILE* f;
    sqlite3* db = NULL;
    int area, a2;
    area = gmb_rdd_use(NULL, "A", "ARRAY");
    if (area < 1) return -1;
    if (gmb_rdd_set_fields("ID,NAME") != 0) return -2;
    if (gmb_rdd_append("1,ALICE") != 0 || gmb_rdd_append("2,BOB") != 0) return -3;
    if (gmb_rdd_lastrec() != 2) return -4;
    gmb_rdd_gotop();
    if (gmb_rdd_get_field(1, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "ALICE") != 0) return -5;
    gmb_rdd_skip(1);
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "BOB") != 0) return -6;
    if (gmb_rdd_select_alias("A") != area) return -7;
    gmb_rdd_gotop();
    if (!gmb_rdd_found()) return -22;
    if (gmb_rdd_locate("NAME", "BOB") != 1) return -23;
    if (gmb_rdd_recno() != 2) return -24;
    if (gmb_rdd_locate("NAME", "NOPE") != 0 || gmb_rdd_found()) return -25;
    if (gmb_rdd_alias(buf, (int)sizeof(buf)) != 0 || strcmp(buf, "A") != 0) return -26;
    if (gmb_rdd_driver(buf, (int)sizeof(buf)) != 0 || strcmp(buf, "ARRAY") != 0) return -27;
    gmb_rdd_gotop();
    if (gmb_rdd_delete() != 0) return -16;
    if (gmb_rdd_lastrec() != 2) return -17; /* soft-delete: LASTREC invariato */
    if (!gmb_rdd_deleted()) return -28;
    if (gmb_rdd_recall() != 0 || gmb_rdd_deleted()) return -29;
    gmb_rdd_gotop();
    if (gmb_rdd_delete() != 0) return -30;
    gmb_rdd_set_deleted(1);
    gmb_rdd_gotop();
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "BOB") != 0) return -18;
    if (gmb_rdd_pack() != 0) return -31;
    if (gmb_rdd_lastrec() != 1) return -32;
    gmb_rdd_set_deleted(0);
    gmb_rdd_append("3,CAROL");
    gmb_rdd_set_filter("NAME", "CAROL");
    gmb_rdd_gotop();
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "CAROL") != 0) return -33;
    gmb_rdd_clear_filter();
    gmb_rdd_set_order("NAME");
    gmb_rdd_gotop();
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "BOB") != 0) return -40;
    if (gmb_rdd_seek("CAROL") != 1) return -41;
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "CAROL") != 0) return -42;
    if (gmb_rdd_seek("NOPE") != 0 || gmb_rdd_found()) return -43;
    gmb_rdd_set_order(NULL);
    gmb_rdd_gotop();
    if (gmb_rdd_copy_to("/tmp/gmb_rdd_copy.csv", "CSV") != 0) return -34;
    gmb_rdd_close_all();

    snprintf(csvpath, sizeof(csvpath), "/tmp/gmb_rdd_sc.csv");
    f = fopen(csvpath, "w");
    if (!f) return -8;
    fputs("ID,NAME\n10,CAROL\n20,DAVE\n", f);
    fclose(f);
    a2 = gmb_rdd_use(csvpath, "C", "CSV");
    if (a2 < 1) return -9;
    if (gmb_rdd_lastrec() != 2) return -10;
    gmb_rdd_gotop();
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "CAROL") != 0) return -11;
    if (gmb_rdd_replace_field(1, "CAROLINE") != 0) return -19;
    gmb_rdd_close(0);
    a2 = gmb_rdd_use(csvpath, "C2", "CSV");
    if (a2 < 1) return -20;
    gmb_rdd_gotop();
    if (gmb_rdd_get_field_by_name("NAME", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "CAROLINE") != 0) return -21;
    gmb_rdd_close(0);
    remove(csvpath);

    {
        char sdfpath[256];
        snprintf(sdfpath, sizeof(sdfpath), "/tmp/gmb_rdd_sc.sdf");
        f = fopen(sdfpath, "w");
        if (!f) return -35;
        fputs("alpha\nbeta\n", f);
        fclose(f);
        a2 = gmb_rdd_use(sdfpath, "S", "SDF");
        if (a2 < 1) return -36;
        if (gmb_rdd_lastrec() != 2) return -37;
        gmb_rdd_gotop();
        if (gmb_rdd_get_field(0, buf, (int)sizeof(buf)) != 0 || strcmp(buf, "alpha") != 0) return -38;
        if (gmb_rdd_driver(buf, (int)sizeof(buf)) != 0 || strcmp(buf, "SDF") != 0) return -39;
        gmb_rdd_close(0);
        remove(sdfpath);
    }

    snprintf(dbpath, sizeof(dbpath), "/tmp/gmb_rdd_sc.sqlite");
    remove(dbpath);
    if (sqlite3_open(dbpath, &db) != SQLITE_OK) return -12;
    sqlite3_exec(db, "CREATE TABLE p (id INTEGER, name TEXT); INSERT INTO p VALUES (1,'EVE');", NULL, NULL, NULL);
    sqlite3_close(db);
    a2 = gmb_rdd_use(dbpath, "p", "SQL");
    if (a2 < 1) return -13;
    if (gmb_rdd_lastrec() != 1) return -14;
    gmb_rdd_gotop();
    if (gmb_rdd_get_field_by_name("name", buf, (int)sizeof(buf)) != 0 || strcmp(buf, "EVE") != 0) return -15;
    gmb_rdd_close(0);
    remove(dbpath);
    return 0;
}

/* ── RDD campo-livello: funzioni aggiuntive (parita' IlRuntime.Rdd*) ── */
/* GETFLDSTATE: con row-buffer attivo lo stato è quello del buffer (0 immodificato,
 * 1 modificato, 2 cancellato — ADR §4); senza buffer comportamento preesistente. */
int gmb_rdd_get_field_state(int idx)
{
    if (idx < 0) { gmb_rdd_bad_field(); return 0; }
    int s = gmb_rdd_rb_state(gmb_rdd_current_area(), idx);
    gmb_rdd_area* a;
    if (s >= 0) return s;
    a = cur();
    if (!a || !a->used || idx < 0 || idx >= a->nfields) return 0;
    if (a->del && a->recno >= 1 && a->recno <= a->lastrec && a->del[a->recno - 1]) return 2;
    return 0;
}

/* TABLEUPDATE/TABLEREVERT: row-buffer del runtime condiviso (ADR). Vero = 1, falso = 0
 * (i backend convertono nella convenzione bit del bytecode, vedi boundary VM/C). */
/* Convenzione bit del linguaggio: VERO = -1 (come RLOCK/IlConversion), FALSO = 0. */
int gmb_rdd_table_revert(void) { return gmb_rdd_rowbuf_revert(gmb_rdd_current_area()) ? -1 : 0; }
int gmb_rdd_table_update(void) { return gmb_rdd_rowbuf_update(gmb_rdd_current_area()) ? -1 : 0; }
/* RLOCK: lock di RECORD → richiede un record corrente (senza, come nell'area `:memory:` vuota
 * del sample RDD_Omonime, il mondo gestito risponde 0 = fallito mentre FLOCK riesce: il file
 * lock non ha bisogno di un record). Prima ritornava 1 con la sola area aperta. */
/* RLock/FLock dell'area corrente (cell-store RDD). Il risultato su aree di ALTRI motori
 * (ISAM/DBF/SQL) dipende dal driver — l'IL usa session.Locks con esito diverso (ISAM→1,
 * SQL→0). Non "assumere" qui: lasciare 0 per le aree non-cell-store e gestire per-driver
 * negli emitter (RDD_Mixed_Demo ISAM vs RDD_SQL_CRUD SQL). */
/* RLock/FLock dell'area corrente (cell-store RDD). Su un'area SQL il driver non ha lock di
 * file/record → fallisce (parità IL: session.Locks→driver.FileLock su SQL = falso). */
int gmb_rdd_rlock(void) { gmb_rdd_area* a = cur(); if (!a || !a->used || a->eof || a->recno < 1) return 0; a->locked = 1; return 1; }
int gmb_rdd_flock(void) { gmb_rdd_area* a = cur(); if (!a || !a->used) return 0; a->locked = 1; return 1; }
/* UNLOCK / SET LOCK TIMEOUT.
 * Il motore C è a processo singolo e non ha un lock manager con retry (a differenza del
 * runtime C#: DatabaseService.Locks): l'unlock è quindi "rilascia l'area corrente" e il
 * timeout è uno stato dell'area, interrogabile e onorato dalle lock che lo usano.
 * UNLOCK ritorna 1 se c'era un'area da sbloccare, 0 altrimenti (per l'emitter C99Native). */
int gmb_rdd_unlock(void)
{
    gmb_rdd_area* a = cur();
    if (!a || !a->used) return 0;
    a->locked = 0;
    return 1;
}

/* ── Condizione di SCOPE per i comandi RDD (`RDD REPLACE … FOR <cond>`) ──
 * Il bytecode passa la condizione come TESTO (misurato con GMB_DUMP_CHUNK: `'Nome = "Bob"'`),
 * quindi qui si valuta sul record CORRENTE il sottoinsieme documentato:
 *     <campo> <op> <letterale>
 * con op in = <> < > <= >=, letterale numerico o stringa fra apici.
 * Numeri: confronto numerico se ENTRAMBI numerici, altrimenti confronto stringa
 * case-insensitive (come fanno i driver). Qualsiasi altra forma (AND/OR/funzioni) → -1 =
 * non valutabile: il chiamante deve produrre un ERRORE ESPLICITO, mai applicare a tutti. */
static int rdd_cond_trim_cmp(const char* a, const char* b)
{
    while (*a == ' ' || *a == '\t') a++;
    while (*b == ' ' || *b == '\t') b++;
    size_t la = strlen(a); while (la && (a[la-1] == ' ' || a[la-1] == '\t')) la--;
    size_t lb = strlen(b); while (lb && (b[lb-1] == ' ' || b[lb-1] == '\t')) lb--;
    if (la != lb) return (int)la - (int)lb;
    for (size_t i = 0; i < la; i++)
    {
        int ca = tolower((unsigned char)a[i]), cb = tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
    }
    return 0;
}

int gmb_rdd_condition_match(const char* cond)
{
    char field[64], lit[256], op[3];
    const char* p = cond;
    size_t i;
    if (!p) return -1;
    while (*p == ' ' || *p == '\t') p++;
    for (i = 0; i < sizeof(field) - 1 && *p && *p != ' ' && *p != '\t'
                && *p != '=' && *p != '<' && *p != '>' && *p != '!' ; i++) field[i] = *p++;
    field[i] = 0;
    if (!field[0]) return -1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '<' && p[1] == '>') { op[0] = '<'; op[1] = '>'; op[2] = 0; p += 2; }
    else if (*p == '<' && p[1] == '=') { op[0] = '<'; op[1] = '='; op[2] = 0; p += 2; }
    else if (*p == '>' && p[1] == '=') { op[0] = '>'; op[1] = '='; op[2] = 0; p += 2; }
    else if (*p == '=' || *p == '<' || *p == '>') { op[0] = *p; op[1] = 0; p++; }
    else return -1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') { p++; for (i = 0; i < sizeof(lit) - 1 && *p && *p != '"'; i++) lit[i] = *p++; lit[i] = 0; }
    else { for (i = 0; i < sizeof(lit) - 1 && *p; i++) lit[i] = *p++; lit[i] = 0; }
    /* AND/OR/funzioni: fuori dal sottoinsieme → non valutabile. */
    if (strstr(lit, " AND ") || strstr(lit, " OR ") || strchr(lit, '(')) return -1;

    int idx = gmb_rdd_field_index(field);
    if (idx < 0) return -1;
    char cur[256] = "";
    gmb_rdd_get_field(idx, cur, (int)sizeof(cur));

    /* numerico se entrambi lo sono */
    {
        char* e1 = NULL; char* e2 = NULL;
        double a = strtod(cur, &e1), b = strtod(lit, &e2);
        int both_num = (e1 && *e1 == 0 && cur[0] != 0) && (e2 && *e2 == 0 && lit[0] != 0);
        if (both_num)
        {
            if (strcmp(op, "=") == 0)  return a == b;
            if (strcmp(op, "<>") == 0) return a != b;
            if (strcmp(op, "<") == 0)  return a <  b;
            if (strcmp(op, ">") == 0)  return a >  b;
            if (strcmp(op, "<=") == 0) return a <= b;
            return a >= b;
        }
    }
    {
        int c = rdd_cond_trim_cmp(cur, lit);
        if (strcmp(op, "=") == 0)  return c == 0;
        if (strcmp(op, "<>") == 0) return c != 0;
        if (strcmp(op, "<") == 0)  return c <  0;
        if (strcmp(op, ">") == 0)  return c >  0;
        if (strcmp(op, "<=") == 0) return c <= 0;
        return c >= 0;
    }
}

int gmb_rdd_set_lock_timeout(int ms)
{
    gmb_rdd_lock_timeout_ms = ms > 0 ? ms : 0;   /* <0/0 → nessun retry, come Locks.LockTimeoutMs */
    return gmb_rdd_lock_timeout_ms;
}

int gmb_rdd_lock_timeout(void) { return gmb_rdd_lock_timeout_ms; }
/* SET REPROCESS <ms>: politica di retry dei lock. Il motore C è a processo singolo e i lock
 * non falliscono mai (non c'è contesa), quindi il valore è uno STATO dell'area di sessione,
 * interrogabile e onorato da chi lo legge — non un no-op silenzioso: `gmb_rdd_reprocess()`
 * ritorna ciò che è stato impostato. Nel mondo gestito è `DataSession.Locks.RetryBaseMs`. */
int gmb_rdd_set_reprocess(int ms)
{
    gmb_rdd_reprocess_ms = ms > 0 ? ms : 0;
    return gmb_rdd_reprocess_ms;
}

/* Registro eventi RDD (RDD ON EVENT / OFF EVENT) — parità con IL/PCode
 * (SqlDataSession.Events.On/Off). Il motore C non FIRA gli eventi, ma la registrazione è
 * uno STATO REALE e interrogabile (gmb_rdd_event_count): NON un no-op silenzioso. */
#define GMB_RDD_MAX_EVENTS 32
static char g_rdd_events[GMB_RDD_MAX_EVENTS][48];
static int g_rdd_event_n = 0;
int gmb_rdd_onevent(const char* name)
{
    int i;
    if (!name || !*name) return -1;
    for (i = 0; i < g_rdd_event_n; i++) if (strcmp(g_rdd_events[i], name) == 0) return 0;
    if (g_rdd_event_n >= GMB_RDD_MAX_EVENTS) return -1;
    snprintf(g_rdd_events[g_rdd_event_n], sizeof(g_rdd_events[0]), "%s", name);
    g_rdd_event_n++;
    return 0;
}
int gmb_rdd_offevent(const char* name)
{
    int i, j;
    if (!name) return -1;
    for (i = 0; i < g_rdd_event_n; i++)
        if (strcmp(g_rdd_events[i], name) == 0) {
            for (j = i; j + 1 < g_rdd_event_n; j++)
                memcpy(g_rdd_events[j], g_rdd_events[j + 1], sizeof(g_rdd_events[0]));
            g_rdd_event_n--;
            return 0;
        }
    return 0;
}
int gmb_rdd_event_count(void) { return g_rdd_event_n; }
const char* gmb_rdd_event_at(int i) { return (i >= 0 && i < g_rdd_event_n) ? g_rdd_events[i] : ""; }

int gmb_rdd_reprocess(void) { return gmb_rdd_reprocess_ms; }
/* VALTYPE/FLOAT/CURVAL/TABLEUPDATE/TABLEREVERT/BUFFER — usati dall'emitter C (e il
 * transpiler C# li CHIAMAVA già). Semantica copiata dall'oracolo IL (IlRuntime.Rdd*):
 * VALTYPE = nome del driver ("DBF"/"ARRAY"/…); FLOAT/CURVAL = identità sui numeri;
 * TABLEUPDATE/TABLEREVERT/BUFFER = row-buffer del runtime condiviso (gmb_rdd_rowbuf.c,
 * ADR-RowBuffer-RDD-Canonica) — le varianti con area selezionano l'area e delegano. */
gmb_string_t gmb_rdd_valtype_str(void)
{
    gmb_rdd_area* a = cur();
    const char* n = "";
    if (a && a->used) {
        switch (a->kind) {
        case GMB_RDD_ARRAY: n = "ARRAY"; break;
        case GMB_RDD_CSV:   n = "CSV";   break;
        case GMB_RDD_SDF:   n = "SDF";   break;
        case GMB_RDD_SQL:   n = "SQL";   break;
        case GMB_RDD_DBF:   n = "DBF";   break;
        default:            n = "";      break;
        }
    }
    return gmb_string_alloc(n);
}

gmb_double_t gmb_rdd_float(gmb_double_t v)  { return v; }
gmb_double_t gmb_rdd_curval(gmb_double_t v) { return v; }
/* Varianti con area: selezionano l'area e delegano al row-buffer (UNICA implementazione). */
gmb_double_t gmb_rdd_tableupdate(int area)
{
    if (area > 0) gmb_rdd_select(area);
    return (gmb_double_t)gmb_rdd_table_update();
}
gmb_double_t gmb_rdd_tablerevert(int area)
{
    if (area > 0) gmb_rdd_select(area);
    return (gmb_double_t)gmb_rdd_table_revert();
}
gmb_double_t gmb_rdd_buffer(int area, int mode)
{
    if (area > 0) gmb_rdd_select(area);
    return gmb_rdd_rowbuf_activate(gmb_rdd_current_area(), mode) ? -1.0 : 0.0;
}

/* OLDVAL: solo dal row-buffer (snapshot); senza buffer → stringa vuota (ADR §5, pari al
 * gestito senza `_originalValues`). */
const char* gmb_rdd_oldval(int idx)
{
    static char buf[GMB_RDD_CELL];
    buf[0] = 0;
    if (idx < 0) { gmb_rdd_bad_field(); return ""; }
    if (gmb_rdd_rb_oldval(gmb_rdd_current_area(), idx, buf, sizeof(buf))) return buf;
    return "";
}

const char* gmb_rdd_today(void)
{
    static char buf[16];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buf;
}
const char* gmb_rdd_ctod(const char* s)
{
    static char buf[16];
    int y = 0, m = 0, d = 0;
    if (!s || !*s || sscanf(s, "%d-%d-%d", &y, &m, &d) != 3) return "";
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return buf;
}
const char* gmb_rdd_dtoc(const char* d)
{
    static char buf[16];
    int y = 0, m = 0, dd = 0;
    if (!d || !*d || sscanf(d, "%d-%d-%d", &y, &m, &dd) != 3) return "";
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, dd);
    return buf;
}

int gmb_rdd_field_index(const char* name)
{
    gmb_rdd_area* a = cur();
    int i;
    if (!name) return -1;
    if (a && a->used)
        for (i = 0; i < a->nfields; i++)
            if (strcasecmp(a->fields[i], name) == 0) return i;
    /* Area ISAM (cell-store vuoto): fallback sul nome via hook del motore. */
    return gmb_rdd_engine_isam_field_index(name);
}

static char g_rdd_default_driver[16] = "ARRAY";
int gmb_rdd_set_default(const char* driver) {
    if (driver && *driver) { snprintf(g_rdd_default_driver, sizeof(g_rdd_default_driver), "%s", driver); return 1; }
    return 0;
}
const char* gmb_rdd_get_default(void) { return g_rdd_default_driver; }
