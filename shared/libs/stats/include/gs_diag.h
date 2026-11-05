/*
 * gs_diag.h — diagnostiche di convergenza MCMC.
 *
 * Catena di Markov campionata, memorizzata per righe: X[i*n + j] è la
 * dimensione i (0..d-1) al campione j (0..n-1) — stessa convenzione di volesti
 * (`diagnostics/`). Funzioni pure C99 (solo libc/libm).
 *
 *   - ESS: effective sample size (Geyer, initial positive sequence);
 *   - PSRF univariato: Gelman-Rubin split-half per dimensione (R-hat);
 *   - PSRF multivariato: Brooks-Gelman (max autovalore di W^-1 B);
 *   - Geweke: test di Hotelling a due campioni (prima vs ultima porzione);
 *   - thinning: sottocampionamento a passo fisso.
 */
#ifndef GMBSTATS_GS_DIAG_H
#define GMBSTATS_GS_DIAG_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ESS per dimensione (X d×n, ess[d]). Ritorna 0, oppure -1 su input non valido. */
int gs_diag_ess(const gc_real *X, int d, int n, gc_real *ess);

/* PSRF (R-hat) univariato per dimensione, split-half. rhat[d]. 0 / -1. */
int gs_diag_psrf_univariate(const gc_real *X, int d, int n, gc_real *rhat);

/* PSRF multivariato (Brooks-Gelman). Ritorna R, oppure -1 su errore. */
gc_real gs_diag_psrf_multivariate(const gc_real *X, int d, int n);

/* Geweke (Hotelling T^2 + F): 1 = H0 non rifiutata, 0 = rifiutata, -1 errore.
 * frac_first/frac_last in (0,1]; alpha = livello (es. 0.05). */
int gs_diag_geweke(const gc_real *X, int d, int n,
                   gc_real frac_first, gc_real frac_last, gc_real alpha);

/* Thinning: out[k] = x[k*thin], k = 0.. . Ritorna il numero di campioni
 * scritti, oppure -1 su input non valido. */
int gs_diag_thin(const gc_real *x, int n, int thin, gc_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_DIAG_H */
