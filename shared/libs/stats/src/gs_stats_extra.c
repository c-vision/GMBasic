/* gs_stats_extra.c — descrittiva robusta e test non parametrici (stats). */
#include "gs_sim.h"
#include "gs_dist.h"
#include "gs_stats2.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

double gs_huber_mean(const double *x, int n, double a, double b)
{
    double mu = 0.0, prev;
    int i, it;
    if (!x || n < 1 || a <= 0.0 || b <= 0.0) return 0.0 / 0.0;
    for (i = 0; i < n; i++) mu += x[i];
    mu /= n;
    for (it = 0; it < 100; it++) {
        double g = 0.0, gp = 0.0, gm = 0.0, eps;
        double mup = mu + 1e-6, mum = mu - 1e-6;
        prev = mu;
        for (i = 0; i < n; i++) {
            double e1 = exp(a * (x[i] - mu)), e2 = exp(-a * (x[i] - mu));
            g += (e1 - e2) / (e1 + e2 + b);
            e1 = exp(a * (x[i] - mup)); e2 = exp(-a * (x[i] - mup));
            gp += (e1 - e2) / (e1 + e2 + b);
            e1 = exp(a * (x[i] - mum)); e2 = exp(-a * (x[i] - mum));
            gm += (e1 - e2) / (e1 + e2 + b);
        }
        g = -g / n;
        gp = -gp / n;
        gm = -gm / n;
        eps = gp - gm;
        if (fabs(eps) < 1e-14) eps = (eps >= 0 ? 1e-14 : -1e-14);
        mu -= g / (eps / (2e-6));
        if (fabs(mu - prev) < 1e-11) break;
    }
    return mu;
}

/* ---- Ensemble inverse-variance (timemachines precision_weighted_skater).
 * y: [y0, xhat1, std1, xhat2, std2, ...] concatenato; e: exponente di blending
 * (0 = media semplice, alto = winner-take-all); k passi. Ritorna la previsione
 * combinata per il primo passo (horizon interp in forecast[0..k-1]). */
double gs_kendall_tau(const double *x, const double *y, int n)
{
    int i, j, c = 0, d = 0;
    if (!x || !y || n < 2) return 0.0 / 0.0;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            double a = x[i] - x[j], b = y[i] - y[j];
            if (a * b > 0) c++;
            else if (a * b < 0) d++;
        }
    {
        double total = (double)(n * (n - 1)) / 2.0;
        return total > 0 ? (c - d) / total : 0.0;
    }
}

/* ---- Copula gaussiana bivariata: CDF via normale bivariata. ---- */
static double bvn_cdf(double a, double b, double rho)
{
    double sum = 0.0;
    static const double xw[6] = {0.1713244923791704,0.3607615730481386,0.4679139345726910,
                                 0.4679139345726910,0.3607615730481386,0.1713244923791704};
    static const double xx[6] = {-0.9324695142031521,-0.6612093864662645,-0.2386191860831969,
                                  0.2386191860831969,0.6612093864662645,0.9324695142031521};
    int i;
    {
        double lo = -6.0, hi = a, mid = 0.5 * (lo + hi), half = 0.5 * (hi - lo);
        for (i = 0; i < 6; i++) {
            double t = xx[i], w = xw[i];
            double x = mid + half * t;
            double z = (b - rho * x) / sqrt(1.0 - rho * rho);
            sum += w * gs_dist_normal_cdf(0, 1, z) * gs_dist_normal_pdf(0, 1, x);
        }
        return sum * half;
    }
}
double gs_perm_test_pvalue(const double *a, int na, const double *b, int nb,
                           int samples, unsigned seed)
{
    double *pool, mean_diff;
    double cnt = 0.0;
    int n = na + nb, i, s;
    unsigned st = seed ? seed : 12345u;
    if (!a || !b || na < 1 || nb < 1 || samples < 1) return 0.0 / 0.0;
    pool = (double *)malloc((size_t)n * sizeof(double));
    if (!pool) return 0.0 / 0.0;
    for (i = 0; i < na; i++) pool[i] = a[i];
    for (i = 0; i < nb; i++) pool[na + i] = b[i];
    mean_diff = 0.0;
    for (i = 0; i < na; i++) mean_diff += a[i];
    mean_diff /= na;
    for (i = 0; i < nb; i++) mean_diff -= b[i];
    mean_diff /= nb;
    mean_diff = fabs(mean_diff);
    for (s = 0; s < samples; s++) {
        double m1 = 0.0, m2 = 0.0;
        /* shuffle parziale: pesca na indici senza rimpiazzo */
        int pick[512], k;
        if (n > 512) { free(pool); return 0.0 / 0.0; }
        for (i = 0; i < n; i++) pick[i] = i;
        for (i = n - 1; i > 0; i--) {
            int j = (int)((double)((st = st * 1103515245u + 12345u) % 100000u) / 100000.0 * (i + 1));
            if (j > i) j = i;
            { int t = pick[i]; pick[i] = pick[j]; pick[j] = t; }
        }
        for (k = 0; k < na; k++) m1 += pool[pick[k]];
        m1 /= na;
        for (k = na; k < n; k++) m2 += pool[pick[k]];
        m2 /= nb;
        if (fabs(m1 - m2) > mean_diff) cnt += 1.0;
    }
    free(pool);
    return cnt / samples;
}
/* ==================== Factor characteristics (Analysis/) ==================== */

/* ---- Skewness campionaria bias-corrected (pyanomaly nanskew). ---- */
int gs_quantile_anomaly(const double *x, int n, double lq, double uq, double k,
                        int mode, int *out)
{
    double *copy;
    double ql, qu, med, lb, ub;
    int i, j;
    if (!x || !out || n < 4 || lq <= 0 || uq >= 1 || lq >= uq) return -1;
    copy = (double *)malloc((size_t)n * sizeof(double));
    if (!copy) return -1;
    for (i = 0; i < n; i++) copy[i] = x[i];
    for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (copy[j] < copy[i]) { double t = copy[i]; copy[i] = copy[j]; copy[j] = t; }
    ql = copy[(int)(lq * (n - 1))];
    qu = copy[(int)(uq * (n - 1))];
    med = copy[n / 2];
    lb = ql - k * (med - ql);
    ub = qu - k * (med - qu);
    free(copy);
    for (i = 0; i < n; i++) out[i] = 0;
    if (mode == 1) {
        for (i = 1; i < n - 1; i++) {
            double c1 = x[i - 1] != 0 ? (x[i] - x[i - 1]) / fabs(x[i - 1]) : 0.0;
            double c2 = x[i] != 0 ? (x[i + 1] - x[i]) / fabs(x[i]) : 0.0;
            if ((c1 < lb && c2 < lb) || (c1 > ub && c2 > ub)) out[i] = 1;
        }
    } else {
        for (i = 0; i < n; i++) if (x[i] < lb || x[i] > ub) out[i] = 1;
    }
    return 0;
}

/* ==================== Copule, clustering, microstruttura (Jupyter) ==================== */

/* ---- Kendall tau su coppie di serie. ---- */
double gs_skewness_corrected(const double *x, int n)
{
    double m, s, m3 = 0.0;
    int i;
    if (!x || n < 3) return 0.0 / 0.0;
    m = 0.0; for (i = 0; i < n; i++) m += x[i]; m /= n;
    s = 0.0; for (i = 0; i < n; i++) { double d = x[i] - m; s += d * d; }
    s = sqrt(s / n);
    if (s <= 0) return 0.0;
    for (i = 0; i < n; i++) { double d = (x[i] - m) / s; m3 += d * d * d; }
    /* G1 bias-corrected (scipy.stats.skew bias=False) */
    return m3 / (double)n * sqrt((double)n * (n - 1)) / (double)(n - 2);
}
/* ---- Coskewness (Harvey-Siddique): res = CAPM residual, mkt demeaned. ---- */

static int gs_stats2_dcmp(const void *a, const void *b)
{ gc_real x = *(const gc_real*)a, y = *(const gc_real*)b; return x < y ? -1 : (x > y ? 1 : 0); }

gc_real gs_gini(const gc_real *x, int n)
{
    gc_real *c, sum = 0.0, num = 0.0, m; int i;
    if (!x || n < 2) return 0.0/0.0;
    c = (gc_real*)malloc((size_t)n*sizeof(gc_real));
    if (!c) return 0.0/0.0;
    for (i=0;i<n;i++){ c[i]=x[i]; sum+=x[i]; }
    if (sum <= 0.0) { free(c); return 0.0/0.0; }
    qsort(c,(size_t)n,sizeof(gc_real),gs_stats2_dcmp);
    for (i=0;i<n;i++) num += (gc_real)(i+1)*c[i];
    m = (2.0*num)/((gc_real)n*sum) - ((gc_real)n+1.0)/(gc_real)n;
    free(c);
    return m;
}

gc_real gs_theil(const gc_real *x, int n)
{
    gc_real mu = 0.0, t = 0.0; int i;
    if (!x || n < 1) return 0.0/0.0;
    for (i=0;i<n;i++){ if (x[i] <= 0.0) return 0.0/0.0; mu += x[i]; }
    mu /= (gc_real)n;
    for (i=0;i<n;i++){ gc_real r = x[i]/mu; t += r*log(r); }
    return t/(gc_real)n;
}

gc_real gs_chebyshev_distance(const gc_real *x, const gc_real *y, int n)
{
    gc_real m = 0.0; int i;
    if (!x || !y || n < 1) return 0.0/0.0;
    for (i=0;i<n;i++){ gc_real d = x[i]-y[i]; if (d < 0) d = -d; if (d > m) m = d; }
    return m;
}

