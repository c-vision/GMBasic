/*
 * gm_interp.c — interpolazione e integrazione (wrapper del legacy GMBasic).
 */
#include "gm_interp.h"
#include <math.h>

extern double gm_interp_linear(const double *, const double *, int, double);
extern double gm_interp_cubic_spline(const double *, const double *, int, double);
extern double gm_interp_pchip(const double *, const double *, int, double);
extern double gm_interp_akima(const double *, const double *, int, double);
extern double gm_interp_polynomial(const double *, const double *, int, double);
extern double gm_interp_step(const double *, const double *, int, double);
extern double gm_integrate_trapezoid(const double *, const double *, int);
extern double *gmi_cumtrapezoid(const double *, const double *, int, long *);


gm_status gm_integrate_cumtrapezoid(const gm_real *y, const gm_real *x, int n, gm_real *out)
{
    long outn = 0;
    double *r;
    int i;
    if (!y || !x || !out || n < 1) return GM_ERR_ARG;
    r = gmi_cumtrapezoid(y, x, n, &outn);
    if (!r) return GM_ERR_INTERNAL;
    for (i = 0; i < n && i < (int)outn; i++) out[i] = r[i];
    /* il legacy alloca con malloc; liberiamo per evitare leak (free via libc) */
    { extern void free(void *); free(r); }
    return GM_OK;
}

gm_real gm_interp2_bilinear(const gm_real *x, int nx, const gm_real *y, int ny,
                            const gm_real *f, gm_real px, gm_real py)
{
    int i, j;
    gm_real t, u, f00, f10, f01, f11;
    if (!x || !y || !f || nx < 2 || ny < 2) return NAN;
    if (px <= x[0]) { i = 0; t = 0.0; }
    else if (px >= x[nx - 1]) { i = nx - 2; t = 1.0; }
    else {
        for (i = 0; i < nx - 1; i++) if (px <= x[i + 1]) break;
        t = (px - x[i]) / (x[i + 1] - x[i]);
    }
    if (py <= y[0]) { j = 0; u = 0.0; }
    else if (py >= y[ny - 1]) { j = ny - 2; u = 1.0; }
    else {
        for (j = 0; j < ny - 1; j++) if (py <= y[j + 1]) break;
        u = (py - y[j]) / (y[j + 1] - y[j]);
    }
    f00 = f[i * ny + j];
    f10 = f[(i + 1) * ny + j];
    f01 = f[i * ny + j + 1];
    f11 = f[(i + 1) * ny + j + 1];
    return (1.0 - t) * (1.0 - u) * f00 + t * (1.0 - u) * f10
           + (1.0 - t) * u * f01 + t * u * f11;
}

/* Hagan-West Monotone Convex (da IRCE #223): interpolation of discount factors
 * from pillar zero rates. Discrete forwards g_i = (z_{i+1} t_{i+1} - z_i t_i)/h_i;
 * pillar instantaneous forwards via weighted harmonic average with monotone
 * clamp; quadratic forward within each interval; analytic integral. */
gm_real gm_interp_monotone_convex(const gm_real *t, const gm_real *z, int n,
                                  gm_real tx, gm_real *fwd_out)
{
    gm_real g[128], f[128], h, x, integral;
    int i, lo, hi, idx;
    if (!t || !z || n < 2 || n > 128) return 0.0 / 0.0;
    for (i = 0; i < n - 1; i++) if (!(t[i + 1] > t[i])) return 0.0 / 0.0;
    for (i = 0; i < n - 1; i++) {
        h = t[i + 1] - t[i];
        g[i] = (z[i + 1] * t[i + 1] - z[i] * t[i]) / h;
    }
    g[n - 1] = g[n - 2];
    f[0] = g[0];
    f[n - 1] = g[n - 2];
    for (i = 1; i < n - 1; i++) {
        double hp = t[i] - t[i - 1], hn = t[i + 1] - t[i];
        double w1 = 2.0 * hn + hp, w2 = hn + 2.0 * hp;
        double fv;
        if (g[i - 1] * g[i] <= 0.0) {
            fv = 0.0;
        } else {
            fv = (w1 + w2) / (w1 / g[i - 1] + w2 / g[i]);
            {
                double hi = 2.0 * (g[i - 1] < g[i] ? g[i - 1] : g[i]);
                if (fv < 0.0) fv = 0.0;
                else if (fv > hi) fv = hi;
            }
        }
        f[i] = fv;
    }
    if (tx <= 0.0) return 1.0;
    if (tx <= t[0]) {
        if (fwd_out) *fwd_out = z[0];
        return exp(-z[0] * tx);
    }
    if (tx >= t[n - 1]) {
        if (fwd_out) *fwd_out = f[n - 1];
        return exp(-z[n - 1] * t[n - 1]) * exp(-f[n - 1] * (tx - t[n - 1]));
    }
    lo = 0; hi = n - 2;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (t[mid] <= tx) lo = mid;
        else hi = mid - 1;
    }
    idx = lo;
    h = t[idx + 1] - t[idx];
    x = (tx - t[idx]) / h;
    integral = g[idx] * (tx - t[idx])
             + (f[idx] - g[idx]) * h * (x - 2.0 * x * x + x * x * x)
             + (f[idx + 1] - g[idx]) * h * (-x * x + x * x * x);
    if (fwd_out)
        *fwd_out = g[idx] + (f[idx] - g[idx]) * (1.0 - 4.0 * x + 3.0 * x * x)
                          + (f[idx + 1] - g[idx]) * (-2.0 * x + 3.0 * x * x);
    return exp(-z[idx] * t[idx]) * exp(-integral);
}
