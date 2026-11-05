/*
 * gs_test.c — test di ipotesi (da scipy.stats, algoritmo del port C# GMBasic).
 */
#include "gs_test.h"
#include "gs_dist.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

static gc_real mean(const gc_real *x, int n)
{
    gc_real s = 0.0; int i;
    for (i = 0; i < n; i++) s += x[i];
    return n > 0 ? s / n : 0.0;
}

static gc_real sample_var(const gc_real *x, int n, gc_real m)
{
    gc_real s = 0.0; int i;
    if (n < 2) return 0.0;
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; s += d * d; }
    return s / (n - 1);
}

static gc_real two_sided_norm(gc_real z)
{
    return 2.0 * (1.0 - gs_dist_normal_cdf(0.0, 1.0, fabs(z)));
}

static gc_real two_sided_t(gc_real t, gc_real df)
{
    return 2.0 * gs_dist_student_t_cdf(df, -fabs(t));
}

static int cmp_real(const void *a, const void *b)
{
    gc_real x = *(const gc_real *)a, y = *(const gc_real *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

gs_test_result gs_test_ttest_ind(const gc_real *x, int nx, const gc_real *y, int ny)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real mx, my, vx, vy, sp2, t, df;
    if (!x || !y || nx < 2 || ny < 2) return r;
    mx = mean(x, nx); my = mean(y, ny);
    vx = sample_var(x, nx, mx); vy = sample_var(y, ny, my);
    df = (gc_real)(nx + ny - 2);
    sp2 = ((nx - 1) * vx + (ny - 1) * vy) / df;
    t = (mx - my) / sqrt(sp2 * (1.0 / nx + 1.0 / ny));
    r.statistic = t; r.pvalue = two_sided_t(t, df); r.df = df;
    return r;
}

gs_test_result gs_test_ttest_1samp(const gc_real *x, int n, gc_real popmean)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real mx, v, se, t, df;
    if (!x || n < 2) return r;
    mx = mean(x, n); v = sample_var(x, n, mx);
    df = (gc_real)(n - 1);
    se = sqrt(v / n);
    t = (mx - popmean) / se;
    r.statistic = t; r.pvalue = two_sided_t(t, df); r.df = df;
    return r;
}

gs_test_result gs_test_ttest_rel(const gc_real *x, const gc_real *y, int n)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real *d; gc_real md, vd, se, t, df;
    int i;
    if (!x || !y || n < 2) return r;
    d = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!d) return r;
    for (i = 0; i < n; i++) d[i] = x[i] - y[i];
    md = mean(d, n); vd = sample_var(d, n, md);
    df = (gc_real)(n - 1);
    se = sqrt(vd / n);
    t = se > 0 ? md / se : 0.0;
    r.statistic = t; r.pvalue = two_sided_t(t, df); r.df = df;
    free(d);
    return r;
}

gs_test_result gs_test_ztest_1samp(const gc_real *x, int n, gc_real popmean, gc_real popstd)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real z;
    if (!x || n < 1 || popstd <= 0) return r;
    z = (mean(x, n) - popmean) / (popstd / sqrt((gc_real)n));
    r.statistic = z; r.pvalue = two_sided_norm(z);
    return r;
}

gs_test_result gs_test_chi2_gof(const gc_real *observed, const gc_real *expected, int k)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real chi2 = 0.0;
    int i;
    if (!observed || !expected || k < 2) return r;
    for (i = 0; i < k; i++) {
        if (expected[i] <= 0) return r;
        { gc_real d = observed[i] - expected[i]; chi2 += d * d / expected[i]; }
    }
    r.statistic = chi2; r.df = (gc_real)(k - 1);
    r.pvalue = 1.0 - gs_dist_chi2_cdf(r.df, chi2);
    return r;
}

gs_test_result gs_test_chi2_contingency(const gc_real *table, int rows, int cols)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real *rowsum, *colsum, total = 0.0, chi2 = 0.0;
    int i, j;
    if (!table || rows < 2 || cols < 2) return r;
    rowsum = (gc_real *)calloc((size_t)rows, sizeof(gc_real));
    colsum = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    if (!rowsum || !colsum) { free(rowsum); free(colsum); return r; }
    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++) { rowsum[i] += table[i * cols + j]; colsum[j] += table[i * cols + j]; total += table[i * cols + j]; }
    if (total == 0) { free(rowsum); free(colsum); return r; }
    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++) {
            gc_real e = rowsum[i] * colsum[j] / total;
            if (e > 0) { gc_real d = table[i * cols + j] - e; chi2 += d * d / e; }
        }
    r.statistic = chi2; r.df = (gc_real)((rows - 1) * (cols - 1));
    r.pvalue = 1.0 - gs_dist_chi2_cdf(r.df, chi2);
    free(rowsum); free(colsum);
    return r;
}

static gc_real hyper_prob(int k, int row1, int col1, int total)
{
    /* C(col1,k)*C(total-col1,row1-k)/C(total,row1) in log-spazio */
    extern double gm_special_lgamma(double);
    double ln = gm_special_lgamma(col1 + 1.0) - gm_special_lgamma(k + 1.0) - gm_special_lgamma(col1 - k + 1.0)
              + gm_special_lgamma(total - col1 + 1.0) - gm_special_lgamma(row1 - k + 1.0) - gm_special_lgamma(total - col1 - row1 + k + 1.0)
              - (gm_special_lgamma(total + 1.0) - gm_special_lgamma(row1 + 1.0) - gm_special_lgamma(total - row1 + 1.0));
    return exp(ln);
}

gs_test_result gs_test_fisher_exact(int a, int b, int c, int d)
{
    gs_test_result r = {0.0, 1.0, NAN};
    int row1, row2, col1, total, lo, hi, v;
    gc_real obs, p = 0.0;
    if (a < 0 || b < 0 || c < 0 || d < 0) return r;
    row1 = a + b; row2 = c + d; col1 = a + c; total = row1 + row2;
    if (total == 0) return r;
    lo = (col1 - row2 > 0) ? col1 - row2 : 0;
    hi = col1 < row1 ? col1 : row1;
    obs = hyper_prob(a, row1, col1, total);
    for (v = lo; v <= hi; v++) {
        gc_real pr = hyper_prob(v, row1, col1, total);
        if (pr <= obs * (1.0 + 1e-12)) p += pr;
    }
    r.statistic = (b == 0 || c == 0) ? INFINITY : (gc_real)a * d / ((gc_real)b * c);
    r.pvalue = p > 1.0 ? 1.0 : p;
    return r;
}

gs_test_result gs_test_ks_2samp(const gc_real *x, int nx, const gc_real *y, int ny)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real *sx, *sy, dmax = 0.0;
    int i = 0, j = 0;
    if (!x || !y || nx < 1 || ny < 1) return r;
    sx = (gc_real *)malloc((size_t)nx * sizeof(gc_real));
    sy = (gc_real *)malloc((size_t)ny * sizeof(gc_real));
    if (!sx || !sy) { free(sx); free(sy); return r; }
    memcpy(sx, x, (size_t)nx * sizeof(gc_real));
    memcpy(sy, y, (size_t)ny * sizeof(gc_real));
    qsort(sx, (size_t)nx, sizeof(gc_real), cmp_real);
    qsort(sy, (size_t)ny, sizeof(gc_real), cmp_real);
    while (i < nx || j < ny) {
        gc_real val = (j >= ny || (i < nx && sx[i] <= sy[j])) ? sx[i] : sy[j];
        gc_real f1, f2, dd;
        while (i < nx && sx[i] <= val) i++;
        while (j < ny && sy[j] <= val) j++;
        f1 = (gc_real)i / nx; f2 = (gc_real)j / ny;
        dd = fabs(f1 - f2);
        if (dd > dmax) dmax = dd;
    }
    /* p-value asintotico (SciPy): 2*sum (-1)^{k-1} exp(-2 k^2 lambda) */
    {
        gc_real en = sqrt((gc_real)nx * ny / (nx + ny));
        gc_real lam = (en + 0.12 + 0.11 / en) * dmax;
        gc_real s = 0.0; int k;
        for (k = 1; k <= 100; k++) {
            gc_real term = exp(-2.0 * k * k * lam * lam);
            s += ((k % 2) ? 1.0 : -1.0) * term;
            if (term < 1e-14) break;
        }
        s *= 2.0;
        if (s < 0.0) s = 0.0; if (s > 1.0) s = 1.0;
        r.pvalue = s;
    }
    r.statistic = dmax;
    free(sx); free(sy);
    return r;
}

/* D'Agostino skew Z (approssimazione standard, scipy.stats.skewtest). */
static gc_real skew_test_z(const gc_real *x, int n)
{
    gc_real m = mean(x, n), m2 = 0.0, m3 = 0.0, g1, y, beta2, w2, delta, alpha, z;
    int i;
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; m2 += d * d; m3 += d * d * d; }
    m2 /= n; m3 /= n;
    g1 = m2 > 0 ? m3 / pow(m2, 1.5) : 0.0;
    y = g1 * sqrt(((gc_real)n + 1) * (n + 3) / (6.0 * (n - 2)));
    beta2 = 3.0 * ((gc_real)n * n + 27 * n - 70) * (n + 1) * (n + 3) / ((n - 2.0) * (n + 5) * (n + 7) * (n + 9));
    w2 = -1.0 + sqrt(2.0 * (beta2 - 1.0));
    delta = 1.0 / sqrt(0.5 * log(w2));
    alpha = sqrt(2.0 / (w2 - 1.0));
    z = delta * log(y / alpha + sqrt((y / alpha) * (y / alpha) + 1.0));
    return z;
}

static gc_real kurtosis_test_z(const gc_real *x, int n)
{
    gc_real m = mean(x, n), m2 = 0.0, m4 = 0.0, b2, e, varb2, xx, sqrtbeta1, a, term1, term2, z;
    int i;
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
    return z;
}

gs_test_result gs_test_normaltest(const gc_real *x, int n)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real z1, z2, k2;
    if (!x || n < 20) return r;
    z1 = skew_test_z(x, n); z2 = kurtosis_test_z(x, n);
    k2 = z1 * z1 + z2 * z2;
    r.statistic = k2; r.pvalue = 1.0 - gs_dist_chi2_cdf(2.0, k2); r.df = 2.0;
    return r;
}

gs_test_result gs_test_shapiro(const gc_real *x, int n)
{
    gs_test_result r = {NAN, NAN, NAN};
    gc_real *s;
    int i, half;
    gc_real *ns, sumsq = 0.0, rr, w1, w2, s2, scale, num = 0.0, m, ss, w;
    if (!x || n < 3 || n > 5000) return r;
    s = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!s) return r;
    memcpy(s, x, (size_t)n * sizeof(gc_real));
    qsort(s, (size_t)n, sizeof(gc_real), cmp_real);
    if (s[0] == s[n - 1]) { free(s); r.statistic = 1.0; r.pvalue = 1.0; return r; }
    half = n / 2;
    ns = (gc_real *)malloc((size_t)half * sizeof(gc_real));
    if (!ns) { free(s); return r; }
    for (i = 0; i < half; i++) {
        gc_real p = (i + 1.0 - 0.375) / (n + 0.25);
        ns[i] = gs_dist_normal_ppf(0.0, 1.0, p);
        sumsq += ns[i] * ns[i];
    }
    sumsq *= 2.0;
    rr = 1.0 / sqrt((gc_real)n);
    w1 = (0.221157 * rr - 0.147981 * rr * rr - 2.07119 * pow(rr, 3) + 4.434685 * pow(rr, 4) - 2.706056 * pow(rr, 5)) - ns[0] / sqrt(sumsq);
    w2 = 0.0;
    if (n > 5)
        w2 = (0.042981 * rr - 0.293762 * rr * rr - 1.752461 * pow(rr, 3) + 5.682633 * pow(rr, 4) - 3.582633 * pow(rr, 5)) - ns[1] / sqrt(sumsq);
    s2 = (n > 5) ? 2.0 * (ns[0] * ns[0] + ns[1] * ns[1]) : 2.0 * ns[0] * ns[0];
    scale = sqrt((sumsq - s2) / (1.0 - ((n > 5) ? 2.0 * (w1 * w1 + w2 * w2) : 2.0 * w1 * w1)));
    for (i = 0; i < half; i++) {
        gc_real wt = (i == 0) ? w1 : (i == 1 && n > 5 ? w2 : -ns[i] / scale);
        num += wt * (s[n - 1 - i] - s[i]);
    }
    m = mean(s, n);
    ss = 0.0;
    for (i = 0; i < n; i++) { gc_real d = s[i] - m; ss += d * d; }
    w = num * num / ss;
    if (w < 0.0) w = 0.0; if (w > 1.0) w = 1.0;
    r.statistic = w;
    /* p-value: AS R94 (semplificato, come il port C#) */
    {
        gc_real tr = log(1.0 - w), loc, sc, z;
        if (n == 3) { r.pvalue = 6.0 / 3.14159265358979323846 * (asin(sqrt(w)) - asin(sqrt(0.75))); if (r.pvalue < 0) r.pvalue = 0; }
        else if (n <= 11) {
            gc_real g = -2.273 + 0.459 * n;
            if (tr >= g) r.pvalue = 1e-300;
            else {
                tr = -log(g - tr);
                loc = 0.544 - 0.39978 * n + 0.025054 * n * n - 0.0006714 * n * n * n;
                sc = exp(1.3822 - 0.77857 * n + 0.062767 * n * n - 0.0020322 * n * n * n);
                z = (tr - loc) / sc;
                r.pvalue = 1.0 - gs_dist_normal_cdf(0, 1, z);
            }
        } else {
            gc_real ln = log((gc_real)n);
            loc = -1.5861 - 0.31082 * ln - 0.083751 * ln * ln + 0.0038915 * ln * ln * ln;
            sc = exp(-0.4803 - 0.082676 * ln + 0.0030302 * ln * ln);
            z = (tr - loc) / sc;
            r.pvalue = 1.0 - gs_dist_normal_cdf(0, 1, z);
        }
        if (r.pvalue < 0) r.pvalue = 0; if (r.pvalue > 1) r.pvalue = 1;
    }
    free(s); free(ns);
    return r;
}

gs_test_result gs_test_anova(const gc_real *groups, const int *sizes, int k)
{
    gs_test_result r = {0.0, NAN, NAN};
    void gs_analysis_anova(const double *, int *, int, double *, double *, double *, double *);
    double f = 0, p = 0, ssb = 0, ssw = 0;
    int *sz; int i;
    if (!groups || !sizes || k < 2) return r;
    sz = (int *)malloc((size_t)k * sizeof(int));
    if (!sz) return r;
    for (i = 0; i < k; i++) sz[i] = sizes[i];
    gs_analysis_anova(groups, sz, k, &f, &p, &ssb, &ssw);
    r.statistic = f; r.pvalue = p; r.df = (gc_real)(k - 1);
    free(sz);
    return r;
}

gs_test_result gs_test_ftest(const gc_real *x, int nx, const gc_real *y, int ny)
{
    gs_test_result r = {0.0, NAN, NAN};
    gc_real vx, vy, f, df1, df2, p;
    if (!x || !y || nx < 2 || ny < 2) return r;
    vx = sample_var(x, nx, mean(x, nx));
    vy = sample_var(y, ny, mean(y, ny));
    if (vx <= 0 || vy <= 0) return r;
    f = vx / vy; df1 = (gc_real)(nx - 1); df2 = (gc_real)(ny - 1);
    { gc_real cdf = gs_dist_f_cdf(df1, df2, f); p = 2.0 * (cdf < 0.5 ? cdf : 1.0 - cdf); }
    r.statistic = f; r.pvalue = p; r.df = df1;
    return r;
}

void gs_analysis_anova(const double* groups, int* groupSizes, int k, double* f, double* p, double* ssb, double* ssw)
{
    double grandMean = 0; int N = 0;
    const double* g = groups;
    for (int gi = 0; gi < k; gi++) { for (int i = 0; i < groupSizes[gi]; i++) grandMean += g[i]; g += groupSizes[gi]; N += groupSizes[gi]; }
    if (N == 0 || k < 2) { *f = 0; *p = 1; *ssb = 0; *ssw = 0; return; }
    grandMean /= N;
    double ssbV = 0, sswV = 0;
    g = groups;
    for (int gi = 0; gi < k; gi++)
    {
        double gm = 0;
        for (int i = 0; i < groupSizes[gi]; i++) gm += g[i];
        gm /= groupSizes[gi];
        ssbV += groupSizes[gi] * pow(gm - grandMean, 2);
        for (int i = 0; i < groupSizes[gi]; i++) sswV += pow(g[i] - gm, 2);
        g += groupSizes[gi];
    }
    double msb = ssbV / (k - 1);
    double msw = N > k ? sswV / (N - k) : 1;
    double fv = msw != 0 ? msb / msw : 0;
    /* gs_dist_f_cdf(d1=dfBetween=k-1, d2=dfWithin=N-k, x=fv) — prima gli argomenti erano
     * scambiati (fv, k-1, N-k): CDF valutata con d1=fv, d2=dfBetween, x=dfWithin → p-value
     * errato (es. 0.299 vs canone 0.0709 per F(1,6,4.8)). */
    double pv = fv > 0 ? 1.0 - gs_dist_f_cdf((double)(k - 1), (double)(N - k), fv) : 1.0;
    *f = fv; *p = pv; *ssb = ssbV; *ssw = sswV;
}
