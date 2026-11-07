/* gm_quant_general.h — valutazione azionaria e rendimenti (namespace TRADING). */
#ifndef GMBQUANT_GENERAL_H
#define GMBQUANT_GENERAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Valore dell'azione a crescita zero: D / r. */
double gq_stock_value_zero_growth(double dividend, double required_rate);
/* Valore con crescita costante (Gordon): D1 / (r - g). */
double gq_stock_value_const_growth(double next_dividend, double rate, double growth);
/* Valore di un'azione privilegiata: D / r. */
double gq_preferred_stock_value(double dividend, double discount_rate);
/* CAPM: rf + beta * (rm - rf). */
double gq_capm(double risk_free_rate, double beta, double market_return);
/* Premio al rischio: rendimento - risk free. */
double gq_risk_premium(double asset_return, double risk_free_return);
/* Rendimento totale azionario: ((P1 - P0) + dividendi) / P0. */
double gq_total_stock_return(double initial_price, double end_price, double dividends);
/* Capital gains yield: (P1 - P0) / P0. */
double gq_capital_gains_yield(double initial_price, double end_price);
/* Holding period return: prod(1 + r_i) - 1. */
double gq_holding_period_return(const double *returns, int n);
/* Media geometrica dei rendimenti: prod(1 + r_i)^(1/n) - 1. */
double gq_geometric_mean_return(const double *returns, int n);
/* Tasso di crescita sostenibile: retention * ROE. */
double gq_growth_rate(double retention_rate, double roe);
/* Rendimento equivalente tassabile: tfy / (1 - tax_rate). */
double gq_tax_equivalent_yield(double tax_free_yield, double tax_rate);
/* Spread bid-ask: ask - bid. */
double gq_bid_ask_spread(double bid, double ask);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_GENERAL_H */
