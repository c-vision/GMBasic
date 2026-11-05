/*
 * gs_regression.h — regressione, distanze multivariate e metriche di errore.
 * Progetto gmbstats-regression (indipendente da gs_stats2.h).
 */
#ifndef GMBSTATS_GS_REGRESSION_H
#define GMBSTATS_GS_REGRESSION_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

gc_real gs_mahalanobis(const gc_real *x, const gc_real *mu, const gc_real *Sinv, int n);
gc_real gs_hotelling_t2(const gc_real *xbar, const gc_real *mu, const gc_real *Sinv, int p);
/* OLS multipla: X n×p row-major, y n. */
gc_status gs_regression_multi(const gc_real *X, const gc_real *y, int n, int p, gc_real *beta);
gc_status gs_ridge_regression(const gc_real *X, const gc_real *y, int n, int p,
                              gc_real lambda, gc_real *beta);
gc_status gs_logistic_regression(const gc_real *X, const gc_real *y, int n, int p,
                                 gc_real *beta, int maxit, gc_real tol);

/* Metriche di errore/forecast: MAPE, SMAPE, RMSE, RAE, Theil U1/U2. */
double gs_mape(const double *y, const double *yh, int n);
double gs_mae(const double *y, const double *yh, int n);
double gs_directional_accuracy(const double *y, const double *yh, int n);
double gs_smape(const double *y, const double *yh, int n);
double gs_rmse(const double *y, const double *yh, int n);
double gs_rae(const double *y, const double *yh, int n);
double gs_theil_u2(const double *y, const double *yh, int n);
double gs_theil_u1(const double *y, const double *yh, int n);

/* Welford online moments (mean/var one-pass streaming). */
typedef struct { gc_real count, mean, m2; } gs_online;
void gs_online_update(gs_online *o, gc_real x);
gc_real gs_online_mean(const gs_online *o);
gc_real gs_online_var(const gs_online *o);      /* varianza campionaria m2/(n-1) */
gc_real gs_online_std(const gs_online *o);
void gs_online_merge(gs_online *a, const gs_online *b);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_REGRESSION_H */
