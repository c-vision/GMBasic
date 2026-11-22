/* gmb_dist_flat.h — PONTE delle DISTRIBUZIONI per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. `CommonLib/Numerics/Stats.cs` calcolava pdf/cdf/pmf in C# (MathNet) mentre VM/C e
 * C99Native usano `gmb_dist_*` di `gmb_math.c`: una sola sorgente numerica tramite questa lib.
 *
 * ATTENZIONE ALL'ORDINE DEGLI ARGOMENTI: qui è quello dei KERNEL, che per due famiglie differisce
 * da quello dei metodi C#:
 *   - `gmb_dstf_triangular_*` è (min, MAX, MODE, x) mentre il C# è (min, mode, max, x);
 *   - `gmb_dstf_skewnormal_pdf` è (x, location, scale, shape) mentre il C# è (mean, sd, skew, x). */
#ifndef GMB_DIST_FLAT_H
#define GMB_DIST_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

double gmb_dstf_normal_pdf(double mean, double sd, double x);
double gmb_dstf_normal_cdf(double mean, double sd, double x);
double gmb_dstf_lognormal_pdf(double mu, double sigma, double x);
double gmb_dstf_lognormal_cdf(double mu, double sigma, double x);
double gmb_dstf_chi_pdf(double df, double x);
double gmb_dstf_chi_cdf(double df, double x);
double gmb_dstf_chi2_pdf(double df, double x);
double gmb_dstf_chi2_cdf(double df, double x);
double gmb_dstf_beta_pdf(double a, double b, double x);
double gmb_dstf_beta_cdf(double a, double b, double x);
double gmb_dstf_student_t_pdf(double df, double x);
double gmb_dstf_student_t_cdf(double df, double x);
double gmb_dstf_weibull_pdf(double shape, double scale, double x);
double gmb_dstf_weibull_cdf(double shape, double scale, double x);
double gmb_dstf_f_pdf(double d1, double d2, double x);
double gmb_dstf_f_cdf(double d1, double d2, double x);
double gmb_dstf_exp_pdf(double rate, double x);
double gmb_dstf_exp_cdf(double rate, double x);
double gmb_dstf_pareto_pdf(double xm, double alpha, double x);
double gmb_dstf_pareto_cdf(double xm, double alpha, double x);
double gmb_dstf_logistic_pdf(double mu, double s, double x);
double gmb_dstf_logistic_cdf(double mu, double s, double x);
double gmb_dstf_triangular_pdf(double min, double max, double mode, double x);
double gmb_dstf_triangular_cdf(double min, double max, double mode, double x);
double gmb_dstf_uniform_pdf(double a, double b, double x);
double gmb_dstf_skewnormal_pdf(double x, double location, double scale, double shape);
double gmb_dstf_bernoulli_pmf(double p, int k);
double gmb_dstf_binomial_pmf(double p, int n, int k);
double gmb_dstf_poisson_pmf(double lambda, int k);
double gmb_dstf_geometric_pmf(double p, int k);
double gmb_dstf_negbinom_pmf(double r, double p, int k);
double gmb_dstf_hypergeom_pmf(int N, int K, int n, int k);

/* ── Inverse (ppf/isf) e CDF uniforme — argomenti nell'ordine dei KERNEL ───────────────────── */
double gmb_dstf_normal_ppf(double mean, double sd, double p);
double gmb_dstf_normal_isf(double mean, double sd, double p);
double gmb_dstf_lognormal_ppf(double mu, double sigma, double p);
double gmb_dstf_lognormal_isf(double mu, double sigma, double p);
double gmb_dstf_chi_ppf(double df, double p);
double gmb_dstf_chi_isf(double df, double p);
double gmb_dstf_chi2_ppf(double df, double p);
double gmb_dstf_chi2_isf(double df, double p);
double gmb_dstf_beta_ppf(double a, double b, double p);
double gmb_dstf_beta_isf(double a, double b, double p);
double gmb_dstf_student_t_ppf(double df, double p);
double gmb_dstf_student_t_isf(double df, double p);
double gmb_dstf_f_ppf(double d1, double d2, double p);
double gmb_dstf_f_isf(double d1, double d2, double p);
double gmb_dstf_exp_ppf(double rate, double p);
double gmb_dstf_exp_isf(double rate, double p);
double gmb_dstf_weibull_ppf(double shape, double scale, double p);
double gmb_dstf_weibull_isf(double shape, double scale, double p);
double gmb_dstf_logistic_ppf(double loc, double scale, double p);
double gmb_dstf_logistic_isf(double loc, double scale, double p);
double gmb_dstf_uniform_ppf(double a, double b, double p);
double gmb_dstf_uniform_isf(double a, double b, double p);
double gmb_dstf_uniform_cdf(double a, double b, double x);
double gmb_dstf_pareto_ppf(double scale, double shape, double p);
double gmb_dstf_pareto_isf(double scale, double shape, double p);
double gmb_dstf_triangular_ppf(double min, double max, double mode, double p);
double gmb_dstf_triangular_isf(double min, double max, double mode, double p);

double gmb_dstf_gamma_pdf(double shape, double scale, double x);
double gmb_dstf_gamma_cdf(double shape, double scale, double x);
double gmb_dstf_gamma_sf(double shape, double scale, double x);
double gmb_dstf_gamma_ppf(double shape, double scale, double p);
double gmb_dstf_gamma_isf(double shape, double scale, double p);
double gmb_dstf_ztpoisson_pmf(double lambda, int k);
double gmb_dstf_ztpoisson_cdf(double lambda, int k);
double gmb_dstf_mode(const double* a, int n);
double gmb_dstf_moment(const double* a, int n, int order);
double gmb_dstf_iqr(const double* a, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DIST_FLAT_H */
