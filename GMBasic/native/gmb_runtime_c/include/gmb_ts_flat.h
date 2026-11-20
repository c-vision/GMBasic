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

/* Errore dell'ultima chiamata (stringa nel runtime; valida finché non viene sovrascritta). */
const char*   gmb_tsf_last_error(void);
void          gmb_tsf_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TS_FLAT_H */
