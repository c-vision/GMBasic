/* gs_ts2.c — serie temporali (riscrittura). */
#include "gs_ts2.h"

#include <stdlib.h>

gc_status gs_ts_arma_sim(gc_real *x, int n, const gc_real *ar, int p,
                         const gc_real *ma, int q, gs_rng *rng)
{
    gc_real *e;
    int t, i;
    if (x == NULL || n <= 0 || (p > 0 && ar == NULL) || (q > 0 && ma == NULL) || rng == NULL)
        return GC_ERR_ARG;
    e = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    if (!e) return GC_ERR_NO_MEM;
    for (t = 0; t < n; t++) {
        gc_real v = gs_rng_normal(rng);
        e[t] = v;
        for (i = 0; i < p && i < t; i++) v += ar[i] * x[t - 1 - i];
        for (i = 0; i < q && i < t; i++) v += ma[i] * e[t - 1 - i];
        x[t] = v;
    }
    free(e);
    return GC_OK;
}

/* coefficienti binomiali (d choose k) in ricorrenza. */
static gc_real *binom_coeffs(gc_real d, int n)
{
    gc_real *c = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    int k;
    if (!c) return NULL;
    c[0] = 1.0;
    for (k = 1; k < n; k++) c[k] = c[k - 1] * (d - (gc_real)(k - 1)) / (gc_real)k;
    return c;
}

gc_status gs_ts_frac_diff(const gc_real *x, int n, gc_real d, gc_real *out)
{
    gc_real *c;
    int t, k;
    if (x == NULL || out == NULL || n <= 0) return GC_ERR_ARG;
    c = binom_coeffs(-d, n);
    if (!c) return GC_ERR_NO_MEM;
    for (t = 0; t < n; t++) {
        gc_real s = 0.0;
        for (k = 0; k <= t; k++) s += c[k] * x[t - k];
        out[t] = s;
    }
    free(c);
    return GC_OK;
}

gc_status gs_ts_frac_int(const gc_real *x, int n, gc_real d, gc_real *out)
{
    gc_real *c;
    int t, k;
    if (x == NULL || out == NULL || n <= 0) return GC_ERR_ARG;
    c = binom_coeffs(d, n);
    if (!c) return GC_ERR_NO_MEM;
    for (t = 0; t < n; t++) {
        gc_real s = 0.0;
        for (k = 0; k <= t; k++) s += c[k] * x[t - k];
        out[t] = s;
    }
    free(c);
    return GC_OK;
}
