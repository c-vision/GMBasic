/* gs_cs.c — operatori cross-sectional: zscore, neutralize, rank percentile. */
#include "gs_cs.h"

#include <math.h>

gc_status gs_cs_zscore(const gc_real *x, int n, gc_real *out)
{
    gc_real mean = 0.0, var = 0.0, sd;
    int i;
    if (!x || !out || n < 2) return GC_ERR_ARG;
    for (i = 0; i < n; i++) mean += x[i];
    mean /= (gc_real)n;
    for (i = 0; i < n; i++) { gc_real d = x[i] - mean; var += d * d; }
    var /= (gc_real)(n - 1);
    sd = sqrt(var);
    for (i = 0; i < n; i++)
        out[i] = (sd > 0.0) ? ((x[i] - mean) / sd) : 0.0;
    return GC_OK;
}

gc_status gs_cs_neutralize(const gc_real *y, const gc_real *x, int n, gc_real *out)
{
    gc_real mx = 0.0, my = 0.0, sxx = 0.0, sxy = 0.0, beta;
    int i;
    if (!y || !x || !out || n < 2) return GC_ERR_ARG;
    for (i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= (gc_real)n; my /= (gc_real)n;
    for (i = 0; i < n; i++) {
        gc_real dx = x[i] - mx;
        sxx += dx * dx;
        sxy += dx * (y[i] - my);
    }
    beta = (sxx > 0.0) ? (sxy / sxx) : 0.0;
    for (i = 0; i < n; i++)
        out[i] = (y[i] - my) - beta * (x[i] - mx);
    return GC_OK;
}

gc_status gs_cs_rank(const gc_real *x, int n, gc_real *out)
{
    int i, j;
    if (!x || !out || n < 2) return GC_ERR_ARG;
    for (i = 0; i < n; i++) {
        int less = 0, equal = 0;
        for (j = 0; j < n; j++) {
            if (x[j] < x[i]) less++;
            else if (x[j] == x[i]) equal++;
        }
        /* rango medio (1-based) = less + (equal+1)/2 */
        out[i] = ((gc_real)less + ((gc_real)equal + 1.0) * 0.5);
    }
    for (i = 0; i < n; i++) out[i] = (out[i] - 1.0) / (gc_real)(n - 1);
    return GC_OK;
}
