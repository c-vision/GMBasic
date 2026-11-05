/* gm_integral.c — equazioni integrali di Fredholm/Volterra (riscrittura). */
#include "gm_integral.h"
#include "gm_linalg.h"

#include <stdlib.h>
#include <string.h>

static gm_real trap_w(gm_real a, gm_real b, int n, int j)
{
    gm_real h = (b - a) / (gm_real)(n - 1);
    if (j == 0 || j == n - 1) return 0.5 * h;
    return h;
}

gm_status gm_fredholm2(gm_real (*K)(gm_real, gm_real, void *), void *kctx,
                       gm_real (*f)(gm_real, void *), void *fctx,
                       gm_real a, gm_real b, int n, gm_real *u)
{
    gm_real h;
    gm_real *A, *rhs;
    int i, j;
    if (K == NULL || f == NULL || u == NULL || n < 2) return GM_ERR_ARG;
    h = (b - a) / (gm_real)(n - 1);
    A = (gm_real *)calloc((size_t)n * (size_t)n, sizeof(gm_real));
    rhs = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!A || !rhs) { free(A); free(rhs); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++) {
        gm_real xi = a + h * (gm_real)i;
        rhs[i] = f(xi, fctx);
        for (j = 0; j < n; j++) {
            gm_real xj = a + h * (gm_real)j;
            A[(size_t)i * (size_t)n + (size_t)j] = (i == j ? 1.0 : 0.0) - trap_w(a, b, n, j) * K(xi, xj, kctx);
        }
    }
    if (gm_linalg_solve(n, A, rhs, u) != GM_OK) { free(A); free(rhs); return GM_ERR_SINGULAR; }
    free(A); free(rhs);
    return GM_OK;
}

gm_status gm_volterra2(gm_real (*K)(gm_real, gm_real, void *), void *kctx,
                       gm_real (*f)(gm_real, void *), void *fctx,
                       gm_real a, gm_real b, int n, gm_real *u)
{
    gm_real h;
    int i, j;
    if (K == NULL || f == NULL || u == NULL || n < 2) return GM_ERR_ARG;
    h = (b - a) / (gm_real)(n - 1);
    for (i = 0; i < n; i++) {
        gm_real xi = a + h * (gm_real)i;
        gm_real acc = f(xi, fctx);
        for (j = 0; j < i; j++) {
            gm_real xj = a + h * (gm_real)j;
            gm_real w = (j == 0) ? 0.5 * h : h;
            acc += w * K(xi, xj, kctx) * u[j];
        }
        /* termine diagonale j=i con peso 0 (regola trapezoidale: u_i appare con peso 0 a sinistra) */
        u[i] = acc;
    }
    return GM_OK;
}
