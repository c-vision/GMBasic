/* gmb_ts_flat.c — PONTE SHARED delle funzioni TIMESERIES del runtime condiviso.
 *
 * Il runtime è linkato staticamente nei host nativi: questa lib espone ai backend MANAGED
 * (P/Invoke) le stesse funzioni che usano VM/C e C99Native. Nessuna logica qui: solo inoltro —
 * una sola implementazione (gmb_timeseries.c).
 *
 * Ownership: i buffer tornano dal runtime (calloc) → il chiamante copia e libera con free(). */
#include "gmb_ts_flat.h"

gmb_double_t* gmb_tsf_moving_average(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_moving_average(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_ema(gmb_double_t* data, gmb_int_t n, double alpha, gmb_int_t* out_len) {
    return gmb_ts_ema(data, n, alpha, out_len);
}

gmb_double_t* gmb_tsf_returns(gmb_double_t* data, gmb_int_t n, int log_returns, gmb_int_t* out_len) {
    return gmb_ts_returns(data, n, log_returns, out_len);
}

gmb_double_t* gmb_tsf_difference(gmb_double_t* data, gmb_int_t n, gmb_int_t order, gmb_int_t* out_len) {
    return gmb_ts_difference(data, n, order, out_len);
}

double gmb_tsf_volatility(gmb_double_t* data, gmb_int_t n, int log_returns) {
    return gmb_ts_volatility(data, n, log_returns);
}

double gmb_tsf_autocorrelation(gmb_double_t* data, gmb_int_t n, gmb_int_t lag) {
    return gmb_ts_autocorrelation(data, n, lag);
}

/* ── Onda 2 ─────────────────────────────────────────────────────────────────────────────────── */

double* gmb_tsf_fracdiff(const double* x, int n, double d, int* out_len) {
    return gmb_ts_fracdiff(x, n, d, out_len);
}

double* gmb_tsf_kalman1d(const double* z, int n, double a, double h, double q, double r, int* out_len) {
    return gmb_ts_kalman1d(z, n, a, h, q, r, out_len);
}

double* gmb_tsf_stationary_bootstrap(const double* x, int n, int window, int seed, int* out_len) {
    return gmb_ts_stationary_bootstrap(x, n, window, seed, out_len);
}

double gmb_tsf_sample_entropy(const double* x, int n, int m, double r) {
    return gmb_ts_sample_entropy(x, n, m, r);
}

double gmb_tsf_approx_entropy(const double* x, int n, int m, double r) {
    return gmb_ts_approx_entropy(x, n, m, r);
}

double gmb_tsf_perm_entropy(const double* x, int n, int m, int lag) {
    return gmb_ts_perm_entropy(x, n, m, lag);
}

double gmb_tsf_lz_complexity(const double* x, int n) {
    return gmb_ts_lz_complexity(x, n);
}

double gmb_tsf_lyapunov(const double* x, int n, int m, int tau) {
    return gmb_ts_lyapunov(x, n, m, tau);
}

double* gmb_tsf_bma_weights(const double* ic, int n, int* out_len) {
    return gmb_ts_bma_weights(ic, n, out_len);
}

double gmb_tsf_hmm_forward(const double* lpi, const double* lA, const double* lE, int K, int n) {
    return gmb_ts_hmm_forward(lpi, lA, lE, K, n);
}

double* gmb_tsf_hmm_viterbi(const double* lpi, const double* lA, const double* lE, int K, int n, int* outn) {
    return gmb_ts_hmm_viterbi(lpi, lA, lE, K, n, outn);
}

gmb_double_t** gmb_tsf_covariance_obs(const double* const* obs, int n_obs, int n_vars) {
    return gmb_ts_covariance_obs(obs, n_obs, n_vars);
}

/* ── Onda 3 ─────────────────────────────────────────────────────────────────────────────────── */

gmb_double_t* gmb_tsf_deseasonalize(gmb_double_t* data, gmb_int_t n, gmb_int_t season, gmb_int_t* out_len) {
    return gmb_ts_deseasonalize(data, n, season, out_len);
}

gmb_double_t* gmb_tsf_outliers(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len) {
    return gmb_ts_outliers(data, n, threshold, out_len);
}

double gmb_tsf_hurst_dfa(const double* x, int n, int min_box) {
    return gmb_hurst_dfa(x, n, min_box);
}

gmb_double_t* gmb_tsf_rolling_mean(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_mean(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_stddev(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_stddev(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_skewness(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_skewness(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_kurtosis(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_kurtosis(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_median(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_median(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_quantile(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double q, gmb_int_t* out_len) {
    return gmb_ts_rolling_quantile(data, n, window, q, out_len);
}

gmb_double_t* gmb_tsf_rolling_sharpe(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, gmb_int_t* out_len) {
    return gmb_ts_rolling_sharpe(data, n, window, rf, out_len);
}

gmb_double_t* gmb_tsf_rolling_mad(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_mad(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_zscore(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_zscore(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_autocorr(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t lag, gmb_int_t* out_len) {
    return gmb_ts_rolling_autocorr(data, n, window, lag, out_len);
}

gmb_double_t* gmb_tsf_rolling_cov(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_cov(x, y, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_corr(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_corr(x, y, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_minmax(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_minmax(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_regression(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_regression(x, y, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolling_regression_single(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    return gmb_ts_rolling_regression_single(data, n, window, out_len);
}

gmb_double_t* gmb_tsf_rolldelta(gmb_double_t* series, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    return gmb_ts_rolldelta(series, n, period, out_len);
}

const char* gmb_tsf_last_error(void) { return gmb_ts_last_error(); }
void gmb_tsf_clear_error(void) { gmb_ts_clear_error(); }
