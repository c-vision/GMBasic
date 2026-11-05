/*
 * gs_nonparam.c — correlazioni, test non parametrici, descrittive (da scipy.stats).
 */
#include "gs_nonparam.h"
#include "gs_dist.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

static gc_real np_mean(const gc_real *x, int n)
{
    gc_real s = 0.0; int i;
    for (i = 0; i < n; i++) s += x[i];
    return n > 0 ? s / n : 0.0;
}

static gc_real two_t(gc_real t, gc_real df) { return 2.0 * gs_dist_student_t_cdf(df, -fabs(t)); }
static gc_real two_n(gc_real z) { return 2.0 * (1.0 - gs_dist_normal_cdf(0, 1, fabs(z))); }

/* ranghi medi (SciPy rankdata 'average') */
static void rank_data(const gc_real *data, int n, gc_real *ranks)
{
    int *idx = (int *)malloc((size_t)n * sizeof(int));
    int i = 0;
    for (i = 0; i < n; i++) idx[i] = i;
    for (i = 1; i < n; i++) { int k = idx[i], q = i - 1; while (q >= 0 && data[idx[q]] > data[k]) { idx[q + 1] = idx[q]; q--; } idx[q + 1] = k; }
    i = 0;
    while (i < n) {
        int j = i;
        while (j + 1 < n && data[idx[j + 1]] == data[idx[i]]) j++;
        { gc_real avg = 1.0 + (i + j) / 2.0; int k; for (k = i; k <= j; k++) ranks[idx[k]] = avg; }
        i = j + 1;
    }
    free(idx);
}

gs_test_result gs_pearsonr(const gc_real *x, const gc_real *y, int n)
{
    gs_test_result r = {NAN, NAN, NAN};
    gc_real mx, my, sxx = 0, syy = 0, sxy = 0, rr, t, df;
    int i;
    if (!x || !y || n < 2) return r;
    mx = np_mean(x, n); my = np_mean(y, n);
    for (i = 0; i < n; i++) { gc_real dx = x[i] - mx, dy = y[i] - my; sxx += dx * dx; syy += dy * dy; sxy += dx * dy; }
    if (sxx == 0 || syy == 0) return r;
    rr = sxy / sqrt(sxx * syy);
    df = (gc_real)(n - 2);
    if (fabs(rr) >= 1.0) { r.statistic = rr; r.pvalue = 0.0; r.df = df; return r; }
    t = rr * sqrt(df / (1.0 - rr * rr));
    r.statistic = rr; r.pvalue = two_t(t, df); r.df = df;
    return r;
}

gs_test_result gs_spearmanr(const gc_real *x, const gc_real *y, int n)
{
    gc_real *rx, *ry;
    gs_test_result r = {NAN, NAN, NAN};
    if (!x || !y || n < 2) return r;
    rx = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    ry = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!rx || !ry) { free(rx); free(ry); return r; }
    rank_data(x, n, rx); rank_data(y, n, ry);
    r = gs_pearsonr(rx, ry, n);
    free(rx); free(ry);
    return r;
}

gs_test_result gs_kendalltau(const gc_real *x, const gc_real *y, int n)
{
    gs_test_result r = {NAN, NAN, NAN};
    int i, j, conc = 0, disc = 0, tx = 0, ty = 0;
    gc_real tau, v;
    if (!x || !y || n < 2) return r;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            gc_real a = x[j] - x[i], b = y[j] - y[i];
            gc_real p = a * b;
            if (p > 0) conc++; else if (p < 0) disc++;
            if (a == 0) tx++;
            if (b == 0) ty++;
        }
    { int n0 = n * (n - 1) / 2; gc_real den = sqrt((gc_real)(n0 - tx) * (n0 - ty)); tau = den > 0 ? (gc_real)(conc - disc) / den : 0.0; }
    v = (2.0 * (2 * n + 5)) / (9.0 * n * (n - 1));
    r.statistic = tau; r.pvalue = v > 0 ? two_n(tau / sqrt(v)) : NAN;
    return r;
}

gs_linreg_result gs_linregress(const gc_real *x, const gc_real *y, int n)
{
    gs_linreg_result r = {NAN, NAN, NAN, NAN, NAN};
    gc_real mx, my, sxx = 0, sxy = 0, ss = 0, sstot = 0, slope, intercept, df, se, t;
    int i;
    if (!x || !y || n < 2) return r;
    mx = np_mean(x, n); my = np_mean(y, n);
    for (i = 0; i < n; i++) { gc_real dx = x[i] - mx; sxx += dx * dx; sxy += dx * (y[i] - my); }
    if (sxx == 0) return r;
    slope = sxy / sxx; intercept = my - slope * mx;
    for (i = 0; i < n; i++) { gc_real e = y[i] - (intercept + slope * x[i]); ss += e * e; }
    df = (gc_real)(n - 2);
    se = sqrt(ss / df / sxx);
    t = se > 0 ? slope / se : 0.0;
    for (i = 0; i < n; i++) sstot += (y[i] - my) * (y[i] - my);
    r.slope = slope; r.intercept = intercept; r.stderr = se;
    r.pvalue = two_t(t, df);
    r.r = sstot > 0 ? (slope >= 0 ? 1 : -1) * sqrt(1.0 - ss / sstot < 0 ? 0 : 1.0 - ss / sstot) : 0.0;
    return r;
}

static int cmp_real(const void *a, const void *b) { gc_real x = *(const gc_real *)a, y = *(const gc_real *)b; return x < y ? -1 : (x > y ? 1 : 0); }

gs_test_result gs_mannwhitneyu(const gc_real *x, int nx, const gc_real *y, int ny)
{
    gs_test_result r = {NAN, NAN, NAN};
    gc_real u = 0, mean, varu, z;
    int i, j, n;
    if (!x || !y || nx < 1 || ny < 1) return r;
    for (i = 0; i < nx; i++) for (j = 0; j < ny; j++) { if (x[i] > y[j]) u += 1.0; else if (x[i] == y[j]) u += 0.5; }
    n = nx + ny;
    mean = nx * ny / 2.0;
    {
        gc_real *all = (gc_real *)malloc((size_t)n * sizeof(gc_real));
        gc_real ties = 0;
        int k = 0;
        if (!all) return r;
        for (i = 0; i < nx; i++) all[k++] = x[i];
        for (j = 0; j < ny; j++) all[k++] = y[j];
        qsort(all, (size_t)n, sizeof(gc_real), cmp_real);
        i = 0;
        while (i < n) { int jj = i; while (jj + 1 < n && all[jj + 1] == all[i]) jj++; { gc_real t = jj - i + 1; ties += t * (t - 1) * (t + 1); } i = jj + 1; }
        varu = (nx * ny / 12.0) * ((n + 1.0) - ties / (n * (n - 1.0)));
        free(all);
    }
    if (varu <= 0) { r.statistic = u; return r; }
    z = (u - mean) / sqrt(varu);
    r.statistic = u; r.pvalue = two_n(z);
    return r;
}

gs_test_result gs_wilcoxon(const gc_real *x, const gc_real *y, int n)
{
    gs_test_result r = {0.0, 1.0, NAN};
    gc_real *diffs, *absd, *ranks, w = 0;
    int m = 0, i;
    if (!x || !y || n < 1) return r;
    diffs = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    absd = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    ranks = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!diffs || !absd || !ranks) { free(diffs); free(absd); free(ranks); return r; }
    for (i = 0; i < n; i++) { gc_real d = x[i] - y[i]; if (d != 0) { diffs[m] = d; absd[m] = fabs(d); m++; } }
    if (m == 0) { free(diffs); free(absd); free(ranks); return r; }
    rank_data(absd, m, ranks);
    for (i = 0; i < m; i++) if (diffs[i] > 0) w += ranks[i];
    {
        gc_real mean = m * (m + 1.0) / 4.0;
        gc_real varw = m * (m + 1.0) * (2 * m + 1.0) / 24.0;
        if (varw > 0) { gc_real z = (w - mean) / sqrt(varw); r.pvalue = two_n(z); }
    }
    r.statistic = w;
    free(diffs); free(absd); free(ranks);
    return r;
}

gs_test_result gs_kruskal(const gc_real *groups, const int *sizes, int k)
{
    gs_test_result r = {NAN, NAN, NAN};
    int total = 0, i, g, idx = 0;
    gc_real *all, *ranks, h = 0;
    if (!groups || !sizes || k < 2) return r;
    for (g = 0; g < k; g++) total += sizes[g];
    all = (gc_real *)malloc((size_t)total * sizeof(gc_real));
    ranks = (gc_real *)malloc((size_t)total * sizeof(gc_real));
    if (!all || !ranks) { free(all); free(ranks); return r; }
    for (i = 0; i < total; i++) all[i] = groups[i];
    rank_data(all, total, ranks);
    idx = 0;
    for (g = 0; g < k; g++) {
        gc_real rsum = 0;
        for (i = 0; i < sizes[g]; i++) rsum += ranks[idx++];
        h += rsum * rsum / sizes[g];
    }
    h = 12.0 / (total * (gc_real)(total + 1)) * h - 3.0 * (total + 1);
    r.statistic = h; r.df = (gc_real)(k - 1);
    r.pvalue = 1.0 - gs_dist_chi2_cdf(r.df, h);
    free(all); free(ranks);
    return r;
}

gs_test_result gs_jarque_bera(const gc_real *x, int n)
{
    gs_test_result r = {NAN, NAN, NAN};
    gc_real m, m2 = 0, m3 = 0, m4 = 0, s, kk, jb;
    int i;
    if (!x || n < 3) return r;
    m = np_mean(x, n);
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; m2 += d * d; m3 += d * d * d; m4 += d * d * d * d; }
    m2 /= n; m3 /= n; m4 /= n;
    s = m2 > 0 ? m3 / pow(m2, 1.5) : 0;
    kk = m2 > 0 ? m4 / (m2 * m2) : 0;
    jb = (n / 6.0) * (s * s + (kk - 3.0) * (kk - 3.0));
    r.statistic = jb; r.pvalue = 1.0 - gs_dist_chi2_cdf(2.0, jb); r.df = 2.0;
    return r;
}

gs_test_result gs_skewtest(const gc_real *x, int n)
{
    gs_test_result r = {NAN, NAN, NAN};
    gc_real m, m2 = 0, m3 = 0, g1, y, beta2, w2, delta, alpha, z;
    int i;
    if (!x || n < 3) return r;
    m = np_mean(x, n);
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; m2 += d * d; m3 += d * d * d; }
    m2 /= n; m3 /= n;
    g1 = m2 > 0 ? m3 / pow(m2, 1.5) : 0;
    y = g1 * sqrt(((gc_real)n + 1) * (n + 3) / (6.0 * (n - 2)));
    beta2 = 3.0 * ((gc_real)n * n + 27 * n - 70) * (n + 1) * (n + 3) / ((n - 2.0) * (n + 5) * (n + 7) * (n + 9));
    w2 = -1.0 + sqrt(2.0 * (beta2 - 1.0));
    delta = 1.0 / sqrt(0.5 * log(w2));
    alpha = sqrt(2.0 / (w2 - 1.0));
    z = delta * log(y / alpha + sqrt((y / alpha) * (y / alpha) + 1.0));
    r.statistic = z; r.pvalue = two_n(z);
    return r;
}

gs_test_result gs_kurtosistest(const gc_real *x, int n)
{
    gs_test_result r = {NAN, NAN, NAN};
    gc_real m, m2 = 0, m4 = 0, b2, e, varb2, xx, sqrtbeta1, a, term1, term2, z;
    int i;
    if (!x || n < 4) return r;
    m = np_mean(x, n);
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; m2 += d * d; m4 += d * d * d * d; }
    m2 /= n; m4 /= n;
    b2 = m2 > 0 ? m4 / (m2 * m2) : 3.0;
    e = 3.0 * (n - 1) / (n + 1);
    varb2 = 24.0 * (gc_real)n * (n - 2) * (n - 3) / ((n + 1.0) * (n + 1) * (n + 3) * (n + 5));
    xx = (b2 - e) / sqrt(varb2);
    sqrtbeta1 = 6.0 * ((gc_real)n * n - 5 * n + 2) / ((n + 7.0) * (n + 9)) * sqrt(6.0 * (n + 3) * (n + 5) / ((gc_real)n * (n - 2) * (n - 3)));
    a = 6.0 + 8.0 / sqrtbeta1 * (2.0 / sqrtbeta1 + sqrt(1.0 + 4.0 / (sqrtbeta1 * sqrtbeta1)));
    term1 = 1.0 - 2.0 / (9.0 * a);
    term2 = (1.0 - 2.0 / a) / (1.0 + xx * sqrt(2.0 / (a - 4.0)));
    z = (term1 - pow(term2, 1.0 / 3.0)) / sqrt(2.0 / (9.0 * a));
    r.statistic = z; r.pvalue = two_n(z);
    return r;
}

gc_real gs_mode(const gc_real *x, int n)
{
    int i, j, best = 0, bestc = 0;
    if (!x || n == 0) return NAN;
    for (i = 0; i < n; i++) {
        int c = 0;
        for (j = 0; j < n; j++) if (x[j] == x[i]) c++;
        if (c > bestc || (c == bestc && x[i] < x[best])) { bestc = c; best = i; }
    }
    return x[best];
}

gc_real gs_moment(const gc_real *x, int n, int order)
{
    gc_real m, s = 0;
    int i;
    if (!x || n == 0) return NAN;
    m = np_mean(x, n);
    for (i = 0; i < n; i++) s += pow(x[i] - m, order);
    return s / n;
}

gc_real gs_iqr(const gc_real *x, int n)
{
    gc_real *s;
    gc_real q1, q3, pos, frac;
    int lo;
    if (!x || n < 1) return NAN;
    s = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!s) return NAN;
    memcpy(s, x, (size_t)n * sizeof(gc_real));
    qsort(s, (size_t)n, sizeof(gc_real), cmp_real);
    pos = 0.25 * (n - 1); lo = (int)floor(pos); frac = pos - lo;
    q1 = (lo + 1 >= n) ? s[n - 1] : s[lo] + frac * (s[lo + 1] - s[lo]);
    pos = 0.75 * (n - 1); lo = (int)floor(pos); frac = pos - lo;
    q3 = (lo + 1 >= n) ? s[n - 1] : s[lo] + frac * (s[lo + 1] - s[lo]);
    free(s);
    return q3 - q1;
}

gs_describe_result gs_describe(const gc_real *x, int n)
{
    gs_describe_result r = {0, NAN, NAN, NAN, NAN, NAN, NAN};
    gc_real m, var = 0, m3 = 0, m4 = 0, sd;
    int i;
    if (!x || n == 0) return r;
    m = np_mean(x, n);
    r.nobs = n; r.mean = m;
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; var += d * d; m3 += d * d * d; m4 += d * d * d * d; }
    var /= n; m3 /= n; m4 /= n;
    sd = sqrt(var);
    r.min = x[0]; r.max = x[0];
    for (i = 1; i < n; i++) { if (x[i] < r.min) r.min = x[i]; if (x[i] > r.max) r.max = x[i]; }
    r.variance = var;
    r.skewness = (sd > 0) ? m3 / (sd * sd * sd) : 0.0;
    r.kurtosis = (var > 0) ? m4 / (var * var) : 0.0;
    return r;
}
