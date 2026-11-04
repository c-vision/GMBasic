/* gmb_finance_qlnet.c — parità common/CommonLib/Trading/QLNet.cs:256 (PortfolioMath + cost/constraints)
 * QLNet è una libreria C# (NuGet) → NON linkabile dal runtime C. Si porta la parte
 * math pura (dot product, w'Σw, Sharpe, Markowitz/Kelly cost, vincoli) senza QLNet;
 * l'ottimizzazione usa i solver generici già presenti (gmb_opt_pso).
 */
#include "gmb_runtime.h"
#include <math.h>
#include <float.h>

double gmb_qlnet_portfolio_return(const double* w, const double* mu, int n)
{
    double r = 0;
    for (int i = 0; i < n; i++) r += w[i] * mu[i];
    return r;
}

void gmb_qlnet_portfolio_risk(const double* w, const double* cov, int n, double* variance, double* stdev)
{
    /* cov row-major n×n; variance = w' Σ w */
    double v = 0;
    for (int i = 0; i < n; i++) {
        double s = 0;
        for (int j = 0; j < n; j++) s += cov[(size_t)i * n + j] * w[j];
        v += w[i] * s;
    }
    if (v < 0) v = 0;
    if (variance) *variance = v;
    if (stdev) *stdev = sqrt(v);
}

double gmb_qlnet_sharpe(double portfolioReturn, double portfolioStDev, double riskFreeRate)
{
    return portfolioStDev > 1e-9 ? (portfolioReturn - riskFreeRate) / portfolioStDev : 0.0;
}

/* MarkowitzCostFunction.value: varianza w'Σw */
double gmb_qlnet_markowitz_value(const double* w, const double* cov, int n)
{
    double var;
    gmb_qlnet_portfolio_risk(w, cov, n, &var, NULL);
    return var;
}

/* KellyCostFunction.value: 0.5 w'Σw - w'μ */
double gmb_qlnet_kelly_value(const double* w, const double* cov, const double* mu, int n)
{
    return 0.5 * gmb_qlnet_markowitz_value(w, cov, n) - gmb_qlnet_portfolio_return(w, mu, n);
}

/* OneSumConstraint: |Σw - 1| < 1e-9 */
int gmb_qlnet_onesum(const double* w, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++) s += w[i];
    return fabs(s - 1.0) < 1e-9;
}

/* NonNegativeConstraint: tutti i pesi >= -1e-9 */
int gmb_qlnet_nonneg(const double* w, int n)
{
    for (int i = 0; i < n; i++) if (w[i] < -1e-9) return 0;
    return 1;
}

/* TargetReturnConstraint: w'μ >= target - 1e-9 */
int gmb_qlnet_target_return(const double* w, const double* mu, int n, double target)
{
    return gmb_qlnet_portfolio_return(w, mu, n) >= target - 1e-9;
}

/* BoundaryConstraint: lower[i] <= w[i] <= upper[i] */
int gmb_qlnet_boundary(const double* w, const double* lower, const double* upper, int n)
{
    for (int i = 0; i < n; i++) {
        if (w[i] < -1e-9) return 0;
        if (lower && w[i] < lower[i] - 1e-9) return 0;
        if (upper && w[i] > upper[i] + 1e-9) return 0;
    }
    return 1;
}
