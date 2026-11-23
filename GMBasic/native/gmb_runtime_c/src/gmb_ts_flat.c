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

gmb_double_t* gmb_tsf_outliers_iqr(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len) {
    return gmb_ts_outliers_iqr(data, n, threshold, out_len);
}

gmb_double_t* gmb_tsf_rolldelta_order(gmb_double_t* series, gmb_int_t n, gmb_int_t window, gmb_int_t order, gmb_int_t* out_len) {
    return gmb_ts_rolldelta_order(series, n, window, order, out_len);
}

gmb_double_t* gmb_tsf_decompose_matrix(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    return gmb_ts_decompose_matrix(data, n, period, out_len);
}

gmb_double_t* gmb_tsf_rolling_sortino(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, double target, gmb_int_t* out_len) {
    return gmb_ts_rolling_sortino(data, n, window, rf, target, out_len);
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

/* ── Onda 4: estrazione dai dict (`gmb_val_t`) ─────────────────────────────────────────────── */

/* Copia una lista del dict in un buffer C (capacità `cap`); ritorna gli elementi copiati. */
static int stf_list_copy(gmb_dict_t d, const char* key, double* out, int cap) {
    gmb_list_t l = gmb_val_as_list(gmb_dict_get(d, key));
    int n = (int)gmb_list_count(l);
    if (n > cap) n = cap;
    for (int i = 0; i < n; i++) out[i] = gmb_val_to_double(gmb_list_get(l, i));
    return n;
}

#define STF_GET(d, key, ptr) do { if (ptr) *(ptr) = gmb_dict_get_or_nan((d), (key)); } while (0)
#define STF_GETI(d, key, ptr) do { if (ptr) *(ptr) = (int)gmb_dict_get_or_nan((d), (key)); } while (0)

int gmb_tsf_garch11(const double* r, int n, double* omega, double* alpha, double* beta) {
    gmb_val_t v = gmb_ts_garch11_flat(r, n);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "Omega", omega); STF_GET(d, "Alpha", alpha); STF_GET(d, "Beta", beta);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_qlr(const double* l_beta, int n_beta, double l0, double kappa_hat,
                double* qlr_t, double* qlr_tilde, int* best_index) {
    gmb_val_t v = gmb_ts_qlr_flat(l_beta, n_beta, l0, kappa_hat);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "QLRT", qlr_t); STF_GET(d, "QLRTilde", qlr_tilde);
    STF_GETI(d, "BestIndex", best_index);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_ou_fit(const double* x, int n, double dt,
                   double* theta, double* mu, double* sigma, double* phi, double* half_life) {
    gmb_val_t v = gmb_ts_ou_fit_flat(x, n, dt);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "Theta", theta); STF_GET(d, "Mu", mu); STF_GET(d, "Sigma", sigma);
    STF_GET(d, "Phi", phi); STF_GET(d, "HalfLife", half_life);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_granger(const double* x, int nx, const double* y, int ny, int p,
                    double* stat, double* pvalue, double* df) {
    gmb_val_t v = gmb_ts_granger_flat(x, nx, y, ny, p);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "Statistic", stat); STF_GET(d, "PValue", pvalue); STF_GET(d, "DegreesOfFreedom", df);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_kpss(const double* series, int n, int bandwidth, int deterministic,
                 double* stat, int* bw, int* observations, int* det, double* lrv,
                 double* c10, double* c5, double* c25, double* c1) {
    gmb_val_t v = gmb_ts_kpss_flat(series, n, bandwidth, deterministic);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "Statistic", stat); STF_GETI(d, "Bandwidth", bw);
    STF_GETI(d, "Observations", observations); STF_GETI(d, "Deterministic", det);
    STF_GET(d, "LongRunVariance", lrv);
    STF_GET(d, "Critical10", c10); STF_GET(d, "Critical5", c5);
    STF_GET(d, "Critical25", c25); STF_GET(d, "Critical1", c1);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_dfgls(const double* series, int n, int lags, int deterministic,
                  double* stat, int* lags_out, int* observations, int* det, double* alpha_bar) {
    gmb_val_t v = gmb_ts_dfgls_flat(series, n, lags, deterministic);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "Statistic", stat); STF_GETI(d, "Lags", lags_out);
    STF_GETI(d, "Observations", observations); STF_GETI(d, "Deterministic", det);
    STF_GET(d, "AlphaBar", alpha_bar);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_engle_granger(const double* y_flat, int n, int k, int dependent, int lags,
                          int with_constant, double* stat, int* lags_out, int* observations,
                          int* n_regressors, int* wc) {
    gmb_val_t v = gmb_ts_engle_granger_flat(y_flat, n, k, dependent, lags, with_constant);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    STF_GET(d, "Statistic", stat); STF_GETI(d, "Lags", lags_out);
    STF_GETI(d, "Observations", observations); STF_GETI(d, "NRegressors", n_regressors);
    STF_GETI(d, "WithConstant", wc);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_johansen(const double* y_flat, int n_obs, int k, int lags,
                     double* eigen_values, double* trace_statistic, double* max_statistic,
                     int* observations) {
    gmb_val_t v = gmb_ts_johansen_flat(y_flat, n_obs, k, lags);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    if (ok && eigen_values)  stf_list_copy(d, "EigenValues", eigen_values, k);
    if (ok && trace_statistic) stf_list_copy(d, "TraceStatistic", trace_statistic, k);
    if (ok && max_statistic) stf_list_copy(d, "MaxStatistic", max_statistic, k);
    STF_GETI(d, "Observations", observations);
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_tsf_hmm_fit(const double* x_flat, int n, int d, int k, int n_iter,
                    double* pi, double* a, double* means, double* covs, double* log_likelihood) {
    gmb_val_t v = gmb_ts_hmm_fit(x_flat, n, d, k, n_iter);
    gmb_dict_t dict = gmb_val_as_dict(v);
    int ok = gmb_dict_count(dict) > 0;
    if (ok && pi)    stf_list_copy(dict, "Pi", pi, k);
    if (ok && a)     stf_list_copy(dict, "A", a, k * k);
    if (ok && means) stf_list_copy(dict, "Means", means, k * d);
    if (ok && covs)  stf_list_copy(dict, "Covs", covs, k * d * d);
    STF_GET(dict, "LogLikelihood", log_likelihood);
    gmb_val_free_temp_dict(v);
    return ok;
}

const char* gmb_tsf_last_error(void) { return gmb_ts_last_error(); }
void gmb_tsf_clear_error(void) { gmb_ts_clear_error(); }

void gmb_tsf_minmax(const gmb_double_t* data, gmb_int_t n, double* out_min, double* out_max) {
    if (out_min) *out_min = gmb_min_arr(data, n);
    if (out_max) *out_max = gmb_max_arr(data, n);
}
