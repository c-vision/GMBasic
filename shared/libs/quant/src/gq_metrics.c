/* gq_metrics.c — metriche di performance e rischio (canone CommonLib.Trading.Metrics). */
#include "gm_quant_metrics.h"
#include "gq_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

double gq_total_return(const double *returns, int n)
{
    if (!returns || n <= 0) return 0.0;
    double t = 1.0;
    for (int i = 0; i < n; i++) t *= (1.0 + returns[i]);
    return t - 1.0;
}

double gq_annualized_return(const double *returns, int n, int periods_per_year)
{
    if (!returns || n <= 0 || periods_per_year <= 0) return 0.0;
    double tr = gq_total_return(returns, n);
    double base = 1.0 + tr;
    if (base < 0.0) return 0.0;
    return pow(base, (double)periods_per_year / (double)n) - 1.0;
}

/* TRADING.METRICS.SHARPERATIO(returns): mean / sample_std * sqrt(252). */
double gq_sharpe_ratio(const double *returns, int n)
{
    if (!returns || n < 2) return 0.0;
    double std = gq_std_sample(returns, n);
    if (std == 0.0) return 0.0;
    return (gq_mean(returns, n) / std) * sqrt(252.0);
}

double gq_sharpe_ratio_rf(const double *returns, int n, double risk_free, int periods_per_year)
{
    if (!returns || n < 2 || periods_per_year <= 0) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < n; i++) mean += returns[i] - risk_free;
    mean /= (double)n;
    double s = 0.0;
    for (int i = 0; i < n; i++) { double e = returns[i] - risk_free; s += (e - mean) * (e - mean); }
    double std = sqrt(s / (double)(n - 1));
    if (std == 0.0) return 0.0;
    return sqrt((double)periods_per_year) * mean / std;
}

double gq_sortino_ratio(const double *returns, int n, double risk_free, int periods_per_year)
{
    if (!returns || n < 2 || periods_per_year <= 0) return 0.0;
    double sum = 0.0, sum_sq = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; i++)
    {
        double e = returns[i] - risk_free;
        sum += e;
        if (e < 0.0) { sum_sq += e * e; cnt++; }
    }
    if (cnt == 0) return 0.0;
    double dd = sqrt(sum_sq / (double)cnt);
    if (dd == 0.0) return 0.0;
    return sqrt((double)periods_per_year) * (sum / (double)n) / dd;
}

double gq_calmar_ratio(const double *returns, int n, int periods_per_year)
{
    if (!returns || n <= 0) return 0.0;
    double mdd = gq_max_drawdown(returns, n);
    if (mdd == 0.0) return 0.0;
    return gq_annualized_return(returns, n, periods_per_year) / fabs(mdd);
}

double gq_information_ratio(const double *returns, const double *benchmark, int n, int periods_per_year)
{
    if (!returns || !benchmark || n < 2 || periods_per_year <= 0) return 0.0;
    double *diff = (double *)malloc(sizeof(double) * (size_t)n);
    if (!diff) return 0.0;
    for (int i = 0; i < n; i++) diff[i] = returns[i] - benchmark[i];
    double std = gq_std_sample(diff, n);
    double r;
    if (std == 0.0) r = 0.0;
    else r = sqrt((double)periods_per_year) * gq_mean(diff, n) / std;
    free(diff);
    return r;
}

double gq_treynor_ratio(const double *returns, const double *benchmark, int n,
                        double risk_free, int periods_per_year)
{
    if (!returns || !benchmark || n < 2 || periods_per_year <= 0) return 0.0;
    double beta = gq_beta(returns, benchmark, n);
    if (fabs(beta) < 1e-9) return 0.0;
    double ar = gq_annualized_return(returns, n, periods_per_year);
    double arf = pow(1.0 + risk_free, (double)periods_per_year) - 1.0;
    return (ar - arf) / beta;
}

double gq_max_drawdown(const double *returns, int n)
{
    if (!returns || n <= 0) return 0.0;
    double cum = 1.0 + returns[0], peak = cum, mdd = 0.0;
    for (int i = 1; i < n; i++)
    {
        cum *= (1.0 + returns[i]);
        if (cum > peak) peak = cum;
        double dd = (cum - peak) / peak;
        if (dd < mdd) mdd = dd;
    }
    return mdd;
}

double gq_downside_deviation(const double *returns, int n, double threshold)
{
    if (!returns || n <= 0) return 0.0;
    double sum = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; i++)
        if (returns[i] < threshold) { double d = returns[i] - threshold; sum += d * d; cnt++; }
    return cnt > 0 ? sqrt(sum / (double)cnt) : 0.0;
}

double gq_value_at_risk(const double *returns, int n, double confidence)
{
    if (!returns || n <= 0) return 0.0;
    int p = (int)floor((1.0 - confidence) * 100.0 + 0.5);
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    return gq_percentile_nearest(returns, n, (double)p);
}

double gq_conditional_value_at_risk(const double *returns, int n, double confidence)
{
    if (!returns || n <= 0) return 0.0;
    double v = gq_value_at_risk(returns, n, confidence);
    double sum = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; i++)
        if (returns[i] <= v) { sum += returns[i]; cnt++; }
    return cnt > 0 ? sum / (double)cnt : 0.0;
}

double gq_beta(const double *returns, const double *benchmark, int n)
{
    if (!returns || !benchmark || n < 2) return 0.0;
    double var_b = gq_var_sample(benchmark, n);
    if (var_b == 0.0) return 0.0;
    return gq_cov_sample(returns, benchmark, n) / var_b;
}

double gq_alpha(const double *returns, const double *benchmark, int n,
                double risk_free, int periods_per_year)
{
    if (!returns || !benchmark || n < 2) return 0.0;
    double beta = gq_beta(returns, benchmark, n);
    double ar = gq_annualized_return(returns, n, periods_per_year);
    double arb = gq_annualized_return(benchmark, n, periods_per_year);
    double arf = pow(1.0 + risk_free, (double)periods_per_year) - 1.0;
    return ar - (arf + beta * (arb - arf));
}

double gq_returns_skewness(const double *returns, int n)
{
    return gq_skew_sample(returns, n);
}

double gq_returns_kurtosis(const double *returns, int n)
{
    return gq_kurt_excess(returns, n);
}

/* Jarque-Bera: p-value = 1 - ChiSquared(2).CDF(stat) = exp(-stat / 2). */
int gq_jarque_bera(const double *returns, int n, double out[2])
{
    if (!out) return 0;
    if (!returns || n < 4) { out[0] = 0.0; out[1] = 1.0; return 1; }
    double s = gq_skew_sample(returns, n);
    double k = gq_kurt_excess(returns, n);
    double stat = ((double)n / 6.0) * (s * s + 0.25 * k * k);
    out[0] = stat;
    out[1] = exp(-stat / 2.0);
    return 1;
}

double gq_annualized_tracking_error(const double *returns, const double *benchmark, int n,
                                    int periods_per_year)
{
    if (!returns || !benchmark || n < 2 || periods_per_year <= 0) return 0.0;
    double *diff = (double *)malloc(sizeof(double) * (size_t)n);
    if (!diff) return 0.0;
    for (int i = 0; i < n; i++) diff[i] = returns[i] - benchmark[i];
    double r = sqrt((double)periods_per_year) * gq_std_sample(diff, n);
    free(diff);
    return r;
}

double gq_omega_ratio(const double *returns, int n, double threshold)
{
    if (!returns || n <= 0) return 0.0;
    double up = 0.0, dn = 0.0;
    for (int i = 0; i < n; i++)
    {
        double d = returns[i] - threshold;
        if (d > 0.0) up += d; else dn -= d;
    }
    if (dn == 0.0) return INFINITY;
    return up / dn;
}

double gq_ulcer_index(const double *returns, int n)
{
    if (!returns || n <= 0) return 0.0;
    double cum = 1.0 + returns[0], peak = cum, sum = 0.0;
    for (int i = 1; i < n; i++)
    {
        cum *= (1.0 + returns[i]);
        if (cum > peak) peak = cum;
        double dd = (cum - peak) / peak * 100.0;
        sum += dd * dd;
    }
    /* il primo periodo contribuisce con drawdown 0 (peak == cum). */
    return sqrt(sum / (double)n);
}
