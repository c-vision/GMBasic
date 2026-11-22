/* gmb_analysis_flat.c — PONTE SHARED di analisi/regressione del runtime condiviso.
 *
 * Nessuna formula qui: solo inoltro (una sola implementazione: gmb_math.c). */
#include "gmb_analysis_flat.h"

double gmb_anf_euclidean_distance_squared(const double* a, const double* b, int n) {
    return gmb_euclidean_distance_squared(a, b, n);
}

void gmb_anf_linear_regression(const double* x, const double* y, int n,
                               double* slope, double* intercept, double* r2) {
    gmb_analysis_linear_regression(x, y, n, slope, intercept, r2);
}

double* gmb_anf_yule_walker(const double* acf, int n, int p, gmb_int_t* out_len) {
    return gmb_yule_walker(acf, n, p, out_len);
}

double* gmb_anf_ma_coefficients(const double* residuals, int n, int q, gmb_int_t* out_len) {
    return gmb_ma_coefficients(residuals, n, q, out_len);
}

double gmb_anf_min_semivariance(const double* returns, int n, double threshold) {
    return gmb_tm_min_semivariance(returns, n, threshold);
}

/* ── Descrittive ──────────────────────────────────────────────────────────────────────────── */

double gmb_anf_mean(const double* a, int n) { return gmb_mean(a, n); }
double gmb_anf_median(const double* a, int n) { return gmb_median(a, n); }
double gmb_anf_variance(const double* a, int n) { return gmb_variance(a, n); }
double gmb_anf_stddev(const double* a, int n) { return gmb_stddev(a, n); }
double gmb_anf_skewness(const double* a, int n) { return gmb_skewness(a, n); }
double gmb_anf_kurtosis(const double* a, int n) { return gmb_kurtosis(a, n); }
double gmb_anf_correlation(const double* x, const double* y, int n) { return gmb_correlation(x, y, n); }
double gmb_anf_min(const double* a, int n) { return gmb_min_arr(a, n); }
double gmb_anf_max(const double* a, int n) { return gmb_max_arr(a, n); }
double gmb_anf_percentile(const double* a, int n, double percent) { return gmb_percentile(a, n, percent); }
double gmb_anf_mad(const double* a, int n) { return gmb_mad(a, n); }

/* ── RMT e SVM ────────────────────────────────────────────────────────────────────────────── */

double gmb_anf_rmt_lambda_plus(int n, int T, double sigma2) { return gmb_mv_rmt_lambda_plus(n, T, sigma2); }
double gmb_anf_rmt_lambda_minus(int n, int T, double sigma2) { return gmb_mv_rmt_lambda_minus(n, T, sigma2); }
double gmb_anf_rmt_coherence(const double* v, int n) { return gmb_mv_rmt_coherence(v, n); }
double gmb_anf_rmt_spectral_norm(const double* a, int n) { return gmb_mv_rmt_spectral_norm(a, n); }
double gmb_anf_rmt_davis_kahan(double e_norm, double gap) { return gmb_mv_rmt_davis_kahan(e_norm, gap); }
double gmb_anf_rmt_principal_angle(const double* u, const double* v, int n) { return gmb_mv_rmt_principal_angle(u, v, n); }
double gmb_anf_rmt_tightness(double observed, double bound) { return gmb_mv_rmt_tightness(observed, bound); }
void gmb_anf_svm_fit(const double* x, const double* y, int n, int d, int k, double c, int max_iter, double* w) {
    gmb_mv_svm_fit(x, y, n, d, k, c, max_iter, w);
}
int gmb_anf_svm_predict(const double* w, const double* x, int d, int k) { return gmb_mv_svm_predict(w, x, d, k); }
