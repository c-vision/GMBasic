/* FILE: native/gmb_runtime_c/src/gmb_rdd_rowbuf.c
 * Row-buffer RDD (table buffering Clipper) — UNICA implementazione (§0bis).
 * Spec: AiWiki/01-Projects/GMBasic/Decisions/ADR-RowBuffer-RDD-Canonica.md
 *
 * Il buffer vive PER AREA (numero 1..GMB_RDD_MAX_AREA), snapshot del RECORD CORRENTE.
 * Motori: cell-store RDD del runtime (raw_* in gmb_rdd.c) oppure ISAM via hook
 * registrato dal possessore del motore commerciale (gmb_rdd_rowbuf_set_engine).
 * Indici campo 0-based; la conversione 1-based del bytecode è nei boundary dei backend. */
#include "gmb_rdd_rowbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define RB_CELL 256
#define RB_MAX_AREA 255
#define RB_MAX_FIELDS 64

typedef struct
{
    int active;
    int mode;
    int recno;
    int nfields;
    char* old;   /* nfields * RB_CELL — snapshot all'apertura */
    char* cur;   /* nfields * RB_CELL — valore di buffer */
    unsigned char* state; /* nfields — 0 immodificato, 1 modificato */
} rbuf_t;

static rbuf_t g_rb[RB_MAX_AREA + 1];
static const gmb_rdd_engine_hooks* g_hooks;

void gmb_rdd_rowbuf_set_engine(const gmb_rdd_engine_hooks* hooks) { g_hooks = hooks; }

static rbuf_t* rb_of(int area)
{
    if (area < 1 || area > RB_MAX_AREA) return NULL;
    return &g_rb[area];
}

static void rb_release(rbuf_t* b)
{
    free(b->old); b->old = NULL;
    free(b->cur); b->cur = NULL;
    free(b->state); b->state = NULL;
    b->active = 0; b->mode = 0; b->nfields = 0; b->recno = 0;
}

void gmb_rdd_rowbuf_forget(int area)
{
    rbuf_t* b = rb_of(area);
    if (b && b->active) rb_release(b);
}

/* ── engine probe/passthrough (usati anche da gmb_rdd.c per la via ISAM) ── */
static int engine_nfields(void)
{
    int n = gmb_rdd_raw_field_count();
    if (n > 0) return n;
    if (g_hooks && g_hooks->active && g_hooks->active() && g_hooks->nfields) return g_hooks->nfields();
    return 0;
}

static int engine_read(int ord0, char* buf, int cap)
{
    if (gmb_rdd_raw_read(ord0, buf, cap) == 0) return 0;
    if (g_hooks && g_hooks->active && g_hooks->active() && g_hooks->read_field) return g_hooks->read_field(ord0, buf, cap);
    return -1;
}

int gmb_rdd_engine_isam_active(void)
{
    return (g_hooks && g_hooks->active && g_hooks->active()) ? 1 : 0;
}

int gmb_rdd_engine_isam_nfields(void)
{
    return (g_hooks && g_hooks->nfields && g_hooks->active && g_hooks->active()) ? g_hooks->nfields() : 0;
}

/* Nome campo → ordinale 0-based sul motore ISAM (via hook field_name). */
int gmb_rdd_engine_isam_field_index(const char* name)
{
    char nm[64];
    int i, n;
    if (!name || !name[0]) return -1;
    if (!g_hooks || !g_hooks->active || !g_hooks->active() || !g_hooks->field_name) return -1;
    n = gmb_rdd_engine_isam_nfields();
    for (i = 0; i < n; i++)
    {
        nm[0] = 0;
        if (g_hooks->field_name(i, nm, (int)sizeof(nm)) == 0)
        {
            const char* x = nm; while (*x == ' ') x++;
            const char* y = name; while (*y == ' ') y++;
            int k = 0;
            while (x[k] && y[k] && tolower((unsigned char)x[k]) == tolower((unsigned char)y[k])) k++;
            if (x[k] == 0 && y[k] == 0) return i;
        }
    }
    return -1;
}

int gmb_rdd_engine_isam_read(int ord0, char* buf, int cap)
{
    if (!g_hooks || !g_hooks->active || !g_hooks->active() || !g_hooks->read_field) return -1;
    return g_hooks->read_field(ord0, buf, cap);
}

/* Scrittura a campo singolo via ISAM: read-modify-write dell'intero record (l'unica API
 * di put del motore è per-record). Mossa dal ramo inline di gmb_syscall_rdd.c (§0bis). */
int gmb_rdd_engine_isam_write_field(int ord0, const char* value)
{
    const char* vals[RB_MAX_FIELDS];
    char pool[RB_MAX_FIELDS][RB_CELL];
    int n, i;
    if (!g_hooks || !g_hooks->active || !g_hooks->active()) return -1;
    if (!g_hooks->nfields || !g_hooks->read_field || !g_hooks->write_record) return -1;
    n = g_hooks->nfields();
    if (n <= 0 || n > RB_MAX_FIELDS || ord0 < 0 || ord0 >= n) return -1;
    for (i = 0; i < n; i++)
    {
        pool[i][0] = 0;
        if (i == ord0) snprintf(pool[i], RB_CELL, "%s", value ? value : "");
        else if (g_hooks->read_field(i, pool[i], RB_CELL) != 0) return -1;
        vals[i] = pool[i];
    }
    return g_hooks->write_record(vals, n);
}

/* ── intercettori: restituiscono 1 quando il buffer ha gestito l'operazione ── */
int gmb_rdd_rb_read(int area, int idx, char* buf, int cap)
{
    rbuf_t* b = rb_of(area);
    if (!b || !b->active || idx < 0 || idx >= b->nfields || !b->cur) return 0;
    strncpy(buf, b->cur + (size_t)idx * RB_CELL, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 1;
}

int gmb_rdd_rb_write(int area, int idx, const char* value)
{
    rbuf_t* b = rb_of(area);
    char* slot;
    if (!b || !b->active || idx < 0 || idx >= b->nfields || !b->cur) return 0;
    slot = b->cur + (size_t)idx * RB_CELL;
    strncpy(slot, value ? value : "", RB_CELL - 1);
    slot[RB_CELL - 1] = 0;
    b->state[idx] = strcmp(slot, b->old + (size_t)idx * RB_CELL) == 0 ? 0 : 1;
    return 1;
}

int gmb_rdd_rb_oldval(int area, int idx, char* buf, int cap)
{
    rbuf_t* b = rb_of(area);
    if (!b || !b->active || idx < 0 || idx >= b->nfields || !b->old) return 0;
    strncpy(buf, b->old + (size_t)idx * RB_CELL, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return 1;
}

int gmb_rdd_rb_state(int area, int idx)
{
    rbuf_t* b = rb_of(area);
    if (!b || !b->active) return -1;
    if (idx < 0 || idx >= b->nfields) return -1;
    if (gmb_rdd_raw_deleted() == 1) return 2;
    return b->state[idx];
}

/* ── ciclo di vita ── */
int gmb_rdd_rowbuf_activate(int area, int mode)
{
    rbuf_t* b = rb_of(area);
    int n, i, rec;
    if (!b) return 0;
    if (b->active) rb_release(b);
    if (mode == 0 || mode == 1) return 0;          /* disattivazione esplicita */
    if (mode != 2 && mode != 3) return 0;
    n = engine_nfields();
    if (n > RB_MAX_FIELDS) return 0;
    /* Tabella SCHEMALESS (ARRAY senza schema): field-count 0. Il gestito (RowsBuffer) usa un
     * dict DINAMICO (chiavi aggiunte da SetField) → attiva comunque; qui l'array è fisso, quindi
     * si riserva la capacità massima così SETFIELD/GETFIELD su qualunque ordinale funzionano. */
    if (n < 1) n = RB_MAX_FIELDS;
    rec = gmb_rdd_raw_recno();
    if (rec <= 0 && g_hooks && g_hooks->active && g_hooks->active() && g_hooks->recno) rec = g_hooks->recno();
    /* rec <= 0 (tabella vuota / nessun record posizionato): attiva COMUNQUE un append-buffer,
     * come il gestito (RowsBuffer.Activate: GetCurrentRecord() == null → _isAppended=true).
     * Prima ritornava 0 → `RDD.BUFFER(1,3)` = 0 e GETFIELD/OLDVAL/GETFLDSTATE vuoti su tutte
     * le gambe C (VM-C, C99Native). */
    if (rec < 0) rec = 0;
    b->old = (char*)calloc((size_t)n, RB_CELL);
    b->cur = (char*)calloc((size_t)n, RB_CELL);
    b->state = (unsigned char*)calloc((size_t)n, 1);
    if (!b->old || !b->cur || !b->state) { rb_release(b); return 0; }
    for (i = 0; i < n; i++)
    {
        if (engine_read(i, b->old + (size_t)i * RB_CELL, RB_CELL) != 0)
        {
            /* Cella non leggibile (campo schemaless): il gestito (RowsBuffer su
             * GetCurrentRecord) espone il DEFAULT numerico 0, non vuoto → parità OLDVAL
             * (RDD_Memory_CRUD: OLDVAL(1,1)= 0 su IL, era vuoto sul C). */
            snprintf(b->old + (size_t)i * RB_CELL, RB_CELL, "0");
        }
        memcpy(b->cur + (size_t)i * RB_CELL, b->old + (size_t)i * RB_CELL, RB_CELL);
    }
    b->nfields = n; b->recno = rec; b->mode = mode; b->active = 1;
    return 1;
}

int gmb_rdd_rowbuf_update(int area)
{
    rbuf_t* b = rb_of(area);
    int i, committed = 0;
    if (!b || !b->active) return 0;
    if (gmb_rdd_raw_recno() > 0 || gmb_rdd_raw_field_count() > 0)
    {
        for (i = 0; i < b->nfields; i++)
            if (b->state[i] && gmb_rdd_raw_write(i, b->cur + (size_t)i * RB_CELL) == 0) committed = 1;
    }
    else if (g_hooks && g_hooks->active && g_hooks->active() && g_hooks->write_record)
    {
        const char* vals[RB_MAX_FIELDS];
        for (i = 0; i < b->nfields; i++) vals[i] = b->cur + (size_t)i * RB_CELL;
        committed = g_hooks->write_record(vals, b->nfields) == 0 ? 1 : 0;
    }
    for (i = 0; i < b->nfields; i++) b->state[i] = 0;
    rb_release(b);
    return committed;
}

int gmb_rdd_rowbuf_revert(int area)
{
    rbuf_t* b = rb_of(area);
    if (!b || !b->active) return 0;
    rb_release(b);
    return 1;
}
