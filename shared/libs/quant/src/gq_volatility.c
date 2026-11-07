/* gq_volatility.c — volatilità e deviazione downside (namespace VOLATILITY). */
#include "gm_quant_volatility.h"
#include "gq_internal.h"

#include <math.h>

double gq_annualized_volatility(const double *returns, int n, int periods_per_year)
{
    if (!returns || n < 2 || periods_per_year <= 0) return 0.0;
    return sqrt((double)periods_per_year) * gq_std_sample(returns, n);
}

double gq_vol_downside_deviation(const double *returns, int n, double threshold)
{
    if (!returns || n <= 0) return 0.0;
    double sum = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; i++)
        if (returns[i] < threshold) { double d = returns[i] - threshold; sum += d * d; cnt++; }
    return cnt > 0 ? sqrt(sum / (double)cnt) : 0.0;
}

double gq_annualized_downside_deviation(const double *returns, int n, double threshold,
                                        int periods_per_year)
{
    if (periods_per_year <= 0) return 0.0;
    return sqrt((double)periods_per_year) * gq_vol_downside_deviation(returns, n, threshold);
}
