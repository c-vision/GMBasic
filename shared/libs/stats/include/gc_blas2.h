/*
 * gc_blas2.h — BLAS di livello 2 (da stdlib-js blas-base).
 *
 * Matrici row-major. Operazioni matrice-vettore: gemv, ger (rank-1),
 * trmv/trsv (triangolari), symv/syr/syr2 (simmetriche). Varianti float32/64.
 *
 * Convenzioni flag: upper (1=triangolo superiore, 0=inferiore),
 * trans (0=A, 1=A^T), unit (1=diagonale unitaria).
 */
#ifndef GMBCOMPUTE_GC_BLAS2_H
#define GMBCOMPUTE_GC_BLAS2_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* y = alpha*A*x + beta*y, A (m×n) row-major con leading dimension lda. */
void gc_blas_sgemv(int m, int n, float alpha, const float *A, int lda,
                   const float *x, int incx, float beta, float *y, int incy);
void gc_blas_dgemv(int m, int n, double alpha, const double *A, int lda,
                   const double *x, int incx, double beta, double *y, int incy);

/* A = alpha*x*y^T + A, A (m×n) row-major. */
void gc_blas_sger(int m, int n, float alpha, const float *x, int incx,
                  const float *y, int incy, float *A, int lda);
void gc_blas_dger(int m, int n, double alpha, const double *x, int incx,
                  const double *y, int incy, double *A, int lda);

/* x = op(A)*x, A triangolare n×n. */
void gc_blas_strmv(int n, int upper, int trans, int unit, const float *A, int lda,
                   float *x, int incx);
void gc_blas_dtrmv(int n, int upper, int trans, int unit, const double *A, int lda,
                   double *x, int incx);

/* risolve op(A)*x = b, A triangolare n×n. */
void gc_blas_strsv(int n, int upper, int trans, int unit, const float *A, int lda,
                   float *x, int incx);
void gc_blas_dtrsv(int n, int upper, int trans, int unit, const double *A, int lda,
                   double *x, int incx);

/* y = alpha*A*x + beta*y, A simmetrica n×n (solo il triangolo indicato). */
void gc_blas_ssymv(int n, int upper, float alpha, const float *A, int lda,
                   const float *x, int incx, float beta, float *y, int incy);
void gc_blas_dsymv(int n, int upper, double alpha, const double *A, int lda,
                   const double *x, int incx, double beta, double *y, int incy);

/* A = alpha*x*x^T + A, A simmetrica. */
void gc_blas_ssyr(int n, int upper, float alpha, const float *x, int incx, float *A, int lda);
void gc_blas_dsyr(int n, int upper, double alpha, const double *x, int incx, double *A, int lda);

/* A = alpha*(x*y^T + y*x^T) + A, A simmetrica. */
void gc_blas_ssyr2(int n, int upper, float alpha, const float *x, int incx,
                   const float *y, int incy, float *A, int lda);
void gc_blas_dsyr2(int n, int upper, double alpha, const double *x, int incx,
                   const double *y, int incy, double *A, int lda);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_BLAS2_H */
