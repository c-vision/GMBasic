/* gc_sparse.c — strutture COO/CSR/CSC e conversioni. */
#include "gc_sparse.h"

#include <stdlib.h>
#include <string.h>

/* ============================== ciclo di vita ============================== */

gc_status gc_coo_init(gc_coo *A, int rows, int cols, int nnz)
{
    if (A == NULL || rows < 0 || cols < 0 || nnz < 0) return GC_ERR_ARG;
    A->rows = rows; A->cols = cols; A->nnz = nnz;
    A->row = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    A->col = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    A->val = (gc_real *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(gc_real));
    if (!A->row || !A->col || !A->val) { gc_coo_free(A); return GC_ERR_NO_MEM; }
    return GC_OK;
}

void gc_coo_free(gc_coo *A)
{
    if (A == NULL) return;
    free(A->row); free(A->col); free(A->val);
    A->row = A->col = NULL; A->val = NULL; A->nnz = 0;
}

gc_status gc_csr_init(gc_csr *A, int rows, int cols, int nnz)
{
    if (A == NULL || rows < 0 || cols < 0 || nnz < 0) return GC_ERR_ARG;
    A->rows = rows; A->cols = cols; A->nnz = nnz;
    A->rowptr = (int *)calloc((size_t)rows + 1, sizeof(int));
    A->colidx = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    A->val = (gc_real *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(gc_real));
    if (!A->rowptr || !A->colidx || !A->val) { gc_csr_free(A); return GC_ERR_NO_MEM; }
    return GC_OK;
}

void gc_csr_free(gc_csr *A)
{
    if (A == NULL) return;
    free(A->rowptr); free(A->colidx); free(A->val);
    A->rowptr = A->colidx = NULL; A->val = NULL; A->nnz = 0;
}

gc_status gc_csc_init(gc_csc *A, int rows, int cols, int nnz)
{
    if (A == NULL || rows < 0 || cols < 0 || nnz < 0) return GC_ERR_ARG;
    A->rows = rows; A->cols = cols; A->nnz = nnz;
    A->colptr = (int *)calloc((size_t)cols + 1, sizeof(int));
    A->rowidx = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    A->val = (gc_real *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(gc_real));
    if (!A->colptr || !A->rowidx || !A->val) { gc_csc_free(A); return GC_ERR_NO_MEM; }
    return GC_OK;
}

void gc_csc_free(gc_csc *A)
{
    if (A == NULL) return;
    free(A->colptr); free(A->rowidx); free(A->val);
    A->colptr = A->rowidx = NULL; A->val = NULL; A->nnz = 0;
}

/* ============================== COO -> CSR/CSC ============================== */

static int sort_dedup(int lo, int hi, int *idx, gc_real *val)
{
    int i, w;
    for (i = lo + 1; i < hi; i++) {
        int ki = idx[i];
        gc_real kv = val[i];
        int j = i - 1;
        while (j >= lo && idx[j] > ki) { idx[j + 1] = idx[j]; val[j + 1] = val[j]; j--; }
        idx[j + 1] = ki; val[j + 1] = kv;
    }
    w = lo;
    for (i = lo; i < hi; i++) {
        if (w > lo && idx[i] == idx[w - 1]) {
            val[w - 1] += val[i];
        } else {
            idx[w] = idx[i]; val[w] = val[i]; w++;
        }
    }
    return w;
}

static gc_status coo_build(const gc_coo *A, int by_row, int **ptr_out, int **idx_out,
                           gc_real **val_out, int *nnz_out)
{
    int n = by_row ? A->rows : A->cols;
    int *ptr, *idx, *cur, *newptr, *tmpidx;
    gc_real *val, *tmpval;
    int i, r, w = 0;
    if (A == NULL || ptr_out == NULL) return GC_ERR_ARG;
    ptr = (int *)calloc((size_t)n + 1, sizeof(int));
    if (!ptr) return GC_ERR_NO_MEM;
    for (i = 0; i < A->nnz; i++) ptr[(by_row ? A->row[i] : A->col[i]) + 1]++;
    for (i = 0; i < n; i++) ptr[i + 1] += ptr[i];
    idx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    val = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    cur = (int *)malloc((size_t)(n + 1) * sizeof(int));
    if (!idx || !val || !cur) { free(ptr); free(idx); free(val); free(cur); return GC_ERR_NO_MEM; }
    memcpy(cur, ptr, (size_t)(n + 1) * sizeof(int));
    for (i = 0; i < A->nnz; i++) {
        int g = by_row ? A->row[i] : A->col[i];
        int pos = cur[g]++;
        idx[pos] = by_row ? A->col[i] : A->row[i];
        val[pos] = A->val[i];
    }
    tmpidx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    tmpval = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    newptr = (int *)calloc((size_t)n + 1, sizeof(int));
    if (!tmpidx || !tmpval || !newptr) {
        free(ptr); free(idx); free(val); free(cur); free(tmpidx); free(tmpval); free(newptr);
        return GC_ERR_NO_MEM;
    }
    for (r = 0; r < n; r++) {
        int lo = ptr[r], hi = ptr[r + 1], k, newhi;
        newptr[r] = w;
        newhi = sort_dedup(lo, hi, idx, val);
        for (k = lo; k < newhi; k++) { tmpidx[w] = idx[k]; tmpval[w] = val[k]; w++; }
    }
    newptr[n] = w;
    free(ptr); free(idx); free(val); free(cur);
    *ptr_out = newptr;
    *idx_out = tmpidx;
    *val_out = tmpval;
    *nnz_out = w;
    return GC_OK;
}

gc_status gc_coo_to_csr(const gc_coo *A, gc_csr *out)
{
    gc_status st;
    if (out == NULL) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    st = coo_build(A, 1, &out->rowptr, &out->colidx, &out->val, &out->nnz);
    if (st != GC_OK) return st;
    out->rows = A->rows; out->cols = A->cols;
    return GC_OK;
}

gc_status gc_coo_to_csc(const gc_coo *A, gc_csc *out)
{
    gc_status st;
    if (out == NULL) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    st = coo_build(A, 0, &out->colptr, &out->rowidx, &out->val, &out->nnz);
    if (st != GC_OK) return st;
    out->rows = A->rows; out->cols = A->cols;
    return GC_OK;
}

/* ============================== trasposizione ============================== */

gc_status gc_csr_transpose(const gc_csr *A, gc_csc *out)
{
    int i;
    if (A == NULL || out == NULL) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    out->rows = A->cols; out->cols = A->rows; out->nnz = A->nnz;
    out->colptr = (int *)malloc((size_t)(A->rows + 1) * sizeof(int));
    out->rowidx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    out->val = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    if (!out->colptr || !out->rowidx || !out->val) { gc_csc_free(out); return GC_ERR_NO_MEM; }
    for (i = 0; i <= A->rows; i++) out->colptr[i] = A->rowptr[i];
    for (i = 0; i < A->nnz; i++) { out->rowidx[i] = A->colidx[i]; out->val[i] = A->val[i]; }
    return GC_OK;
}

gc_status gc_csc_transpose(const gc_csc *A, gc_csr *out)
{
    int i;
    if (A == NULL || out == NULL) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    out->rows = A->cols; out->cols = A->rows; out->nnz = A->nnz;
    out->rowptr = (int *)malloc((size_t)(A->cols + 1) * sizeof(int));
    out->colidx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    out->val = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    if (!out->rowptr || !out->colidx || !out->val) { gc_csr_free(out); return GC_ERR_NO_MEM; }
    for (i = 0; i <= A->cols; i++) out->rowptr[i] = A->colptr[i];
    for (i = 0; i < A->nnz; i++) { out->colidx[i] = A->rowidx[i]; out->val[i] = A->val[i]; }
    return GC_OK;
}

/* ============================== conversioni ============================== */

gc_status gc_csr_to_csc(const gc_csr *A, gc_csc *out)
{
    int *cur, i, k;
    if (A == NULL || out == NULL) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    out->rows = A->rows; out->cols = A->cols; out->nnz = A->nnz;
    out->colptr = (int *)calloc((size_t)A->cols + 1, sizeof(int));
    out->rowidx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    out->val = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    cur = (int *)malloc((size_t)(A->cols > 0 ? A->cols : 1) * sizeof(int));
    if (!out->colptr || !out->rowidx || !out->val || !cur) {
        gc_csc_free(out); free(cur); return GC_ERR_NO_MEM;
    }
    for (k = 0; k < A->nnz; k++) out->colptr[A->colidx[k] + 1]++;
    for (i = 0; i < A->cols; i++) out->colptr[i + 1] += out->colptr[i];
    memcpy(cur, out->colptr, (size_t)A->cols * sizeof(int));
    for (i = 0; i < A->rows; i++)
        for (k = A->rowptr[i]; k < A->rowptr[i + 1]; k++) {
            int j = A->colidx[k];
            int pos = cur[j]++;
            out->rowidx[pos] = i;   /* righe crescenti -> ordinate per colonna */
            out->val[pos] = A->val[k];
        }
    free(cur);
    return GC_OK;
}

gc_status gc_csc_to_csr(const gc_csc *A, gc_csr *out)
{
    int *cur, i, k;
    if (A == NULL || out == NULL) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    out->rows = A->rows; out->cols = A->cols; out->nnz = A->nnz;
    out->rowptr = (int *)calloc((size_t)A->rows + 1, sizeof(int));
    out->colidx = (int *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int));
    out->val = (gc_real *)malloc((size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real));
    cur = (int *)malloc((size_t)(A->rows > 0 ? A->rows : 1) * sizeof(int));
    if (!out->rowptr || !out->colidx || !out->val || !cur) {
        gc_csr_free(out); free(cur); return GC_ERR_NO_MEM;
    }
    for (k = 0; k < A->nnz; k++) out->rowptr[A->rowidx[k] + 1]++;
    for (i = 0; i < A->rows; i++) out->rowptr[i + 1] += out->rowptr[i];
    memcpy(cur, out->rowptr, (size_t)A->rows * sizeof(int));
    for (i = 0; i < A->cols; i++)
        for (k = A->colptr[i]; k < A->colptr[i + 1]; k++) {
            int r = A->rowidx[k];
            int pos = cur[r]++;
            out->colidx[pos] = i;   /* colonne crescenti -> ordinate per riga */
            out->val[pos] = A->val[k];
        }
    free(cur);
    return GC_OK;
}

gc_status gc_dense_to_csr(int rows, int cols, const gc_real *A, int lda, gc_csr *out)
{
    int *cur, i, j, nnz = 0;
    if (A == NULL || out == NULL || rows < 0 || cols < 0) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    out->rows = rows; out->cols = cols;
    out->rowptr = (int *)calloc((size_t)rows + 1, sizeof(int));
    if (!out->rowptr) return GC_ERR_NO_MEM;
    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++)
            if (A[(size_t)i * (size_t)lda + (size_t)j] != 0.0) out->rowptr[i + 1]++;
    for (i = 0; i < rows; i++) out->rowptr[i + 1] += out->rowptr[i];
    nnz = out->rowptr[rows];
    out->nnz = nnz;
    out->colidx = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    out->val = (gc_real *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(gc_real));
    cur = (int *)malloc((size_t)(rows > 0 ? rows : 1) * sizeof(int));
    if (!out->colidx || !out->val || !cur) { gc_csr_free(out); free(cur); return GC_ERR_NO_MEM; }
    memcpy(cur, out->rowptr, (size_t)rows * sizeof(int));
    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++) {
            gc_real v = A[(size_t)i * (size_t)lda + (size_t)j];
            if (v != 0.0) {
                int pos = cur[i]++;
                out->colidx[pos] = j;
                out->val[pos] = v;
            }
        }
    free(cur);
    return GC_OK;
}

int gc_csr_nnz(const gc_csr *A)
{
    return (A == NULL) ? 0 : A->nnz;
}

void gc_csr_row_nnz(const gc_csr *A, int *out)
{
    int i;
    if (A == NULL || out == NULL) return;
    for (i = 0; i < A->rows; i++) out[i] = A->rowptr[i + 1] - A->rowptr[i];
}
