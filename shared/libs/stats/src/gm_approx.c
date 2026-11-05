/* gm_approx.c — approssimazione di funzioni (riscrittura). */
#include "gm_approx.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gm_real gm_newton_divided(const gm_real *x, const gm_real *y, int n, gm_real t)
{
    gm_real *c;
    gm_real r;
    int i, j;
    if (x == NULL || y == NULL || n <= 0) return 0.0;
    c = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!c) return 0.0;
    memcpy(c, y, (size_t)n * sizeof(gm_real));
    for (j = 1; j < n; j++)
        for (i = n - 1; i >= j; i--)
            c[i] = (c[i] - c[i - 1]) / (x[i] - x[i - j]);
    r = c[n - 1];
    for (i = n - 2; i >= 0; i--) r = r * (t - x[i]) + c[i];
    free(c);
    return r;
}

gm_status gm_hermite_interp(const gm_real *x, const gm_real *y, const gm_real *dy,
                            int n, gm_real *coef)
{
    gm_real *z, *Q;
    int m = 2 * n, i, j;
    if (x == NULL || y == NULL || dy == NULL || coef == NULL || n <= 0) return GM_ERR_ARG;
    z = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    Q = (gm_real *)malloc((size_t)m * (size_t)m * sizeof(gm_real));
    if (!z || !Q) { free(z); free(Q); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++) { z[2 * i] = x[i]; z[2 * i + 1] = x[i]; }
    for (i = 0; i < n; i++) {
        Q[(size_t)(2 * i) * (size_t)m + 0] = y[i];
        Q[(size_t)(2 * i + 1) * (size_t)m + 0] = y[i];
        Q[(size_t)(2 * i + 1) * (size_t)m + 1] = dy[i];
        if (i > 0)
            Q[(size_t)(2 * i) * (size_t)m + 1] =
                (Q[(size_t)(2 * i) * (size_t)m + 0] - Q[(size_t)(2 * i - 1) * (size_t)m + 0]) / (z[2 * i] - z[2 * i - 1]);
    }
    for (j = 2; j < m; j++)
        for (i = j; i < m; i++)
            Q[(size_t)i * (size_t)m + (size_t)j] =
                (Q[(size_t)i * (size_t)m + (size_t)(j - 1)] - Q[(size_t)(i - 1) * (size_t)m + (size_t)(j - 1)]) / (z[i] - z[i - j]);
    for (i = 0; i < m; i++) coef[i] = Q[(size_t)i * (size_t)m + (size_t)i];
    free(z); free(Q);
    return GM_OK;
}

gm_status gm_chebyshev_nodes(int n, gm_real a, gm_real b, gm_real *x)
{
    int i;
    if (x == NULL || n <= 0) return GM_ERR_ARG;
    for (i = 0; i < n; i++)
        x[i] = 0.5 * (a + b) + 0.5 * (b - a) * cos(3.14159265358979323846 * (2.0 * (gm_real)i + 1.0) / (2.0 * (gm_real)n));
    return GM_OK;
}

gm_real gm_legendre_eval(int n, gm_real x)
{
    gm_real p0 = 1.0, p1 = x;
    int k;
    if (n == 0) return 1.0;
    if (n == 1) return x;
    for (k = 2; k <= n; k++) {
        gm_real p2 = ((2.0 * k - 1.0) * x * p1 - (k - 1.0) * p0) / (gm_real)k;
        p0 = p1; p1 = p2;
    }
    return p1;
}

gm_status gm_remez(gm_approx_fn f, void *ctx, gm_real a, gm_real b, int n,
                   gm_real *coef, int maxit)
{
    /* grado n -> n+1 coefficienti, n+2 punti di riferimento */
    int m = n + 2, i, k, it;
    gm_real *xr, *A, *rhs, *sol;
    if (f == NULL || coef == NULL || n < 0 || m < 2) return GM_ERR_ARG;
    xr = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    A = (gm_real *)malloc((size_t)m * (size_t)m * sizeof(gm_real));
    rhs = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    sol = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    if (!xr || !A || !rhs || !sol) { free(xr); free(A); free(rhs); free(sol); return GM_ERR_NO_MEM; }
    /* riferimento iniziale di Chebyshev */
    for (i = 0; i < m; i++)
        xr[i] = 0.5 * (a + b) + 0.5 * (b - a) * cos(3.14159265358979323846 * (gm_real)i / (gm_real)(m - 1));
    for (it = 0; it < maxit; it++) {
        /* sistema: sum c_k xr_i^k + (-1)^i E = f(xr_i) */
        for (i = 0; i < m; i++) {
            gm_real p = 1.0;
            for (k = 0; k <= n; k++) { A[(size_t)i * (size_t)m + (size_t)k] = p; p *= xr[i]; }
            A[(size_t)i * (size_t)m + (size_t)(n + 1)] = (i % 2 == 0) ? 1.0 : -1.0;
            rhs[i] = f(xr[i], ctx);
        }
        if (gm_linalg_solve(m, A, rhs, sol) != GM_OK) { free(xr); free(A); free(rhs); free(sol); return GM_ERR_SINGULAR; }
        /* scambio di Remez: seleziona gli m massimi locali dell'errore */
        {
            int ng = 2000 * m, cnt = 0, best_i = -1;
            gm_real *ex = (gm_real *)malloc((size_t)ng * sizeof(gm_real));
            int *locs = (int *)malloc((size_t)ng * sizeof(int));
            int *pick = (int *)malloc((size_t)m * sizeof(int));
            if (ex && locs && pick) {
                for (i = 0; i < ng; i++) {
                    gm_real t = a + (b - a) * (gm_real)i / (gm_real)(ng - 1);
                    gm_real p = 0.0, e;
                    for (k = n; k >= 0; k--) p = p * t + sol[k];
                    e = f(t, ctx) - p;
                    ex[i] = e;
                }
                for (i = 1; i < ng - 1; i++) {
                    gm_real ae = fabs(ex[i]);
                    if (ae >= fabs(ex[i - 1]) && ae >= fabs(ex[i + 1]) && ae > 0.0) locs[cnt++] = i;
                }
                if (cnt >= m) {
                    /* prendi gli m estremi di modulo massimo, poi riordina */
                    int j, s;
                    for (j = 0; j < m; j++) {
                        int bi = 0;
                        gm_real bv = -1.0;
                        for (s = 0; s < cnt; s++) {
                            gm_real v = fabs(ex[locs[s]]);
                            if (v > bv) { bv = v; bi = s; }
                        }
                        pick[j] = locs[bi];
                        locs[bi] = locs[cnt - 1];
                        cnt--;
                    }
                    for (j = 1; j < m; j++) {
                        int key = pick[j], s2 = j - 1;
                        while (s2 >= 0 && pick[s2] > key) { pick[s2 + 1] = pick[s2]; s2--; }
                        pick[s2 + 1] = key;
                    }
                    for (j = 0; j < m; j++) xr[j] = a + (b - a) * (gm_real)pick[j] / (gm_real)(ng - 1);
                }
                (void)best_i;
            }
            free(ex); free(locs); free(pick);
        }
        /* copia i coefficienti correnti */
        for (k = 0; k <= n; k++) coef[k] = sol[k];
    }
    free(xr); free(A); free(rhs); free(sol);
    return GM_OK;
}

/* ===================== Armoniche sferiche reali (da HamGNN/e3nn) ===================== */

gm_real gm_assoc_legendre(int l, int m, gm_real x)
{
    gm_real pmm = 1.0;
    int i;
    if (l < 0 || m < 0 || m > l) return 0.0;
    if (m > 0) {
        gm_real somx2 = sqrt((1.0 - x) * (1.0 + x));
        gm_real fact = 1.0;
        for (i = 1; i <= m; i++) { pmm *= fact * somx2; fact += 2.0; }
    }
    if (l == m) return pmm;
    {
        gm_real pmmp1 = x * (2.0 * m + 1.0) * pmm;
        if (l == m + 1) return pmmp1;
        for (i = m + 2; i <= l; i++) {
            gm_real pll = ((2.0 * i - 1.0) * x * pmmp1 - (i + m - 1.0) * pmm) / (gm_real)(i - m);
            pmm = pmmp1; pmmp1 = pll;
        }
        return pmmp1;
    }
}

static gm_real sh_norm(int l, int m)
{
    /* N_l^m = sqrt((2l+1)/(4pi) * (l-m)!/(l+m)!) */
    gm_real ratio = 1.0;
    int i;
    for (i = l - m + 1; i <= l + m; i++) ratio /= (gm_real)i;
    return sqrt((2.0 * l + 1.0) / (4.0 * 3.14159265358979323846) * ratio);
}

gm_real gm_real_sh(int l, int m, gm_real theta, gm_real phi)
{
    gm_real x = cos(theta), N, P;
    int am = m < 0 ? -m : m;
    if (l < 0 || am > l) return 0.0;
    N = sh_norm(l, am);
    P = gm_assoc_legendre(l, am, x);
    if (m == 0) return N * P;
    if (m > 0) return 1.4142135623730951 * N * P * cos((gm_real)am * phi);
    return 1.4142135623730951 * N * P * sin((gm_real)am * phi);
}

gm_status gm_real_sh_all(int lmax, gm_real theta, gm_real phi, gm_real *out)
{
    int l, m, k = 0;
    if (!out || lmax < 0) return GM_ERR_ARG;
    for (l = 0; l <= lmax; l++)
        for (m = -l; m <= l; m++) out[k++] = gm_real_sh(l, m, theta, phi);
    return GM_OK;
}
