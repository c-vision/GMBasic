/* gq_general.c — valutazione azionaria e rendimenti (semantica CommonLib.Trading.General). */
#include "gm_quant_general.h"

#include <math.h>

double gq_stock_value_zero_growth(double dividend, double required_rate)
{
    if (required_rate == 0.0) return 0.0;
    return dividend / required_rate;
}

double gq_stock_value_const_growth(double next_dividend, double rate, double growth)
{
    double den = rate - growth;
    if (den == 0.0 || den < 0.0) return 0.0;
    return next_dividend / den;
}

double gq_preferred_stock_value(double dividend, double discount_rate)
{
    if (discount_rate == 0.0) return 0.0;
    return dividend / discount_rate;
}

double gq_capm(double risk_free_rate, double beta, double market_return)
{
    return risk_free_rate + beta * (market_return - risk_free_rate);
}

double gq_risk_premium(double asset_return, double risk_free_return)
{
    return asset_return - risk_free_return;
}

double gq_total_stock_return(double initial_price, double end_price, double dividends)
{
    if (initial_price == 0.0) return 0.0;
    return ((end_price - initial_price) + dividends) / initial_price;
}

double gq_capital_gains_yield(double initial_price, double end_price)
{
    if (initial_price == 0.0) return 0.0;
    return (end_price - initial_price) / initial_price;
}

double gq_holding_period_return(const double *returns, int n)
{
    if (!returns || n <= 0) return 0.0;
    double t = 1.0;
    for (int i = 0; i < n; i++) t *= (1.0 + returns[i]);
    return t - 1.0;
}

double gq_geometric_mean_return(const double *returns, int n)
{
    if (!returns || n <= 0) return 0.0;
    double p = 1.0;
    for (int i = 0; i < n; i++) p *= (1.0 + returns[i]);
    if (p <= 0.0) return 0.0;
    return pow(p, 1.0 / (double)n) - 1.0;
}

double gq_growth_rate(double retention_rate, double roe)
{
    return retention_rate * roe;
}

double gq_tax_equivalent_yield(double tax_free_yield, double tax_rate)
{
    if (tax_rate < 0.0 || tax_rate >= 1.0) return 0.0;
    return tax_free_yield / (1.0 - tax_rate);
}

double gq_bid_ask_spread(double bid, double ask)
{
    return ask - bid;
}
