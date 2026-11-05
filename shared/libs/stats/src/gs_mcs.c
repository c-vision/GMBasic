/* gs_mcs.c — Model Confidence Set (da et_al. inference/mcs.h). */
#include "gs_mcs.h"

#include <math.h>
#include <stdlib.h>

static unsigned long long mcs_rand(unsigned long long *s)
{
    *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (*s >> 33);
}

/* varianza di lungo periodo (Bartlett) della media di una serie centrata. */
static gc_real hac_var_mean(const gc_real *s, int n, int lag)
{
    gc_real g0 = 0.0, v;
    int t, j;
    for (t = 0; t < n; t++) g0 += s[t] * s[t];
    g0 /= (gc_real)n;
    v = g0;
    for (j = 1; j <= lag; j++) {
        gc_real gj = 0.0;
        for (t = j; t < n; t++) gj += s[t] * s[t - j];
        gj /= (gc_real)n;
        v += 2.0 * (1.0 - (gc_real)j / (gc_real)(lag + 1)) * gj;
    }
    return v / (gc_real)n;
}

/* serie di perdita relativa del modello i rispetto agli altri attivi, centrata. */
static void rel_series(int n, int m, const gc_real *losses, const int *active,
                       int i, gc_real mean_i, gc_real *d)
{
    int t, j;
    for (t = 0; t < n; t++) {
        gc_real others = 0.0;
        int c = 0;
        for (j = 0; j < m; j++)
            if (active[j] && j != i) { others += losses[t * m + j]; c++; }
        d[t] = losses[t * m + i] - (c > 0 ? others / (gc_real)c : 0.0);
    }
    {
        gc_real mu = 0.0;
        for (t = 0; t < n; t++) mu += d[t];
        mu /= (gc_real)n;
        for (t = 0; t < n; t++) d[t] -= mu;
    }
    (void)mean_i;
}

int gs_mcs(int n, int m, const gc_real *losses, const gs_mcs_options *opt,
           int *in_set, gs_mcs_result *out)
{
    int *active, *idx;
    gc_real *mean, *dbar, *se, *d, *rel, *Tstar;
    int i, t, b, lag, n_active, rounds, worst, B, bl;
    gc_real alpha, pvalue = 0.0, Tstat = 0.0;
    unsigned long long seed;
    if (n <= 1 || m < 2 || !losses || !in_set) return 0;
    alpha = opt ? opt->alpha : 0.05;
    B = opt ? opt->bootstrap : 1000;
    bl = opt ? opt->block_length : 10;
    if (B < 1) B = 1;
    if (bl < 1) bl = 1;
    if (bl > n) bl = n;
    lag = bl - 1;
    seed = (opt && opt->seed) ? opt->seed : 123ULL;
    active = (int *)malloc((size_t)m * sizeof(int));
    idx = (int *)malloc((size_t)n * sizeof(int));
    mean = (gc_real *)malloc((size_t)m * sizeof(gc_real));
    dbar = (gc_real *)malloc((size_t)m * sizeof(gc_real));
    se = (gc_real *)malloc((size_t)m * sizeof(gc_real));
    d = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    rel = (gc_real *)malloc((size_t)m * n * sizeof(gc_real));
    Tstar = (gc_real *)malloc((size_t)B * sizeof(gc_real));
    if (!active || !idx || !mean || !dbar || !se || !d || !rel || !Tstar) {
        free(active); free(idx); free(mean); free(dbar); free(se); free(d); free(rel); free(Tstar); return 0;
    }
    for (i = 0; i < m; i++) active[i] = 1;
    n_active = m;
    for (rounds = 0; rounds < m - 1 && n_active > 1; rounds++) {
        gc_real sum_active = 0.0;
        for (i = 0; i < m; i++) {
            gc_real s = 0.0;
            for (t = 0; t < n; t++) s += losses[t * m + i];
            mean[i] = s / (gc_real)n;
            if (active[i]) sum_active += mean[i];
        }
        for (i = 0; i < m; i++) {
            if (!active[i]) { dbar[i] = 0.0; se[i] = 1.0; continue; }
            dbar[i] = mean[i] - (sum_active - mean[i]) / (gc_real)(n_active - 1);
            rel_series(n, m, losses, active, i, mean[i], rel + (size_t)i * n);
            se[i] = sqrt(hac_var_mean(rel + (size_t)i * n, n, lag));
            if (!(se[i] > 0.0)) se[i] = 1e-300;
        }
        Tstat = -1e300; worst = -1;
        for (i = 0; i < m; i++) if (active[i]) {
            gc_real ti = dbar[i] / se[i];
            if (ti > Tstat) { Tstat = ti; worst = i; }
        }
        /* moving block bootstrap: stessi blocchi per tutti i modelli */
        {
            int nblocks = (n + bl - 1) / bl;
            for (b = 0; b < B; b++) {
                gc_real Tb = -1e300;
                int bb, q;
                for (bb = 0; bb < nblocks; bb++) {
                    int maxstart = n - bl;
                    int start = (int)(mcs_rand(&seed) % (unsigned long long)(maxstart + 1));
                    for (q = 0; q < bl && bb * bl + q < n; q++) idx[bb * bl + q] = start + q;
                }
                for (i = 0; i < m; i++) if (active[i]) {
                    gc_real mu = 0.0, si;
                    for (t = 0; t < n; t++) mu += rel[(size_t)i * n + idx[t]];
                    mu /= (gc_real)n;
                    si = mu / se[i];
                    if (si > Tb) Tb = si;
                }
                Tstar[b] = Tb;
            }
            { int ge = 0; for (b = 0; b < B; b++) if (Tstar[b] >= Tstat) ge++;
              pvalue = (gc_real)(ge + 1) / (gc_real)(B + 1); }
        }
        if (pvalue > alpha) break;
        active[worst] = 0;
        n_active--;
    }
    {
        int kept = 0;
        for (i = 0; i < m; i++) { in_set[i] = active[i]; if (active[i]) kept++; }
        if (out) { out->n_models = m; out->n_kept = kept; out->pvalue = pvalue; }
        free(active); free(idx); free(mean); free(dbar); free(se); free(d); free(rel); free(Tstar);
        return kept;
    }
}
