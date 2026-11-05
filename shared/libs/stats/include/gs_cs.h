/*
 * gs_cs.h — operatori cross-sectional (panel): z-score e neutralizzazione rispetto
 * a un fattore. Statistica pura (nessun tipo/semantica di dominio).
 * Fonte: S51 (feature engineering quant) — riusabili per il ranking convesso (C11).
 */
#ifndef GMBSTATS_GS_CS_H
#define GMBSTATS_GS_CS_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cross-sectional z-score: (x_i - mean)/sd, sd campionario (ddof = 1).
 * Se sd == 0 scrive 0 su tutte le uscite. Richiede n >= 2. */
gc_status gs_cs_zscore(const gc_real *x, int n, gc_real *out);

/* Neutralizzazione rispetto al fattore x: residuo di y su x (OLS con intercetta),
 * out_i = (y_i - mean_y) - beta*(x_i - mean_x), beta = cov(x,y)/var(x).
 * Se var(x) == 0 scrive y_i - mean_y. Richiede n >= 2. */
gc_status gs_cs_neutralize(const gc_real *y, const gc_real *x, int n, gc_real *out);

/* Cross-sectional rank percentile in [0,1]: (r-1)/(n-1) con r = rango 1-based
 * (ties = rango medio). Richiede n >= 2. Fonte S51 (cs_rank). */
gc_status gs_cs_rank(const gc_real *x, int n, gc_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_CS_H */
