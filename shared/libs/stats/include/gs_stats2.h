/*
 * gs_stats2.h — statistica avanzata (riscrittura).
 *
 * GARCH(1,1), filtro di Kalman, MCMC Metropolis, bootstrap, volatilità, OU,
 * disuguaglianza/distanze. Regressione e metriche: gs_regression.h.
 */
#ifndef GMBSTATS_GS_STATS2_H
#define GMBSTATS_GS_STATS2_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DCC-GARCH step (Engle): Q_t=(1-a-b)Qbar + a z z' + b Q_{t-1}; R=D^-1/2 Q D^-1/2.
 * zprev[n], Qprev[n*n], Qbar[n*n]; Qout/Rout n*n. */
int gs_dcc_step(const double *zprev, const double *Qprev, const double *Qbar, int n,
                double a, double b, double *Qout, double *Rout);
gc_status gs_garch11(const gc_real *r, int n, gc_real *omega, gc_real *alpha, gc_real *beta);
/* Estimatori di volatilita OHLC (da QuantLib, BSD-3): metodo 0=Parkinson,
 * 1=Garman-Klass, 2=Rogers-Satchell, 3=Yang-Zhang. o/h/l/c array di bar;
 * year_frac = frazione d'anno per barra (es. 1/252). Vol annualizzata. */
typedef enum { GS_VOL_PARKINSON = 0, GS_VOL_GARMANKLASS, GS_VOL_ROGERS_SATCHELL, GS_VOL_YANG_ZHANG, GS_VOL_GKYZ } gs_vol_ohlc_method;
gc_status gs_vol_ohlc(const gc_real *o, const gc_real *h, const gc_real *l, const gc_real *c,
                      int n, gs_vol_ohlc_method method, gc_real year_frac, gc_real *vol);
/* Hodges-Tompkins: std close-to-close * sqrt(ppy) * fattore di correzione
 * 1/(1 - h/n + (h^2-1)/(3n^2)), n = count - h + 1. */
gc_status gs_vol_hodges_tompkins(const gc_real *c, int n, int window,
                                 gc_real year_frac, gc_real *vol);
/* Filtro di Kalman scalare: x_k = a x_{k-1} + w, z_k = h x_k + v. */
gc_status gs_kalman1d(const gc_real *z, int n, gc_real a, gc_real h, gc_real q, gc_real r,
                      gc_real x0, gc_real p0, gc_real *xout);
/* MCMC Metropolis: logp è la log-densità (a meno di costante). */
gc_status gs_mcmc_metropolis(gc_real (*logp)(const gc_real *, int, void *), void *ctx,
                             gc_real *x, int d, int iters, gc_real step,
                             unsigned long seed, gc_real *samples);
/* Bootstrap della media; restituisce la media campionaria e imposta *se. */
gc_real gs_bootstrap_mean(const gc_real *x, int n, int B, unsigned long seed, gc_real *se);

/* Permutation test a due campioni (differenza di medie, two-sided): p-value
 * empirico via label shuffling (B permutazioni). x/y campioni, nx/ny. */
gc_real gs_permutation_test(const gc_real *x, int nx, const gc_real *y, int ny,
                            int B, unsigned long seed);

/* Stima dei parametri Ornstein-Uhlenbeck (AR(1) esatto, da QFLib): phi =
 * sxy/sxx, mu = alpha/(1-phi), theta = -ln(phi)/dt, half_life = ln2/theta,
 * sigma da SSE con varianza di transizione esatta. dt = passo (anni). */
int gs_ou_fit(const gc_real *x, int n, gc_real dt,
              gc_real *theta, gc_real *mu, gc_real *sigma,
              gc_real *phi, gc_real *half_life);

/* Disuguaglianza / distanze (trading/feeds, wbgapi/chebyshev). */
/* Indice di Gini (0=uguaglianza, 1=max disuguaglianza). x non negativi. */
gc_real gs_gini(const gc_real *x, int n);
/* Indice di Theil T = (1/n) sum (x_i/mean) ln(x_i/mean). x>0. */
gc_real gs_theil(const gc_real *x, int n);
/* Distanza di Chebyshev D = max_i |x_i - y_i|. */
gc_real gs_chebyshev_distance(const gc_real *x, const gc_real *y, int n);


#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_STATS2_H */
