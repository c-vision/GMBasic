/* gs_basic_abi.c — wrapper con la FIRMA BASIC dei nomi storici `STATS.*`.
 *
 * Perché esiste (2026-11-18): la migrazione a FFI richiede che la libreria sia la sola fonte.
 * La lib `gmbstats` espone le distribuzioni nell'ordine C (`gs_dist_chi2_cdf(df, x)`), mentre il
 * namespace BASIC storico `STATS.ChiSquaredCdf(x, df)` ha `x` per primo. Un `.inc` dichiara i
 * parametri NELL'ORDINE in cui vanno passati alla funzione C: quindi per conservare la firma
 * BASIC senza duplicare la matematica serve un wrapper di una riga per funzione.
 *
 * Regola: qui NON c'è logica, solo inversione/alias di parametri verso `gs_*`.
 */
#include "gs_dist.h"

/* ── chi-quadro ───────────────────────────────────────────────────────────── */
double gs_basic_chi2_pdf(double x, double df) { return gs_dist_chi2_pdf(df, x); }
double gs_basic_chi2_cdf(double x, double df) { return gs_dist_chi2_cdf(df, x); }
double gs_basic_chi2_sf(double x, double df) { return gs_dist_chi2_sf(df, x); }
double gs_basic_chi2_ppf(double p, double df) { return gs_dist_chi2_ppf(df, p); }
double gs_basic_chi2_isf(double p, double df) { return gs_dist_chi2_isf(df, p); }
