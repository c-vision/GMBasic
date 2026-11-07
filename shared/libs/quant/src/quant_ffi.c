/*
 * quant_ffi.c — shim ABI per l'FFI di GMBASIC sulla libreria `libgmbquant`.
 *
 * Ogni funzione `ofx_q_*` ha la firma che il marshalling FFI sa costruire:
 *   scalari           -> double / int64_t
 *   array DOUBLE      -> (const double *a, int64_t n)  [auto-estensione d0]
 *   due array         -> (const double *a, int64_t na, const double *b, int64_t nb)
 *   array di uscita   -> (..., double *out, int64_t nout)
 *
 * Non c'e' stato: ogni chiamata e' indipendente. Gli input non validi producono 0
 * (stessa semantica dei builtin: nessuna eccezione oltre l'FFI).
 */
#include "gm_quant.h"

#include <stdint.h>

/* ── General (namespace TRADING) ─────────────────────────────────────────── */

double ofx_q_stock_zero_growth(double dividend, double rate)
{ return gq_stock_value_zero_growth(dividend, rate); }

double ofx_q_stock_const_growth(double next_dividend, double rate, double growth)
{ return gq_stock_value_const_growth(next_dividend, rate, growth); }

double ofx_q_preferred_value(double dividend, double rate)
{ return gq_preferred_stock_value(dividend, rate); }

double ofx_q_capm(double rf, double beta, double market)
{ return gq_capm(rf, beta, market); }

double ofx_q_risk_premium(double asset, double rf)
{ return gq_risk_premium(asset, rf); }

double ofx_q_total_stock_return(double p0, double p1, double dividends)
{ return gq_total_stock_return(p0, p1, dividends); }

double ofx_q_capital_gains_yield(double p0, double p1)
{ return gq_capital_gains_yield(p0, p1); }

double ofx_q_holding_period_return(const double *r, int64_t n)
{ return gq_holding_period_return(r, (int)n); }

double ofx_q_geometric_mean_return(const double *r, int64_t n)
{ return gq_geometric_mean_return(r, (int)n); }

double ofx_q_growth_rate(double retention, double roe)
{ return gq_growth_rate(retention, roe); }

double ofx_q_tax_equivalent_yield(double tfy, double tax)
{ return gq_tax_equivalent_yield(tfy, tax); }

double ofx_q_bid_ask_spread(double bid, double ask)
{ return gq_bid_ask_spread(bid, ask); }

/* ── Metrics (namespace METRICS) ─────────────────────────────────────────── */

double ofx_q_total_return(const double *r, int64_t n)
{ return gq_total_return(r, (int)n); }

double ofx_q_annualized_return(const double *r, int64_t n, int64_t ppy)
{ return gq_annualized_return(r, (int)n, (int)ppy); }

double ofx_q_sharpe(const double *r, int64_t n)
{ return gq_sharpe_ratio(r, (int)n); }

double ofx_q_sortino(const double *r, int64_t n, double rf, int64_t ppy)
{ return gq_sortino_ratio(r, (int)n, rf, (int)ppy); }

double ofx_q_calmar(const double *r, int64_t n, int64_t ppy)
{ return gq_calmar_ratio(r, (int)n, (int)ppy); }

double ofx_q_information(const double *r, int64_t nr, const double *b, int64_t nb, int64_t ppy)
{ return gq_information_ratio(r, b, (int)(nr < nb ? nr : nb), (int)ppy); }

double ofx_q_treynor(const double *r, int64_t nr, const double *b, int64_t nb,
                     double rf, int64_t ppy)
{ return gq_treynor_ratio(r, b, (int)(nr < nb ? nr : nb), rf, (int)ppy); }

double ofx_q_max_drawdown(const double *r, int64_t n)
{ return gq_max_drawdown(r, (int)n); }

double ofx_q_downside_deviation(const double *r, int64_t n, double threshold)
{ return gq_downside_deviation(r, (int)n, threshold); }

double ofx_q_value_at_risk(const double *r, int64_t n, double confidence)
{ return gq_value_at_risk(r, (int)n, confidence); }

double ofx_q_cvar(const double *r, int64_t n, double confidence)
{ return gq_conditional_value_at_risk(r, (int)n, confidence); }

double ofx_q_beta(const double *r, int64_t nr, const double *b, int64_t nb)
{ return gq_beta(r, b, (int)(nr < nb ? nr : nb)); }

double ofx_q_alpha(const double *r, int64_t nr, const double *b, int64_t nb,
                   double rf, int64_t ppy)
{ return gq_alpha(r, b, (int)(nr < nb ? nr : nb), rf, (int)ppy); }

double ofx_q_skewness(const double *r, int64_t n)
{ return gq_returns_skewness(r, (int)n); }

double ofx_q_kurtosis(const double *r, int64_t n)
{ return gq_returns_kurtosis(r, (int)n); }

int64_t ofx_q_jarque_bera(const double *r, int64_t n, double *out, int64_t nout)
{
    double tmp[2];
    int ok;
    if (!out || nout < 2) return 0;
    ok = gq_jarque_bera(r, (int)n, tmp);
    if (!ok) return 0;
    out[0] = tmp[0];
    out[1] = tmp[1];
    return 1;
}

double ofx_q_tracking_error(const double *r, int64_t nr, const double *b, int64_t nb, int64_t ppy)
{ return gq_annualized_tracking_error(r, b, (int)(nr < nb ? nr : nb), (int)ppy); }

double ofx_q_omega(const double *r, int64_t n, double threshold)
{ return gq_omega_ratio(r, (int)n, threshold); }

double ofx_q_ulcer(const double *r, int64_t n)
{ return gq_ulcer_index(r, (int)n); }

/* ── Volatility (namespace VOLATILITY) ───────────────────────────────────── */

double ofx_q_annualized_volatility(const double *r, int64_t n, int64_t ppy)
{ return gq_annualized_volatility(r, (int)n, (int)ppy); }

double ofx_q_vol_downside(const double *r, int64_t n, double threshold)
{ return gq_vol_downside_deviation(r, (int)n, threshold); }

double ofx_q_ann_downside(const double *r, int64_t n, double threshold, int64_t ppy)
{ return gq_annualized_downside_deviation(r, (int)n, threshold, (int)ppy); }

/* ── Fundamentals (namespace FUNDAMENTALS) ───────────────────────────────── */

double ofx_q_bvps(double equity, double shares)
{ return gq_book_value_per_share(equity, shares); }

double ofx_q_diluted_eps(double ni, double shares, double other)
{ return gq_diluted_eps(ni, shares, other); }

double ofx_q_payout_ratio(double dividends, double ni)
{ return gq_dividend_payout_ratio(dividends, ni); }

double ofx_q_dividend_yield(double dividends, double price)
{ return gq_dividend_yield(dividends, price); }

double ofx_q_dps(double dividends, double shares)
{ return gq_dps(dividends, shares); }

double ofx_q_eps(double ni, double shares)
{ return gq_eps(ni, shares); }

double ofx_q_equity_multiplier(double assets, double equity)
{ return gq_equity_multiplier(assets, equity); }

double ofx_q_nav(double assets, double liabilities, double shares)
{ return gq_nav(assets, liabilities, shares); }

double ofx_q_pb_ratio(double price, double book)
{ return gq_pb_ratio(price, book); }

double ofx_q_pe_ratio(double price, double eps)
{ return gq_pe_ratio(price, eps); }

double ofx_q_ps_ratio(double price, double sps)
{ return gq_ps_ratio(price, sps); }
