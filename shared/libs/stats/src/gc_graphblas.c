/* gc_graphblas.c — operazioni sparse stile GraphBLAS (da SuiteSparse:GraphBLAS). */
#include "gc_graphblas.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- operatori binari ---- */
static gc_real op_add(gc_real a, gc_real b) { return a + b; }
static gc_real op_mul(gc_real a, gc_real b) { return a * b; }
static gc_real op_min(gc_real a, gc_real b) { return a < b ? a : b; }
static gc_real op_max(gc_real a, gc_real b) { return a > b ? a : b; }
static gc_real op_pair(gc_real a, gc_real b) { (void)a; (void)b; return 1.0; }

/* ---- semiring predefiniti ---- */
static const gc_semiring SR_PLUS_TIMES = { op_add, op_mul, 0.0 };
static const gc_semiring SR_MIN_PLUS   = { op_min, op_add, (gc_real)INFINITY };
static const gc_semiring SR_MAX_PLUS   = { op_max, op_add, (gc_real)-INFINITY };
static const gc_semiring SR_MAX_MIN    = { op_max, op_min, (gc_real)-INFINITY };
static const gc_semiring SR_MIN_MAX    = { op_min, op_max, (gc_real)INFINITY };
static const gc_semiring SR_PLUS_MIN   = { op_add, op_min, 0.0 };
static const gc_semiring SR_PLUS_MAX   = { op_add, op_max, 0.0 };
static const gc_semiring SR_ANY_PAIR   = { op_max, op_pair, (gc_real)-INFINITY };
static const gc_semiring SR_TIMES_PLUS = { op_mul, op_add, 1.0 };

const gc_semiring *gc_semiring_plus_times(void) { return &SR_PLUS_TIMES; }
const gc_semiring *gc_semiring_min_plus(void) { return &SR_MIN_PLUS; }
const gc_semiring *gc_semiring_max_plus(void) { return &SR_MAX_PLUS; }
const gc_semiring *gc_semiring_max_min(void) { return &SR_MAX_MIN; }
const gc_semiring *gc_semiring_min_max(void) { return &SR_MIN_MAX; }
const gc_semiring *gc_semiring_plus_min(void) { return &SR_PLUS_MIN; }
const gc_semiring *gc_semiring_plus_max(void) { return &SR_PLUS_MAX; }
const gc_semiring *gc_semiring_any_pair(void) { return &SR_ANY_PAIR; }
const gc_semiring *gc_semiring_times_plus(void) { return &SR_TIMES_PLUS; }

gc_status gc_mxv(const gc_csr *A, const gc_real *x, gc_real *y, const gc_semiring *s)
{
    int i, k;
    if (!A || !x || !y || !s) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) {
        gc_real acc = s->identity;
        for (k = A->rowptr[i]; k < A->rowptr[i + 1]; k++)
            acc = s->add(acc, s->mul(A->val[k], x[A->colidx[k]]));
        y[i] = acc;
    }
    return GC_OK;
}

gc_status gc_mxm(const gc_csr *A, const gc_csr *B, gc_csr *C, const gc_semiring *s)
{
    int i, k, j;
    gc_real *acc;
    int *mark;
    int *rowptr, *colidx, nnz = 0, cap = 0, nz_row;
    gc_real *val;
    if (!A || !B || !C || !s || A->cols != B->rows) return GC_ERR_ARG;
    acc = (gc_real *)malloc((size_t)B->cols * sizeof(gc_real));
    mark = (int *)malloc((size_t)B->cols * sizeof(int));
    if (!acc || !mark) { free(acc); free(mark); return GC_ERR_NO_MEM; }
    for (j = 0; j < B->cols; j++) { mark[j] = -1; acc[j] = s->identity; }
    cap = A->nnz + 1;
    rowptr = (int *)malloc((size_t)(A->rows + 1) * sizeof(int));
    colidx = (int *)malloc((size_t)cap * sizeof(int));
    val = (gc_real *)malloc((size_t)cap * sizeof(gc_real));
    if (!rowptr || !colidx || !val) { free(acc); free(mark); free(rowptr); free(colidx); free(val); return GC_ERR_NO_MEM; }
    rowptr[0] = 0;
    for (i = 0; i < A->rows; i++) {
        int nmarked = 0, *mlist;
        mlist = (int *)malloc((size_t)B->cols * sizeof(int));
        for (k = A->rowptr[i]; k < A->rowptr[i + 1]; k++) {
            int b = A->colidx[k];
            gc_real av = A->val[k];
            int t;
            for (t = B->rowptr[b]; t < B->rowptr[b + 1]; t++) {
                int c = B->colidx[t];
                if (mark[c] != i) { mark[c] = i; acc[c] = s->identity; mlist[nmarked++] = c; }
                acc[c] = s->add(acc[c], s->mul(av, B->val[t]));
            }
        }
        /* ordina le colonne della riga (per CSR canonico) */
        { int a, b2; for (a = 1; a < nmarked; a++) { int key = mlist[a]; gc_real kv = acc[key]; b2 = a - 1;
              while (b2 >= 0 && mlist[b2] > key) { mlist[b2 + 1] = mlist[b2]; b2--; }
              mlist[b2 + 1] = key; (void)kv; } }
        nz_row = nmarked;
        if (nnz + nz_row > cap) {
            while (nnz + nz_row > cap) cap *= 2;
            colidx = (int *)realloc(colidx, (size_t)cap * sizeof(int));
            val = (gc_real *)realloc(val, (size_t)cap * sizeof(gc_real));
            if (!colidx || !val) { free(acc); free(mark); free(mlist); free(rowptr); free(colidx); free(val); return GC_ERR_NO_MEM; }
        }
        for (j = 0; j < nz_row; j++) { colidx[nnz] = mlist[j]; val[nnz] = acc[mlist[j]]; nnz++; }
        rowptr[i + 1] = nnz;
        free(mlist);
    }
    free(acc); free(mark);
    C->rows = A->rows; C->cols = B->cols; C->nnz = nnz;
    C->rowptr = rowptr; C->colidx = colidx; C->val = val;
    return GC_OK;
}

gc_status gc_kronecker(const gc_csr *A, const gc_csr *B, gc_csr *C)
{
    int i, j, k, l, nnz = 0, r, c;
    int *rowptr;
    int *colidx;
    gc_real *val;
    if (!A || !B || !C) return GC_ERR_ARG;
    rowptr = (int *)malloc((size_t)(A->rows * B->rows + 1) * sizeof(int));
    colidx = (int *)malloc((size_t)(A->nnz * B->nnz + 1) * sizeof(int));
    val = (gc_real *)malloc((size_t)(A->nnz * B->nnz + 1) * sizeof(gc_real));
    if (!rowptr || !colidx || !val) { free(rowptr); free(colidx); free(val); return GC_ERR_NO_MEM; }
    for (i = 0; i < A->rows; i++) {
        for (k = 0; k < B->rows; k++) {
            r = i * B->rows + k;
            rowptr[r] = nnz;
            for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) {
                int ac = A->colidx[j];
                gc_real av = A->val[j];
                for (l = B->rowptr[k]; l < B->rowptr[k + 1]; l++) {
                    c = ac * B->cols + B->colidx[l];
                    colidx[nnz] = c;
                    val[nnz] = av * B->val[l];
                    nnz++;
                }
            }
        }
    }
    rowptr[A->rows * B->rows] = nnz;
    C->rows = A->rows * B->rows; C->cols = A->cols * B->cols; C->nnz = nnz;
    C->rowptr = rowptr; C->colidx = colidx; C->val = val;
    return GC_OK;
}

gc_status gc_apply(const gc_csr *A, gc_unop op, gc_csr *out)
{
    int k;
    if (!A || !op || !out) return GC_ERR_ARG;
    out->rows = A->rows; out->cols = A->cols; out->nnz = A->nnz;
    out->rowptr = (int *)malloc((size_t)(A->rows + 1) * sizeof(int));
    out->colidx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    out->val = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    if (!out->rowptr || !out->colidx || !out->val) {
        free(out->rowptr); free(out->colidx); free(out->val);
        out->rowptr = out->colidx = NULL; out->val = NULL; return GC_ERR_NO_MEM;
    }
    memcpy(out->rowptr, A->rowptr, (size_t)(A->rows + 1) * sizeof(int));
    for (k = 0; k < A->nnz; k++) { out->colidx[k] = A->colidx[k]; out->val[k] = op(A->val[k]); }
    return GC_OK;
}

gc_status gc_reduce(const gc_csr *A, const gc_semiring *s, gc_real *result)
{
    int k;
    gc_real acc;
    if (!A || !s || !result) return GC_ERR_ARG;
    acc = s->identity;
    for (k = 0; k < A->nnz; k++) acc = s->add(acc, A->val[k]);
    *result = acc;
    return GC_OK;
}

gc_status gc_eadd(const gc_csr *A, const gc_csr *B, gc_csr *C)
{
    int i, pa, pb, nnz = 0;
    if (!A || !B || !C || A->rows != B->rows || A->cols != B->cols) return GC_ERR_ARG;
    C->rows = A->rows; C->cols = A->cols;
    C->rowptr = (int *)malloc((size_t)(A->rows + 1) * sizeof(int));
    C->colidx = (int *)malloc((size_t)(A->nnz + B->nnz + 1) * sizeof(int));
    C->val = (gc_real *)malloc((size_t)(A->nnz + B->nnz + 1) * sizeof(gc_real));
    if (!C->rowptr || !C->colidx || !C->val) {
        free(C->rowptr); free(C->colidx); free(C->val);
        C->rowptr = C->colidx = NULL; C->val = NULL; return GC_ERR_NO_MEM;
    }
    for (i = 0; i < A->rows; i++) {
        C->rowptr[i] = nnz;
        pa = A->rowptr[i]; pb = B->rowptr[i];
        while (pa < A->rowptr[i + 1] || pb < B->rowptr[i + 1]) {
            if (pb >= B->rowptr[i + 1] || (pa < A->rowptr[i + 1] && A->colidx[pa] < B->colidx[pb])) {
                C->colidx[nnz] = A->colidx[pa]; C->val[nnz] = A->val[pa]; pa++;
            } else if (pa >= A->rowptr[i + 1] || B->colidx[pb] < A->colidx[pa]) {
                C->colidx[nnz] = B->colidx[pb]; C->val[nnz] = B->val[pb]; pb++;
            } else {
                C->colidx[nnz] = A->colidx[pa]; C->val[nnz] = A->val[pa] + B->val[pb]; pa++; pb++;
            }
            nnz++;
        }
    }
    C->rowptr[A->rows] = nnz;
    C->nnz = nnz;
    return GC_OK;
}
