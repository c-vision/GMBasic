/* gs_stats2_sim.c — MCMC, bootstrap e permutation test (simulation).
 * Estratti da gs_stats2.c. */
#include "gs_stats2.h"
#include "gs_sim.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned long lcg(unsigned long *s)
{
    *s = (*s * 6364136223846793005ULL + 1442695040888963407ULL);
    return (*s >> 33);
}

static gc_real urand(unsigned long *s)
{
    return (gc_real)lcg(s) / 2147483648.0;
}

gc_status gs_mcmc_metropolis(gc_real (*logp)(const gc_real *, int, void *), void *ctx,
                             gc_real *x, int d, int iters, gc_real step,
                             unsigned long seed, gc_real *samples)
{
    gc_real lp, *xp;
    int i, k;
    unsigned long s = seed ? seed : 1;
    if (logp == NULL || x == NULL || d <= 0 || iters <= 0) return GC_ERR_ARG;
    xp = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    if (!xp) return GC_ERR_NO_MEM;
    lp = logp(x, d, ctx);
    for (k = 0; k < iters; k++) {
        for (i = 0; i < d; i++) xp[i] = x[i] + step * (urand(&s) - 0.5) * 2.0;
        {
            gc_real lpn = logp(xp, d, ctx);
            if (lpn >= lp || urand(&s) < exp(lpn - lp)) {
                memcpy(x, xp, (size_t)d * sizeof(gc_real));
                lp = lpn;
            }
        }
        if (samples) memcpy(samples + (size_t)k * (size_t)d, x, (size_t)d * sizeof(gc_real));
    }
    free(xp);
    return GC_OK;
}

gc_real gs_bootstrap_mean(const gc_real *x, int n, int B, unsigned long seed, gc_real *se)
{
    gc_real mean = 0.0, var = 0.0;
    int i, b;
    unsigned long s = seed ? seed : 1;
    if (x == NULL || n <= 0 || B <= 0) { if (se) *se = 0.0; return 0.0; }
    for (i = 0; i < n; i++) mean += x[i];
    mean /= (gc_real)n;
    for (b = 0; b < B; b++) {
        gc_real m = 0.0;
        for (i = 0; i < n; i++) m += x[lcg(&s) % (unsigned long)n];
        m /= (gc_real)n;
        var += (m - mean) * (m - mean);
    }
    if (se) *se = sqrt(var / (gc_real)(B - 1 > 0 ? B - 1 : 1));
    return mean;
}

/* ---- Estimatori di volatilita OHLC (QuantLib, BSD-3): Parkinson,
 * Garman-Klass, Rogers-Satchell, Yang-Zhang. ---- */
gc_real gs_permutation_test(const gc_real *x, int nx, const gc_real *y, int ny,
                            int B, unsigned long seed)
{
    gc_real *combined, obs, extreme = 0;
    int n, i, j;
    gs_rng rng;
    if (!x || !y || nx < 1 || ny < 1 || B < 1) return 0.0 / 0.0;
    n = nx + ny;
    combined = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!combined) return 0.0 / 0.0;
    for (i = 0; i < nx; i++) combined[i] = x[i];
    for (i = 0; i < ny; i++) combined[nx + i] = y[i];
    {
        gc_real mx = 0.0, my = 0.0;
        for (i = 0; i < nx; i++) mx += x[i];
        for (i = 0; i < ny; i++) my += y[i];
        obs = mx / (gc_real)nx - my / (gc_real)ny;
    }
    rng = gs_rng_make(seed);
    for (i = 0; i < B; i++) {
        gc_real mx = 0.0, my = 0.0;
        /* Fisher-Yates su combined */
        for (j = n - 1; j > 0; j--) {
            int k = gs_rng_int(&rng, j + 1);
            gc_real t = combined[j]; combined[j] = combined[k]; combined[k] = t;
        }
        for (j = 0; j < nx; j++) mx += combined[j];
        for (j = nx; j < n; j++) my += combined[j];
        {
            gc_real d = mx / (gc_real)nx - my / (gc_real)ny;
            if (fabs(d) >= fabs(obs)) extreme += 1.0;
        }
    }
    free(combined);
    return extreme / (gc_real)B;
}

/* OU fit (AR(1) esatto): X[t+1] = a + phi*X[t] + eps.
 * phi = sxy/sxx, a = mean_y - phi*mean_x, mu = a/(1-phi),
 * theta = -ln(phi)/dt, half_life = ln2/theta,
 * sigma^2 = (SSE/n) * 2*theta/(1 - e^{-2 theta dt}). */
