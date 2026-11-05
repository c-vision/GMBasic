/*
 * gs_num.h — metodi numerici/statistici da AI/Math:
 *  - Monte Carlo variance reduction (antitetiche, control variate, importance,
 *    Halton);
 *  - bootstrap a blocchi, jackknife;
 *  - Extreme Value / Peaks-Over-Threshold (Hill, GPD, VaR/ES);
 *  - tail dependence empirica;
 *  - EM per misture gaussiane 1D;
 *  - Gaussian Process regression;
 *  - particle filter (bootstrap).
 */
#ifndef GMBSTATS_GS_NUM_H
#define GMBSTATS_GS_NUM_H

#include "gc_core.h"
#include "gs_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Monte Carlo variance reduction ---- */
/* Media antithetica: mean_i (h(u_i)+h(1-u_i))/2. Le due serie sono gia' valutate. */
double gs_mc_antithetic_mean(const gc_real *h_u, const gc_real *h_1mu, int n);
/* Control variate: mean(y) - beta*(mean(x)-mu_x), beta = cov(y,x)/var(x). */
double gs_mc_control_variate(const gc_real *y, const gc_real *x, int n,
                             gc_real mu_x, gc_real *out_beta);
/* Importance sampling: media di h(x)/p(x) forniti per campione. */
double gs_mc_importance_mean(const gc_real *h_over_p, int n);
/* Sequenza di Halton: radical inverse dell'indice in base b. */
double gs_halton(int index, int base);
/* n punti Halton in dim dimensioni (primi `dim` numeri primi come basi):
 * out[i*dim + d]. Ritorna 0/-1. */
int gs_halton_seq(int n, int dim, gc_real *out);

/* ---- Resampling ---- */
/* Bootstrap a blocchi mobili sulla media. out_mean e out_se (opz.). */
int gs_bootstrap_block(const gc_real *x, int n, int block, int nrep,
                       gs_rng *rng, gc_real *out_mean, gc_real *out_se);
/* Jackknife leave-one-out sulla media. */
int gs_jackknife_mean(const gc_real *x, int n, gc_real *out_mean, gc_real *out_se);

/* ---- Extreme Value / POT ---- */
/* Hill estimator dell'indice di coda xi = 1/alpha su un campione (copia interna). */
int gs_evt_hill(const gc_real *x, int n, int k, gc_real *out_xi);
/* GPD (Peaks-Over-Threshold) via method of moments su exceedances > 0. */
int gs_evt_gpd_fit(const gc_real *exceed, int n, gc_real *out_xi, gc_real *out_beta);
/* VaR ed ES condizionali (POT) per probabilita' p: n = totale osservazioni,
 * nexc = numero di exceedances oltre soglia `thr`. */
int gs_evt_pot_var_es(gc_real thr, gc_real xi, gc_real beta, int n, int nexc,
                      gc_real p, gc_real *out_var, gc_real *out_es);
/* Tail dependence empirica (upper/lower) da k osservazioni estreme. */
int gs_tail_dependence(const gc_real *x, const gc_real *y, int n, int k,
                       gc_real *out_upper, gc_real *out_lower);

/* ---- EM (Gaussian mixture 1D) ---- */
/* Stima k componenti gaussiane via EM. out_mu[k], out_var[k], out_w[k]. */
int gs_gmm_em_1d(const gc_real *x, int n, int k, int iters,
                 gc_real *out_mu, gc_real *out_var, gc_real *out_w);

/* ---- Gaussian Process regression ---- */
/* RBF k(x,x')=exp(-0.5 (x-x')^2 / l2). Dati (x,y) n, predici in xs (m punti).
 * out_mean[m], out_var[m]. noise = varianza rumore. */
int gs_gp_regression(const gc_real *x, const gc_real *y, int n,
                     const gc_real *xs, int m, gc_real l2, gc_real noise,
                     gc_real *out_mean, gc_real *out_var);

/* ---- Particle filter (bootstrap) ---- */
/* x_t = a*x_{t-1} + N(0,q); y_t = x_t + N(0,r). T osservazioni, N particelle.
 * out_mean[T], out_var[T] stime filtrate. */
int gs_particle_filter(const gc_real *obs, int T, gc_real a, gc_real q,
                       gc_real r, int N, gs_rng *rng,
                       gc_real *out_mean, gc_real *out_var);

#ifdef __cplusplus
}
#endif
#endif

/* --- Filtri Butterworth (da Sources/R mFilter) --- */
/* Low-pass Butterworth di ordine 1 o 2, cutoff in periodo (campioni).
 * out[0..n-1]. Ritorna 0/-1. */
int gs_butterworth_lp(const double *x, int n, double period, int order, double *out);
/* Band-pass = LP(period=low) - LP(period=high) (low<high in periodo). */
int gs_butterworth_bp(const double *x, int n, double low_period, double high_period,
                      int order, double *out);
