/* gm_deriv.c — differenziazione numerica (riscrittura). */
#include "gm_deriv.h"

#include <stdlib.h>
#include <string.h>

gm_real gm_deriv_forward(gm_fn1 f, void *ctx, gm_real x, gm_real h)
{
    return (f(x + h, ctx) - f(x, ctx)) / h;
}

gm_real gm_deriv_backward(gm_fn1 f, void *ctx, gm_real x, gm_real h)
{
    return (f(x, ctx) - f(x - h, ctx)) / h;
}

gm_real gm_deriv_central(gm_fn1 f, void *ctx, gm_real x, gm_real h)
{
    return (f(x + h, ctx) - f(x - h, ctx)) / (2.0 * h);
}

gm_real gm_deriv_second(gm_fn1 f, void *ctx, gm_real x, gm_real h)
{
    return (f(x + h, ctx) - 2.0 * f(x, ctx) + f(x - h, ctx)) / (h * h);
}

gm_real gm_deriv_richardson(gm_fn1 f, void *ctx, gm_real x, gm_real h, int levels)
{
    gm_real *R;
    int k, i;
    if (levels < 1) return 0.0;
    R = (gm_real *)malloc((size_t)levels * sizeof(gm_real));
    if (!R) return 0.0;
    for (k = 0; k < levels; k++) {
        gm_real hk = h / (gm_real)(1 << k);
        R[k] = gm_deriv_central(f, ctx, x, hk);
    }
    for (k = 1; k < levels; k++) {
        gm_real p = 4.0;
        for (i = levels - 1; i >= k; i--) {
            R[i] = R[i] + (R[i] - R[i - 1]) / (p - 1.0);
        }
    }
    {
        gm_real res = R[levels - 1];
        free(R);
        return res;
    }
}

void gm_grad_fd(gm_real (*f)(const gm_real *, int, void *), void *ctx,
                const gm_real *x, int n, gm_real *g, gm_real h)
{
    gm_real *xp;
    int i;
    if (f == NULL || x == NULL || g == NULL || n <= 0) return;
    xp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!xp) return;
    memcpy(xp, x, (size_t)n * sizeof(gm_real));
    for (i = 0; i < n; i++) {
        gm_real fp, fm;
        xp[i] = x[i] + h; fp = f(xp, n, ctx);
        xp[i] = x[i] - h; fm = f(xp, n, ctx);
        g[i] = (fp - fm) / (2.0 * h);
        xp[i] = x[i];
    }
    free(xp);
}

void gm_hessian_fd(gm_real (*f)(const gm_real *, int, void *), void *ctx,
                   const gm_real *x, int n, gm_real *H, gm_real h)
{
    gm_real *xp;
    int i, j;
    if (f == NULL || x == NULL || H == NULL || n <= 0) return;
    xp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!xp) return;
    memcpy(xp, x, (size_t)n * sizeof(gm_real));
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (i == j) {
                gm_real fp, f0, fm;
                xp[i] = x[i] + h; fp = f(xp, n, ctx);
                xp[i] = x[i];     f0 = f(xp, n, ctx);
                xp[i] = x[i] - h; fm = f(xp, n, ctx);
                H[(size_t)i * (size_t)n + (size_t)j] = (fp - 2.0 * f0 + fm) / (h * h);
                xp[i] = x[i];
            } else {
                gm_real fpp, fpm, fmp, fmm;
                xp[i] = x[i] + h; xp[j] = x[j] + h; fpp = f(xp, n, ctx);
                xp[j] = x[j] - h; fpm = f(xp, n, ctx);
                xp[i] = x[i] - h; fmm = f(xp, n, ctx);
                xp[j] = x[j] + h; fmp = f(xp, n, ctx);
                H[(size_t)i * (size_t)n + (size_t)j] = (fpp - fpm - fmp + fmm) / (4.0 * h * h);
                xp[i] = x[i]; xp[j] = x[j];
            }
        }
    }
    free(xp);
}
