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

#ifdef __cplusplus
}
#endif

#endif /* GMB_ANALYSIS_FLAT_H */
