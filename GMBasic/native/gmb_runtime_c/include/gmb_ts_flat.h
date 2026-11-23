/* gmb_ts_flat.h — PONTE delle funzioni TIMESERIES per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. Il runtime C è linkato STATICAMENTE nei host nativi: le gambe IL/PCodeVM non possono
 * chiamarne le funzioni. `libgmb_matrix_flat` risolve lo stesso problema per i kernel
 * matriciali; questa lib fa lo stesso per TIMESERIES, così la funzione ha UNA sola
 * implementazione (quella del runtime condiviso) e le 4 gambe danno gli stessi valori.
 *
 * CONTRATTO. Le funzioni che ritornano `gmb_double_t*` allocano il buffer nel runtime (calloc):
 * il chiamante COPIA i valori e poi chiama `free()`. Su errore ritornano NULL con
 * `*out_len = 0` e il messaggio in `gmb_ts_last_error()` (azzerare prima con
 * `gmb_ts_clear_error()`). */
#ifndef GMB_TS_FLAT_H
#define GMB_TS_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

gmb_double_t* gmb_tsf_moving_average(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_ema(gmb_double_t* data, gmb_int_t n, double alpha, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_returns(gmb_double_t* data, gmb_int_t n, int log_returns, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_difference(gmb_double_t* data, gmb_int_t n, gmb_int_t order, gmb_int_t* out_len);
double        gmb_tsf_volatility(gmb_double_t* data, gmb_int_t n, int log_returns);
double        gmb_tsf_autocorrelation(gmb_double_t* data, gmb_int_t n, gmb_int_t lag);

/* ── Onda 2: firme IDENTICHE ai kernel di gmb_timeseries.c (int, non gmb_int_t) ──────────────
 * Qui non serve la larghezza di gmb_int_t: gli argomenti sono lunghezze/ordini/indici che il
 * kernel stesso riceve come `int`. `*out_len` vale 0 quando il kernel rifiuta l'input. */
double*       gmb_tsf_fracdiff(const double* x, int n, double d, int* out_len);
double*       gmb_tsf_kalman1d(const double* z, int n, double a, double h, double q, double r, int* out_len);
double*       gmb_tsf_stationary_bootstrap(const double* x, int n, int window, int seed, int* out_len);
double        gmb_tsf_sample_entropy(const double* x, int n, int m, double r);
double        gmb_tsf_approx_entropy(const double* x, int n, int m, double r);
double        gmb_tsf_perm_entropy(const double* x, int n, int m, int lag);
double        gmb_tsf_lz_complexity(const double* x, int n);
double        gmb_tsf_lyapunov(const double* x, int n, int m, int tau);
double*       gmb_tsf_bma_weights(const double* ic, int n, int* out_len);
double        gmb_tsf_hmm_forward(const double* lpi, const double* lA, const double* lE, int K, int n);
double*       gmb_tsf_hmm_viterbi(const double* lpi, const double* lA, const double* lE, int K, int n, int* outn);

/* Covarianza di n_obs osservazioni × n_vars variabili (Bessel, n_obs-1): il kernel ritorna un
 * jagged `row[i][j]`. Il chiamante libera OGNI riga e poi il vettore dei puntatori con free(). */
gmb_double_t** gmb_tsf_covariance_obs(const double* const* obs, int n_obs, int n_vars);

/* ── Onda 3: destagionalizzazione, outlier, Hurst e famiglia ROLLING* ────────────────────────
 * Stesse firme dei kernel; `*out_len` = 0 quando il kernel rifiuta l'input. */
gmb_double_t* gmb_tsf_deseasonalize(gmb_double_t* data, gmb_int_t n, gmb_int_t season, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_outliers(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len);
double        gmb_tsf_hurst_dfa(const double* x, int n, int min_box);
gmb_double_t* gmb_tsf_outliers_iqr(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolldelta_order(gmb_double_t* series, gmb_int_t n, gmb_int_t window, gmb_int_t order, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_decompose_matrix(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_sortino(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, double target, gmb_int_t* out_len);
void          gmb_tsf_minmax(const gmb_double_t* data, gmb_int_t n, double* out_min, double* out_max);

gmb_double_t* gmb_tsf_rolling_mean(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_stddev(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_skewness(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_kurtosis(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_median(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_quantile(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double q, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_sharpe(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_mad(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_zscore(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_autocorr(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t lag, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_cov(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_corr(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_minmax(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_regression(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolling_regression_single(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_tsf_rolldelta(gmb_double_t* series, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len);

/* ── Onda 4: kernel che ritornano `gmb_val_t` (dict) ─────────────────────────────────────────
 * Il runtime costruisce un dict con chiavi fisse; qui si fa solo l'estrazione (marshalling).
 * Ritorno 1 = dict valido, 0 = il kernel ha segnalato input non valido; le chiavi assenti
 * diventano NaN (o lista vuota). Capacità degli array: `k`/`K` come documentato a fianco. */
int gmb_tsf_garch11(const double* r, int n, double* omega, double* alpha, double* beta);
int gmb_tsf_qlr(const double* l_beta, int n_beta, double l0, double kappa_hat,
                double* qlr_t, double* qlr_tilde, int* best_index);
int gmb_tsf_ou_fit(const double* x, int n, double dt,
                   double* theta, double* mu, double* sigma, double* phi, double* half_life);
int gmb_tsf_granger(const double* x, int nx, const double* y, int ny, int p,
                    double* stat, double* pvalue, double* df);
int gmb_tsf_kpss(const double* series, int n, int bandwidth, int deterministic,
                 double* stat, int* bw, int* observations, int* det, double* lrv,
                 double* c10, double* c5, double* c25, double* c1);
int gmb_tsf_dfgls(const double* series, int n, int lags, int deterministic,
                  double* stat, int* lags_out, int* observations, int* det, double* alpha_bar);
int gmb_tsf_engle_granger(const double* y_flat, int n, int k, int dependent, int lags,
                          int with_constant, double* stat, int* lags_out, int* observations,
                          int* n_regressors, int* wc);
/* `eigen_values`/`trace_statistic`/`max_statistic`: k elementi. */
int gmb_tsf_johansen(const double* y_flat, int n_obs, int k, int lags,
                     double* eigen_values, double* trace_statistic, double* max_statistic,
                     int* observations);
/* `pi`: k; `a`: k*k; `means`: k*d; `covs`: k*d*d. */
int gmb_tsf_hmm_fit(const double* x_flat, int n, int d, int k, int n_iter,
                    double* pi, double* a, double* means, double* covs, double* log_likelihood);

/* Errore dell'ultima chiamata (stringa nel runtime; valida finché non viene sovrascritta). */
const char*   gmb_tsf_last_error(void);
void          gmb_tsf_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TS_FLAT_H */
