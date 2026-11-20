/* gmb_dist_flat.c — PONTE SHARED delle distribuzioni del runtime condiviso.
 *
 * Nessuna formula qui: solo inoltro a gmb_math.c. L'ordine degli argomenti è quello dei kernel
 * (vedi la testata di gmb_dist_flat.h per le due famiglie con ordine diverso dal C#). */
#include "gmb_dist_flat.h"

double gmb_dstf_normal_pdf(double mean, double sd, double x) { return gmb_dist_normal_pdf(mean, sd, x); }
double gmb_dstf_normal_cdf(double mean, double sd, double x) { return gmb_dist_normal_cdf(mean, sd, x); }
double gmb_dstf_lognormal_pdf(double mu, double sigma, double x) { return gmb_dist_lognormal_pdf(mu, sigma, x); }
double gmb_dstf_lognormal_cdf(double mu, double sigma, double x) { return gmb_dist_lognormal_cdf(mu, sigma, x); }
double gmb_dstf_chi_pdf(double df, double x) { return gmb_dist_chi_pdf(df, x); }
double gmb_dstf_chi_cdf(double df, double x) { return gmb_dist_chi_cdf(df, x); }
double gmb_dstf_chi2_pdf(double df, double x) { return gmb_dist_chi2_pdf(df, x); }
double gmb_dstf_chi2_cdf(double df, double x) { return gmb_dist_chi2_cdf(df, x); }
double gmb_dstf_beta_pdf(double a, double b, double x) { return gmb_dist_beta_pdf(a, b, x); }
double gmb_dstf_beta_cdf(double a, double b, double x) { return gmb_dist_beta_cdf(a, b, x); }
double gmb_dstf_student_t_pdf(double df, double x) { return gmb_dist_student_t_pdf(df, x); }
double gmb_dstf_student_t_cdf(double df, double x) { return gmb_dist_student_t_cdf(df, x); }
double gmb_dstf_weibull_pdf(double shape, double scale, double x) { return gmb_dist_weibull_pdf(shape, scale, x); }
double gmb_dstf_weibull_cdf(double shape, double scale, double x) { return gmb_dist_weibull_cdf(shape, scale, x); }
double gmb_dstf_f_pdf(double d1, double d2, double x) { return gmb_dist_f_pdf(d1, d2, x); }
double gmb_dstf_f_cdf(double d1, double d2, double x) { return gmb_dist_f_cdf(d1, d2, x); }
double gmb_dstf_exp_pdf(double rate, double x) { return gmb_dist_exp_pdf(rate, x); }
double gmb_dstf_exp_cdf(double rate, double x) { return gmb_dist_exp_cdf(rate, x); }
double gmb_dstf_pareto_pdf(double xm, double alpha, double x) { return gmb_dist_pareto_pdf(xm, alpha, x); }
double gmb_dstf_pareto_cdf(double xm, double alpha, double x) { return gmb_dist_pareto_cdf(xm, alpha, x); }
double gmb_dstf_logistic_pdf(double mu, double s, double x) { return gmb_dist_logistic_pdf(mu, s, x); }
double gmb_dstf_logistic_cdf(double mu, double s, double x) { return gmb_dist_logistic_cdf(mu, s, x); }
double gmb_dstf_triangular_pdf(double min, double max, double mode, double x) { return gmb_dist_triangular_pdf(min, max, mode, x); }
double gmb_dstf_triangular_cdf(double min, double max, double mode, double x) { return gmb_dist_triangular_cdf(min, max, mode, x); }
double gmb_dstf_uniform_pdf(double a, double b, double x) { return gmb_dist_uniform_pdf(a, b, x); }
double gmb_dstf_skewnormal_pdf(double x, double location, double scale, double shape) { return gmb_dist_skewnormal_pdf(x, location, scale, shape); }
double gmb_dstf_bernoulli_pmf(double p, int k) { return gmb_dist_bernoulli_pmf(p, k); }
double gmb_dstf_binomial_pmf(double p, int n, int k) { return gmb_dist_binomial_pmf(p, n, k); }
double gmb_dstf_poisson_pmf(double lambda, int k) { return gmb_dist_poisson_pmf(lambda, k); }
double gmb_dstf_geometric_pmf(double p, int k) { return gmb_dist_geometric_pmf(p, k); }
double gmb_dstf_negbinom_pmf(double r, double p, int k) { return gmb_dist_negbinom_pmf(r, p, k); }
double gmb_dstf_hypergeom_pmf(int N, int K, int n, int k) { return gmb_dist_hypergeom_pmf(N, K, n, k); }
