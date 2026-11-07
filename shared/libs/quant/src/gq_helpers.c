/* gq_helpers.c — medie, varianza, covarianza, momenti, percentile, ordinamento. */
#include "gq_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void gq_sort(double *x, int n)
{
    for (int i = 1; i < n; i++)
    {
        double key = x[i];
        int j = i - 1;
        while (j >= 0 && x[j] > key) { x[j + 1] = x[j]; j--; }
        x[j + 1] = key;
    }
}

double gq_mean(const double *x, int n)
{
    if (!x || n <= 0) return 0.0;
    double s = 0.0;
    for (int i = 0; i < n; i++) s += x[i];
    return s / (double)n;
}

double gq_var_sample(const double *x, int n)
{
    if (!x || n <= 1) return 0.0;
    double m = gq_mean(x, n), s = 0.0;
    for (int i = 0; i < n; i++) { double d = x[i] - m; s += d * d; }
    return s / (double)(n - 1);
}

double gq_std_sample(const double *x, int n)
{
    return sqrt(gq_var_sample(x, n));
}

double gq_cov_sample(const double *x, const double *y, int n)
{
    if (!x || !y || n <= 1) return 0.0;
    double mx = gq_mean(x, n), my = gq_mean(y, n), s = 0.0;
    for (int i = 0; i < n; i++) s += (x[i] - mx) * (y[i] - my);
    return s / (double)(n - 1);
}

/* Skewness campionaria con correzione di Fisher (MathNet): G1 * sqrt(n(n-1))/(n-2). */
double gq_skew_sample(const double *x, int n)
{
    if (!x || n < 3) return 0.0;
    double m = gq_mean(x, n), s2 = 0.0, s3 = 0.0;
    for (int i = 0; i < n; i++) { double d = x[i] - m; s2 += d * d; s3 += d * d * d; }
    double var = s2 / (double)n;
    double std3 = pow(var, 1.5);
    if (std3 == 0.0) return 0.0;
    double g1 = (s3 / (double)n) / std3;
    return g1 * sqrt((double)n * (double)(n - 1)) / (double)(n - 2);
}

/* Curtosi campionaria in eccesso non distorta (MathNet). */
double gq_kurt_excess(const double *x, int n)
{
    if (!x || n < 4) return 0.0;
    double m = gq_mean(x, n), m2 = 0.0, m4 = 0.0;
    for (int i = 0; i < n; i++) { double d = x[i] - m; m2 += d * d; m4 += d * d * d * d; }
    m2 /= (double)n;
    m4 /= (double)n;
    if (m2 == 0.0) return 0.0;
    double g2 = m4 / (m2 * m2);
    double dn = (double)n;
    return (dn - 1.0) / ((dn - 2.0) * (dn - 3.0)) * ((dn + 1.0) * g2 - 3.0 * (dn - 1.0));
}

/* Percentile nearest di MathNet: idx = round(p/100 * (n-1)) con banker's rounding. */
double gq_percentile_nearest(const double *x, int n, double pct)
{
    if (!x || n <= 0) return 0.0;
    double *s = (double *)malloc(sizeof(double) * (size_t)n);
    if (!s) return 0.0;
    memcpy(s, x, sizeof(double) * (size_t)n);
    gq_sort(s, n);
    double pos = (pct / 100.0) * (double)(n - 1);
    double fl = floor(pos);
    double diff = pos - fl;
    long idx = (long)fl;
    if (diff > 0.5) idx++;
    else if (diff == 0.5 && ((long)fl % 2) != 0) idx++;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    double r = s[idx];
    free(s);
    return r;
}
