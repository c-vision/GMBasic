/* gs_estimation.c — MLE di distribuzioni e model selection (AIC/BIC).
 * Progetto gmbstats-fitting (link minimo). */
#include "gs_estimation.h"
#include "gm_special.h"

#include <math.h>
#include <stdlib.h>

static int _mean_sd(const double *x, int n, double *mu, double *sigma)
{
    int i;
    double m = 0.0, v = 0.0;
    if (!x || n < 1) return -1;
    for (i = 0; i < n; i++) m += x[i];
    m /= (double)n;
    for (i = 0; i < n; i++) v += (x[i] - m) * (x[i] - m);
    *mu = m;
    *sigma = sqrt(v / (double)n);
    return 0;
}


int gs_fit_normal(const double *x, int n, double *mu, double *sigma)
{
    if (!mu || !sigma) return -1;
    return _mean_sd(x, n, mu, sigma);
}

double gs_loglik_normal(const double *x, int n, double mu, double sigma)
{
    int i;
    double s = 0.0;
    if (!x || n < 1 || sigma <= 0.0) return 0.0 / 0.0;
    for (i = 0; i < n; i++) { double d = x[i] - mu; s += d * d; }
    return -0.5 * (double)n * (log(2.0 * 3.14159265358979323846) + log(sigma * sigma))
           - s / (2.0 * sigma * sigma);
}

int gs_fit_lognormal(const double *x, int n, double *mu, double *sigma)
{
    double *lx;
    int i, rc;
    if (!x || n < 1 || !mu || !sigma) return -1;
    lx = (double *)malloc(sizeof(double) * (size_t)n);
    if (!lx) return -1;
    for (i = 0; i < n; i++) {
        if (x[i] <= 0.0) { free(lx); return -1; }
        lx[i] = log(x[i]);
    }
    rc = _mean_sd(lx, n, mu, sigma);
    free(lx);
    return rc;
}

int gs_fit_exponential(const double *x, int n, double *lambda)
{
    int i;
    double m = 0.0;
    if (!x || n < 1 || !lambda) return -1;
    for (i = 0; i < n; i++) m += x[i];
    m /= (double)n;
    if (m <= 0.0) return -1;
    *lambda = 1.0 / m;
    return 0;
}

double gs_loglik_exponential(const double *x, int n, double lambda)
{
    int i;
    double s = 0.0;
    if (!x || n < 1 || lambda <= 0.0) return 0.0 / 0.0;
    for (i = 0; i < n; i++) s += x[i];
    return (double)n * log(lambda) - lambda * s;
}

int gs_fit_poisson(const double *x, int n, double *lambda)
{
    int i;
    double m = 0.0;
    if (!x || n < 1 || !lambda) return -1;
    for (i = 0; i < n; i++) m += x[i];
    *lambda = m / (double)n;
    return 0;
}

double gs_loglik_poisson(const double *x, int n, double lambda)
{
    int i;
    double ll = 0.0;
    if (!x || n < 1 || lambda <= 0.0) return 0.0 / 0.0;
    for (i = 0; i < n; i++) ll += x[i] * log(lambda) - lambda - lgamma(x[i] + 1.0);
    return ll;
}

int gs_fit_gamma(const double *x, int n, double *shape, double *scale)
{
    int i, it;
    double xbar = 0.0, mlog = 0.0, s, k, h = 1e-4;
    if (!x || n < 1 || !shape || !scale) return -1;
    for (i = 0; i < n; i++) {
        if (x[i] <= 0.0) return -1;
        xbar += x[i]; mlog += log(x[i]);
    }
    xbar /= (double)n; mlog /= (double)n;
    s = log(xbar) - mlog;
    k = (s > 0.0) ? (3.0 - s + sqrt((3.0 - s) * (3.0 - s) + 24.0 * s)) / (12.0 * s) : 1.0;
    for (it = 0; it < 200; it++) {
        double f = log(k) - gm_special_digamma(k) - s;
        double dpsi = (gm_special_digamma(k + h) - gm_special_digamma(k - h)) / (2.0 * h);
        double fp = 1.0 / k - dpsi;
        double step;
        if (fabs(fp) < 1e-14) break;
        step = f / fp;
        k -= step;
        if (k <= 1e-9) k = 1e-9;
        if (fabs(step) < 1e-10) break;
    }
    *shape = k;
    *scale = xbar / k;
    return 0;
}

double gs_loglik_gamma(const double *x, int n, double shape, double scale)
{
    int i;
    double ll = 0.0;
    if (!x || n < 1 || shape <= 0.0 || scale <= 0.0) return 0.0 / 0.0;
    for (i = 0; i < n; i++) {
        if (x[i] <= 0.0) return -1.0 / 0.0;
        ll += (shape - 1.0) * log(x[i]) - x[i] / scale
              - shape * log(scale) - lgamma(shape);
    }
    return ll;
}

int gs_fit_weibull(const double *x, int n, double *shape, double *scale)
{
    int i, it;
    double mlog = 0.0, k = 1.0, h = 1e-4;
    if (!x || n < 1 || !shape || !scale) return -1;
    for (i = 0; i < n; i++) {
        if (x[i] <= 0.0) return -1;
        mlog += log(x[i]);
    }
    mlog /= (double)n;
    for (it = 0; it < 200; it++) {
        double s1 = 0.0, s2 = 0.0, g, gp, step;
        int j;
        for (j = 0; j < n; j++) {
            double xk = pow(x[j], k);
            s1 += xk * log(x[j]);
            s2 += xk;
        }
        g = s1 / s2 - 1.0 / k - mlog;
        /* derivata numerica */
        {
            double kp = k + h, km = k - h, a1 = 0.0, a2 = 0.0, b1 = 0.0, b2 = 0.0;
            for (j = 0; j < n; j++) {
                double xp = pow(x[j], kp), xm = pow(x[j], km);
                a1 += xp * log(x[j]); a2 += xp;
                b1 += xm * log(x[j]); b2 += xm;
            }
            gp = ((a1 / a2 - 1.0 / kp) - (b1 / b2 - 1.0 / km)) / (2.0 * h);
        }
        if (fabs(gp) < 1e-14) break;
        step = g / gp;
        k -= step;
        if (k <= 1e-9) k = 1e-9;
        if (fabs(step) < 1e-10) break;
    }
    {
        double s2 = 0.0;
        for (i = 0; i < n; i++) s2 += pow(x[i], k);
        *shape = k;
        *scale = pow(s2 / (double)n, 1.0 / k);
    }
    return 0;
}

double gs_loglik_weibull(const double *x, int n, double shape, double scale)
{
    int i;
    double ll = 0.0;
    if (!x || n < 1 || shape <= 0.0 || scale <= 0.0) return 0.0 / 0.0;
    for (i = 0; i < n; i++) {
        if (x[i] <= 0.0) return -1.0 / 0.0;
        ll += log(shape) - shape * log(scale) + (shape - 1.0) * log(x[i])
              - pow(x[i] / scale, shape);
    }
    return ll;
}

double gs_aic(double loglik, int k) { return 2.0 * (double)k - 2.0 * loglik; }
double gs_bic(double loglik, int k, int n)
{
    return (double)k * log((double)n) - 2.0 * loglik;
}

/* ---- Gambler's ruin ---- */

