/* gs_basic_abi.c — wrapper con la FIRMA BASIC dei nomi storici `STATS.*`.
 *
 * GENERATO dai prototipi `gs_dist*` e dalle firme BASIC (2026-11-18). Qui NON c'è logica: solo
 * l'inversione dei parametri dove la BASIC vuole `x`/`p` PRIMO e la C lo vuole ULTIMO.
 * Un `.inc` dichiara i parametri nell'ordine in cui vengono passati alla C: senza questo strato
 * la firma BASIC si romperebbe.
 */
#include "gs_dist.h"
#include "gs_dist2.h"

double gs_basic_beta_cdf(double x, double alpha, double beta) { return gs_dist_beta_cdf(alpha, beta, x); }
double gs_basic_beta_pdf(double x, double alpha, double beta) { return gs_dist_beta_pdf(alpha, beta, x); }
double gs_basic_beta_ppf(double p, double alpha, double beta) { return gs_dist_beta_ppf(alpha, beta, p); }
double gs_basic_beta_sf(double x, double alpha, double beta) { return gs_dist_beta_sf(alpha, beta, x); }
double gs_basic_chi_cdf(double x, double k) { return gs_dist_chi_cdf(k, x); }
double gs_basic_chi_pdf(double x, double k) { return gs_dist_chi_pdf(k, x); }
double gs_basic_chi_ppf(double p, double k) { return gs_dist_chi_ppf(k, p); }
double gs_basic_chi_sf(double x, double k) { return gs_dist_chi_sf(k, x); }
double gs_basic_exp_cdf(double x, double rate) { return gs_dist_exp_cdf(rate, x); }
double gs_basic_exp_isf(double p, double rate) { return gs_dist_exp_isf(rate, p); }
double gs_basic_exp_pdf(double x, double rate) { return gs_dist_exp_pdf(rate, x); }
double gs_basic_exp_ppf(double p, double rate) { return gs_dist_exp_ppf(rate, p); }
double gs_basic_exp_sf(double x, double rate) { return gs_dist_exp_sf(rate, x); }
double gs_basic_f_cdf(double x, double d1, double d2) { return gs_dist_f_cdf(d1, d2, x); }
double gs_basic_f_isf(double p, double d1, double d2) { return gs_dist_f_isf(d1, d2, p); }
double gs_basic_f_pdf(double x, double d1, double d2) { return gs_dist_f_pdf(d1, d2, x); }
double gs_basic_f_ppf(double p, double d1, double d2) { return gs_dist_f_ppf(d1, d2, p); }
double gs_basic_f_sf(double x, double d1, double d2) { return gs_dist_f_sf(d1, d2, x); }
double gs_basic_logistic_pdf(double x, double location, double scale) { return gs_dist_logistic_pdf(location, scale, x); }
double gs_basic_logistic_ppf(double p, double location, double scale) { return gs_dist_logistic_ppf(location, scale, p); }
double gs_basic_nc_beta_cdf(double x, double a, double b, double nc) { return gs_dist_nc_beta_cdf(a, b, nc, x); }
double gs_basic_nc_beta_isf(double p, double a, double b, double nc) { return gs_dist_nc_beta_isf(a, b, nc, p); }
double gs_basic_nc_beta_pdf(double x, double a, double b, double nc) { return gs_dist_nc_beta_pdf(a, b, nc, x); }
double gs_basic_nc_beta_ppf(double p, double a, double b, double nc) { return gs_dist_nc_beta_ppf(a, b, nc, p); }
double gs_basic_nc_chi2_cdf(double x, double df, double nc) { return gs_dist_nc_chi2_cdf(df, nc, x); }
double gs_basic_nc_chi2_isf(double p, double df, double nc) { return gs_dist_nc_chi2_isf(df, nc, p); }
double gs_basic_nc_chi2_pdf(double x, double df, double nc) { return gs_dist_nc_chi2_pdf(df, nc, x); }
double gs_basic_nc_chi2_ppf(double p, double df, double nc) { return gs_dist_nc_chi2_ppf(df, nc, p); }
double gs_basic_nc_gamma_cdf(double x, double shape, double nc) { return gs_dist_nc_gamma_cdf(shape, nc, x); }
double gs_basic_nc_gamma_isf(double p, double shape, double nc) { return gs_dist_nc_gamma_isf(shape, nc, p); }
double gs_basic_nc_gamma_pdf(double x, double shape, double nc) { return gs_dist_nc_gamma_pdf(shape, nc, x); }
double gs_basic_nc_gamma_ppf(double p, double shape, double nc) { return gs_dist_nc_gamma_ppf(shape, nc, p); }
double gs_basic_pareto_cdf(double x, double scale, double shape) { return gs_dist_pareto_cdf(scale, shape, x); }
double gs_basic_pareto_pdf(double x, double scale, double shape) { return gs_dist_pareto_pdf(scale, shape, x); }
double gs_basic_pareto_ppf(double p, double scale, double shape) { return gs_dist_pareto_ppf(scale, shape, p); }
double gs_basic_pareto_sf(double x, double scale, double shape) { return gs_dist_pareto_sf(scale, shape, x); }
double gs_basic_student_t_cdf(double x, double nu) { return gs_dist_student_t_cdf(nu, x); }
double gs_basic_student_t_isf(double p, double nu) { return gs_dist_student_t_isf(nu, p); }
double gs_basic_student_t_pdf(double x, double nu) { return gs_dist_student_t_pdf(nu, x); }
double gs_basic_student_t_ppf(double p, double nu) { return gs_dist_student_t_ppf(nu, p); }
double gs_basic_student_t_sf(double x, double nu) { return gs_dist_student_t_sf(nu, x); }
double gs_basic_uniform_cdf(double x, double a, double b) { return gs_dist_uniform_cdf(a, b, x); }
double gs_basic_uniform_sf(double x, double a, double b) { return gs_dist_uniform_sf(a, b, x); }
double gs_basic_weibull_cdf(double x, double shape, double scale) { return gs_dist_weibull_cdf(shape, scale, x); }
double gs_basic_weibull_pdf(double x, double shape, double scale) { return gs_dist_weibull_pdf(shape, scale, x); }
double gs_basic_weibull_ppf(double p, double shape, double scale) { return gs_dist_weibull_ppf(shape, scale, p); }
double gs_basic_weibull_sf(double x, double shape, double scale) { return gs_dist_weibull_sf(shape, scale, x); }

/* ── chi-quadro (prima ondata, 2026-11-18) ─────────────────────────────────── */
double gs_basic_chi2_pdf(double x, double df) { return gs_dist_chi2_pdf(df, x); }
double gs_basic_chi2_cdf(double x, double df) { return gs_dist_chi2_cdf(df, x); }
double gs_basic_chi2_sf(double x, double df) { return gs_dist_chi2_sf(df, x); }
double gs_basic_chi2_ppf(double p, double df) { return gs_dist_chi2_ppf(df, p); }
double gs_basic_chi2_isf(double p, double df) { return gs_dist_chi2_isf(df, p); }

/* ── famiglie con parametri OPTIONAL (2026-11-19): firma BASIC, x/p primo ── */
double gs_basic_normal_pdf(double x, double mu, double sigma) { return gs_dist_normal_pdf(mu, sigma, x); }
double gs_basic_normal_cdf(double x, double mu, double sigma) { return gs_dist_normal_cdf(mu, sigma, x); }
double gs_basic_normal_sf(double x, double mu, double sigma) { return gs_dist_normal_sf(mu, sigma, x); }
double gs_basic_normal_ppf(double p, double mu, double sigma) { return gs_dist_normal_ppf(mu, sigma, p); }
double gs_basic_normal_isf(double p, double mu, double sigma) { return gs_dist_normal_isf(mu, sigma, p); }
double gs_basic_lognormal_pdf(double x, double mu, double sigma) { return gs_dist_lognormal_pdf(mu, sigma, x); }
double gs_basic_lognormal_cdf(double x, double mu, double sigma) { return gs_dist_lognormal_cdf(mu, sigma, x); }
double gs_basic_lognormal_sf(double x, double mu, double sigma) { return gs_dist_lognormal_sf(mu, sigma, x); }
double gs_basic_lognormal_ppf(double p, double mu, double sigma) { return gs_dist_lognormal_ppf(mu, sigma, p); }
double gs_basic_logistic_cdf(double x, double location, double scale) { return gs_dist_logistic_cdf(location, scale, x); }
double gs_basic_logistic_sf(double x, double location, double scale) { return gs_dist_logistic_sf(location, scale, x); }
double gs_basic_uniform_pdf(double x, double a, double b) { return gs_dist_uniform_pdf(a, b, x); }
double gs_basic_uniform_ppf(double p, double a, double b) { return gs_dist_uniform_ppf(a, b, p); }
double gs_basic_triangular_pdf(double x, double min, double mode, double max) { return gs_dist_triangular_pdf(min, max, mode, x); }
double gs_basic_triangular_ppf(double p, double min, double mode, double max) { return gs_dist_triangular_ppf(min, max, mode, p); }

/* ── completamenti (gs_dist_extra.c): firma BASIC, x/p primo ── */
double gs_basic_lognormal_isf(double p, double mu, double sigma) { return gs_dist_lognormal_isf(mu, sigma, p); }
double gs_basic_uniform_isf(double p, double a, double b) { return gs_dist_uniform_isf(a, b, p); }
double gs_basic_triangular_cdf(double x, double min, double mode, double max) { return gs_dist_triangular_cdf(min, max, mode, x); }
double gs_basic_triangular_sf(double x, double min, double mode, double max) { return gs_dist_triangular_sf(min, max, mode, x); }
double gs_basic_triangular_isf(double p, double min, double mode, double max) { return gs_dist_triangular_isf(min, max, mode, p); }
