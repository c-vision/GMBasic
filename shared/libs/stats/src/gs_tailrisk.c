/* gs_tailrisk.c — tail richness: frequency ratio oltre k sigma vs normale. */
#include "gs_tailrisk.h"
#include "gs_dist.h"

#include <math.h>

gc_real gs_tail_expected_rate(gc_real k)
{
    if (k < 0.0) k = -k;
    /* 2*(1-Phi(k)) = erfc(k/sqrt(2)); usa la CDF della libreria per coerenza. */
    return 2.0 * (1.0 - gs_dist_normal_cdf(0.0, 1.0, k));
}

gc_status gs_tail_richness(const gc_real *x, int n, gc_real k,
                           gc_real *out_richness, gc_real *out_obs, gc_real *out_exp)
{
    gc_real mean = 0.0, var = 0.0, sd, exp_rate, obs_rate, thr;
    int i, cnt = 0;

    if (!x || n < 2) return GC_ERR_ARG;
    if (k < 0.0) k = -k;

    for (i = 0; i < n; i++) mean += x[i];
    mean /= (gc_real)n;
    for (i = 0; i < n; i++) {
        gc_real d = x[i] - mean;
        var += d * d;
    }
    var /= (gc_real)(n - 1);
    sd = sqrt(var);
    if (sd <= 0.0) return GC_ERR_ARG;

    thr = k * sd;
    for (i = 0; i < n; i++) {
        gc_real d = x[i] - mean;
        if (d > thr || d < -thr) cnt++;
    }
    obs_rate = (gc_real)cnt / (gc_real)n;
    exp_rate = gs_tail_expected_rate(k);

    if (out_obs) *out_obs = obs_rate;
    if (out_exp) *out_exp = exp_rate;
    if (out_richness) *out_richness = (exp_rate > 0.0) ? (obs_rate / exp_rate) : 0.0;
    return GC_OK;
}
