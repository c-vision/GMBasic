/* gs_stats.c — statistica descrittiva SIMD. */
#include "gs_stats.h"
#include "gs_dist.h"
#include "gm_linalg.h"
#include "gs_dist2.h"
#include "gc_array.h"

#include <math.h>
#include <stdlib.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define GM_NEON 1
#endif

gc_real gs_stats_mean(const gc_real *a, int n)
{
    if (a == NULL || n <= 0) return 0.0;
    return gc_array_sum(a, n) / (gc_real)n;
}

/* Momenti centrali 2/3/4 in una passata SIMD. */
static void moments(const gc_real *a, int n, gc_real mean,
                    gc_real *m2, gc_real *m3, gc_real *m4)
{
    int i = 0;
    gc_real s2 = 0.0, s3 = 0.0, s4 = 0.0;
#ifdef GM_NEON
    if (n >= 2) {
        float64x2_t vmean = vdupq_n_f64(mean);
        float64x2_t a2 = vdupq_n_f64(0.0), a3 = vdupq_n_f64(0.0), a4 = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) {
            float64x2_t d = vsubq_f64(vld1q_f64(a + i), vmean);
            float64x2_t d2 = vmulq_f64(d, d);
            a2 = vaddq_f64(a2, d2);
            a3 = vfmaq_f64(a3, d2, d);
            a4 = vfmaq_f64(a4, d2, d2);
        }
        s2 = vaddvq_f64(a2);
        s3 = vaddvq_f64(a3);
        s4 = vaddvq_f64(a4);
    }
#endif
    for (; i < n; i++) {
        gc_real d = a[i] - mean;
        gc_real d2 = d * d;
        s2 += d2;
        s3 += d2 * d;
        s4 += d2 * d2;
    }
    if (m2) *m2 = s2;
    if (m3) *m3 = s3;
    if (m4) *m4 = s4;
}

gc_real gs_stats_variance(const gc_real *a, int n, int sample)
{
    gc_real mean, m2;
    int denom;
    if (a == NULL || n <= 0) return 0.0;
    if (sample) {
        if (n < 2) return 0.0;
        denom = n - 1;
    } else {
        denom = n;
    }
    mean = gs_stats_mean(a, n);
    moments(a, n, mean, &m2, NULL, NULL);
    return m2 / (gc_real)denom;
}

gc_real gs_stats_stddev(const gc_real *a, int n, int sample)
{
    return sqrt(gs_stats_variance(a, n, sample));
}

gc_real gs_stats_skewness(const gc_real *a, int n)
{
    gc_real mean, m2, m3;
    if (a == NULL || n < 1) return 0.0;
    mean = gs_stats_mean(a, n);
    moments(a, n, mean, &m2, &m3, NULL);
    if (m2 == 0.0) return 0.0;
    return (m3 / (gc_real)n) / pow(m2 / (gc_real)n, 1.5);
}

gc_real gs_stats_kurtosis(const gc_real *a, int n)
{
    gc_real mean, m2, m4;
    gc_real v;
    if (a == NULL || n < 1) return 0.0;
    mean = gs_stats_mean(a, n);
    moments(a, n, mean, &m2, NULL, &m4);
    v = m2 / (gc_real)n;
    if (v == 0.0) return 0.0;
    return (m4 / (gc_real)n) / (v * v) - 3.0;
}

gc_real gs_stats_covariance(const gc_real *a, const gc_real *b, int n, int sample)
{
    gc_real ma, mb, s = 0.0;
    int i = 0, denom;
    if (a == NULL || b == NULL || n <= 0) return 0.0;
    if (sample) {
        if (n < 2) return 0.0;
        denom = n - 1;
    } else {
        denom = n;
    }
    ma = gs_stats_mean(a, n);
    mb = gs_stats_mean(b, n);
#ifdef GM_NEON
    if (n >= 2) {
        float64x2_t va = vdupq_n_f64(ma), vb = vdupq_n_f64(mb);
        float64x2_t acc = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) {
            float64x2_t da = vsubq_f64(vld1q_f64(a + i), va);
            float64x2_t db = vsubq_f64(vld1q_f64(b + i), vb);
            acc = vfmaq_f64(acc, da, db);
        }
        s = vaddvq_f64(acc);
    }
#endif
    for (; i < n; i++) s += (a[i] - ma) * (b[i] - mb);
    return s / (gc_real)denom;
}

gc_real gs_stats_correlation(const gc_real *a, const gc_real *b, int n)
{
    gc_real sa = gs_stats_stddev(a, n, 0);
    gc_real sb = gs_stats_stddev(b, n, 0);
    if (sa == 0.0 || sb == 0.0) return 0.0;
    return gs_stats_covariance(a, b, n, 0) / (sa * sb);
}

gc_real gs_stats_min(const gc_real *a, int n) { return gc_array_min(a, n); }
gc_real gs_stats_max(const gc_real *a, int n) { return gc_array_max(a, n); }
gc_real gs_stats_range(const gc_real *a, int n) { return gc_array_max(a, n) - gc_array_min(a, n); }

static int cmp_real(const void *x, const void *y)
{
    gc_real a = *(const gc_real *)x;
    gc_real b = *(const gc_real *)y;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

gc_real gs_stats_percentile(const gc_real *a, int n, gc_real p)
{
    gc_real *copy;
    gc_real rank, frac, out;
    int lo, hi;
    if (a == NULL || n <= 0) return 0.0;
    if (n == 1) return a[0];
    if (p <= 0.0) return gc_array_min(a, n);
    if (p >= 100.0) return gc_array_max(a, n);
    copy = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (copy == NULL) return 0.0;
    gc_array_copy(a, copy, n);
    qsort(copy, (size_t)n, sizeof(gc_real), cmp_real);
    rank = (p / 100.0) * (gc_real)(n - 1);
    lo = (int)floor(rank);
    hi = lo + 1;
    if (hi >= n) hi = n - 1;
    frac = rank - (gc_real)lo;
    out = copy[lo] + frac * (copy[hi] - copy[lo]);
    free(copy);
    return out;
}

gc_real gs_stats_median(const gc_real *a, int n)
{
    return gs_stats_percentile(a, n, 50.0);
}

gc_real gs_stats_mad(const gc_real *a, int n)
{
    gc_real med;
    gc_real *dev;
    gc_real out;
    int i;
    if (a == NULL || n <= 0) return 0.0;
    med = gs_stats_median(a, n);
    dev = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (dev == NULL) return 0.0;
    for (i = 0; i < n; i++) dev[i] = fabs(a[i] - med);
    out = gs_stats_median(dev, n);
    free(dev);
    return out;
}

double gs_hurst_dfa(const double *x, int n, int min_box)
{
    double *y, mean = 0.0;
    int i, j, t;
    double *ls, *ls_x;
    int nb = 0;
    if (!x || n < 20 || min_box < 2) return 0.0 / 0.0;
    y = (double *)malloc((size_t)n * sizeof(double));
    ls = (double *)malloc((size_t)64 * sizeof(double));
    ls_x = (double *)malloc((size_t)64 * sizeof(double));
    if (!y || !ls || !ls_x) { free(y); free(ls); free(ls_x); return 0.0 / 0.0; }
    for (i = 0; i < n; i++) mean += x[i];
    mean /= n;
    y[0] = x[0] - mean;
    for (i = 1; i < n; i++) y[i] = y[i - 1] + (x[i] - mean);
    for (t = min_box; t <= n / 2; t *= 2) {
        int nboxes = n / t;
        double F = 0.0;
        for (j = 0; j < nboxes; j++) {
            double mx = 0.0, my = 0.0, sxy = 0.0, sxx = 0.0, slope, inter;
            int k;
            for (k = 0; k < t; k++) { mx += k; my += y[j * t + k]; }
            mx /= t; my /= t;
            for (k = 0; k < t; k++) {
                double dx = k - mx, dy = y[j * t + k] - my;
                sxy += dx * dy; sxx += dx * dx;
            }
            slope = sxx > 0 ? sxy / sxx : 0.0;
            inter = my - slope * mx;
            for (k = 0; k < t; k++) {
                double r = y[j * t + k] - (slope * k + inter);
                F += r * r;
            }
        }
        F = sqrt(F / (nboxes * t));
        ls[nb] = log(F);
        ls_x[nb] = log((double)t);
        nb++;
    }
    free(y);
    {
        double mx = 0.0, my = 0.0, sxy = 0.0, sxx = 0.0, h;
        for (i = 0; i < nb; i++) { mx += ls_x[i]; my += ls[i]; }
        mx /= nb; my /= nb;
        for (i = 0; i < nb; i++) {
            double dx = ls_x[i] - mx, dy = ls[i] - my;
            sxy += dx * dy; sxx += dx * dx;
        }
        h = sxx > 0 ? sxy / sxx : 0.0;
        free(ls); free(ls_x);
        return h;
    }
}

int gs_frac_diff(const double *x, int n, double d, double *out)
{
    double *w;
    int i, k;
    if (!x || !out || n < 1 || d < 0.0) return GC_ERR_ARG;
    w = (double *)malloc((size_t)n * sizeof(double));
    if (!w) return GC_ERR_NO_MEM;
    w[0] = 1.0;
    for (k = 1; k < n; k++) w[k] = -w[k - 1] * (d - k + 1) / k;
    for (i = 0; i < n; i++) {
        double s = 0.0;
        for (k = 0; k <= i; k++) s += w[k] * x[i - k];
        out[i] = s;
    }
    free(w);
    return GC_OK;
}

int gs_stationary_bootstrap(const double *x, int n, int window, unsigned seed,
                            double *out)
{
    double p = window > 0 ? 1.0 / window : 0.0;
    int idx, i;
    unsigned s = seed ? seed : 12345u;
    if (!x || !out || n < 1 || window < 1) return GC_ERR_ARG;
    idx = (int)((s = s * 1103515245u + 12345u) % (unsigned)n);
    out[0] = x[idx];
    for (i = 1; i < n; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        if (u > p) {
            double u2 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            idx = (int)(u2 * (double)n) % n;
        } else {
            idx = (idx + 1) % n;
        }
        out[i] = x[idx];
    }
    return GC_OK;
}

double gs_merton_timing(int n1, int N1, int n, int N)
{
    double mu, sigma, z;
    if (N <= 0 || N1 <= 0 || N1 > N || n1 > N1) return 0.0 / 0.0;
    mu = (double)n * N1 / N;
    sigma = sqrt((double)n1 * N1 * (N - N1) * (N - n) / ((double)N * N * (N - 1)));
    if (sigma <= 0) return 0.0 / 0.0;
    z = (n1 - mu) / sigma;
    return gs_dist_normal_cdf(0.0, 1.0, -z);   /* P(Z > z) */
}

/* --- Entropie --- */

static double entropy_dist(const double *x, int i, int j, int m2)
{
    int k;
    double dmax = -1e300;
    for (k = 0; k < m2; k++) {
        double d = fabs(x[i + k] - x[j + k]);
        if (d > dmax) dmax = d;
    }
    return dmax;
}

double gs_entropy_sample(const double *x, int n, int m, double r)
{
    int i, j;
    double B = 0.0, A = 0.0;
    if (!x || n < m + 1 || m < 1 || r <= 0) return 0.0 / 0.0;
    for (i = 0; i < n - m; i++)
        for (j = 0; j < n - m; j++) {
            if (i == j) continue;
            if (entropy_dist(x, i, j, m) <= r) B += 1.0;
        }
    for (i = 0; i < n - m - 1; i++)
        for (j = 0; j < n - m - 1; j++) {
            if (i == j) continue;
            if (entropy_dist(x, i, j, m + 1) <= r) A += 1.0;
        }
    if (B == 0 || A == 0) return 0.0 / 0.0;
    return -log(A / B);
}

double gs_entropy_approx(const double *x, int n, int m, double r)
{
    int i, j;
    double B = 0.0, A = 0.0;
    if (!x || n < m + 1 || m < 1 || r <= 0) return 0.0 / 0.0;
    for (i = 0; i <= n - m; i++)
        for (j = 0; j <= n - m; j++) {
            if (entropy_dist(x, i, j, m) <= r) B += 1.0;
        }
    for (i = 0; i <= n - m - 1; i++)
        for (j = 0; j <= n - m - 1; j++) {
            if (entropy_dist(x, i, j, m + 1) <= r) A += 1.0;
        }
    if (B == 0 || A == 0) return 0.0 / 0.0;
    return log(B / (double)(n - m + 1)) - log(A / (double)(n - m));
}

double gs_entropy_perm(const double *x, int n, int m, int lag)
{
    int *order, *seen;
    int i, j, k, cnt;
    double ent = 0.0;
    (void)n; (void)m;
    if (!x || m < 2 || lag < 1) return 0.0 / 0.0;
    order = (int *)malloc((size_t)m * sizeof(int));
    /* `seen` e' indicizzato [q * n + j] con q < m: servono m*n interi (prima n: overflow di heap,
     * crash dipendente dal layout, emerso col gate FFI estensioni 2026-11-06). */
    seen = (int *)malloc((size_t)m * (size_t)n * sizeof(int));
    if (!order || !seen) { free(order); free(seen); return 0.0 / 0.0; }
    for (i = 0; i < n; i++) seen[i] = 0;
    cnt = 0;
    for (i = 0; i <= n - m * lag; i++) {
        int *seq = (int *)malloc((size_t)m * sizeof(int));
        int is_new = 1;
        for (k = 0; k < m; k++) order[k] = k;
        (void)is_new;
        for (j = 0; j < m; j++)
            for (k = j + 1; k < m; k++)
                if (x[i + order[k] * lag] < x[i + order[j] * lag]) { int t = order[j]; order[j] = order[k]; order[k] = t; }
        for (j = 0; j < m; j++) seq[j] = order[j];
        for (j = 0; j < cnt; j++) {
            int same = 1;
            int q;
            for (q = 0; q < m; q++) if (seen[q * n + j] != seq[q]) { same = 0; break; }
            if (same) { is_new = 0; break; }
        (void)is_new;
        }
        if (is_new) {
            for (j = 0; j < m; j++) seen[j * n + cnt] = seq[j];
            cnt++;
        }
        free(seq);
    }
    /* cnt = numero di pattern distinti; PE normalizzata richiede le frequenze,
     * qui usiamo la diversita' di pattern (max: m!) */
    free(order); free(seen);
    {
        int npat = 1, q;
        for (q = 2; q <= m; q++) npat *= q;
        if (cnt <= 1) return 0.0;
        ent = log((double)cnt) / log((double)npat);
        return ent;
    }
}

double gs_entropy_lz(const double *x, int n)
{
    double med;
    double *sorted;
    int *b;
    int i, j, c = 0, ii = 0;
    if (!x || n < 2) return 0.0 / 0.0;
    sorted = (double *)malloc((size_t)n * sizeof(double));
    b = (int *)malloc((size_t)n * sizeof(int));
    if (!sorted || !b) { free(sorted); free(b); return 0.0 / 0.0; }
    for (i = 0; i < n; i++) sorted[i] = x[i];
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (sorted[j] < sorted[i]) { double t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    med = sorted[n / 2];
    for (i = 0; i < n; i++) b[i] = x[i] >= med ? 1 : 0;
    free(sorted);
    /* greedy LZ76: frase = match piu' lungo nella storia + 1 simbolo */
    while (ii < n) {
        int maxlen = 0, start;
        for (start = 0; start < ii; start++) {
            int m = 0;
            while (ii + m < n && start + m < ii && b[start + m] == b[ii + m]) m++;
            if (m > maxlen) maxlen = m;
        }
        c++;
        ii += maxlen + 1;
    }
    free(b);
    return (double)c;
}

double gs_lyapunov(const double *x, int n, int m, int tau)
{
    int i, j, k;
    int M = n - (m - 1) * tau;
    int theiler = m * tau > 10 ? m * tau : 10;
    double *lambdas;
    double *d0;
    int *nbr;
    if (!x || n < m * tau + 8 || m < 2 || tau < 1) return 0.0 / 0.0;
    d0 = (double *)malloc((size_t)M * sizeof(double));
    nbr = (int *)malloc((size_t)M * sizeof(int));
    lambdas = (double *)calloc((size_t)(M / 2), sizeof(double));
    if (!d0 || !nbr || !lambdas) { free(d0); free(nbr); free(lambdas); return 0.0 / 0.0; }
    /* nearest neighbor con Theiler window */
    for (i = 0; i < M; i++) {
        double best = 1e300;
        int bj = -1;
        for (j = 0; j < M; j++) {
            double d2 = 0.0;
            int q;
            if (abs(i - j) < theiler) continue;
            for (q = 0; q < m; q++) {
                double dd = x[i + q * tau] - x[j + q * tau];
                d2 += dd * dd;
            }
            if (d2 > 0 && d2 < best) { best = d2; bj = j; }
        }
        d0[i] = best > 0 ? sqrt(best) : 1e-9;
        nbr[i] = bj;
    }

    /* divergenza media logaritmica D(k) */
    for (k = 1; k < M / 2; k++) {
        double s = 0.0;
        int cnt = 0;

        for (i = 0; i < M - k; i++) {
            int jj = nbr[i];
            if (jj >= 0 && jj + k < M) {
                double d2 = 0.0;
                int q;
                for (q = 0; q < m; q++) {
                    double dd = x[i + k + q * tau] - x[jj + k + q * tau];
                    d2 += dd * dd;
                }
                if (d2 > 0) { s += 0.5 * log(d2 / (d0[i] * d0[i])); cnt++; }
            }
        }
        lambdas[k] = cnt > 0 ? s / cnt : 0.0;

    }
    /* slope nella regione lineare: da k=M/32 a M/8 */
    {
        int k0 = 1;
        int k1 = 8 < M / 2 ? 8 : M / 2;
        double mx = 0.0, my = 0.0, sxy = 0.0, sxx = 0.0, lam;
        int cnt = 0;
        if (k1 > M / 2 - 1) k1 = M / 2 - 1;
        if (k1 <= k0) k1 = k0 + 1;
        for (k = k0; k < k1; k++) { mx += k; my += lambdas[k]; cnt++; }
        if (cnt == 0) { free(d0); free(nbr); free(lambdas); return 0.0; }
        mx /= cnt; my /= cnt;
        for (k = k0; k < k1; k++) {
            double dx = k - mx, dy = lambdas[k] - my;
            sxy += dx * dy; sxx += dx * dx;
        }
        lam = sxx > 0 ? sxy / sxx : 0.0;
        free(d0); free(nbr); free(lambdas);
        return lam;
    }
}

int gs_granger(const double *x, const double *y, int n, int p, double *f_stat,
               double *p_value)
{
    /* y causa x? F-test: RSS_restricted (solo x) vs RSS_unrestricted (x+y). */
    double *Xr, *Xu, *br, *bu, *e;
    double rss_r, rss_u, F, pval;
    int i, j, k, m;
    if (!x || !y || !f_stat || !p_value || n < 3 * p + 2 || p < 1) return GC_ERR_ARG;
    m = n - p;
    Xr = (double *)malloc((size_t)m * (p + 1) * sizeof(double));
    Xu = (double *)malloc((size_t)m * (2 * p + 1) * sizeof(double));
    br = (double *)malloc((size_t)(p + 1) * sizeof(double));
    bu = (double *)malloc((size_t)(2 * p + 1) * sizeof(double));
    e = (double *)malloc((size_t)m * sizeof(double));
    if (!Xr || !Xu || !br || !bu || !e) { free(Xr); free(Xu); free(br); free(bu); free(e); return GC_ERR_NO_MEM; }
    for (i = p; i < n; i++) {
        Xr[(i - p) * (p + 1) + 0] = 1.0;
        for (j = 1; j <= p; j++) Xr[(i - p) * (p + 1) + j] = x[i - j];
        Xu[(i - p) * (2 * p + 1) + 0] = 1.0;
        for (j = 1; j <= p; j++) Xu[(i - p) * (2 * p + 1) + j] = x[i - j];
        for (j = 1; j <= p; j++) Xu[(i - p) * (2 * p + 1) + p + j] = y[i - j];
    }
    /* OLS ristretto */
    {
        double *AtA = (double *)calloc((size_t)(p + 1) * (p + 1), sizeof(double));
        double *Atb = (double *)calloc((size_t)(p + 1), sizeof(double));
        if (!AtA || !Atb) { free(AtA); free(Atb); free(Xr); free(Xu); free(br); free(bu); free(e); return GC_ERR_NO_MEM; }
        for (i = 0; i < m; i++)
            for (j = 0; j <= p; j++) {
                for (k = 0; k <= p; k++) AtA[j * (p + 1) + k] += Xr[i * (p + 1) + j] * Xr[i * (p + 1) + k];
                Atb[j] += Xr[i * (p + 1) + j] * x[i + p];
            }
        if (gm_linalg_solve(p + 1, AtA, Atb, br) != 0) { free(AtA); free(Atb); free(Xr); free(Xu); free(br); free(bu); free(e); return GC_ERR_SINGULAR; }
        rss_r = 0.0;
        for (i = 0; i < m; i++) {
            double r = x[i + p];
            for (j = 0; j <= p; j++) r -= br[j] * Xr[i * (p + 1) + j];
            rss_r += r * r;
        }
        free(AtA); free(Atb);
    }
    /* OLS unrestricted */
    {
        double *AtA = (double *)calloc((size_t)(2 * p + 1) * (2 * p + 1), sizeof(double));
        double *Atb = (double *)calloc((size_t)(2 * p + 1), sizeof(double));
        if (!AtA || !Atb) { free(AtA); free(Atb); free(Xr); free(Xu); free(br); free(bu); free(e); return GC_ERR_NO_MEM; }
        for (i = 0; i < m; i++)
            for (j = 0; j <= 2 * p; j++) {
                for (k = 0; k <= 2 * p; k++) AtA[j * (2 * p + 1) + k] += Xu[i * (2 * p + 1) + j] * Xu[i * (2 * p + 1) + k];
                Atb[j] += Xu[i * (2 * p + 1) + j] * x[i + p];
            }
        if (gm_linalg_solve(2 * p + 1, AtA, Atb, bu) != 0) { free(AtA); free(Atb); free(Xr); free(Xu); free(br); free(bu); free(e); return GC_ERR_SINGULAR; }
        rss_u = 0.0;
        for (i = 0; i < m; i++) {
            double r = x[i + p];
            for (j = 0; j <= 2 * p; j++) r -= bu[j] * Xu[i * (2 * p + 1) + j];
            rss_u += r * r;
        }
        free(AtA); free(Atb);
    }
    if (rss_u <= 0) { free(Xr); free(Xu); free(br); free(bu); free(e); return GC_ERR_ARG; }
    F = ((rss_r - rss_u) / p) / (rss_u / (m - (2 * p + 1)));
    /* p-value: F(p, d2) -> beta CDF */
    {
        double d2 = m - (2 * p + 1);
        double xb = p * F / (p * F + d2);
        pval = 1.0 - gs_dist2_beta_cdf(xb, p / 2.0, d2 / 2.0);
    }
    *f_stat = F;
    *p_value = pval;
    free(Xr); free(Xu); free(br); free(bu); free(e);
    return GC_OK;
}

double gs_entropy_multiscale(const double *x, int n, int scale, int m, double r)
{
    double *cg;
    int nc, i;
    double se;
    if (!x || n < scale * (m + 1) || scale < 1 || m < 1 || r <= 0) return 0.0 / 0.0;
    nc = n / scale;
    cg = (double *)malloc((size_t)nc * sizeof(double));
    if (!cg) return 0.0 / 0.0;
    for (i = 0; i < nc; i++) {
        double s = 0.0;
        int j;
        for (j = 0; j < scale; j++) s += x[i * scale + j];
        cg[i] = s / scale;
    }
    se = gs_entropy_sample(cg, nc, m, r);
    free(cg);
    return se;
}

/* ---- Bayesian Model Averaging (van Dijk 2007): w_i ∝ exp(-0.5*(IC_i - min IC)). ---- */
int gs_bma_weights(const double *ic, int m, double *w)
{
    int i; double minic, s = 0.0;
    if (!ic || !w || m < 1) return GC_ERR_ARG;
    minic = ic[0];
    for (i = 1; i < m; i++) if (ic[i] < minic) minic = ic[i];
    for (i = 0; i < m; i++) { w[i] = exp(-0.5 * (ic[i] - minic)); s += w[i]; }
    if (!(s > 0.0)) return GC_ERR_SINGULAR;
    for (i = 0; i < m; i++) w[i] /= s;
    return 0;
}

/* ---- Forecast combination (Bates-Granger): mode 0 = media, 1 = pesi inversi MSE. ---- */
int gs_fc_weights(const double *mse, int m, int mode, double *w)
{
    int i; double s = 0.0;
    if (!mse || !w || m < 1 || mode < 0 || mode > 1) return GC_ERR_ARG;
    if (mode == 0) { for (i = 0; i < m; i++) w[i] = 1.0 / m; return 0; }
    for (i = 0; i < m; i++) {
        if (!(mse[i] > 0.0)) return GC_ERR_ARG;
        w[i] = 1.0 / mse[i]; s += w[i];
    }
    for (i = 0; i < m; i++) w[i] /= s;
    return 0;
}

/* ---- Combina previsioni: fc[m] con pesi w[m]. ---- */
double gs_fc_combine(const double *fc, const double *w, int m)
{
    int i; double s = 0.0;
    if (!fc || !w || m < 1) return 0.0;
    for (i = 0; i < m; i++) s += fc[i] * w[i];
    return s;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
gc_real gs_stats_correlation__ffi(const gc_real *a, int64_t a_ext, const gc_real *b, int64_t b_ext, int n) { return gs_stats_correlation(a, b, n); }
gc_real gs_stats_covariance__ffi(const gc_real *a, int64_t a_ext, const gc_real *b, int64_t b_ext, int n, int sample) { return gs_stats_covariance(a, b, n, sample); }
gc_real gs_stats_kurtosis__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_kurtosis(a, n); }
gc_real gs_stats_mad__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_mad(a, n); }
gc_real gs_stats_max__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_max(a, n); }
gc_real gs_stats_mean__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_mean(a, n); }
gc_real gs_stats_median__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_median(a, n); }
gc_real gs_stats_min__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_min(a, n); }
gc_real gs_stats_percentile__ffi(const gc_real *a, int64_t a_ext, int n, gc_real p) { return gs_stats_percentile(a, n, p); }
gc_real gs_stats_range__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_range(a, n); }
gc_real gs_stats_skewness__ffi(const gc_real *a, int64_t a_ext, int n) { return gs_stats_skewness(a, n); }
gc_real gs_stats_stddev__ffi(const gc_real *a, int64_t a_ext, int n, int sample) { return gs_stats_stddev(a, n, sample); }
gc_real gs_stats_variance__ffi(const gc_real *a, int64_t a_ext, int n, int sample) { return gs_stats_variance(a, n, sample); }
