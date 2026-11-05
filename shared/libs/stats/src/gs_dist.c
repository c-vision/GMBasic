/*
 * gs_dist.c — distribuzioni (wrapper del legacy GMBasic).
 */
#include "gs_dist.h"
#include "gs_dist2.h"
#include <math.h>

#define D(name) extern double gmi_##name

/* primitive matematiche gmbmath */
double gm_special_beta(double a, double b);
double gm_special_betaincinv(double a, double b, double y);
double gmi_comb(double n, double k);
double gm_special_erfinv(double x);
double gm_special_gamma(double x);
double gm_special_gammainc(double a, double x);
double gm_special_gammaincinv(double a, double y);
double gm_special_betainc(double a, double b, double x);
static double gs_normal_cdf(double z) { return 0.5 * erfc(-z / sqrt(2.0)); }


double gs_dist_normal_pdf(double mean, double sd, double x) {
    if (sd <= 0) return 0;
    double z = (x - mean) / sd;
    return exp(-0.5 * z * z) / (sd * sqrt(2 * 3.141592653589793));
}
double gs_dist_normal_cdf(double mean, double sd, double x) {
    if (sd <= 0) return 0;
    return gs_normal_cdf((x - mean) / sd);
}
double gs_dist_normal_ppf(double mean, double sd, double p)
    { return mean + sd * sqrt(2.0) * gm_special_erfinv(2.0 * p - 1.0); }
double gs_dist_normal_isf(double mean, double sd, double p)
    { return gs_dist_normal_ppf(mean, sd, 1.0 - p); }
double gs_dist_chi2_pdf(double df, double x) {
    if (df <= 0 || x < 0) return 0;
    return pow(2.0, -df / 2.0) * pow(x, df / 2.0 - 1.0) * exp(-x / 2.0) / tgamma(df / 2.0);
}
double gs_dist_chi2_cdf(double df, double x) {
    if (df <= 0) return 0;
    return x < 0 ? 0 : gm_special_gammainc(df / 2.0, x / 2.0);
}
double gs_dist_chi2_ppf(double df, double p)
    { return 2.0 * gm_special_gammaincinv(df / 2.0, p); }
double gs_dist_chi2_isf(double df, double p)
    { return 2.0 * gm_special_gammaincinv(df / 2.0, 1.0 - p); }
double gs_dist_chi_pdf(double df, double x) {
    if (df <= 0 || x < 0) return 0;
    /* Chi (non chi-squared): 2^(1-k/2) x^(k-1) e^(-x^2/2) / Γ(k/2) — parità scipy/IL. */
    return pow(2.0, 1.0 - df / 2.0) * pow(x, df - 1.0) * exp(-x * x / 2.0) / tgamma(df / 2.0);
}
double gs_dist_chi_cdf(double df, double x) {
    if (df <= 0) return 0;
    /* Chi CDF = P(k/2, x^2/2) — prima era la formula chi-squared P(k/2, x/2). */
    return x < 0 ? 0 : gm_special_gammainc(df / 2.0, x * x / 2.0);
}
double gs_dist_chi_ppf(double df, double p)
    { return sqrt(gs_dist_chi2_ppf(df, p)); }
double gs_dist_beta_pdf(double a, double b, double x) {
    if (a <= 0 || b <= 0) return NAN;
    if (x < 0 || x > 1) return 0;
    return pow(x, a-1) * pow(1-x, b-1) / gm_special_beta(a, b);
}
double gs_dist_beta_cdf(double a, double b, double x) {
    if (a <= 0 || b <= 0) return NAN;
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    return gm_special_betainc(a, b, x);
}
double gs_dist_beta_ppf(double a, double b, double p)
    { return gm_special_betaincinv(a, b, p); }
double gs_dist_student_t_pdf(double df, double x) {
    if (df <= 0) return 0;
    return tgamma((df+1)/2) / (sqrt(df * 3.141592653589793) * tgamma(df/2)) * pow(1 + x*x/df, -(df+1)/2);
}
double gs_dist_student_t_cdf(double df, double x) {
    if (df <= 0) return 0;
    double z = df / (df + x*x);
    double ib = gm_special_betainc(df/2, 0.5, z);
    return x >= 0 ? 1 - 0.5*ib : 0.5*ib;
}
double gs_dist_student_t_ppf(double df, double p)
{
    if (p == 0.5) return 0.0;
    double sign = p > 0.5 ? 1.0 : -1.0;
    return sign * sqrt(gs_dist_f_ppf(1.0, df, fabs(2.0 * p - 1.0)));
}
double gs_dist_student_t_isf(double df, double p)
    { return gs_dist_student_t_ppf(df, 1.0 - p); }
double gs_dist_f_pdf(double d1, double d2, double x) {
    if (d1 <= 0 || d2 <= 0) return NAN;
    if (x < 0) return 0;
    if (x == 0 && d1 < 2) return 1.0 / 0.0;
    return exp(lgamma((d1+d2)/2) - lgamma(d1/2) - lgamma(d2/2)
               + (d1/2)*log(d1) + (d2/2)*log(d2) + (d1/2-1)*log(x)
               - ((d1+d2)/2)*log(d2 + d1*x));
}
double gs_dist_f_cdf(double d1, double d2, double x) {
    if (d1 <= 0 || d2 <= 0) return NAN;
    if (x < 0) return 0;
    double y = d1 * x / (d1 * x + d2);
    return gm_special_betainc(d1/2, d2/2, y);
}
double gs_dist_f_ppf(double d1, double d2, double p)
{
    double z = gm_special_betaincinv(d1 / 2.0, d2 / 2.0, p);
    return d2 * z / (d1 * (1.0 - z));
}
double gs_dist_f_isf(double d1, double d2, double p)
    { return gs_dist_f_ppf(d1, d2, 1.0 - p); }
double gs_dist_exp_pdf(double rate, double x) {
    if (rate <= 0) return 0;
    return x < 0 ? 0 : rate * exp(-rate * x);
}
double gs_dist_exp_cdf(double rate, double x) {
    if (rate <= 0) return 0;
    return x < 0 ? 0 : 1 - exp(-rate * x);
}
double gs_dist_exp_ppf(double rate, double p)
    { return -log(1.0 - p) / rate; }
double gs_dist_exp_isf(double rate, double p)
    { return -log(p) / rate; }
double gs_dist_weibull_pdf(double shape, double scale, double x) {
    if (shape <= 0 || scale <= 0) return NAN;
    if (x < 0) return 0;
    return (shape/scale) * pow(x/scale, shape-1) * exp(-pow(x/scale, shape));
}
double gs_dist_weibull_cdf(double shape, double scale, double x) {
    if (shape <= 0 || scale <= 0) return NAN;
    return x < 0 ? 0 : 1 - exp(-pow(x/scale, shape));
}
double gs_dist_weibull_ppf(double shape, double scale, double p)
    { return scale * pow(-log(1.0 - p), 1.0 / shape); }
double gs_dist_lognormal_pdf(double mu, double sigma, double x) {
    if (sigma <= 0 || x <= 0) return 0;
    double z = (log(x) - mu) / sigma;
    return exp(-0.5 * z * z) / (x * sigma * sqrt(2 * 3.141592653589793));
}
double gs_dist_lognormal_cdf(double mu, double sigma, double x) {
    if (sigma <= 0 || x <= 0) return 0;
    return gs_normal_cdf((log(x) - mu) / sigma);
}
double gs_dist_lognormal_ppf(double mu, double sigma, double p)
    { return exp(gs_dist_normal_ppf(mu, sigma, p)); }
double gs_dist_pareto_pdf(double xm, double alpha, double x) {
    if (alpha <= 0 || xm <= 0 || x < xm) return 0;
    return alpha * pow(xm, alpha) / pow(x, alpha + 1);
}
double gs_dist_pareto_cdf(double xm, double alpha, double x) {
    if (alpha <= 0 || xm <= 0 || x < xm) return 0;
    return 1 - pow(xm / x, alpha);
}
double gs_dist_pareto_ppf(double scale, double shape, double p)
    { return scale / pow(1.0 - p, 1.0 / shape); }
double gs_dist_logistic_pdf(double mu, double s, double x) {
    if (s <= 0) return 0;
    double z = (x - mu) / s;
    double e = exp(-z);
    return e / (s * (1 + e) * (1 + e));
}
double gs_dist_logistic_cdf(double mu, double s, double x) {
    if (s <= 0) return 0;
    return 1 / (1 + exp(-(x - mu) / s));
}
double gs_dist_logistic_ppf(double loc, double scale, double p)
    { return loc + scale * log(p / (1.0 - p)); }
double gs_dist_triangular_pdf(double a, double b, double c, double x) {
    /* parametrizzazione invalida (max<=min o mode fuori [min,max]) → NaN, come il
     * MathNet/CommonLib C# (Triangular.PDF lancia, l'IL restituisce NaN). */
    if (b <= a || c < a || c > b) return NAN;
    if (x < a || x > b) return 0;
    if (x <= c) return (x == a) ? 0 : 2 * (x - a) / ((b - a) * (c - a));
    return 2 * (b - x) / ((b - a) * (b - c));
}
double gs_dist_triangular_ppf(double a, double b, double c, double p)
{
    /* a=min, b=max, c=mode */
    double cstd = (c - a) / (b - a);
    double xstd = (p < cstd) ? sqrt(cstd * p) : 1.0 - sqrt((1.0 - cstd) * (1.0 - p));
    return a + (b - a) * xstd;
}
double gs_dist_uniform_pdf(double a, double b, double x) {
    if (b <= a) return 0;
    return (x >= a && x <= b) ? 1.0 / (b - a) : 0;
}
double gs_dist_uniform_cdf(double a, double b, double x) {
    if (b <= a) return 0;
    if (x <= a) return 0;
    if (x >= b) return 1;
    return (x - a) / (b - a);
}
double gs_dist_uniform_ppf(double a, double b, double p)
    { return a + p * (b - a); }
double gs_dist_skewnormal_pdf(double x, double location, double scale, double shape) {
    if (scale <= 0) return 0;
    double z = (x - location) / scale;
    double phi = exp(-0.5 * z * z) / sqrt(2 * 3.141592653589793);
    return (2.0 / scale) * phi * gs_normal_cdf(shape * z);
}
double gs_dist_bernoulli_pmf(double p, int k) {
    if (p < 0 || p > 1) return 0;
    return k == 0 ? 1 - p : k == 1 ? p : 0;
}
double gs_dist_binomial_pmf(double p, int n, int k) {
    if (p < 0 || p > 1 || n < 0 || k < 0 || k > n) return 0;
    return gmi_comb(n, k) * pow(p, k) * pow(1 - p, n - k);
}
double gs_dist_poisson_pmf(double lambda, int k) {
    if (lambda <= 0 || k < 0) return 0;
    return exp(-lambda) * pow(lambda, k) / tgamma(k + 1);
}
double gs_dist_geometric_pmf(double p, int k) {
    if (p < 0 || p > 1 || k <= 0) return 0;
    return p * pow(1 - p, k - 1);
}
double gs_dist_negbinom_pmf(double r, double p, int k) {
    if (r <= 0 || p < 0 || p > 1 || k < 0) return 0;
    return gmi_comb(r + k - 1, k) * pow(p, r) * pow(1 - p, k);
}
double gs_dist_hypergeom_pmf(int N, int K, int n, int k) {
    if (k < 0 || k > n || k > K || n - k > N - K) return 0;
    return gmi_comb(K, k) * gmi_comb(N - K, n - k) / gmi_comb(N, n);
}

/* Zero-truncated Poisson (ESL #166, clean-room). P(X=k) = lambda^k e^{-lambda}
 * / (k! (1 - e^{-lambda})), k >= 1. */

static gc_real ztp_denom(gc_real lambda) { return -expm1(-lambda); } /* 1 - e^{-lambda} */

gc_real gs_dist_ztpoisson_pmf(gc_real lambda, int k)
{
    if (lambda <= 0.0) return NAN;
    if (k < 1) return 0.0;
    return exp((gc_real)k * log(lambda) - lgamma((gc_real)k + 1.0) - lambda
               - log(ztp_denom(lambda)));
}

gc_real gs_dist_ztpoisson_cdf(gc_real lambda, int k)
{
    gc_real p, s;
    int j;
    if (lambda <= 0.0) return NAN;
    if (k < 1) return 0.0;
    p = lambda * exp(-lambda) / ztp_denom(lambda);   /* P(X=1) */
    s = p;
    for (j = 2; j <= k; j++) {
        p *= lambda / (gc_real)j;
        s += p;
        if (p < 1e-15 * s) break;   /* coda trascurabile */
    }
    return s > 1.0 ? 1.0 : s;
}

gc_real gs_dist_ztpoisson_mean(gc_real lambda)
{
    if (lambda <= 0.0) return NAN;
    return lambda / ztp_denom(lambda);
}

gc_real gs_dist_ztpoisson_variance(gc_real lambda)
{
    gc_real m;
    if (lambda <= 0.0) return NAN;
    m = lambda / ztp_denom(lambda);
    return (lambda + lambda * lambda) / ztp_denom(lambda) - m * m;
}

int gs_dist_ztpoisson_sample(gc_real lambda, gc_real u)
{
    gc_real t, s;
    int k;
    if (lambda <= 0.0) return -1;
    if (u < 0.0) u = 0.0;
    if (u >= 1.0) u = 1.0 - 1e-12;
    t = lambda * exp(-lambda) / ztp_denom(lambda);   /* P(X=1) */
    s = t;
    k = 1;
    while (s < u && k < 100000000) {
        k++;
        t *= lambda / (gc_real)k;
        s += t;
    }
    return k;
}
double gs_dist_gamma_pdf(double shape, double scale, double x)
{
    if (x < 0 || shape <= 0 || scale <= 0) return 0;
    return pow(x / scale, shape - 1.0) * exp(-x / scale) / (scale * gm_special_gamma(shape));
}
double gs_dist_gamma_cdf(double shape, double scale, double x)
{
    if (x < 0) return 0;
    return gm_special_gammainc(shape, x / scale);
}
double gs_dist_gamma_ppf(double shape, double scale, double p)
{
    if (p <= 0) return 0;
    if (p >= 1) return INFINITY;
    return gm_special_gammaincinv(shape, p) * scale;
}
double gs_dist_gamma_isf(double shape, double scale, double p)
{
    return gs_dist_gamma_ppf(shape, scale, 1.0 - p);
}

/* Survival function sf(x) = 1 - cdf(x). */
gc_real gs_dist_normal_sf(gc_real m, gc_real s, gc_real x) { return 1.0 - gs_dist_normal_cdf(m, s, x); }
gc_real gs_dist_chi2_sf(gc_real df, gc_real x) { return 1.0 - gs_dist_chi2_cdf(df, x); }
gc_real gs_dist_chi_sf(gc_real df, gc_real x) { return 1.0 - gs_dist_chi_cdf(df, x); }
gc_real gs_dist_beta_sf(gc_real a, gc_real b, gc_real x) { return 1.0 - gs_dist_beta_cdf(a, b, x); }
gc_real gs_dist_student_t_sf(gc_real df, gc_real x) { return 1.0 - gs_dist_student_t_cdf(df, x); }
gc_real gs_dist_f_sf(gc_real d1, gc_real d2, gc_real x) { return 1.0 - gs_dist_f_cdf(d1, d2, x); }
gc_real gs_dist_exp_sf(gc_real r, gc_real x) { return 1.0 - gs_dist_exp_cdf(r, x); }
gc_real gs_dist_weibull_sf(gc_real sh, gc_real sc, gc_real x) { return 1.0 - gs_dist_weibull_cdf(sh, sc, x); }
gc_real gs_dist_lognormal_sf(gc_real mu, gc_real sg, gc_real x) { return 1.0 - gs_dist_lognormal_cdf(mu, sg, x); }
gc_real gs_dist_pareto_sf(gc_real xm, gc_real al, gc_real x) { return 1.0 - gs_dist_pareto_cdf(xm, al, x); }
gc_real gs_dist_logistic_sf(gc_real mu, gc_real s, gc_real x) { return 1.0 - gs_dist_logistic_cdf(mu, s, x); }
gc_real gs_dist_uniform_sf(gc_real a, gc_real b, gc_real x) { return 1.0 - gs_dist_uniform_cdf(a, b, x); }
gc_real gs_dist_gamma_sf(gc_real sh, gc_real sc, gc_real x) { return 1.0 - gs_dist_gamma_cdf(sh, sc, x); }

/* Non centrali (da gs_dist2). */
gc_real gs_dist_nc_chi2_pdf(gc_real df, gc_real nc, gc_real x) { return gs_dist2_nc_chi2_pdf(x, df, nc); }
gc_real gs_dist_nc_chi2_cdf(gc_real df, gc_real nc, gc_real x) { return gs_dist2_nc_chi2_cdf(x, df, nc); }
gc_real gs_dist_nc_chi2_ppf(gc_real df, gc_real nc, gc_real p) { return gs_dist2_nc_chi2_ppf(p, df, nc); }
gc_real gs_dist_nc_chi2_isf(gc_real df, gc_real nc, gc_real p) { return gs_dist2_nc_chi2_isf(p, df, nc); }
gc_real gs_dist_nc_gamma_pdf(gc_real a, gc_real d, gc_real x) { return gs_dist2_nc_gamma_pdf(x, a, d); }
gc_real gs_dist_nc_gamma_cdf(gc_real a, gc_real d, gc_real x) { return gs_dist2_nc_gamma_cdf(x, a, d); }
gc_real gs_dist_nc_gamma_ppf(gc_real a, gc_real d, gc_real p) { return gs_dist2_nc_gamma_ppf(p, a, d); }
gc_real gs_dist_nc_gamma_isf(gc_real a, gc_real d, gc_real p) { return gs_dist2_nc_gamma_isf(p, a, d); }
gc_real gs_dist_nc_beta_pdf(gc_real a, gc_real b, gc_real d, gc_real x) { return gs_dist2_nc_beta_pdf(x, a, b, d); }
gc_real gs_dist_nc_beta_cdf(gc_real a, gc_real b, gc_real d, gc_real x) { return gs_dist2_nc_beta_cdf(x, a, b, d); }
gc_real gs_dist_nc_beta_ppf(gc_real a, gc_real b, gc_real d, gc_real p) { return gs_dist2_nc_beta_ppf(p, a, b, d); }
gc_real gs_dist_nc_beta_isf(gc_real a, gc_real b, gc_real d, gc_real p) { return gs_dist2_nc_beta_isf(p, a, b, d); }
gc_real gs_dist_ncf_pdf(gc_real d1, gc_real d2, gc_real nc, gc_real x) { return gs_dist2_nc_f_pdf(x, d1, d2, nc); }
gc_real gs_dist_ncf_cdf(gc_real d1, gc_real d2, gc_real nc, gc_real x) { return gs_dist2_nc_f_cdf(x, d1, d2, nc); }
gc_real gs_dist_ncf_ppf(gc_real d1, gc_real d2, gc_real nc, gc_real p) { return gs_dist2_nc_f_ppf(p, d1, d2, nc); }
gc_real gs_dist_ncf_isf(gc_real d1, gc_real d2, gc_real nc, gc_real p) { return gs_dist2_nc_f_isf(p, d1, d2, nc); }
gc_real gs_dist_nct_pdf(gc_real df, gc_real nc, gc_real x) { return gs_dist2_nc_t_pdf(x, df, nc); }
gc_real gs_dist_nct_cdf(gc_real df, gc_real nc, gc_real x) { return gs_dist2_nc_t_cdf(x, df, nc); }
gc_real gs_dist_nct_ppf(gc_real df, gc_real nc, gc_real p) { return gs_dist2_nc_t_ppf(p, df, nc); }
gc_real gs_dist_nct_isf(gc_real df, gc_real nc, gc_real p) { return gs_dist2_nc_t_isf(p, df, nc); }

static double gsv_ncdf(double x){ return 0.5*erfc(-x/sqrt(2.0)); }
double gs_bvn_cdf(double h, double k, double rho)
{
    double integ, dt, t, base;
    int i, N = 500;
    if (rho < -0.999999) rho = -0.999999;
    if (rho >  0.999999) rho =  0.999999;
    if (rho == 0.0) return gsv_ncdf(h)*gsv_ncdf(k);
    base = 2.0*3.141592653589793;
    dt = rho/(double)N;
    integ = 0.0;
    for (i = 0; i <= N; i++) {
        double w = (i==0 || i==N) ? 1.0 : ((i%2) ? 4.0 : 2.0);
        t = (double)i*dt;
        { double o = 1.0-t*t, ex = -(h*h - 2.0*h*k*t + k*k)/(2.0*o);
          integ += w*exp(ex)/(base*sqrt(o)); }
    }
    integ *= dt/3.0;
    return gsv_ncdf(h)*gsv_ncdf(k) + integ;
}

/* Regolarized lower incomplete gamma P(a,x) via serie/CF (Press et al.). */
static double gsv_gammp(double a, double x)
{
    if (x < 0.0 || a <= 0.0) return 0.0/0.0;
    if (x == 0.0) return 0.0;
    if (x < a + 1.0) {
        double ap=a, sum=1.0/a, del=sum; int n;
        for (n=1;n<200;n++){ ap+=1.0; del*=x/ap; sum+=del; if (fabs(del)<fabs(sum)*1e-14) break; }
        return sum*exp(-x + a*log(x) - lgamma(a));
    } else {
        double b=x+1.0-a, cc=1.0/1e-300, d=1.0/b, h=d; int i;
        for (i=1;i<200;i++){
            double an=-i*(i-a); b+=2.0; d=an*d+b; if (fabs(d)<1e-300) d=1e-300;
            cc=b+an/cc; if (fabs(cc)<1e-300) cc=1e-300;
            d=1.0/d; { double del=d*cc; h*=del; if (fabs(del-1.0)<1e-14) break; }
        }
        return 1.0 - exp(-x + a*log(x) - lgamma(a))*h;
    }
}
double gs_ncx2_cdf(double x, double df, double ncp)
{
    double lam2 = 0.5*ncp, w, sum=0.0, term; int j;
    if (x <= 0.0) return 0.0;
    if (ncp < 0.0) return 0.0/0.0;
    w = exp(-lam2); term = w;
    for (j = 0; j < 500; j++) {
        double a = 0.5*df + (double)j;
        sum += term * gsv_gammp(a, 0.5*x);
        term *= lam2/(double)(j+1);   /* prossimo peso Poisson */
        if (term < 1e-16 && (double)j > lam2) break;
    }
    if (sum > 1.0) sum = 1.0;
    return sum;
}
