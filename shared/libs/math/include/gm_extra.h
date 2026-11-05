/*
 * gm_extra.h — metodi matematici recuperati dal repo OPTIMAL777/ACI
 * (OptimalTransport, CompressedSensing, WaveletAnalysis, VariationalCalculus,
 * OperatorTheory) e portati come algoritmi C.
 */
#ifndef GMMATH_GM_EXTRA_H
#define GMMATH_GM_EXTRA_H

#include "gm_core.h"

typedef double (*gm_ga_fn)(const double *x, void *ctx);
int gm_genetic_optimize(gm_ga_fn f, void *ctx, const double *lo, const double *hi, int d, int pop, int gens, long seed, double *out_x);

#ifdef __cplusplus
extern "C" {
#endif

/* Compressed sensing: Orthogonal Matching Pursuit. A: m x n (row-major), y: m.
 * Recupera k atomi; out_x: n (sparso). Ritorna 0. */
int gm_omp(const gm_real *A, const gm_real *y, int m, int n, int k, gm_real *out_x);

/* DWT di Haar multilivello (n potenza di 2). out[0..n-1]: approx in testa,
 * dettagli a seguire (convenzione piramidale). */
int gm_dwt_haar(const gm_real *x, int n, int levels, gm_real *out);

/* CWT continuo con wavelet di Morlet: |CWT(a,b)| per ogni scala a e posizione b.
 * out: nscales x n (row-major). */
int gm_cwt_morlet(const gm_real *x, int n, const gm_real *scales, int nscales, gm_real *out);

/* Leggende transform: f*(p) = sup_x (p*x - f(x)). x/f: n; p: np; out: np. */
int gm_legendre_transform(const gm_real *x, const gm_real *f, int n,
                          const gm_real *p, int np, gm_real *out);

/* Distanza di Wasserstein-1 (1D) tra due misure empiriche (pesi uguali). */
gm_real gm_wasserstein1_1d(const gm_real *a, int na, const gm_real *b, int nb);

/* Trasporto ottimo discreto via Sinkhorn-Knopp. a: na, b: nb (pesi),
 * M: na x nb costi; lam regolarizzazione; iters; out_T: na x nb. */
int gm_sinkhorn(const gm_real *a, int na, const gm_real *b, int nb,
                const gm_real *M, gm_real lam, int iters, gm_real *out_T);

/* Raggio spettrale via power iteration su |A| (n x n). */
gm_real gm_spectral_radius(const gm_real *A, int n, int iters);

/* Norma di Hilbert-Schmidt (Frobenius) di A (n x n). */
gm_real gm_hs_norm(const gm_real *A, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_EXTRA_H */