/*
 * gc_dd.h — aritmetica double-float (DD) per precisione FP64-class (da apple-bottom).
 *
 * Ogni FP64 è rappresentato come coppia di float32 {hi, lo} (~48 bit di mantissa,
 * ~1e-15 di precisione relativa). Tecnica Dekker/Knuth con FMA, necessaria sulle
 * GPU Apple (che non hanno FP64 nativo). Usabile su CPU (reference) e nel backend
 * Metal di gmbcompute.
 */
#ifndef GMBCOMPUTE_GC_DD_H
#define GMBCOMPUTE_GC_DD_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gc_dd { float hi, lo; } gc_dd;

/* Conversione. */
gc_dd gc_dd_from_real(gc_real x);
gc_real gc_dd_to_real(gc_dd d);

/* Error-free transformations (Dekker/Knuth). */
void gc_dd_two_sum(float a, float b, float *s, float *e);
void gc_dd_fast_two_sum(float a, float b, float *s, float *e);
void gc_dd_two_product(float a, float b, float *p, float *e);

/* Aritmetica DD. */
gc_dd gc_dd_add(gc_dd a, gc_dd b);
gc_dd gc_dd_sub(gc_dd a, gc_dd b);
gc_dd gc_dd_mul(gc_dd a, gc_dd b);
gc_dd gc_dd_div(gc_dd a, gc_dd b);
gc_dd gc_dd_fma(gc_dd a, gc_dd b, gc_dd c);
gc_dd gc_dd_sqrt(gc_dd a);
gc_dd gc_dd_neg(gc_dd a);
gc_dd gc_dd_abs(gc_dd a);

/* Prodotto scalare DD (accumulo DD). */
gc_dd gc_dd_dot(int n, const gc_dd *a, const gc_dd *b);
/* Prodotto matriciale DD: C (m×n) = A (m×k) B (k×n), accumulo DD. */
void gc_dd_gemm(int m, int n, int k, const gc_dd *A, const gc_dd *B, gc_dd *C);

/* Versioni accelerate (Metal se compilato con GC_HAS_METAL, altrimenti CPU). */
gc_dd gc_dd_dot_accel(int n, const gc_dd *a, const gc_dd *b);
void gc_dd_gemm_accel(int m, int n, int k, const gc_dd *A, const gc_dd *B, gc_dd *C);

/* Conversioni array. */
void gc_dd_from_reals(const gc_real *x, gc_dd *out, int n);
void gc_dd_to_reals(const gc_dd *x, gc_real *out, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_DD_H */
