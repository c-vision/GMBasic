/*
 * gs_tailrisk.h — grassezza delle code di una serie di rendimenti (tail richness).
 *  - frequency ratio di mosse oltre k deviazioni standard vs frequenza normale;
 *  - rapporto > 1 = code piu' grasse della normale (leptocurtosi).
 * Statistica pura (nessun tipo/semantica di dominio). Fonte: S23 (Convexity Score/C9).
 */
#ifndef GMBSTATS_GS_TAILRISK_H
#define GMBSTATS_GS_TAILRISK_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frequenza attesa (two-sided) sotto Normale(0,1) di |z| > k: 2*(1-Phi(k)). */
gc_real gs_tail_expected_rate(gc_real k);

/* Tail Richness Ratio = freq_osservata(|x-mean| > k*sd) / freq_attesa_normale.
 * mean/sd campionari (sd con ddof = 1). Richiede n >= 2 e sd > 0, altrimenti
 * ritorna GC_ERR_ARG (out opzionali restano invariati).
 * - out_richness: rapporto (>= 0; 0 se la frequenza attesa e' nulla);
 * - out_obs:      frequenza osservata in [0,1];
 * - out_exp:      frequenza attesa sotto normale in (0,1].
 * Tutti gli out sono opzionali (NULL ammesso). */
gc_status gs_tail_richness(const gc_real *x, int n, gc_real k,
                           gc_real *out_richness, gc_real *out_obs, gc_real *out_exp);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_TAILRISK_H */
