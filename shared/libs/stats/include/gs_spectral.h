/*
 * gs_spectral.h — strumenti spettrali / Random Matrix Theory (da paper SSRN
 * 2026-09: perturbazione di matrici di correlazione e filtraggio del rumore).
 *
 * Marchenko-Pastur: per una matrice campionaria n x T con q = n/T, il bulk di
 * rumore giace in [lambda-, lambda+]. Il filtraggio sostituisce gli autovalori
 * sotto lambda+ con la loro media e riscala la diagonale, separando i fattori
 * di segnale dalle direzioni di rumore.
 */
#ifndef GMBSTATS_GS_SPECTRAL_H
#define GMBSTATS_GS_SPECTRAL_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Estremi del bulk di Marchenko-Pastur: lambda± = sigma2 (1 ± sqrt(q))^2,
 * q = n/T. Ritorna GC_OK o GC_ERR_ARG. */
gc_status gs_rmt_mp_bounds(int n, int T, gc_real sigma2,
                           gc_real *out_lambda_plus, gc_real *out_lambda_minus);

/* Filtro RMT su matrice simmetrica n x n (covarianza/correlazione):
 * autovalori < lambda+ sostituiti con la media del bulk, ricostruzione, poi
 * riscalatura per ripristinare la diagonale originale (per una correlazione,
 * diagonale = 1). Ritorna GC_OK / GC_ERR_ARG / GC_ERR_NO_MEM. */
gc_status gs_rmt_filter(const gc_real *cov, int n, int T, gc_real *out_cov);

/* Coerenza di un autovettore: n * max_i v_i^2 (>= 1). Vicino a 1 = delocalizzato. */
gc_real gs_rmt_coherence(const gc_real *v, int n);

/* Norma spettrale di A (n x n simmetrica) = max |autovalore|. Bound di Weyl per
 * ||A - B||_2 passando la differenza. */
gc_real gs_rmt_spectral_norm(const gc_real *A, int n);

/* Bound di Davis-Kahan per la rotazione di un autovettore: ||E||_2 / gap. */
gc_real gs_rmt_davis_kahan_bound(gc_real e_norm, gc_real gap);

/* Angolo principale tra due autovettori (radianti): acos(|<u,v>|/(||u|| ||v||)). */
gc_real gs_rmt_principal_angle(const gc_real *u, const gc_real *v, int n);

/* Tightness di un bound: observed/bound (in [0,1] ~ informativo). */
gc_real gs_rmt_tightness(gc_real observed, gc_real bound);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_SPECTRAL_H */
