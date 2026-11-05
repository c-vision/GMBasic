/*
 * gs_dist.h — distribuzioni (SciPy stats), wrapper moderno del legacy.
 *
 * Espone come API gs_dist_* le distribuzioni già presenti nel codice GMBasic:
 * normal, chi, chi2, beta, student_t, weibull, exp, f, lognormal, pareto,
 * logistic, triangular, uniform, skewnormal (pdf/cdf/ppf/isf) e le discrete
 * bernoulli, binomial, poisson, geometric, negbinom, hypergeom (pmf).
 */
#ifndef GMBSTATS_GS_DIST_H
#define GMBSTATS_GS_DIST_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Continue: pdf/cdf/ppf/isf. Parametrizzazione come SciPy (mean/sd, df, a/b, ...). */
gc_real gs_dist_normal_pdf(gc_real mean, gc_real sd, gc_real x);
gc_real gs_dist_normal_cdf(gc_real mean, gc_real sd, gc_real x);
gc_real gs_dist_normal_ppf(gc_real mean, gc_real sd, gc_real p);
gc_real gs_dist_normal_isf(gc_real mean, gc_real sd, gc_real p);

gc_real gs_dist_chi2_pdf(gc_real df, gc_real x);
gc_real gs_dist_chi2_cdf(gc_real df, gc_real x);
gc_real gs_dist_chi2_ppf(gc_real df, gc_real p);
gc_real gs_dist_chi2_isf(gc_real df, gc_real p);
gc_real gs_dist_chi_pdf(gc_real df, gc_real x);
gc_real gs_dist_chi_cdf(gc_real df, gc_real x);
gc_real gs_dist_chi_ppf(gc_real df, gc_real p);

gc_real gs_dist_beta_pdf(gc_real a, gc_real b, gc_real x);
gc_real gs_dist_beta_cdf(gc_real a, gc_real b, gc_real x);
gc_real gs_dist_beta_ppf(gc_real a, gc_real b, gc_real p);

gc_real gs_dist_student_t_pdf(gc_real df, gc_real x);
gc_real gs_dist_student_t_cdf(gc_real df, gc_real x);
gc_real gs_dist_student_t_ppf(gc_real df, gc_real p);
gc_real gs_dist_student_t_isf(gc_real df, gc_real p);

gc_real gs_dist_f_pdf(gc_real d1, gc_real d2, gc_real x);
gc_real gs_dist_f_cdf(gc_real d1, gc_real d2, gc_real x);
gc_real gs_dist_f_ppf(gc_real d1, gc_real d2, gc_real p);
gc_real gs_dist_f_isf(gc_real d1, gc_real d2, gc_real p);

gc_real gs_dist_exp_pdf(gc_real rate, gc_real x);
gc_real gs_dist_exp_cdf(gc_real rate, gc_real x);
gc_real gs_dist_exp_ppf(gc_real rate, gc_real p);
gc_real gs_dist_exp_isf(gc_real rate, gc_real p);

gc_real gs_dist_weibull_pdf(gc_real shape, gc_real scale, gc_real x);
gc_real gs_dist_weibull_cdf(gc_real shape, gc_real scale, gc_real x);
gc_real gs_dist_weibull_ppf(gc_real shape, gc_real scale, gc_real p);

gc_real gs_dist_lognormal_pdf(gc_real mu, gc_real sigma, gc_real x);
gc_real gs_dist_lognormal_cdf(gc_real mu, gc_real sigma, gc_real x);
gc_real gs_dist_lognormal_ppf(gc_real mu, gc_real sigma, gc_real p);

gc_real gs_dist_pareto_pdf(gc_real xm, gc_real alpha, gc_real x);
gc_real gs_dist_pareto_cdf(gc_real xm, gc_real alpha, gc_real x);
gc_real gs_dist_pareto_ppf(gc_real xm, gc_real alpha, gc_real p);

gc_real gs_dist_logistic_pdf(gc_real mu, gc_real s, gc_real x);
gc_real gs_dist_logistic_cdf(gc_real mu, gc_real s, gc_real x);
gc_real gs_dist_logistic_ppf(gc_real mu, gc_real s, gc_real p);

gc_real gs_dist_triangular_pdf(gc_real a, gc_real b, gc_real c, gc_real x);
gc_real gs_dist_triangular_ppf(gc_real a, gc_real b, gc_real c, gc_real p);

gc_real gs_dist_uniform_pdf(gc_real a, gc_real b, gc_real x);
gc_real gs_dist_uniform_cdf(gc_real a, gc_real b, gc_real x);
gc_real gs_dist_uniform_ppf(gc_real a, gc_real b, gc_real p);

gc_real gs_dist_skewnormal_pdf(gc_real x, gc_real location, gc_real scale, gc_real shape);

/* Survival function sf(x) = 1 - cdf(x) (per le continue con cdf). */
gc_real gs_dist_normal_sf(gc_real mean, gc_real sd, gc_real x);
gc_real gs_dist_chi2_sf(gc_real df, gc_real x);
gc_real gs_dist_chi_sf(gc_real df, gc_real x);
gc_real gs_dist_beta_sf(gc_real a, gc_real b, gc_real x);
gc_real gs_dist_student_t_sf(gc_real df, gc_real x);
gc_real gs_dist_f_sf(gc_real d1, gc_real d2, gc_real x);
gc_real gs_dist_exp_sf(gc_real rate, gc_real x);
gc_real gs_dist_weibull_sf(gc_real shape, gc_real scale, gc_real x);
gc_real gs_dist_lognormal_sf(gc_real mu, gc_real sigma, gc_real x);
gc_real gs_dist_pareto_sf(gc_real xm, gc_real alpha, gc_real x);
gc_real gs_dist_logistic_sf(gc_real mu, gc_real s, gc_real x);
gc_real gs_dist_uniform_sf(gc_real a, gc_real b, gc_real x);
gc_real gs_dist_gamma_sf(gc_real shape, gc_real scale, gc_real x);

/* Non centrali (da gs_dist2): pdf/cdf/ppf/isf. */
gc_real gs_dist_nc_chi2_pdf(gc_real df, gc_real nc, gc_real x);
gc_real gs_dist_nc_chi2_cdf(gc_real df, gc_real nc, gc_real x);
gc_real gs_dist_nc_chi2_ppf(gc_real df, gc_real nc, gc_real p);
gc_real gs_dist_nc_chi2_isf(gc_real df, gc_real nc, gc_real p);
gc_real gs_dist_nc_gamma_pdf(gc_real a, gc_real d, gc_real x);
gc_real gs_dist_nc_gamma_cdf(gc_real a, gc_real d, gc_real x);
gc_real gs_dist_nc_gamma_ppf(gc_real a, gc_real d, gc_real p);
gc_real gs_dist_nc_gamma_isf(gc_real a, gc_real d, gc_real p);
gc_real gs_dist_nc_beta_pdf(gc_real a, gc_real b, gc_real d, gc_real x);
gc_real gs_dist_nc_beta_cdf(gc_real a, gc_real b, gc_real d, gc_real x);
gc_real gs_dist_nc_beta_ppf(gc_real a, gc_real b, gc_real d, gc_real p);
gc_real gs_dist_nc_beta_isf(gc_real a, gc_real b, gc_real d, gc_real p);
gc_real gs_dist_ncf_pdf(gc_real d1, gc_real d2, gc_real nc, gc_real x);
gc_real gs_dist_ncf_cdf(gc_real d1, gc_real d2, gc_real nc, gc_real x);
gc_real gs_dist_ncf_ppf(gc_real d1, gc_real d2, gc_real nc, gc_real p);
gc_real gs_dist_ncf_isf(gc_real d1, gc_real d2, gc_real nc, gc_real p);
gc_real gs_dist_nct_pdf(gc_real df, gc_real nc, gc_real x);
gc_real gs_dist_nct_cdf(gc_real df, gc_real nc, gc_real x);
gc_real gs_dist_nct_ppf(gc_real df, gc_real nc, gc_real p);
gc_real gs_dist_nct_isf(gc_real df, gc_real nc, gc_real p);

/* Discrete: pmf. */
gc_real gs_dist_bernoulli_pmf(gc_real p, int k);
gc_real gs_dist_binomial_pmf(gc_real p, int n, int k);
gc_real gs_dist_poisson_pmf(gc_real lambda, int k);
gc_real gs_dist_geometric_pmf(gc_real p, int k);
gc_real gs_dist_negbinom_pmf(gc_real r, gc_real p, int k);
gc_real gs_dist_hypergeom_pmf(int N, int K, int n, int k);

/* Zero-truncated Poisson (lambda > 0): supporto k >= 1,
 * P(X=k) = lambda^k e^{-lambda} / (k! (1 - e^{-lambda})).
 * Da ESL (INET, #166), riscrittura clean-room. */
gc_real gs_dist_ztpoisson_pmf(gc_real lambda, int k);
gc_real gs_dist_ztpoisson_cdf(gc_real lambda, int k);
gc_real gs_dist_ztpoisson_mean(gc_real lambda);
gc_real gs_dist_ztpoisson_variance(gc_real lambda);
/* Campionamento inverse-CDF: u uniforme in [0,1). Ritorna k >= 1 (o -1 se
 * lambda <= 0). */
int gs_dist_ztpoisson_sample(gc_real lambda, gc_real u);

/* Gamma (shape, scale) — da gs_dist2/legacy. */
gc_real gs_dist_gamma_pdf(gc_real shape, gc_real scale, gc_real x);
gc_real gs_dist_gamma_cdf(gc_real shape, gc_real scale, gc_real x);
gc_real gs_dist_gamma_ppf(gc_real shape, gc_real scale, gc_real p);
gc_real gs_dist_gamma_isf(gc_real shape, gc_real scale, gc_real p);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_DIST_H */

/* Normale bivariata standard: Phi2(h,k; rho). (Drezner/West, Simpson 1D.) */
double gs_bvn_cdf(double h, double k, double rho);

/* Chi-quadro non centrale: P(X<=x) con df gradi e ncp noncentralita'. */
double gs_ncx2_cdf(double x, double df, double ncp);
