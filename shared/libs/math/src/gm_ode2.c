/* gm_ode2.c — ODE avanzati e BVP (riscrittura). */
#include "gm_ode2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void rk4_step(gm_ode_fn f, void *ctx, gm_real t, gm_real h, gm_real *y, int n)
{
    gm_real *k1 = malloc((size_t)n * sizeof(gm_real));
    gm_real *k2 = malloc((size_t)n * sizeof(gm_real));
    gm_real *k3 = malloc((size_t)n * sizeof(gm_real));
    gm_real *k4 = malloc((size_t)n * sizeof(gm_real));
    gm_real *tmp = malloc((size_t)n * sizeof(gm_real));
    int i;
    if (!k1 || !k2 || !k3 || !k4 || !tmp) { free(k1); free(k2); free(k3); free(k4); free(tmp); return; }
    f(t, y, k1, n, ctx);
    for (i = 0; i < n; i++) tmp[i] = y[i] + 0.5 * h * k1[i];
    f(t + 0.5 * h, tmp, k2, n, ctx);
    for (i = 0; i < n; i++) tmp[i] = y[i] + 0.5 * h * k2[i];
    f(t + 0.5 * h, tmp, k3, n, ctx);
    for (i = 0; i < n; i++) tmp[i] = y[i] + h * k3[i];
    f(t + h, tmp, k4, n, ctx);
    for (i = 0; i < n; i++) y[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    free(k1); free(k2); free(k3); free(k4); free(tmp);
}

gm_status gm_ode_adams_bashforth4(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                  gm_real a, gm_real b, int steps)
{
    gm_real h = (b - a) / (gm_real)steps;
    gm_real *fy = (gm_real *)malloc((size_t)n * 4 * sizeof(gm_real));
    gm_real *ysave = (gm_real *)malloc((size_t)n * 4 * sizeof(gm_real));
    int i, s;
    if (f == NULL || y == NULL || n <= 0 || steps < 4) { free(fy); free(ysave); return GM_ERR_ARG; }
    if (!fy || !ysave) { free(fy); free(ysave); return GM_ERR_NO_MEM; }
    /* avvia con 3 passi RK4 salvando f */
    for (s = 0; s < 3; s++) {
        f(a + h * s, y, fy + (size_t)(3 - s) * (size_t)n, n, ctx);
        memcpy(ysave + (size_t)(3 - s) * (size_t)n, y, (size_t)n * sizeof(gm_real));
        rk4_step(f, ctx, a + h * s, h, y, n);
    }
    f(a + 3.0 * h, y, fy, n, ctx);
    memcpy(ysave, y, (size_t)n * sizeof(gm_real));
    for (s = 3; s < steps; s++) {
        for (i = 0; i < n; i++)
            y[i] += h / 24.0 * (55.0 * fy[i] - 59.0 * fy[(size_t)n + i]
                  + 37.0 * fy[(size_t)2 * n + i] - 9.0 * fy[(size_t)3 * n + i]);
        for (i = 3; i > 0; i--) memcpy(fy + (size_t)i * (size_t)n, fy + (size_t)(i - 1) * (size_t)n, (size_t)n * sizeof(gm_real));
        f(a + (gm_real)(s + 1) * h, y, fy, n, ctx);
    }
    free(fy); free(ysave);
    return GM_OK;
}

gm_status gm_ode_adams_moulton4(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                gm_real a, gm_real b, int steps)
{
    gm_real h = (b - a) / (gm_real)steps;
    gm_real *fy = (gm_real *)malloc((size_t)n * 4 * sizeof(gm_real));
    gm_real *yp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *fp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i, s;
    if (f == NULL || y == NULL || n <= 0 || steps < 4) { free(fy); free(yp); free(fp); return GM_ERR_ARG; }
    if (!fy || !yp || !fp) { free(fy); free(yp); free(fp); return GM_ERR_NO_MEM; }
    for (s = 0; s < 3; s++) {
        f(a + h * s, y, fy + (size_t)(3 - s) * (size_t)n, n, ctx);
        rk4_step(f, ctx, a + h * s, h, y, n);
    }
    f(a + 3.0 * h, y, fy, n, ctx);
    for (s = 3; s < steps; s++) {
        /* predittore AB4 */
        for (i = 0; i < n; i++)
            yp[i] = y[i] + h / 24.0 * (55.0 * fy[i] - 59.0 * fy[(size_t)n + i]
                  + 37.0 * fy[(size_t)2 * n + i] - 9.0 * fy[(size_t)3 * n + i]);
        f(a + (gm_real)(s + 1) * h, yp, fp, n, ctx);
        /* correttore AM4 */
        for (i = 0; i < n; i++)
            y[i] += h / 24.0 * (9.0 * fp[i] + 19.0 * fy[i]
                  - 5.0 * fy[(size_t)n + i] + fy[(size_t)2 * n + i]);
        for (i = 3; i > 0; i--) memcpy(fy + (size_t)i * (size_t)n, fy + (size_t)(i - 1) * (size_t)n, (size_t)n * sizeof(gm_real));
        f(a + (gm_real)(s + 1) * h, y, fy, n, ctx);
    }
    free(fy); free(yp); free(fp);
    return GM_OK;
}

gm_status gm_ode_bdf2(gm_ode_fn f, void *ctx, gm_real *y, int n,
                      gm_real a, gm_real b, int steps, int maxit, gm_real tol)
{
    gm_real h = (b - a) / (gm_real)steps;
    gm_real *y0 = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *yn = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *fn = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i, s, k;
    if (f == NULL || y == NULL || n <= 0 || steps < 2) { free(y0); free(yn); free(fn); return GM_ERR_ARG; }
    if (!y0 || !yn || !fn) { free(y0); free(yn); free(fn); return GM_ERR_NO_MEM; }
    memcpy(y0, y, (size_t)n * sizeof(gm_real));
    rk4_step(f, ctx, a, h, y, n); /* primo passo */
    for (s = 1; s < steps; s++) {
        memcpy(yn, y, (size_t)n * sizeof(gm_real));
        for (k = 0; k < maxit; k++) {
            f(a + (gm_real)(s + 1) * h, yn, fn, n, ctx);
            for (i = 0; i < n; i++) {
                gm_real g = yn[i] - (4.0 / 3.0) * y[i] + (1.0 / 3.0) * y0[i] - (2.0 / 3.0) * h * fn[i];
                yn[i] -= 0.5 * g; /* punto fisso smorzato */
            }
            if (k == maxit - 1) break;
        }
        memcpy(y0, y, (size_t)n * sizeof(gm_real));
        memcpy(y, yn, (size_t)n * sizeof(gm_real));
    }
    free(y0); free(yn); free(fn);
    (void)tol;
    return GM_OK;
}

gm_status gm_ode_implicit_midpoint(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                   gm_real a, gm_real b, int steps, int maxit, gm_real tol)
{
    gm_real h = (b - a) / (gm_real)steps;
    gm_real *ym = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *fm = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *yn = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i, s, k;
    if (f == NULL || y == NULL || n <= 0 || steps <= 0) { free(ym); free(fm); free(yn); return GM_ERR_ARG; }
    if (!ym || !fm || !yn) { free(ym); free(fm); free(yn); return GM_ERR_NO_MEM; }
    for (s = 0; s < steps; s++) {
        for (i = 0; i < n; i++) yn[i] = y[i]; /* guess */
        for (k = 0; k < maxit; k++) {
            for (i = 0; i < n; i++) ym[i] = 0.5 * (y[i] + yn[i]);
            f(a + ((gm_real)s + 0.5) * h, ym, fm, n, ctx);
            { gm_real diff = 0.0; for (i = 0; i < n; i++) { gm_real nn = y[i] + h * fm[i]; gm_real d = nn - yn[i]; diff += d * d; yn[i] = nn; } if (sqrt(diff) <= tol) break; }
        }
        memcpy(y, yn, (size_t)n * sizeof(gm_real));
    }
    free(ym); free(fm); free(yn);
    return GM_OK;
}

gm_status gm_ode_symplectic_euler(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                  gm_real a, gm_real b, int steps)
{
    gm_real h = (b - a) / (gm_real)steps;
    gm_real *dy = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i, s;
    if (f == NULL || y == NULL || n <= 0) { free(dy); return GM_ERR_ARG; }
    if (!dy) return GM_ERR_NO_MEM;
    for (s = 0; s < steps; s++) {
        f(a + h * s, y, dy, n, ctx);
        for (i = 0; i < n; i++) y[i] += h * dy[i];
    }
    free(dy);
    return GM_OK;
}

gm_status gm_ode_verlet(gm_ode_fn acc, void *ctx, gm_real *y, int n,
                        gm_real a, gm_real b, int steps)
{
    /* y = (pos[0..n/2-1], vel[n/2..n-1]); acc calcola le accelerazioni da pos. */
    int m = n / 2, i, s;
    gm_real h = (b - a) / (gm_real)steps;
    gm_real *accv = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    gm_real *accn = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    if (acc == NULL || y == NULL || n < 2 || n % 2 != 0) { free(accv); free(accn); return GM_ERR_ARG; }
    if (!accv || !accn) { free(accv); free(accn); return GM_ERR_NO_MEM; }
    acc(a, y, accv, m, ctx);
    for (s = 0; s < steps; s++) {
        for (i = 0; i < m; i++) {
            y[i] += h * y[m + i] + 0.5 * h * h * accv[i];
            y[m + i] += 0.5 * h * accv[i];
        }
        acc(a + (gm_real)(s + 1) * h, y, accn, m, ctx);
        for (i = 0; i < m; i++) y[m + i] += 0.5 * h * accn[i];
        memcpy(accv, accn, (size_t)m * sizeof(gm_real));
    }
    free(accv); free(accn);
    return GM_OK;
}

/* --- BVP --- */

/* Integra il sistema (y, y') con RK4 da a a b, riempiendo y[0..n-1]. */
static void bvp_integrate(void (*f)(gm_real, gm_real, gm_real, gm_real *, void *), void *ctx,
                          gm_real a, gm_real b, gm_real alpha, gm_real slope, int n, gm_real *y)
{
    gm_real h = (b - a) / (gm_real)(n - 1);
    gm_real yy = alpha, vv = slope, d;
    int i;
    y[0] = yy;
    for (i = 0; i < n - 1; i++) {
        gm_real x = a + h * (gm_real)i;
        gm_real k1y, k1v, k2y, k2v, k3y, k3v, k4y, k4v;
        k1y = vv; f(x, yy, vv, &d, ctx); k1v = d;
        k2y = vv + 0.5 * h * k1v; f(x + 0.5 * h, yy + 0.5 * h * k1y, vv + 0.5 * h * k1v, &d, ctx); k2v = d;
        k3y = vv + 0.5 * h * k2v; f(x + 0.5 * h, yy + 0.5 * h * k2y, vv + 0.5 * h * k2v, &d, ctx); k3v = d;
        k4y = vv + h * k3v;       f(x + h, yy + h * k3y, vv + h * k3v, &d, ctx); k4v = d;
        yy += h / 6.0 * (k1y + 2.0 * k2y + 2.0 * k3y + k4y);
        vv += h / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
        y[i + 1] = yy;
    }
}

static void bvp_fd_sweep(void (*f)(gm_real, gm_real, gm_real, gm_real *, void *), void *ctx,
                         gm_real a, gm_real b, gm_real alpha, gm_real beta, int n, gm_real *y)
{
    gm_real h = (b - a) / (gm_real)(n - 1);
    int i;
    y[0] = alpha; y[n - 1] = beta;
    for (i = 1; i < n - 1; i++) {
        gm_real fp;
        f(a + h * (gm_real)i, y[i], (y[i + 1] - y[i - 1]) / (2.0 * h), &fp, ctx);
        y[i] = 0.5 * (y[i - 1] + y[i + 1] - h * h * fp);
    }
}

gm_status gm_bvp_fd(void (*f)(gm_real, gm_real, gm_real, gm_real *, void *),
                    void *ctx, gm_real a, gm_real b, gm_real alpha, gm_real beta,
                    int n, gm_real *y)
{
    int iter, i;
    if (f == NULL || y == NULL || n < 3) return GM_ERR_ARG;
    for (i = 1; i < n - 1; i++) y[i] = alpha + (beta - alpha) * (gm_real)i / (gm_real)(n - 1);
    for (iter = 0; iter < 20000; iter++) bvp_fd_sweep(f, ctx, a, b, alpha, beta, n, y);
    return GM_OK;
}

gm_status gm_bvp_shooting(void (*f)(gm_real, gm_real, gm_real, gm_real *, void *),
                          void *ctx, gm_real a, gm_real b, gm_real alpha, gm_real beta,
                          int n, gm_real *y)
{
    gm_real s0 = 0.0, s1 = 1.0, yb0, yb1;
    gm_real *yt;
    int iter;
    if (f == NULL || y == NULL || n < 3) return GM_ERR_ARG;
    yt = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!yt) return GM_ERR_NO_MEM;
    bvp_integrate(f, ctx, a, b, alpha, s0, n, yt); yb0 = yt[n - 1];
    bvp_integrate(f, ctx, a, b, alpha, s1, n, yt); yb1 = yt[n - 1];
    for (iter = 0; iter < 100; iter++) {
        gm_real denom = yb1 - yb0, s2;
        if (fabs(yb1 - beta) < 1e-13 || denom == 0.0) break;
        s2 = s1 - (yb1 - beta) * (s1 - s0) / denom;
        s0 = s1; yb0 = yb1; s1 = s2;
        bvp_integrate(f, ctx, a, b, alpha, s1, n, yt); yb1 = yt[n - 1];
    }
    bvp_integrate(f, ctx, a, b, alpha, s1, n, y);
    free(yt);
    return GM_OK;
}
