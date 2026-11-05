/*
 * gs_mvdist.h — distribuzioni multivariate (da et_al.).
 *
 * Densità logaritmica di una normale multivariata e di una Student-t
 * multivariata, via Cholesky (L L^T = cov) e forma quadratica di Mahalanobis.
 */
#ifndef GMBSTATS_GS_MVDIST_H
#define GMBSTATS_GS_MVDIST_H

#include "gc_core.h"

/* Copule archimedeane: CDF C(u,v) e campionamento (Marshall-Olkin frailty). */
double gs_copula_clayton_cdf(double u, double v, double theta);
double gs_copula_gumbel_cdf(double u, double v, double theta);
double gs_copula_frank_cdf(double u, double v, double theta);
int gs_copula_clayton_sample(double theta, unsigned seed, double *u, double *v);
int gs_copula_gumbel_sample(double theta, unsigned seed, double *u, double *v);
int gs_copula_frank_sample(double theta, unsigned seed, double *u, double *v);

#ifdef __cplusplus
extern "C" {
#endif

/* log N(x | loc, cov), x/loc lunghezza d, cov d×d (row-major). */
gc_real gs_mvgauss_logpdf(int d, const gc_real *x, const gc_real *loc, const gc_real *cov);
/* log t_nu(x | loc, cov). nu = INFINITY delega alla normale. */
gc_real gs_mvstudent_logpdf(int d, const gc_real *x, const gc_real *loc,
                            const gc_real *cov, gc_real nu);
/* pdf (esponenziale del logpdf). */
gc_real gs_mvgauss_pdf(int d, const gc_real *x, const gc_real *loc, const gc_real *cov);
gc_real gs_mvstudent_pdf(int d, const gc_real *x, const gc_real *loc,
                         const gc_real *cov, gc_real nu);

/* Copule archimedeane: CDF C(u,v) e campionamento (Marshall-Olkin frailty). */
double gs_copula_clayton_cdf(double u, double v, double theta);
double gs_copula_gumbel_cdf(double u, double v, double theta);
double gs_copula_frank_cdf(double u, double v, double theta);
int gs_copula_clayton_sample(double theta, unsigned seed, double *u, double *v);
int gs_copula_gumbel_sample(double theta, unsigned seed, double *u, double *v);
int gs_copula_frank_sample(double theta, unsigned seed, double *u, double *v);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_MVDIST_H */
