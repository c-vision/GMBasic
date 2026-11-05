/*
 * gs_estimation.h — MLE di distribuzioni e model selection.
 * Progetto gmbstats-fitting.
 */
#ifndef GMBSTATS_GS_ESTIMATION_H
#define GMBSTATS_GS_ESTIMATION_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- MLE + model selection ---- */
int gs_fit_normal(const double *x, int n, double *mu, double *sigma);
double gs_loglik_normal(const double *x, int n, double mu, double sigma);
int gs_fit_lognormal(const double *x, int n, double *mu, double *sigma);
int gs_fit_exponential(const double *x, int n, double *lambda);
double gs_loglik_exponential(const double *x, int n, double lambda);
int gs_fit_poisson(const double *x, int n, double *lambda);
double gs_loglik_poisson(const double *x, int n, double lambda);
int gs_fit_gamma(const double *x, int n, double *shape, double *scale);
double gs_loglik_gamma(const double *x, int n, double shape, double scale);
int gs_fit_weibull(const double *x, int n, double *shape, double *scale);
double gs_loglik_weibull(const double *x, int n, double shape, double scale);
/* AIC = 2k - 2 logL; BIC = k ln(n) - 2 logL. */
double gs_aic(double loglik, int k);
double gs_bic(double loglik, int k, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_ESTIMATION_H */
