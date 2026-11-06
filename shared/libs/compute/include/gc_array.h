/*
 * gc_array.h — kernel densi su array di gc_real (SIMD).
 *
 * Elementwise, riduzioni e operazioni fuse (axpy/axpby) sui quali si basano
 * statistica e algebra. Vettorizzati su NEON (aarch64) con fallback scalare.
 */
#ifndef GMBCOMPUTE_GC_ARRAY_H
#define GMBCOMPUTE_GC_ARRAY_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Riduzioni. */
gc_real gc_array_sum(const gc_real *a, int n);
gc_real gc_array_sum_sq(const gc_real *a, int n);
gc_real gc_array_dot(const gc_real *a, const gc_real *b, int n);
gc_real gc_array_norm2(const gc_real *a, int n);
gc_real gc_array_norm_inf(const gc_real *a, int n);
gc_real gc_array_asum(const gc_real *a, int n);
gc_real gc_array_min(const gc_real *a, int n);
gc_real gc_array_max(const gc_real *a, int n);
gc_real gc_array_amax(const gc_real *a, int n);
gc_real gc_array_amin(const gc_real *a, int n);
int gc_array_imin(const gc_real *a, int n);
int gc_array_imax(const gc_real *a, int n);

/* Elementwise. `c` può coincidere con `a`/`b` (in-place). */
void gc_array_add(const gc_real *a, const gc_real *b, gc_real *c, int n);
void gc_array_sub(const gc_real *a, const gc_real *b, gc_real *c, int n);
void gc_array_mul(const gc_real *a, const gc_real *b, gc_real *c, int n);
void gc_array_div(const gc_real *a, const gc_real *b, gc_real *c, int n);
void gc_array_scale(gc_real *a, gc_real s, int n);
void gc_array_add_scalar(gc_real *a, gc_real s, int n);
void gc_array_copy(const gc_real *a, gc_real *c, int n);
void gc_array_fill(gc_real *a, gc_real v, int n);
void gc_array_sqrt(const gc_real *a, gc_real *c, int n);
void gc_array_abs(const gc_real *a, gc_real *c, int n);

/* Fuse (read once / write once). */
void gc_array_axpy(gc_real alpha, const gc_real *x, gc_real *y, int n);
void gc_array_axpby(gc_real alpha, const gc_real *x, gc_real beta, gc_real *y, int n);
/* y = alpha * x (scrittura). */
void gc_array_scaled(gc_real alpha, const gc_real *x, gc_real *y, int n);
/* c = a + s * b (fusione elementwise). */
void gc_array_add_scaled(const gc_real *a, gc_real s, const gc_real *b, gc_real *c, int n);

/* linspace: n valori equispaziati in [a,b] (estremi inclusi). */
void gc_linspace(gc_real a, gc_real b, int n, gc_real *out);
/* arange: valori start, start+step, ... < stop (se step>0). Ritorna il conteggio
 * scritto in out (fino a maxn). (da LinearSystemsToolkit) */
int gc_arange(gc_real start, gc_real stop, gc_real step, gc_real *out, int maxn);

/* Riduzioni con scarto (per varianza/covarianza/correlazione in una passata SIMD). */
gc_real gc_array_sum_sq_dev(const gc_real *a, int n, gc_real mean);
gc_real gc_array_dot_dev(const gc_real *a, const gc_real *b, int n, gc_real ma, gc_real mb);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_ARRAY_H */
