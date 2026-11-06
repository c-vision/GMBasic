/* gn_core.c — tipi e operazioni di base di gmbneural. */
#include "gn_core.h"
#include "gc_gemm.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gn_status gn_mat_alloc(gn_mat *m, int rows, int cols)
{
    if (!m || rows <= 0 || cols <= 0) return GN_ERR_ARG;
    m->rows = rows; m->cols = cols;
    m->data = (gn_real *)calloc((size_t)rows * (size_t)cols, sizeof(gn_real));
    if (!m->data) return GN_ERR_NO_MEM;
    return GN_OK;
}

void gn_mat_free(gn_mat *m)
{
    if (!m) return;
    free(m->data);
    m->data = NULL;
    m->rows = m->cols = 0;
}

gn_status gn_mat_zero(gn_mat *m)
{
    if (!m || !m->data) return GN_ERR_ARG;
    memset(m->data, 0, (size_t)m->rows * (size_t)m->cols * sizeof(gn_real));
    return GN_OK;
}

gn_status gn_mat_fill(gn_mat *m, gn_real v)
{
    int i, n;
    if (!m || !m->data) return GN_ERR_ARG;
    n = m->rows * m->cols;
    for (i = 0; i < n; i++) m->data[i] = v;
    return GN_OK;
}

gn_status gn_mat_copy(gn_mat *dst, const gn_mat *src)
{
    if (!dst || !src || dst->rows != src->rows || dst->cols != src->cols) return GN_ERR_ARG;
    memcpy(dst->data, src->data, (size_t)src->rows * (size_t)src->cols * sizeof(gn_real));
    return GN_OK;
}

gn_status gn_mat_mul(gn_mat *C, const gn_mat *A, const gn_mat *B)
{
    if (!C || !A || !B || A->cols != B->rows || C->rows != A->rows || C->cols != B->cols)
        return GN_ERR_ARG;
    gc_gemm(A->rows, B->cols, A->cols, 1.0, A->data, A->cols, B->data, B->cols, 0.0, C->data, C->cols);
    return GN_OK;
}

gn_status gn_mat_mul_nt(gn_mat *C, const gn_mat *A, const gn_mat *B)
{
    /* C = A * B^T */
    int i, j, k;
    if (!C || !A || !B || A->cols != B->cols || C->rows != A->rows || C->cols != B->rows)
        return GN_ERR_ARG;
    for (i = 0; i < A->rows; i++)
        for (j = 0; j < B->rows; j++) {
            gn_real s = 0.0;
            for (k = 0; k < A->cols; k++) s += GN_AT(*A, i, k) * GN_AT(*B, j, k);
            GN_AT(*C, i, j) = s;
        }
    return GN_OK;
}

gn_status gn_mat_mul_tn(gn_mat *C, const gn_mat *A, const gn_mat *B)
{
    /* C = A^T * B */
    int i, j, k;
    if (!C || !A || !B || A->rows != B->rows || C->rows != A->cols || C->cols != B->cols)
        return GN_ERR_ARG;
    for (i = 0; i < A->cols; i++)
        for (j = 0; j < B->cols; j++) {
            gn_real s = 0.0;
            for (k = 0; k < A->rows; k++) s += GN_AT(*A, k, i) * GN_AT(*B, k, j);
            GN_AT(*C, i, j) = s;
        }
    return GN_OK;
}

gn_status gn_mat_mul_tt(gn_mat *C, const gn_mat *A, const gn_mat *B)
{
    /* C = A^T * B^T */
    int i, j, k;
    if (!C || !A || !B || A->rows != B->cols || C->rows != A->cols || C->cols != B->rows)
        return GN_ERR_ARG;
    for (i = 0; i < A->cols; i++)
        for (j = 0; j < B->rows; j++) {
            gn_real s = 0.0;
            for (k = 0; k < A->rows; k++) s += GN_AT(*A, k, i) * GN_AT(*B, j, k);
            GN_AT(*C, i, j) = s;
        }
    return GN_OK;
}

gn_status gn_mat_add(gn_mat *C, const gn_mat *A, const gn_mat *B)
{
    int i, n;
    if (!C || !A || !B || C->rows != A->rows || C->cols != A->cols
        || A->rows != B->rows || A->cols != B->cols) return GN_ERR_ARG;
    n = A->rows * A->cols;
    for (i = 0; i < n; i++) C->data[i] = A->data[i] + B->data[i];
    return GN_OK;
}

gn_status gn_mat_add_scaled(gn_mat *C, const gn_mat *A, gn_real s, const gn_mat *B)
{
    int i, n;
    if (!C || !A || !B || C->rows != A->rows || C->cols != A->cols
        || A->rows != B->rows || A->cols != B->cols) return GN_ERR_ARG;
    n = A->rows * A->cols;
    for (i = 0; i < n; i++) C->data[i] = A->data[i] + s * B->data[i];
    return GN_OK;
}

gn_status gn_mat_scale(gn_mat *A, gn_real s)
{
    int i, n;
    if (!A || !A->data) return GN_ERR_ARG;
    n = A->rows * A->cols;
    for (i = 0; i < n; i++) A->data[i] *= s;
    return GN_OK;
}

gn_status gn_mat_add_inplace(gn_mat *A, const gn_mat *B)
{
    int i, n;
    if (!A || !B || A->rows != B->rows || A->cols != B->cols) return GN_ERR_ARG;
    n = A->rows * A->cols;
    for (i = 0; i < n; i++) A->data[i] += B->data[i];
    return GN_OK;
}

gn_status gn_mat_apply(gn_mat *out, const gn_mat *A, gn_unary f)
{
    int i, n;
    if (!out || !A || !f || out->rows != A->rows || out->cols != A->cols) return GN_ERR_ARG;
    n = A->rows * A->cols;
    for (i = 0; i < n; i++) out->data[i] = f(A->data[i]);
    return GN_OK;
}

gn_real gn_mat_max(const gn_mat *A)
{
    int i, n;
    gn_real mx;
    if (!A || !A->data || A->rows * A->cols == 0) return 0.0;
    n = A->rows * A->cols;
    mx = A->data[0];
    for (i = 1; i < n; i++) if (A->data[i] > mx) mx = A->data[i];
    return mx;
}

gn_real gn_mat_absmax(const gn_mat *A)
{
    int i, n;
    gn_real mx = 0.0;
    if (!A || !A->data) return 0.0;
    n = A->rows * A->cols;
    for (i = 0; i < n; i++) { gn_real a = fabs(A->data[i]); if (a > mx) mx = a; }
    return mx;
}
