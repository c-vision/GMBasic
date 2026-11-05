/* gm_integrate.c — integrazione numerica e ODE (riscrittura). */
#include "gm_integrate.h"

#include <math.h>
#include <stdlib.h>

gm_real gm_integrate_simpson(gm_real (*f)(gm_real, void *), void *ctx,
                             gm_real a, gm_real b, int n)
{
    gm_real h, s;
    int i;
    if (f == NULL || n <= 0) return 0.0;
    if (n % 2 == 1) n++;
    h = (b - a) / (gm_real)n;
    s = f(a, ctx) + f(b, ctx);
    for (i = 1; i < n; i++)
        s += (i % 2 == 0 ? 2.0 : 4.0) * f(a + h * (gm_real)i, ctx);
    return s * h / 3.0;
}

static gm_real adaptive_rec(gm_real (*f)(gm_real, void *), void *ctx,
                            gm_real a, gm_real b, gm_real fa, gm_real fb, gm_real fm,
                            gm_real whole, gm_real tol, int depth)
{
    gm_real m = 0.5 * (a + b);
    gm_real lm = 0.5 * (a + m), rm = 0.5 * (m + b);
    gm_real flm = f(lm, ctx), frm = f(rm, ctx);
    gm_real left = (m - a) / 6.0 * (fa + 4.0 * flm + fm);
    gm_real right = (b - m) / 6.0 * (fm + 4.0 * frm + fb);
    gm_real delta = left + right - whole;
    if (depth <= 0 || fabs(delta) <= 15.0 * tol)
        return left + right + delta / 15.0;
    return adaptive_rec(f, ctx, a, m, fa, fm, flm, left, tol * 0.5, depth - 1)
         + adaptive_rec(f, ctx, m, b, fm, fb, frm, right, tol * 0.5, depth - 1);
}

gm_real gm_integrate_adaptive(gm_real (*f)(gm_real, void *), void *ctx,
                              gm_real a, gm_real b, gm_real tol)
{
    gm_real m = 0.5 * (a + b);
    gm_real fa, fb, fm, whole;
    if (f == NULL) return 0.0;
    fa = f(a, ctx); fb = f(b, ctx); fm = f(m, ctx);
    whole = (b - a) / 6.0 * (fa + 4.0 * fm + fb);
    return adaptive_rec(f, ctx, a, b, fa, fb, fm, whole, tol, 40);
}

gm_real gm_integrate_chebyshev(const gm_real *coef, int m)
{
    /* integrale su [-1,1]: contribuiscono solo i termini pari (2/(1-j^2)) */
    gm_real s = 0.0;
    int j;
    if (coef == NULL || m <= 0) return 0.0;
    for (j = 0; j < m; j++) {
        if (j % 2 == 0) {
            gm_real denom = 1.0 - (gm_real)j * (gm_real)j;
            if (denom != 0.0) s += coef[j] * 2.0 / denom;
        }
    }
    return s;
}

gm_status gm_ode_system(void (*f)(gm_real, const gm_real *, gm_real *, int, void *),
                        void *ctx, gm_real *y, int n, gm_real a, gm_real b, int steps)
{
    gm_real h;
    gm_real *k1, *k2, *k3, *k4, *tmp;
    int i, s;
    if (f == NULL || y == NULL || n <= 0 || steps <= 0) return GM_ERR_ARG;
    h = (b - a) / (gm_real)steps;
    k1 = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    k2 = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    k3 = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    k4 = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    tmp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!k1 || !k2 || !k3 || !k4 || !tmp) {
        free(k1); free(k2); free(k3); free(k4); free(tmp);
        return GM_ERR_NO_MEM;
    }
    for (s = 0; s < steps; s++) {
        gm_real t = a + h * (gm_real)s;
        f(t, y, k1, n, ctx);
        for (i = 0; i < n; i++) tmp[i] = y[i] + 0.5 * h * k1[i];
        f(t + 0.5 * h, tmp, k2, n, ctx);
        for (i = 0; i < n; i++) tmp[i] = y[i] + 0.5 * h * k2[i];
        f(t + 0.5 * h, tmp, k3, n, ctx);
        for (i = 0; i < n; i++) tmp[i] = y[i] + h * k3[i];
        f(t + h, tmp, k4, n, ctx);
        for (i = 0; i < n; i++)
            y[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
    free(k1); free(k2); free(k3); free(k4); free(tmp);
    return GM_OK;
}
