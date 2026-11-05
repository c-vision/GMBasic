/*
 * gs_ts2.h — serie temporali: ARMA e differenziazione frazionaria (da ccmath).
 */
#ifndef GMBSTATS_GS_TS2_H
#define GMBSTATS_GS_TS2_H

#include "gc_core.h"
#include "gs_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Simula una serie ARMA(p,q): x[t] = sum ar[i] x[t-1-i] + e[t] + sum ma[j] e[t-1-j]. */
gc_status gs_ts_arma_sim(gc_real *x, int n, const gc_real *ar, int p,
                         const gc_real *ma, int q, gs_rng *rng);
/* Differenziazione frazionaria (filtro binomiale di ordine d). */
gc_status gs_ts_frac_diff(const gc_real *x, int n, gc_real d, gc_real *out);
/* Integrazione frazionaria (inversa). */
gc_status gs_ts_frac_int(const gc_real *x, int n, gc_real d, gc_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_TS2_H */
