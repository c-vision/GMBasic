/*
 * gc_blas3.h — BLAS di livello 3 (da MuBLIS): SYMM, SYRK, SYR2K, TRMM, TRSM.
 *
 * Matrici row-major. Flag:
 *   side: 0=left, 1=right ; uplo: 0=lower, 1=upper ;
 *   trans: 0=no, 1=trans   ; diag: 0=non-unit, 1=unit.
 * Varianti float32 (s*) e float64 (d*).
 */
#ifndef GMBCOMPUTE_GC_BLAS3_H
#define GMBCOMPUTE_GC_BLAS3_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C = alpha*op(A)*op(B) + beta*C, con A simmetrica (m×m se left, n×n se right). */
void gc_blas_ssymm(int m, int n, float alpha, const float *A, int lda,
                   const float *B, int ldb, float beta, float *C, int ldc,
                   int side, int uplo);
void gc_blas_dsymm(int m, int n, double alpha, const double *A, int lda,
                   const double *B, int ldb, double beta, double *C, int ldc,
                   int side, int uplo);

/* C = alpha*op(A)*op(A)^T + beta*C, C simmetrica n×n (solo il triangolo `uplo`).
 * trans=0: A (n×k) ; trans=1: A (k×n). */
void gc_blas_ssyrk(int n, int k, float alpha, const float *A, int lda,
                   float beta, float *C, int ldc, int uplo, int trans);
void gc_blas_dsyrk(int n, int k, double alpha, const double *A, int lda,
                   double beta, double *C, int ldc, int uplo, int trans);

/* C = alpha*A*B^T + alpha*B*A^T + beta*C (trans=0, A,B n×k) o
 * C = alpha*A^T*B + alpha*B^T*A + beta*C (trans=1, A,B k×n). C simmetrica. */
void gc_blas_ssyr2k(int n, int k, float alpha, const float *A, int lda,
                    const float *B, int ldb, float beta, float *C, int ldc,
                    int uplo, int trans);
void gc_blas_dsyr2k(int n, int k, double alpha, const double *A, int lda,
                    const double *B, int ldb, double beta, double *C, int ldc,
                    int uplo, int trans);

/* B = alpha*op(A)*B (left) o alpha*B*op(A) (right), A triangolare. */
void gc_blas_strmm(int m, int n, float alpha, const float *A, int lda,
                   float *B, int ldb, int side, int uplo, int trans, int diag);
void gc_blas_dtrmm(int m, int n, double alpha, const double *A, int lda,
                   double *B, int ldb, int side, int uplo, int trans, int diag);

/* Risolve op(A)*X = alpha*B (left) o X*op(A) = alpha*B (right). X in B. */
void gc_blas_strsm(int m, int n, float alpha, const float *A, int lda,
                   float *B, int ldb, int side, int uplo, int trans, int diag);
void gc_blas_dtrsm(int m, int n, double alpha, const double *A, int lda,
                   double *B, int ldb, int side, int uplo, int trans, int diag);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_BLAS3_H */
