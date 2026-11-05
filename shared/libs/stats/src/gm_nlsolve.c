/* gm_nlsolve.c — sistemi non lineari (riscrittura). */
#include "gm_nlsolve.h"
#include "gm_linalg.h"
#include "gc_array.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Jacobiano per differenze finite (n×n row-major). */
static void fd_jacobian(gm_nl_fn f, void *ctx, const gm_real *x, gm_real *J, int n)
{
    gm_real *xp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *fx = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *fxp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i, j;
    if (!xp || !fx || !fxp) { free(xp); free(fx); free(fxp); return; }
    f(x, fx, n, ctx);
    for (j = 0; j < n; j++) {
        gm_real h = 1e-7 * (1.0 + fabs(x[j]));
        memcpy(xp, x, (size_t)n * sizeof(gm_real));
        xp[j] += h;
        f(xp, fxp, n, ctx);
        for (i = 0; i < n; i++) J[(size_t)i * (size_t)n + (size_t)j] = (fxp[i] - fx[i]) / h;
    }
    free(xp); free(fx); free(fxp);
}

gm_status gm_newton_system(gm_nl_fn f, void *ctx, gm_real *x, int n,
                           int maxit, gm_real tol, int *iters)
{
    gm_real *fx, *J, *dx;
    int k;
    if (f == NULL || x == NULL || n <= 0) return GM_ERR_ARG;
    fx = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    J = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    dx = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!fx || !J || !dx) { free(fx); free(J); free(dx); return GM_ERR_NO_MEM; }
    for (k = 0; k < maxit; k++) {
        f(x, fx, n, ctx);
        if (gc_array_norm2(fx, n) <= tol) { if (iters) *iters = k; free(fx); free(J); free(dx); return GM_OK; }
        fd_jacobian(f, ctx, x, J, n);
        if (gm_linalg_solve(n, J, fx, dx) != GM_OK) { if (iters) *iters = k; free(fx); free(J); free(dx); return GM_ERR_SINGULAR; }
        { int i; for (i = 0; i < n; i++) x[i] -= dx[i]; }
    }
    if (iters) *iters = maxit;
    free(fx); free(J); free(dx);
    return GM_ERR_NOCONV;
}

gm_status gm_newton_jacobian(gm_nl_fn f, void (*Jf)(const gm_real *, gm_real *, int, void *),
                             void *ctx, gm_real *x, int n, int maxit, gm_real tol, int *iters)
{
    gm_real *fx, *J, *dx;
    int k;
    if (f == NULL || Jf == NULL || x == NULL || n <= 0) return GM_ERR_ARG;
    fx = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    J = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    dx = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!fx || !J || !dx) { free(fx); free(J); free(dx); return GM_ERR_NO_MEM; }
    for (k = 0; k < maxit; k++) {
        f(x, fx, n, ctx);
        if (gc_array_norm2(fx, n) <= tol) { if (iters) *iters = k; free(fx); free(J); free(dx); return GM_OK; }
        Jf(x, J, n, ctx);
        if (gm_linalg_solve(n, J, fx, dx) != GM_OK) { if (iters) *iters = k; free(fx); free(J); free(dx); return GM_ERR_SINGULAR; }
        { int i; for (i = 0; i < n; i++) x[i] -= dx[i]; }
    }
    if (iters) *iters = maxit;
    free(fx); free(J); free(dx);
    return GM_ERR_NOCONV;
}

gm_status gm_broyden_system(gm_nl_fn f, void *ctx, gm_real *x, int n,
                            int maxit, gm_real tol, int *iters)
{
    gm_real *fx, *B, *dx, *dfx, *Bdx;
    int k;
    if (f == NULL || x == NULL || n <= 0) return GM_ERR_ARG;
    fx = malloc((size_t)n * sizeof(gm_real)); dx = malloc((size_t)n * sizeof(gm_real));
    dfx = malloc((size_t)n * sizeof(gm_real)); Bdx = malloc((size_t)n * sizeof(gm_real));
    B = malloc((size_t)n * (size_t)n * sizeof(gm_real));
    if (!fx || !dx || !dfx || !Bdx || !B) { free(fx); free(dx); free(dfx); free(Bdx); free(B); return GM_ERR_NO_MEM; }
    fd_jacobian(f, ctx, x, B, n);
    f(x, fx, n, ctx);
    for (k = 0; k < maxit; k++) {
        int i, j;
        gm_real denom;
        if (gc_array_norm2(fx, n) <= tol) { if (iters) *iters = k; free(fx); free(dx); free(dfx); free(Bdx); free(B); return GM_OK; }
        if (gm_linalg_solve(n, B, fx, dx) != GM_OK) { if (iters) *iters = k; free(fx); free(dx); free(dfx); free(Bdx); free(B); return GM_ERR_SINGULAR; }
        for (i = 0; i < n; i++) x[i] -= dx[i];
        {
            gm_real *fxn = malloc((size_t)n * sizeof(gm_real));
            if (!fxn) break;
            f(x, fxn, n, ctx);
            for (i = 0; i < n; i++) dfx[i] = fxn[i] - fx[i];
            memcpy(fx, fxn, (size_t)n * sizeof(gm_real));
            free(fxn);
        }
        for (i = 0; i < n; i++) Bdx[i] = gc_array_dot(B + (size_t)i * (size_t)n, dx, n);
        denom = gc_array_dot(dx, dx, n);
        if (denom != 0.0)
            for (i = 0; i < n; i++)
                for (j = 0; j < n; j++)
                    B[(size_t)i * (size_t)n + (size_t)j] -= (dfx[i] + Bdx[i]) * dx[j] / denom;
    }
    if (iters) *iters = maxit;
    free(fx); free(dx); free(dfx); free(Bdx); free(B);
    return GM_ERR_NOCONV;
}

gm_status gm_fixed_point(gm_nl_fn g, void *ctx, gm_real *x, int n,
                         int maxit, gm_real tol, int *iters)
{
    gm_real *xn;
    int k;
    if (g == NULL || x == NULL || n <= 0) return GM_ERR_ARG;
    xn = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!xn) return GM_ERR_NO_MEM;
    for (k = 0; k < maxit; k++) {
        int i;
        gm_real diff = 0.0;
        g(x, xn, n, ctx);
        for (i = 0; i < n; i++) { gm_real d = xn[i] - x[i]; diff += d * d; x[i] = xn[i]; }
        if (sqrt(diff) <= tol) { if (iters) *iters = k + 1; free(xn); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(xn);
    return GM_ERR_NOCONV;
}

typedef struct { gm_nl_fn f, f0; void *ctx; gm_real t; } cont_ctx;

static void blend_fn(const gm_real *x, gm_real *fx, int n, void *c)
{
    cont_ctx *cc = (cont_ctx *)c;
    gm_real *a = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *b = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i;
    if (!a || !b) { free(a); free(b); return; }
    cc->f0(x, a, n, cc->ctx);
    cc->f(x, b, n, cc->ctx);
    for (i = 0; i < n; i++) fx[i] = (1.0 - cc->t) * a[i] + cc->t * b[i];
    free(a); free(b);
}

gm_status gm_continuation(gm_nl_fn f, gm_nl_fn f0, void *ctx, gm_real *x, int n,
                          int steps, int maxit, gm_real tol, int *iters)
{
    cont_ctx cc;
    int s;
    if (f == NULL || f0 == NULL || x == NULL || n <= 0 || steps <= 0) return GM_ERR_ARG;
    cc.f = f; cc.f0 = f0; cc.ctx = ctx; cc.t = 0.0;
    if (gm_newton_system(blend_fn, &cc, x, n, maxit, tol, iters) != GM_OK) return GM_ERR_NOCONV;
    for (s = 1; s <= steps; s++) {
        cc.t = (gm_real)s / (gm_real)steps;
        if (gm_newton_system(blend_fn, &cc, x, n, maxit, tol, iters) != GM_OK) return GM_ERR_NOCONV;
    }
    return GM_OK;
}

/* Steffensen (radice scalare, derivative-free): formula di Atkinson 1989,
 * x_{k+1} = x_k - f(x_k)^2 / (f(x_k + f(x_k)) - f(x_k)). */
gm_real gm_steffensen(gm_real (*f)(gm_real, void *), void *ctx,
                      gm_real x0, gm_real tol, int maxit, int *iters)
{
    gm_real x, fx;
    int i;
    if (f == NULL || tol <= 0.0 || maxit <= 0) { if (iters) *iters = -1; return INFINITY; }
    x = x0;
    fx = f(x, ctx);
    if (fabs(fx) < tol) { if (iters) *iters = 0; return x; }
    for (i = 0; i < maxit; i++) {
        gm_real fn, d, xn;
        fn = f(x + fx, ctx);
        d = fn - fx;
        if (d == 0.0) break;
        xn = x - fx * fx / d;
        if (iters) *iters = i + 1;
        if (fabs(xn - x) < tol) return xn;
        x = xn;
        fx = f(x, ctx);
        if (fabs(fx) < tol) return x;
    }
    if (iters) *iters = -1;
    return INFINITY;
}
