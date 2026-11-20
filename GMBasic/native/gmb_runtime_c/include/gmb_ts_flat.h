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

/* Errore dell'ultima chiamata (stringa nel runtime; valida finché non viene sovrascritta). */
const char*   gmb_tsf_last_error(void);
void          gmb_tsf_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TS_FLAT_H */
