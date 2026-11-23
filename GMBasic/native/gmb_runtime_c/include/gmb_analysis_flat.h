/* gmb_analysis_flat.h — PONTE di analisi/regressione per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. `CommonLib/Numerics/Advanced.cs` calcolava in C# (MathNet o formule inline) le stesse
 * grandezze che `gmb_math.c`/`gmb_stats.c` calcolano per VM/C e C99Native. Qui il managed chiama
 * i kernel del runtime: una sola fonte numerica.
 *
 * CONTRATTO. I buffer ritornati sono allocati dal runtime: il chiamante copia e libera con free();
 * `*out_len = 0` quando il kernel rifiuta l'input. */
#ifndef GMB_ANALYSIS_FLAT_H
#define GMB_ANALYSIS_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

double  gmb_anf_euclidean_distance_squared(const double* a, const double* b, int n);
void    gmb_anf_linear_regression(const double* x, const double* y, int n,
                                  double* slope, double* intercept, double* r2);
double* gmb_anf_yule_walker(const double* acf, int n, int p, gmb_int_t* out_len);
double* gmb_anf_ma_coefficients(const double* residuals, int n, int q, gmb_int_t* out_len);
double  gmb_anf_min_semivariance(const double* returns, int n, double threshold);

/* ── Fitting (gmb_fit_*) ──────────────────────────────────────────────────────────────────── */
double  gmb_anf_cheb_eval(double x, const double* coef, int m);
double  gmb_anf_pade_eval(double x, const double* a, int na, const double* b, int nb);
double  gmb_anf_poly_eval(double x, const double* coef, int m);
double  gmb_anf_poly_ls(const double* x, const double* y, int n, int m, double* coef);
void    gmb_anf_rsquared(const double* data, int n, double* r2, double* angle);
double  gmb_anf_rsquared_test(const double* data, int n);

/* ── signal/fft (gmb_welch, gmb_rfft, gmb_irfft) ──────────────────────────────────────────── */
double* gmb_anf_welch(const double* x, int n, int nperseg, double fs, int* out_rows);
double* gmb_anf_rfft(const double* x, int n, gmb_int_t* outn);
double* gmb_anf_irfft(const double* re, const double* im, int m, gmb_int_t* outn);

/* ── Descrittive (gmb_math.c) ─────────────────────────────────────────────────────────────── */
double  gmb_anf_mean(const double* a, int n);
double  gmb_anf_median(const double* a, int n);
double  gmb_anf_variance(const double* a, int n);
double  gmb_anf_stddev(const double* a, int n);
double  gmb_anf_skewness(const double* a, int n);
double  gmb_anf_kurtosis(const double* a, int n);
double  gmb_anf_correlation(const double* x, const double* y, int n);
double  gmb_anf_min(const double* a, int n);
double  gmb_anf_max(const double* a, int n);
/* `percent` è in PERCENTUALE 0..100 (come gmb_percentile), non in frazione. */
double  gmb_anf_percentile(const double* a, int n, double percent);
double  gmb_anf_mad(const double* a, int n);

/* ── RMT e SVM (MULTIVARIATE) ─────────────────────────────────────────────────────────────── */
double gmb_anf_rmt_lambda_plus(int n, int T, double sigma2);
double gmb_anf_rmt_lambda_minus(int n, int T, double sigma2);
double gmb_anf_rmt_coherence(const double* v, int n);
double gmb_anf_rmt_spectral_norm(const double* a, int n);
double gmb_anf_rmt_davis_kahan(double e_norm, double gap);
double gmb_anf_rmt_principal_angle(const double* u, const double* v, int n);
double gmb_anf_rmt_tightness(double observed, double bound);
void   gmb_anf_svm_fit(const double* x, const double* y, int n, int d, int k, double c, int max_iter, double* w);
int    gmb_anf_svm_predict(const double* w, const double* x, int d, int k);

#ifdef __cplusplus
}
#endif

#endif /* GMB_ANALYSIS_FLAT_H */
