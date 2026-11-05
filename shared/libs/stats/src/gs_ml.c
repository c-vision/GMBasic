/* gs_ml.c — ML/copule multivariate (gmbstats-multivariate). */
#include "gs_sim.h"
#include "gs_dist.h"
#include "gs_dist2.h"
#include "gm_linalg.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
double gs_copula_gaussian_cdf2(double u, double v, double rho)
{
    double a, b;
    if (rho >= 1) rho = 0.999999;
    if (rho <= -1) rho = -0.999999;
    a = gs_dist2_normal_quantile(u);
    b = gs_dist2_normal_quantile(v);
    return bvn_cdf(a, b, rho);
}

/* Joe: C = 1 - ((1-u)^a + (1-v)^a - (1-u)^a (1-v)^a)^(1/a). */
double gs_copula_joe_cdf(double u, double v, double alpha)
{
    double du = 1.0 - u, dv = 1.0 - v;
    double t;
    if (alpha <= 1) return u * v;
    t = pow(du, alpha) + pow(dv, alpha) - pow(du * dv, alpha);
    return 1.0 - pow(t > 0 ? t : 0.0, 1.0 / alpha);
}
/* N13: C = (1 + ((u^-1-1)^a + (v^-1-1)^a)^(1/a))^-1. */
double gs_copula_n13_cdf(double u, double v, double alpha)
{
    double a = 1.0 / u - 1.0, b = 1.0 / v - 1.0;
    if (alpha <= 0) return u * v;
    return 1.0 / (1.0 + pow(pow(a, alpha) + pow(b, alpha), 1.0 / alpha));
}
/* N14: C = (1 + ((u^-1/a-1)^a + (v^-1/a-1)^a)^(1/a))^-a. */
double gs_copula_n14_cdf(double u, double v, double alpha)
{
    double a = 1.0 / u - 1.0, b = 1.0 / v - 1.0;
    double g = pow(pow(a, alpha) + pow(b, alpha), 1.0 / alpha);
    if (alpha <= 0) return u * v;
    return pow(1.0 + g, -alpha);
}

/* ---- K-means Lloyd (da machine-learning-tools). X nxd, K cluster.
 * out_centers: Kxd, out_assign: n. Ritorna 0/-1. */
double gs_edit_distance_sim(const char **a, int m, const char **b, int n)
{
    int *dp;
    int i, j, d;
    if (!a || !b) return 0.0 / 0.0;
    dp = (int *)malloc((size_t)(n + 1) * sizeof(int));
    if (!dp) return 0.0 / 0.0;
    for (j = 0; j <= n; j++) dp[j] = j;
    for (i = 1; i <= m; i++) {
        int prev = dp[0];
        dp[0] = i;
        for (j = 1; j <= n; j++) {
            int tmp = dp[j];
            int cost = (strcmp(a[i - 1], b[j - 1]) == 0) ? 0 : 1;
            int v = dp[j - 1] + 1;
            int v2 = dp[j] + 1;
            int v3 = prev + cost;
            dp[j] = v < v2 ? (v < v3 ? v : v3) : (v2 < v3 ? v2 : v3);
            prev = tmp;
        }
    }
    d = dp[n];
    free(dp);
    return 1.0 - (double)d / (m + n > 0 ? m + n : 1);
}

/* ---- Loughran-McDonald composite sentiment: (pos - neg)/words. ---- */
int gs_kmeans(const double *x, int n, int d, int k, int max_iter,
              double *out_centers, int *out_assign)
{
    double *cent, *cnew, best;
    int *count;
    int iter, i, j, c;
    if (!x || !out_centers || !out_assign || n < k || k < 1 || d < 1 || max_iter < 1) return -1;
    cent = (double *)malloc((size_t)k * d * sizeof(double));
    cnew = (double *)malloc((size_t)k * d * sizeof(double));
    count = (int *)malloc((size_t)k * sizeof(int));
    if (!cent || !cnew || !count) { free(cent); free(cnew); free(count); return -1; }
    for (c = 0; c < k; c++) for (j = 0; j < d; j++) cent[c * d + j] = x[c * d + j];
    for (iter = 0; iter < max_iter; iter++) {
        int changed = 0;
        for (i = 0; i < n; i++) {
            best = 1e300;
            for (c = 0; c < k; c++) {
                double s = 0.0;
                for (j = 0; j < d; j++) { double dd = x[i * d + j] - cent[c * d + j]; s += dd * dd; }
                if (s < best) { best = s; out_assign[i] = c; }
            }
        }
        for (c = 0; c < k; c++) { count[c] = 0; for (j = 0; j < d; j++) cnew[c * d + j] = 0.0; }
        for (i = 0; i < n; i++) {
            c = out_assign[i];
            count[c]++;
            for (j = 0; j < d; j++) cnew[c * d + j] += x[i * d + j];
        }
        for (c = 0; c < k; c++) {
            if (count[c] > 0) {
                double prev;
                for (j = 0; j < d; j++) { prev = cent[c * d + j]; cent[c * d + j] = cnew[c * d + j] / count[c]; if (fabs(cent[c * d + j] - prev) > 1e-12) changed = 1; }
            }
        }
        if (!changed) break;
    }
    for (c = 0; c < k; c++) for (j = 0; j < d; j++) out_centers[c * d + j] = cent[c * d + j];
    free(cent); free(cnew); free(count);
    return 0;
}

/* ---- TLS (orthogonal distance) regression: minimizza la distanza
 * perpendicolare. slope = +/- sqrt della soluzione; implementazione via SVD
 * della matrice [x-xbar, y-ybar]. Ritorna slope e intercept. */
int gs_nb_bernoulli_fit(const double *X, const double *y, int n, int d, int k,
                        double *prior, double *alpha)
{
    int c, j, i;
    if (!X || !y || !prior || !alpha || n < 1 || d < 1 || k < 1) return -1;
    for (c = 0; c < k; c++) { prior[c] = 0.0; for (j = 0; j < d; j++) alpha[c * d + j] = 0.0; }
    for (i = 0; i < n; i++) {
        int ci = (int)y[i];
        prior[ci] += 1.0;
        for (j = 0; j < d; j++) alpha[ci * d + j] += X[i * d + j];
    }
    for (c = 0; c < k; c++) {
        prior[c] /= n;
        for (j = 0; j < d; j++) alpha[c * d + j] = (alpha[c * d + j] + 1.0) / (prior[c] * n + 2.0);
    }
    return 0;
}
int gs_nb_bernoulli_predict(const double *prior, const double *alpha, const double *x,
                            int d, int k, int *label)
{
    int c, best = 0, j;
    double bs = -1e300;
    if (!prior || !alpha || !x || !label || d < 1 || k < 1) return -1;
    for (c = 0; c < k; c++) {
        double gx = log(prior[c] > 0 ? prior[c] : 1e-12);
        for (j = 0; j < d; j++) {
            double a = alpha[c * d + j];
            if (a <= 0) a = 1e-12;
            if (a >= 1) a = 1 - 1e-12;
            gx += x[j] > 0 ? log(a) : log1p(-a);
        }
        if (gx > bs) { bs = gx; best = c; }
    }
    *label = best;
    return 0;
}

/* ---- Levenshtein word-level similarity: 1 - dist/(m+n). ---- */
int gs_svm_multiclass(const double *X, const double *y, int n, int d, int k,
                      double C, int max_iter, double *W)
{
    double eta;
    int iter, i, j, c, changed = 1;
    if (!X || !y || !W || n < 1 || d < 1 || k < 2 || C <= 0 || max_iter < 1) return -1;
    for (i = 0; i < k * d; i++) W[i] = 0.0;
    eta = C / n;
    for (iter = 0; iter < max_iter && changed; iter++) {
        changed = 0;
        for (i = 0; i < n; i++) {
            const double *xi = X + (size_t)i * d;
            int yi = (int)y[i];
            for (c = 0; c < k; c++) {
                int lbl = (c == yi) ? 1 : -1;
                double margin = 0.0;
                for (j = 0; j < d; j++) margin += W[c * d + j] * xi[j];
                if (lbl * margin < 1.0) {
                    for (j = 0; j < d; j++) W[c * d + j] += eta * lbl * xi[j];
                    changed = 1;
                }
            }
        }
    }
    return 0;
}
int gs_svm_predict(const double *W, const double *x, int d, int k, int *label)
{
    int c, best = 0, j;
    double bs = -1e300;
    if (!W || !x || !label || d < 1 || k < 1) return -1;
    for (c = 0; c < k; c++) {
        double s = 0.0;
        for (j = 0; j < d; j++) s += W[c * d + j] * x[j];
        if (s > bs) { bs = s; best = c; }
    }
    *label = best;
    return 0;
}

/* ---- Bernoulli Naive Bayes (feature x_j in {0,1}). ---- */
int gs_tls_regression(const double *x, const double *y, int n, double *slope, double *intercept)
{
    double mx = 0, my = 0;
    double sxx = 0, sxy = 0, syy = 0, s, c1, c2;
    int i;
    if (!x || !y || !slope || !intercept || n < 2) return -1;
    for (i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;
    for (i = 0; i < n; i++) {
        double dx = x[i] - mx, dy = y[i] - my;
        sxx += dx * dx; sxy += dx * dy; syy += dy * dy;
    }
    s = sqrt((sxx - syy) * (sxx - syy) + 4.0 * sxy * sxy);
    c1 = (syy - sxx) + s;
    c2 = 2.0 * sxy;
    if (fabs(c2) < 1e-12) {
        *slope = 0.0;
    } else {
        *slope = c1 / c2;
    }
    *intercept = my - (*slope) * mx;
    return 0;
}

/* ---- Probabilistic OBV: buy = V*Phi(dC/std), OBV += |buy - sell| * sign. ---- */
