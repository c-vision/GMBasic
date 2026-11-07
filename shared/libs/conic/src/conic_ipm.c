/*
 * conic_ipm.c — shim FFI del solver interior-point (LP/QP).
 *
 * Tutti i parametri sono espliciti: A (m x n row-major), Q (n x n row-major,
 * solo QP), b, c, tolleranze, iterazioni, vettori di uscita x (n), y (m), z (n).
 * Le varianti CSC prendono la matrice sparsa in formato compress-sparse-column
 * (Aptr n+1, Arow/Aval per gli nnz); per il QP anche Q nel triangolo inferiore.
 *
 * Ritorno: 0 ottimo, 1 max iter, 2 memoria, 3 sistema singolare, -1 argomenti.
 */
#include "ipm.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;

static int *cvt_i(const i64 *src, i64 n)
{
    int *d;
    i64 i;
    if (!src || n <= 0) return NULL;
    d = (int *)malloc((size_t)n * sizeof(int));
    if (!d) return NULL;
    for (i = 0; i < n; i++) d[i] = (int)src[i];
    return d;
}

i64 ofx_ipm_lp(const double *A, int64_t A_n, i64 m, i64 n,
               const double *b, int64_t b_n, const double *c, int64_t c_n,
               double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter,
               double *x, int64_t x_n, double *y, int64_t y_n, double *z, int64_t z_n)
{
    if (!A || !b || !c || !x || !y || !z || m < 1 || n < 1) return -1;
    return (i64)ipm_solve_std(A, NULL, (int)m, (int)n, b, c,
                              tol_gap, tol_pfeas, tol_dfeas, (int)max_iter,
                              x, y, z, NULL, NULL);
}

i64 ofx_ipm_qp(const double *A, int64_t A_n, const double *Q, int64_t Q_n, i64 m, i64 n,
               const double *b, int64_t b_n, const double *c, int64_t c_n,
               double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter,
               double *x, int64_t x_n, double *y, int64_t y_n, double *z, int64_t z_n)
{
    if (!A || !Q || !b || !c || !x || !y || !z || m < 1 || n < 1) return -1;
    return (i64)ipm_solve_std(A, Q, (int)m, (int)n, b, c,
                              tol_gap, tol_pfeas, tol_dfeas, (int)max_iter,
                              x, y, z, NULL, NULL);
}

i64 ofx_ipm_lp_warm(const double *A, int64_t A_n, i64 m, i64 n,
                    const double *b, int64_t b_n, const double *c, int64_t c_n,
                    double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter,
                    double *x, int64_t x_n, double *y, int64_t y_n, double *z, int64_t z_n,
                    const double *x0, int64_t x0_n, const double *y0, int64_t y0_n)
{
    if (!A || !b || !c || !x || !y || !z || !x0 || !y0 || m < 1 || n < 1) return -1;
    return (i64)ipm_solve_std(A, NULL, (int)m, (int)n, b, c,
                              tol_gap, tol_pfeas, tol_dfeas, (int)max_iter,
                              x, y, z, x0, y0);
}

i64 ofx_ipm_qp_warm(const double *A, int64_t A_n, const double *Q, int64_t Q_n, i64 m, i64 n,
                    const double *b, int64_t b_n, const double *c, int64_t c_n,
                    double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter,
                    double *x, int64_t x_n, double *y, int64_t y_n, double *z, int64_t z_n,
                    const double *x0, int64_t x0_n, const double *y0, int64_t y0_n)
{
    if (!A || !Q || !b || !c || !x || !y || !z || !x0 || !y0 || m < 1 || n < 1) return -1;
    return (i64)ipm_solve_std(A, Q, (int)m, (int)n, b, c,
                              tol_gap, tol_pfeas, tol_dfeas, (int)max_iter,
                              x, y, z, x0, y0);
}

i64 ofx_ipm_lp_csc(const i64 *Aptr, int64_t Aptr_n, const i64 *Arow, int64_t Arow_n, const double *Aval, int64_t Aval_n,
                   i64 m, i64 n, const double *b, int64_t b_n, const double *c, int64_t c_n,
                   double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter,
                   double *x, int64_t x_n, double *y, int64_t y_n, double *z, int64_t z_n)
{
    int *ap, *ar;
    i64 r;
    if (!Aptr || !Arow || !Aval || !b || !c || !x || !y || !z || m < 1 || n < 1) return -1;
    ap = cvt_i(Aptr, n + 1);
    ar = cvt_i(Arow, Aptr[n]);
    if (!ap || (Aptr[n] > 0 && !ar)) { free(ap); free(ar); return -1; }
    r = (i64)ipm_solve_std_csc(ap, ar, Aval, (int)m, (int)n, b, c,
                               tol_gap, tol_pfeas, tol_dfeas, (int)max_iter,
                               x, y, z, NULL, NULL);
    free(ap); free(ar);
    return r;
}

i64 ofx_ipm_qp_csc(const i64 *Aptr, int64_t Aptr_n, const i64 *Arow, int64_t Arow_n, const double *Aval, int64_t Aval_n,
                   const i64 *Qptr, int64_t Qptr_n, const i64 *Qrow, int64_t Qrow_n, const double *Qval, int64_t Qval_n,
                   i64 m, i64 n, const double *b, int64_t b_n, const double *c, int64_t c_n,
                   double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter,
                   double *x, int64_t x_n, double *y, int64_t y_n, double *z, int64_t z_n)
{
    int *ap, *ar, *qp, *qr;
    i64 r, nnzq;
    if (!Aptr || !Arow || !Aval || !Qptr || !Qrow || !Qval ||
        !b || !c || !x || !y || !z || m < 1 || n < 1) return -1;
    nnzq = Qptr[n];
    ap = cvt_i(Aptr, n + 1);
    ar = cvt_i(Arow, Aptr[n]);
    qp = cvt_i(Qptr, n + 1);
    qr = cvt_i(Qrow, nnzq);
    if (!ap || !qp || (Aptr[n] > 0 && !ar) || (nnzq > 0 && !qr)) {
        free(ap); free(ar); free(qp); free(qr);
        return -1;
    }
    r = (i64)ipm_solve_qp_csc(ap, ar, Aval, qp, qr, Qval, (int)m, (int)n, b, c,
                              tol_gap, tol_pfeas, tol_dfeas, (int)max_iter,
                              x, y, z, NULL, NULL);
    free(ap); free(ar); free(qp); free(qr);
    return r;
}

#include <stdint.h>
/* Trampolini DECLARE NATIVE (ffi_ext_native.pl): firma originale, estensioni = INT64_MAX. */
i64 ofx_ipm_lp__nat(const double *A, i64 m, i64 n, const double *b, const double *c, double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter, double *x, double *y, double *z) { return ofx_ipm_lp(A, INT64_MAX, m, n, b, INT64_MAX, c, INT64_MAX, tol_gap, tol_pfeas, tol_dfeas, max_iter, x, INT64_MAX, y, INT64_MAX, z, INT64_MAX); }
i64 ofx_ipm_lp_csc__nat(const i64 *Aptr, const i64 *Arow, const double *Aval, i64 m, i64 n, const double *b, const double *c, double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter, double *x, double *y, double *z) { return ofx_ipm_lp_csc(Aptr, INT64_MAX, Arow, INT64_MAX, Aval, INT64_MAX, m, n, b, INT64_MAX, c, INT64_MAX, tol_gap, tol_pfeas, tol_dfeas, max_iter, x, INT64_MAX, y, INT64_MAX, z, INT64_MAX); }
i64 ofx_ipm_lp_warm__nat(const double *A, i64 m, i64 n, const double *b, const double *c, double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter, double *x, double *y, double *z, const double *x0, const double *y0) { return ofx_ipm_lp_warm(A, INT64_MAX, m, n, b, INT64_MAX, c, INT64_MAX, tol_gap, tol_pfeas, tol_dfeas, max_iter, x, INT64_MAX, y, INT64_MAX, z, INT64_MAX, x0, INT64_MAX, y0, INT64_MAX); }
i64 ofx_ipm_qp__nat(const double *A, const double *Q, i64 m, i64 n, const double *b, const double *c, double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter, double *x, double *y, double *z) { return ofx_ipm_qp(A, INT64_MAX, Q, INT64_MAX, m, n, b, INT64_MAX, c, INT64_MAX, tol_gap, tol_pfeas, tol_dfeas, max_iter, x, INT64_MAX, y, INT64_MAX, z, INT64_MAX); }
i64 ofx_ipm_qp_csc__nat(const i64 *Aptr, const i64 *Arow, const double *Aval, const i64 *Qptr, const i64 *Qrow, const double *Qval, i64 m, i64 n, const double *b, const double *c, double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter, double *x, double *y, double *z) { return ofx_ipm_qp_csc(Aptr, INT64_MAX, Arow, INT64_MAX, Aval, INT64_MAX, Qptr, INT64_MAX, Qrow, INT64_MAX, Qval, INT64_MAX, m, n, b, INT64_MAX, c, INT64_MAX, tol_gap, tol_pfeas, tol_dfeas, max_iter, x, INT64_MAX, y, INT64_MAX, z, INT64_MAX); }
i64 ofx_ipm_qp_warm__nat(const double *A, const double *Q, i64 m, i64 n, const double *b, const double *c, double tol_gap, double tol_pfeas, double tol_dfeas, i64 max_iter, double *x, double *y, double *z, const double *x0, const double *y0) { return ofx_ipm_qp_warm(A, INT64_MAX, Q, INT64_MAX, m, n, b, INT64_MAX, c, INT64_MAX, tol_gap, tol_pfeas, tol_dfeas, max_iter, x, INT64_MAX, y, INT64_MAX, z, INT64_MAX, x0, INT64_MAX, y0, INT64_MAX); }
