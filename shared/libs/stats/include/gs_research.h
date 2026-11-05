/*
 * gs_research.h — metodi concreti da AI/Algorithms/Researches:
 *  - p-variation (Labadie-Lehalle): l_p analogo della varianza; p = 1/H per
 *    processi self-similar di esponente H;
 *  - HAR-RV (Heterogeneous Autoregressive, Corsi; componente di SHARP
 *    Cattivelli-Pirino): previsione della volatilità realizzata.
 */
#ifndef GMBSTATS_GS_RESEARCH_H
#define GMBSTATS_GS_RESEARCH_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* p-variation discreta: somma |x[i+scale]-x[i]|^p. */
gc_real gs_p_variation(const gc_real *x, int n, gc_real p, int scale);

/* Esponente p = 1/H dalla self-similarita' H (NaN se H<=0). */
gc_real gs_hurst_to_p(gc_real hurst);

/* HAR-RV: RV_t = c + bd*RV_{t-1} + bw*mean(RV_{t-5..t-1}) + bm*mean(RV_{t-22..t-1}). */
typedef struct {
    gc_real c, bd, bw, bm;   /* coefficienti */
    gc_real r2;              /* R^2 in-sample */
    int n;                   /* osservazioni usate */
} gs_har;

/* Fit OLS su rv[0..n-1]. Ritorna GC_OK / GC_ERR_ARG. */
gc_status gs_har_fit(const gc_real *rv, int n, gs_har *out);

/* Previsione 1-step di RV all'ultimo punto disponibile. */
gc_real gs_har_forecast(const gs_har *m, const gc_real *rv, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_RESEARCH_H */
