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

const char* gmb_tsf_last_error(void) { return gmb_ts_last_error(); }
void gmb_tsf_clear_error(void) { gmb_ts_clear_error(); }
