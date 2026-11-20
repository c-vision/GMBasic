/* gmb_special_flat.c — PONTE SHARED delle funzioni speciali del runtime condiviso.
 *
 * Il runtime è linkato staticamente nei host nativi: questa lib espone ai backend MANAGED
 * (P/Invoke) le stesse funzioni che usano VM/C e C99Native. Nessuna logica qui: solo inoltro —
 * una sola implementazione (gmb_math.c). */
#include "gmb_special_flat.h"

double gmb_spf_gamma(double x) { return gmb_gamma(x); }
double gmb_spf_lgamma(double x) { return gmb_lgamma(x); }
double gmb_spf_beta(double a, double b) { return gmb_beta_fn(a, b); }
double gmb_spf_erf(double x) { return gmb_erf(x); }
double gmb_spf_erfc(double x) { return gmb_erf_c(x); }
double gmb_spf_gamma_inc(double a, double x) { return gmb_gamma_p(a, x); }
double gmb_spf_gamma_incc(double a, double x) { return gmb_gammaincc(a, x); }
double gmb_spf_beta_inc(double a, double b, double x) { return gmb_incbeta(a, b, x); }
double gmb_spf_digamma(double x) { return gmb_digamma(x); }
double gmb_spf_bessel_j(double v, double x) { return gmb_besselj(v, x); }
double gmb_spf_bessel_y(double v, double x) { return gmb_bessely(v, x); }
double gmb_spf_bessel_i(double v, double x) { return gmb_besseli(v, x); }
double gmb_spf_bessel_k(double v, double x) { return gmb_besselk(v, x); }

double gmb_spf_beta_ln(double a, double b) { return gmb_betaln(a, b); }
double gmb_spf_erf_inv(double x) { return gmb_erfinv(x); }
double gmb_spf_erfc_inv(double x) { return gmb_erfcinv(x); }
double gmb_spf_gamma_inc_inv(double a, double y) { return gmb_gammaincinv(a, y); }
double gmb_spf_gamma_incc_inv(double a, double y) { return gmb_gammainccinv(a, y); }
double gmb_spf_beta_inc_inv(double a, double b, double y) { return gmb_betaincinv(a, b, y); }

double gmb_spf_zeta(double s) { return gmb_special_zeta(s); }
double gmb_spf_zeta_hurwitz(double s, double a) { return gmb_special_zeta_hurwitz(s, a); }
double gmb_spf_polygamma(double n, double x) { return gmb_special_polygamma(n, x); }
double gmb_spf_hypergeom_2f1(double a, double b, double c, double x) { return gmb_special_hypergeom_2f1(a, b, c, x); }
double gmb_spf_hypergeom_1f1(double a, double b, double x) { return gmb_special_hypergeom_1f1(a, b, x); }
double gmb_spf_lambert_w(double x) { return gmb_special_lambert_w(x); }
double gmb_spf_mittag_leffler(double alpha, double beta, double x) { return gmb_special_mittag_leffler(alpha, beta, x); }
double gmb_spf_struve_h(double v, double x) { return gmb_special_struve_h(v, x); }
double gmb_spf_kelvin_ber(double x) { return gmb_special_kelvin_ber(x); }
double gmb_spf_kelvin_bei(double x) { return gmb_special_kelvin_bei(x); }
double gmb_spf_parabolic_cylinder(double a, double x) { return gmb_special_parabolic_cylinder(a, x); }
