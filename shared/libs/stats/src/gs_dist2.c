/* gs_dist2.c — distribuzioni non centrali e quantili (riscrittura). */
#include "gs_dist2.h"
#include "gm_special.h"

#include <math.h>

#define DIST2_PI 3.14159265358979323846
#define DIST2_SQRT2 1.41421356237309504880

/* cdf/pdf centrali (gmi_math) */
extern double gm_special_gammainc(double a, double x);
extern double gm_special_betainc(double a, double b, double x);
extern double gm_special_lgamma(double x);
extern double gs_dist_chi2_pdf(double df, double x);
extern double gs_dist_chi2_cdf(double df, double x);
extern double gs_dist_beta_pdf(double a, double b, double x);
extern double gs_dist_beta_cdf(double a, double b, double x);
extern double gs_dist_f_pdf(double d1, double d2, double x);
extern double gs_dist_f_cdf(double d1, double d2, double x);
extern double gs_dist_student_t_pdf(double df, double x);
extern double gs_dist_student_t_cdf(double df, double x);

gc_real gs_dist2_chi2_cdf(gc_real x, gc_real df)
{
    if (x <= 0.0) return 0.0;
    return gm_special_gammainc(df / 2.0, x / 2.0);
}

gc_real gs_dist2_beta_cdf(gc_real x, gc_real a, gc_real b)
{
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    return gm_special_betainc(a, b, x);
}

/* --- inversione numerica della cdf su [lo,hi] con espansione del bracket --- */

typedef gc_real (*dist2_cdf_fn)(gc_real, const void *);

static gc_real dist2_invert(dist2_cdf_fn cdf, const void *ctx, gc_real p,
                            gc_real lo, gc_real hi)
{
    int i;
    if (p <= 0.0) return lo;
    if (p >= 1.0) return hi;
    for (i = 0; i < 200 && cdf(lo, ctx) > p; i++) { hi = lo; lo = lo * 2.0 - 1.0; }
    for (i = 0; i < 200 && cdf(hi, ctx) < p; i++) { lo = hi; hi = hi * 2.0 + 1.0; }
    for (i = 0; i < 400; i++) {
        gc_real mid = 0.5 * (lo + hi);
        if (cdf(mid, ctx) < p) lo = mid; else hi = mid;
        if (hi - lo <= 1e-13 * (1.0 + fabs(mid))) break;
    }
    return 0.5 * (lo + hi);
}

/* ==================== chi-quadro non centrale ==================== */

gc_real gs_dist2_nc_chi2_pdf(gc_real x, gc_real df, gc_real nc)
{
    if (df <= 0.0 || nc < 0.0) return NAN;
    if (nc == 0.0) return gs_dist_chi2_pdf(df, x);
    if (x < 0.0) return 0.0;
    if (x == 0.0) {
        if (df > 2.0) return 0.0;
        if (df == 2.0) return 0.5 * exp(-0.5 * nc);
        return INFINITY;
    }
    gc_real z = sqrt(nc * x);
    gc_real v = 0.5 * df - 1.0;
    gc_real logf = -0.5 * (x + nc) + (0.25 * df - 0.5) * (log(x) - log(nc));
    gc_real b = gm_special_besseli(v, z);
    if (!isfinite(b)) return b;
    return 0.5 * exp(logf) * b;
}

gc_real gs_dist2_nc_chi2_cdf(gc_real x, gc_real df, gc_real nc)
{
    gc_real half = nc / 2.0;
    gc_real w = exp(-half);
    gc_real sum = 0.0;
    int j;
    if (x <= 0.0) return 0.0;
    if (nc == 0.0) return gs_dist2_chi2_cdf(x, df);
    for (j = 0; j < 300; j++) {
        sum += w * gs_dist2_chi2_cdf(x, df + 2.0 * j);
        w *= half / (gc_real)(j + 1);
        if (w < 1e-18 && (gc_real)j > half) break;
    }
    return sum > 1.0 ? 1.0 : sum;
}

static gc_real cdf_nc_chi2(gc_real x, const void *ctx)
{
    const gc_real *c = (const gc_real *)ctx;
    return gs_dist2_nc_chi2_cdf(x, c[0], c[1]);
}

gc_real gs_dist2_nc_chi2_ppf(gc_real p, gc_real df, gc_real nc)
{
    gc_real c[2]; c[0] = df; c[1] = nc;
    return dist2_invert(cdf_nc_chi2, c, p, 0.0, 1.0);
}
gc_real gs_dist2_nc_chi2_isf(gc_real p, gc_real df, gc_real nc)
{
    return gs_dist2_nc_chi2_ppf(1.0 - p, df, nc);
}

/* ==================== gamma non centrale (scale 1) ==================== */

gc_real gs_dist2_nc_gamma_pdf(gc_real x, gc_real a, gc_real d)
{
    /* Y ~ nc chi2(2a, d), X = Y/2 -> f_X(x) = 2 f_Y(2x). */
    if (x < 0.0) return 0.0;
    return 2.0 * gs_dist2_nc_chi2_pdf(2.0 * x, 2.0 * a, d);
}

gc_real gs_dist2_nc_gamma_cdf(gc_real x, gc_real a, gc_real d)
{
    /* chi2 non centrale scalata: nc gamma(a, 1, d) = nc chi2(2a, d)/2 */
    return gs_dist2_nc_chi2_cdf(2.0 * x, 2.0 * a, d);
}

static gc_real cdf_nc_gamma(gc_real x, const void *ctx)
{
    const gc_real *c = (const gc_real *)ctx;
    return gs_dist2_nc_gamma_cdf(x, c[0], c[1]);
}

gc_real gs_dist2_nc_gamma_ppf(gc_real p, gc_real a, gc_real d)
{
    gc_real c[2]; c[0] = a; c[1] = d;
    return dist2_invert(cdf_nc_gamma, c, p, 0.0, 1.0);
}
gc_real gs_dist2_nc_gamma_isf(gc_real p, gc_real a, gc_real d)
{
    return gs_dist2_nc_gamma_ppf(1.0 - p, a, d);
}

/* ==================== beta non centrale ==================== */

gc_real gs_dist2_nc_beta_pdf(gc_real x, gc_real a, gc_real b, gc_real d)
{
    gc_real half = d / 2.0;
    gc_real w = exp(-half);
    gc_real sum = 0.0;
    int j;
    if (x < 0.0 || x > 1.0) return 0.0;
    if (d == 0.0) return gs_dist_beta_pdf(a, b, x);
    for (j = 0; j < 300; j++) {
        sum += w * gs_dist_beta_pdf(a + (gc_real)j, b, x);
        w *= half / (gc_real)(j + 1);
        if (w < 1e-18 && (gc_real)j > half) break;
    }
    return sum;
}

gc_real gs_dist2_nc_beta_cdf(gc_real x, gc_real a, gc_real b, gc_real d)
{
    gc_real half = d / 2.0;
    gc_real w = exp(-half);
    gc_real sum = 0.0;
    int j;
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    if (d == 0.0) return gs_dist2_beta_cdf(x, a, b);
    for (j = 0; j < 300; j++) {
        sum += w * gs_dist2_beta_cdf(x, a + (gc_real)j, b);
        w *= half / (gc_real)(j + 1);
        if (w < 1e-18 && (gc_real)j > half) break;
    }
    return sum > 1.0 ? 1.0 : sum;
}

static gc_real cdf_nc_beta(gc_real x, const void *ctx)
{
    const gc_real *c = (const gc_real *)ctx;
    return gs_dist2_nc_beta_cdf(x, c[0], c[1], c[2]);
}

gc_real gs_dist2_nc_beta_ppf(gc_real p, gc_real a, gc_real b, gc_real d)
{
    gc_real c[3]; c[0] = a; c[1] = b; c[2] = d;
    return dist2_invert(cdf_nc_beta, c, p, 0.0, 1.0);
}
gc_real gs_dist2_nc_beta_isf(gc_real p, gc_real a, gc_real b, gc_real d)
{
    return gs_dist2_nc_beta_ppf(1.0 - p, a, b, d);
}

/* ==================== F non centrale (via beta non centrale) ==================== */
/* Se F ~ ncf(d1,d2,nc) allora Y = d1 F/(d1 F + d2) ~ ncbeta(d1/2, d2/2, nc). */

gc_real gs_dist2_nc_f_cdf(gc_real x, gc_real d1, gc_real d2, gc_real nc)
{
    gc_real y;
    if (x <= 0.0) return 0.0;
    y = d1 * x / (d1 * x + d2);
    return gs_dist2_nc_beta_cdf(y, d1 / 2.0, d2 / 2.0, nc);
}

gc_real gs_dist2_nc_f_pdf(gc_real x, gc_real d1, gc_real d2, gc_real nc)
{
    gc_real y, dydx, half, w, sum;
    int j;
    if (x <= 0.0) return 0.0;
    y = d1 * x / (d1 * x + d2);
    dydx = d1 * d2 / ((d1 * x + d2) * (d1 * x + d2));
    half = nc / 2.0;
    w = exp(-half);
    sum = 0.0;
    for (j = 0; j < 400; j++) {
        sum += w * gs_dist_beta_pdf(d1 / 2.0 + (gc_real)j, d2 / 2.0, y);
        if ((gc_real)j > half && w < 1e-18) break;
        w *= half / (gc_real)(j + 1);
    }
    return sum * dydx;
}

static gc_real cdf_nc_f(gc_real x, const void *ctx)
{
    const gc_real *c = (const gc_real *)ctx;
    return gs_dist2_nc_f_cdf(x, c[0], c[1], c[2]);
}

gc_real gs_dist2_nc_f_ppf(gc_real p, gc_real d1, gc_real d2, gc_real nc)
{
    gc_real c[3]; c[0] = d1; c[1] = d2; c[2] = nc;
    return dist2_invert(cdf_nc_f, c, p, 0.0, 1.0);
}
gc_real gs_dist2_nc_f_isf(gc_real p, gc_real d1, gc_real d2, gc_real nc)
{
    return gs_dist2_nc_f_ppf(1.0 - p, d1, d2, nc);
}

/* ==================== t di Student non centrale (serie di Lenth) ==================== */

/* F~_{df,mu}(x) per x >= 0 (Lenth 1989): Phi(-mu) + 1/2 sum_j [p_j I_y(j+1/2,df/2)
 * + q_j I_y(j+1,df/2)], con y = x^2/(x^2+df), p_j = e^{-h} h^j/j!,
 * q_j = mu/(sqrt(2) Gamma(j+3/2)) e^{-h} h^j, h = mu^2/2. */
static gc_real nc_t_ftilde(gc_real df, gc_real mu, gc_real x)
{
    gc_real phi = 0.5 * erfc(mu / DIST2_SQRT2);
    gc_real half = 0.5 * mu * mu;
    gc_real y = (x * x) / (x * x + df);
    gc_real powr = exp(-half); /* e^{-h} h^j (senza 1/j!) */
    gc_real sum = 0.0;
    int j;
    for (j = 0; j < 400; j++) {
        gc_real pj = powr / exp(gm_special_lgamma((gc_real)j + 1.0));
        gc_real qj = mu / (DIST2_SQRT2 * exp(gm_special_lgamma((gc_real)j + 1.5))) * powr;
        sum += pj * gm_special_betainc((gc_real)j + 0.5, df / 2.0, y)
             + qj * gm_special_betainc((gc_real)j + 1.0, df / 2.0, y);
        if ((gc_real)j > half && fabs(pj) + fabs(qj) < 1e-18) break;
        powr *= half;
    }
    return phi + 0.5 * sum;
}

gc_real gs_dist2_nc_t_cdf(gc_real x, gc_real df, gc_real nc)
{
    if (df <= 0.0) return NAN;
    if (nc == 0.0) return gs_dist_student_t_cdf(df, x);
    if (x < 0.0) return 1.0 - nc_t_ftilde(df, -nc, -x);
    return nc_t_ftilde(df, nc, x);
}

gc_real gs_dist2_nc_t_pdf(gc_real x, gc_real df, gc_real nc)
{
    if (df <= 0.0) return NAN;
    if (nc == 0.0) return gs_dist_student_t_pdf(df, x);
    if (x == 0.0) {
        /* pdf(0) = Gamma((v+1)/2) e^{-d^2/2} / (sqrt(pi v) Gamma(v/2)) */
        return exp(gm_special_lgamma(0.5 * (df + 1.0)) - gm_special_lgamma(0.5 * df)
                   - 0.5 * nc * nc) / sqrt(DIST2_PI * df);
    }
    /* forma di R: (v/x){F_{v+2,mu}(x sqrt(1+2/v)) - F_{v,mu}(x)} */
    return (df / x) * (gs_dist2_nc_t_cdf(x * sqrt(1.0 + 2.0 / df), df + 2.0, nc)
                       - gs_dist2_nc_t_cdf(x, df, nc));
}

static gc_real cdf_nc_t(gc_real x, const void *ctx)
{
    const gc_real *c = (const gc_real *)ctx;
    return gs_dist2_nc_t_cdf(x, c[0], c[1]);
}

gc_real gs_dist2_nc_t_ppf(gc_real p, gc_real df, gc_real nc)
{
    gc_real c[2]; c[0] = df; c[1] = nc;
    return dist2_invert(cdf_nc_t, c, p, -1.0, 1.0);
}
gc_real gs_dist2_nc_t_isf(gc_real p, gc_real df, gc_real nc)
{
    return gs_dist2_nc_t_ppf(1.0 - p, df, nc);
}

/* ==================== quantili centrali ==================== */

typedef struct { gc_real a, b; } dist2_ctx;

static gc_real cdf_gamma(gc_real x, const void *ctx)
{
    const dist2_ctx *c = (const dist2_ctx *)ctx;
    if (x <= 0.0) return 0.0;
    return gm_special_gammainc(c->a, x);
}
static gc_real cdf_beta(gc_real x, const void *ctx)
{
    const dist2_ctx *c = (const dist2_ctx *)ctx;
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    return gm_special_betainc(c->a, c->b, x);
}
static gc_real cdf_normal(gc_real x, const void *ctx)
{
    (void)ctx;
    return 0.5 * erfc(-x / DIST2_SQRT2);
}
static gc_real cdf_chi2(gc_real x, const void *ctx)
{
    const dist2_ctx *c = (const dist2_ctx *)ctx;
    return gs_dist2_chi2_cdf(x, c->a);
}

gc_real gs_dist2_normal_quantile(gc_real p)
{
    return dist2_invert(cdf_normal, NULL, p, -1.0, 1.0);
}
gc_real gs_dist2_gamma_quantile(gc_real p, gc_real a)
{
    dist2_ctx c; c.a = a; c.b = 0.0;
    return dist2_invert(cdf_gamma, &c, p, 0.0, 1.0);
}
gc_real gs_dist2_beta_quantile(gc_real p, gc_real a, gc_real b)
{
    dist2_ctx c; c.a = a; c.b = b;
    return dist2_invert(cdf_beta, &c, p, 0.0, 1.0);
}
gc_real gs_dist2_chi2_quantile(gc_real p, gc_real df)
{
    dist2_ctx c; c.a = df; c.b = 0.0;
    return dist2_invert(cdf_chi2, &c, p, 0.0, 1.0);
}
