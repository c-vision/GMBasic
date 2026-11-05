/*
 * gs_stats.h — statistica descrittiva (SIMD).
 *
 * Riduzioni vettorizzate su NEON; momenti e correlazioni in una/due passate.
 */
#ifndef GMBSTATS_GS_STATS_H
#define GMBSTATS_GS_STATS_H

#include "gc_core.h"

/* Hurst via Detrended Fluctuation Analysis (Peng 1994): cumsum detrendizzato,
 * box di dimensione crescente, trend lineare per box, pendenza log-log. */
double gs_hurst_dfa(const double *x, int n, int min_box);

/* Fractional differentiation (Lopez de Prado): convoluzione con pesi binomiali
 * w_k = -w_{k-1}(d-k+1)/k (convoluzione causale). Ritorna 0/-1. */
int gs_frac_diff(const double *x, int n, double d, double *out);

/* Stationary bootstrap (Politis-Romano): blocco geometrico con p=1/window. */
int gs_stationary_bootstrap(const double *x, int n, int window, unsigned seed,
                            double *out);

/* Merton market-timing test (quantutils): n1 up-days nel mercato, N giorni,
 * N1 giorni mercato up. Statistica z e p-value one-sided. */
double gs_merton_timing(int n1, int N1, int n, int N);

/* Entropie di time series, Lyapunov e Granger (gap del corpus).
 * Sample entropy: -ln(A^m/B^m) con embedding m e tolleranza r.
 * Approximate entropy: variante con self-match e normalizzazione N-m+1.
 * Permutation entropy: pattern ordinali (m, lag), normalizzata per log(m!).
 * Multiscale: coarse-graining (media su finestre di scala s) poi SampEn. */
double gs_entropy_sample(const double *x, int n, int m, double r);
double gs_entropy_approx(const double *x, int n, int m, double r);
double gs_entropy_perm(const double *x, int n, int m, int lag);
double gs_entropy_multiscale(const double *x, int n, int scale, int m, double r);
/* Lempel-Ziv complexity (su serie binarizzata a soglia mediana). */
double gs_entropy_lz(const double *x, int n);
/* Largest Lyapunov exponent (Rosenstein): embedding m, lag tau, slope log-divergenza. */
double gs_lyapunov(const double *x, int n, int m, int tau);
/* Granger causality: F-test AR ristretto vs unrestricted su x (y causa x?). */
int gs_granger(const double *x, const double *y, int n, int p, double *f_stat,
               double *p_value);
/* Bayesian Model Averaging (van Dijk): pesi posteriori da IC (BIC/AIC), w_i ∝ exp(-0.5*(IC_i-min)). */
int gs_bma_weights(const double *ic, int m, double *w);
/* Forecast combination (Bates-Granger): mode 0=media, 1=pesi inversi MSE. */
int gs_fc_weights(const double *mse, int m, int mode, double *w);
/* Combina previsioni fc[m] con pesi w[m]. */
double gs_fc_combine(const double *fc, const double *w, int m);

#ifdef __cplusplus
extern "C" {
#endif

gc_real gs_stats_mean(const gc_real *a, int n);
/* sample != 0 → varianza campionaria (/ (n-1)), altrimenti di popolazione (/ n). */
gc_real gs_stats_variance(const gc_real *a, int n, int sample);
gc_real gs_stats_stddev(const gc_real *a, int n, int sample);
gc_real gs_stats_skewness(const gc_real *a, int n);
/* Curtosi in eccesso (0 per la normale). */
gc_real gs_stats_kurtosis(const gc_real *a, int n);
gc_real gs_stats_covariance(const gc_real *a, const gc_real *b, int n, int sample);
gc_real gs_stats_correlation(const gc_real *a, const gc_real *b, int n);
gc_real gs_stats_min(const gc_real *a, int n);
gc_real gs_stats_max(const gc_real *a, int n);
gc_real gs_stats_range(const gc_real *a, int n);
gc_real gs_stats_median(const gc_real *a, int n);
/* Percentile con interpolazione lineare, p in [0,100]. */
gc_real gs_stats_percentile(const gc_real *a, int n, gc_real p);
/* Deviazione assoluta mediana. */
gc_real gs_stats_mad(const gc_real *a, int n);

/* Hurst via Detrended Fluctuation Analysis (Peng 1994): cumsum detrendizzato,
 * box di dimensione crescente, trend lineare per box, pendenza log-log. */
double gs_hurst_dfa(const double *x, int n, int min_box);

/* Fractional differentiation (Lopez de Prado): convoluzione con pesi binomiali
 * w_k = -w_{k-1}(d-k+1)/k (convoluzione causale). Ritorna 0/-1. */
int gs_frac_diff(const double *x, int n, double d, double *out);

/* Stationary bootstrap (Politis-Romano): blocco geometrico con p=1/window. */
int gs_stationary_bootstrap(const double *x, int n, int window, unsigned seed,
                            double *out);

/* Merton market-timing test (quantutils): n1 up-days nel mercato, N giorni,
 * N1 giorni mercato up. Statistica z e p-value one-sided. */
double gs_merton_timing(int n1, int N1, int n, int N);

/* Entropie di time series, Lyapunov e Granger (gap del corpus).
 * Sample entropy: -ln(A^m/B^m) con embedding m e tolleranza r.
 * Approximate entropy: variante con self-match e normalizzazione N-m+1.
 * Permutation entropy: pattern ordinali (m, lag), normalizzata per log(m!).
 * Multiscale: coarse-graining (media su finestre di scala s) poi SampEn. */
double gs_entropy_sample(const double *x, int n, int m, double r);
double gs_entropy_approx(const double *x, int n, int m, double r);
double gs_entropy_perm(const double *x, int n, int m, int lag);
double gs_entropy_multiscale(const double *x, int n, int scale, int m, double r);
/* Lempel-Ziv complexity (su serie binarizzata a soglia mediana). */
double gs_entropy_lz(const double *x, int n);
/* Largest Lyapunov exponent (Rosenstein): embedding m, lag tau, slope log-divergenza. */
double gs_lyapunov(const double *x, int n, int m, int tau);
/* Granger causality: F-test AR ristretto vs unrestricted su x (y causa x?). */
int gs_granger(const double *x, const double *y, int n, int p, double *f_stat,
               double *p_value);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_STATS_H */
