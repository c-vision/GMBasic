/*
 * sparse_ffi.c — shim ABI per il namespace BASIC SPARSE (matrici sparse di gmbsuite).
 *
 * Due sorgenti, già compilate in libgmbmath:
 *   - gmbcompute/gc_sparse: contenitori COO/CSR/CSC, conversioni, trasposizione
 *   - gmbmath-linalg/gm_sparse_linalg: fattorizzazioni sparse dirette (Cholesky, LU)
 *
 * La superficie BASIC è a HANDLE (LONG > 0): un handle identifica un contenitore e si
 * libera con FREE. La tabella è statica (64 slot), come nelle altre superfici a handle
 * del progetto: i fattori non sono condivisi fra thread.
 *
 * Convenzioni:
 *   - COO: triplette (row, col, val) in ordine di inserimento; i duplicati vengono
 *     sommati e gli indici ordinati dalle conversioni.
 *   - CSR: rowptr(rows+1), colidx(nnz), val(nnz).
 *   - CSC: colptr(cols+1), rowidx(nnz), val(nnz).
 *   - SPCHOLFACTOR vuole la matrice SIMMETRICA in CSC con il solo TRIANGOLO INFERIORE
 *     (rowidx >= colonna); SPLUFACTOR vuole la matrice completa in CSC.
 *
 * Ritorno: 0 = ok, -1 = handle/argomento non valido. I FACTOR ritornano un handle
 * (> 0) oppure -1 (singolare o non definita positiva).
 */
#include "gc_sparse.h"
#include "gm_sparse_linalg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

#define SP_MAX_SLOTS 64

enum sp_kind { SP_NONE = 0, SP_COO, SP_CSR, SP_CSC, SP_CHOL, SP_LU };

typedef struct {
    int kind;
    union {
        gc_coo coo;
        gc_csr csr;
        gc_csc csc;
        SpChol *chol;
        SpluFact *lu;
    } u;
} sp_slot;

static sp_slot g_slots[SP_MAX_SLOTS];

static sp_slot *sp_get(i64 h)
{
    if (h < 1 || h > SP_MAX_SLOTS) return NULL;
    if (g_slots[h - 1].kind == SP_NONE) return NULL;
    return &g_slots[h - 1];
}

static i64 sp_alloc(int kind)
{
    i64 i;
    for (i = 0; i < SP_MAX_SLOTS; i++)
    {
        if (g_slots[i].kind == SP_NONE)
        {
            g_slots[i].kind = kind;
            memset(&g_slots[i].u, 0, sizeof(g_slots[i].u));
            return i + 1;
        }
    }
    return -1;
}

/* ── ciclo di vita ────────────────────────────────────────────────────────── */

/* COONEW(rows, cols, nnz): contenitore di triplette da riempire con COOSET. */
i64 ofx_sp_coo_new(i64 rows, i64 cols, i64 nnz)
{
    i64 h;
    sp_slot *s;
    if (rows < 1 || cols < 1 || nnz < 0) return -1;
    h = sp_alloc(SP_COO);
    if (h < 0) return -1;
    s = &g_slots[h - 1];
    if (gc_coo_init(&s->u.coo, (int)rows, (int)cols, (int)nnz) != GC_OK)
    {
        s->kind = SP_NONE;
        return -1;
    }
    return h;
}

/* COOSET(h, k, row, col, val): riempie la tripletta k (0-based). */
i64 ofx_sp_coo_set(i64 h, i64 k, i64 row, i64 col, double val)
{
    sp_slot *s = sp_get(h);
    gc_coo *A;
    if (!s || s->kind != SP_COO) return -1;
    A = &s->u.coo;
    if (k < 0 || k >= A->nnz || row < 0 || row >= A->rows || col < 0 || col >= A->cols) return -1;
    A->row[k] = (int)row;
    A->col[k] = (int)col;
    A->val[k] = val;
    return 0;
}

/* FREE(h): libera qualunque contenitore o fattore. */
i64 ofx_sp_free(i64 h)
{
    sp_slot *s = sp_get(h);
    if (!s) return -1;
    switch (s->kind)
    {
        case SP_COO: gc_coo_free(&s->u.coo); break;
        case SP_CSR: gc_csr_free(&s->u.csr); break;
        case SP_CSC: gc_csc_free(&s->u.csc); break;
        case SP_CHOL: if (s->u.chol) spchol_free(s->u.chol); break;
        case SP_LU: if (s->u.lu) splu_free(s->u.lu); break;
        default: break;
    }
    memset(&s->u, 0, sizeof(s->u));
    s->kind = SP_NONE;
    return 0;
}

/* SIZES(h, out): out(0) = righe, out(1) = colonne, out(2) = non-zeri. */
i64 ofx_sp_sizes(i64 h, i64 *out, i64 out_n)
{
    sp_slot *s = sp_get(h);
    if (!s || !out || out_n < 3) return -1;
    switch (s->kind)
    {
        case SP_COO: out[0] = s->u.coo.rows; out[1] = s->u.coo.cols; out[2] = s->u.coo.nnz; return 0;
        case SP_CSR: out[0] = s->u.csr.rows; out[1] = s->u.csr.cols; out[2] = s->u.csr.nnz; return 0;
        case SP_CSC: out[0] = s->u.csc.rows; out[1] = s->u.csc.cols; out[2] = s->u.csc.nnz; return 0;
        default: return -1;
    }
}

/* TRIPLET(h, k, out): la k-esima entry nello STOCCAGGIO, come (riga, colonna, valore).
 * Per CSR le entry di una riga sono contigue; per CSC quelle di una colonna. */
i64 ofx_sp_triplet(i64 h, i64 k, double *out, i64 out_n)
{
    sp_slot *s = sp_get(h);
    int r, c;
    if (!s || !out || out_n < 3 || k < 0) return -1;
    switch (s->kind)
    {
        case SP_COO:
            if (k >= s->u.coo.nnz) return -1;
            out[0] = s->u.coo.row[k];
            out[1] = s->u.coo.col[k];
            out[2] = s->u.coo.val[k];
            return 0;
        case SP_CSR:
            if (k >= s->u.csr.nnz) return -1;
            for (r = 0; r < s->u.csr.rows; r++)
                if (k < s->u.csr.rowptr[r + 1]) break;
            out[0] = r;
            out[1] = s->u.csr.colidx[k];
            out[2] = s->u.csr.val[k];
            return 0;
        case SP_CSC:
            if (k >= s->u.csc.nnz) return -1;
            for (c = 0; c < s->u.csc.cols; c++)
                if (k < s->u.csc.colptr[c + 1]) break;
            out[0] = s->u.csc.rowidx[k];
            out[1] = c;
            out[2] = s->u.csc.val[k];
            return 0;
        default:
            return -1;
    }
}

/* ── conversioni ──────────────────────────────────────────────────────────── */

static i64 sp_new_csr(int rows, int cols, int nnz)
{
    i64 h = sp_alloc(SP_CSR);
    if (h < 0) return -1;
    if (gc_csr_init(&g_slots[h - 1].u.csr, rows, cols, nnz) != GC_OK)
    {
        g_slots[h - 1].kind = SP_NONE;
        return -1;
    }
    return h;
}

static i64 sp_new_csc(int rows, int cols, int nnz)
{
    i64 h = sp_alloc(SP_CSC);
    if (h < 0) return -1;
    if (gc_csc_init(&g_slots[h - 1].u.csc, rows, cols, nnz) != GC_OK)
    {
        g_slots[h - 1].kind = SP_NONE;
        return -1;
    }
    return h;
}

i64 ofx_sp_coo_to_csr(i64 h)
{
    sp_slot *s = sp_get(h);
    i64 nh;
    gc_csr tmp;
    if (!s || s->kind != SP_COO) return -1;
    if (gc_coo_to_csr(&s->u.coo, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSR);
    if (nh < 0) { gc_csr_free(&tmp); return -1; }
    g_slots[nh - 1].u.csr = tmp;
    return nh;
}

i64 ofx_sp_coo_to_csc(i64 h)
{
    sp_slot *s = sp_get(h);
    i64 nh;
    gc_csc tmp;
    if (!s || s->kind != SP_COO) return -1;
    if (gc_coo_to_csc(&s->u.coo, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSC);
    if (nh < 0) { gc_csc_free(&tmp); return -1; }
    g_slots[nh - 1].u.csc = tmp;
    return nh;
}

i64 ofx_sp_csr_to_csc(i64 h)
{
    sp_slot *s = sp_get(h);
    i64 nh;
    gc_csc tmp;
    if (!s || s->kind != SP_CSR) return -1;
    if (gc_csr_to_csc(&s->u.csr, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSC);
    if (nh < 0) { gc_csc_free(&tmp); return -1; }
    g_slots[nh - 1].u.csc = tmp;
    return nh;
}

i64 ofx_sp_csc_to_csr(i64 h)
{
    sp_slot *s = sp_get(h);
    i64 nh;
    gc_csr tmp;
    if (!s || s->kind != SP_CSC) return -1;
    if (gc_csc_to_csr(&s->u.csc, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSR);
    if (nh < 0) { gc_csr_free(&tmp); return -1; }
    g_slots[nh - 1].u.csr = tmp;
    return nh;
}

/* CSR -> CSC della TRASPOSTA (e viceversa): stesso pattern, ruoli scambiati. */
i64 ofx_sp_csr_transpose(i64 h)
{
    sp_slot *s = sp_get(h);
    i64 nh;
    gc_csc tmp;
    if (!s || s->kind != SP_CSR) return -1;
    if (gc_csr_transpose(&s->u.csr, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSC);
    if (nh < 0) { gc_csc_free(&tmp); return -1; }
    g_slots[nh - 1].u.csc = tmp;
    return nh;
}

i64 ofx_sp_csc_transpose(i64 h)
{
    sp_slot *s = sp_get(h);
    i64 nh;
    gc_csr tmp;
    if (!s || s->kind != SP_CSC) return -1;
    if (gc_csc_transpose(&s->u.csc, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSR);
    if (nh < 0) { gc_csr_free(&tmp); return -1; }
    g_slots[nh - 1].u.csr = tmp;
    return nh;
}

/* DENSETOCSR(rows, cols, a()): matrice densa row-major -> CSR (gli zeri sono scartati). */
i64 ofx_sp_dense_to_csr(i64 rows, i64 cols, const double *a, i64 a_n)
{
    i64 nh;
    gc_csr tmp;
    if (!a || rows < 1 || cols < 1 || a_n < rows * cols) return -1;
    if (gc_dense_to_csr((int)rows, (int)cols, a, (int)cols, &tmp) != GC_OK) return -1;
    nh = sp_alloc(SP_CSR);
    if (nh < 0) { gc_csr_free(&tmp); return -1; }
    g_slots[nh - 1].u.csr = tmp;
    return nh;
}

/* ── fattorizzazioni dirette ──────────────────────────────────────────────── */

/* Cholesky sparsa di una matrice SIMMETRICA DEFINITA POSITIVA in CSC, solo triangolo
 * inferiore (rowidx >= colonna). Ritorna un handle fattore, oppure -1. */
i64 ofx_sp_chol_factor(i64 h)
{
    sp_slot *s = sp_get(h);
    SpChol *f;
    i64 nh;
    if (!s || s->kind != SP_CSC) return -1;
    f = spchol_factor(s->u.csc.rows, s->u.csc.colptr, s->u.csc.rowidx, s->u.csc.val);
    if (!f) return -1;
    nh = sp_alloc(SP_CHOL);
    if (nh < 0) { spchol_free(f); return -1; }
    g_slots[nh - 1].u.chol = f;
    return nh;
}

/* Risolve K x = b in place sul fattore (b() viene sovrascritto dalla soluzione). */
i64 ofx_sp_chol_solve(i64 hf, double *b, i64 b_n)
{
    sp_slot *s = sp_get(hf);
    if (!s || s->kind != SP_CHOL || !b || b_n < s->u.chol->n) return -1;
    return spchol_solve(s->u.chol, b) == 0 ? 0 : -1;
}

/* LU sparsa con pivoting parziale di una matrice in CSC (anche non simmetrica). */
i64 ofx_sp_lu_factor(i64 h)
{
    sp_slot *s = sp_get(h);
    SpluFact *f;
    i64 nh;
    if (!s || s->kind != SP_CSC) return -1;
    f = splu_factor(s->u.csc.rows, s->u.csc.colptr, s->u.csc.rowidx, s->u.csc.val);
    if (!f) return -1;
    nh = sp_alloc(SP_LU);
    if (nh < 0) { splu_free(f); return -1; }
    g_slots[nh - 1].u.lu = f;
    return nh;
}

/* Risolve A x = b in place (rhs di lunghezza n). 0 = ok, -1 = singolare. */
i64 ofx_sp_lu_solve(i64 hf, double *b, i64 b_n)
{
    sp_slot *s = sp_get(hf);
    if (!s || s->kind != SP_LU || !b) return -1;
    if (b_n < s->u.lu->n) return -1;
    return splu_solve(s->u.lu, b) == 0 ? 0 : -1;
}
