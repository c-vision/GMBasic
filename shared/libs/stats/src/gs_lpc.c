/*
 * gs_lpc.c — LPC: Burg, weighting, filtro di analisi/sintesi (da LPCNet/Opus).
 */
#include "gs_lpc.h"
#include <stdlib.h>
#include <string.h>

int gs_lpc_burg(int n, const gc_real *x, int p, gc_real *a, gc_real *err)
{
    gc_real *f, *b, *tmp;
    gc_real E = 0.0;
    int i, m;
    if (n < 2 || p < 1 || p >= n || !x || !a) return -1;
    f = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    b = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    tmp = (gc_real *)malloc((size_t)(p + 1) * sizeof(gc_real));
    if (!f || !b || !tmp) { free(f); free(b); free(tmp); return -1; }
    for (i = 0; i <= p; i++) a[i] = 0.0;
    a[0] = 1.0;
    for (i = 0; i < n; i++) { f[i] = x[i]; b[i] = x[i]; }
    for (i = 0; i < n; i++) E += x[i] * x[i];
    for (m = 1; m <= p; m++) {
        gc_real num = 0.0, den = 0.0, k;
        for (i = m; i < n; i++) {
            num += f[i] * b[i - 1];
            den += f[i] * f[i] + b[i - 1] * b[i - 1];
        }
        if (den < 1e-30) break;
        k = -2.0 * num / den;
        if (k > 1.0) k = 1.0;
        if (k < -1.0) k = -1.0;
        for (i = 0; i <= p; i++) tmp[i] = a[i];
        for (i = 1; i < m; i++) a[i] = tmp[i] + k * tmp[m - i];
        a[m] = k;
        /* aggiorna gli errori forward/backward */
        for (i = n - 1; i >= m; i--) {
            gc_real fi = f[i], bi = b[i - 1];
            f[i] = fi + k * bi;
            b[i] = bi + k * fi;
        }
        E *= (1.0 - k * k);
        if (E < 1e-30) E = 1e-30;
    }
    if (err) *err = E;
    free(f); free(b); free(tmp);
    return 0;
}

void gs_lpc_weighting(int p, gc_real *a, gc_real gamma)
{
    gc_real g = 1.0;
    int i;
    if (!a || p < 1) return;
    for (i = 1; i <= p; i++) { g *= gamma; a[i] *= g; }
}

int gs_lpc_residual(int n, const gc_real *x, int p, const gc_real *a, gc_real *e)
{
    int t, i;
    if (n < 1 || !x || !a || !e) return -1;
    for (t = 0; t < n; t++) {
        gc_real s = x[t];
        for (i = 1; i <= p && i <= t; i++) s += a[i] * x[t - i];
        e[t] = s;
    }
    return 0;
}

int gs_lpc_synthesis(int n, const gc_real *e, int p, const gc_real *a, gc_real *x)
{
    int t, i;
    if (n < 1 || !e || !a || !x) return -1;
    for (t = 0; t < n; t++) {
        gc_real s = e[t];
        for (i = 1; i <= p && i <= t; i++) s -= a[i] * x[t - i];
        x[t] = s;
    }
    return 0;
}
