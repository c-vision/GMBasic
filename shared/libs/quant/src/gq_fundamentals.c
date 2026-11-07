/* gq_fundamentals.c — metriche fondamentali (namespace FUNDAMENTALS). */
#include "gm_quant_fundamentals.h"

double gq_book_value_per_share(double total_equity, double common_shares)
{
    if (common_shares == 0.0) return 0.0;
    return total_equity / common_shares;
}

double gq_diluted_eps(double net_income, double avg_shares, double other_instruments)
{
    double den = avg_shares + other_instruments;
    if (den == 0.0) return 0.0;
    return net_income / den;
}

double gq_dividend_payout_ratio(double dividends, double net_income)
{
    if (net_income == 0.0) return 0.0;
    return dividends / net_income;
}

/* Canone StdLib: percentuale (5 su 100 -> 5.0). */
double gq_dividend_yield(double dividends, double price)
{
    if (price == 0.0) return 0.0;
    return dividends / price * 100.0;
}

double gq_dps(double dividends, double num_shares)
{
    if (num_shares == 0.0) return 0.0;
    return dividends / num_shares;
}

double gq_eps(double net_income, double avg_shares)
{
    if (avg_shares == 0.0) return 0.0;
    return net_income / avg_shares;
}

double gq_equity_multiplier(double total_assets, double equity)
{
    if (equity == 0.0) return 0.0;
    return total_assets / equity;
}

double gq_nav(double fund_assets, double fund_liabilities, double outstanding_shares)
{
    if (outstanding_shares == 0.0) return 0.0;
    return (fund_assets - fund_liabilities) / outstanding_shares;
}

double gq_pb_ratio(double price, double book_value)
{
    if (book_value == 0.0) return 0.0;
    return price / book_value;
}

double gq_pe_ratio(double price, double eps)
{
    if (eps == 0.0) return 0.0;
    return price / eps;
}

double gq_ps_ratio(double price, double sales_per_share)
{
    if (sales_per_share == 0.0) return 0.0;
    return price / sales_per_share;
}
