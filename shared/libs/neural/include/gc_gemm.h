/*
 * gc_gemm.h — moltiplicazione matriciale densa (SIMD).
 *
 * Row-major. `ldx` è la leading dimension (>= colonne logiche).
 *   gc_gemm : C = alpha * A * B + beta * C        (A: m×k, B: k×n, C: m×n)
 *   gc_gemv : y = alpha * A * x + beta * y        (A: m×k, x: k, y: m)
 *   gc_outer: A = alpha * x yᵀ + beta * A          (x: m, y: n)
 */
#ifndef GMMATH_GC_GEMM_H
#define GMMATH_GC_GEMM_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void gc_gemm(int m, int n, int k, gc_real alpha,
             const gc_real *A, int lda,
             const gc_real *B, int ldb,
             gc_real beta, gc_real *C, int ldc);

void gc_gemv(int m, int k, gc_real alpha,
             const gc_real *A, int lda,
             const gc_real *x,
             gc_real beta, gc_real *y);

void gc_outer(int m, int n, gc_real alpha,
              const gc_real *x, const gc_real *y,
              gc_real beta, gc_real *A, int lda);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GC_GEMM_H */
