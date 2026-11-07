/* gm_quant_fundamentals.h — metriche fondamentali per azione (namespace FUNDAMENTALS). */
#ifndef GMBQUANT_FUNDAMENTALS_H
#define GMBQUANT_FUNDAMENTALS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Book value per share: equity / azioni. */
double gq_book_value_per_share(double total_equity, double common_shares);
/* EPS diluito: net income / (azioni medie + strumenti convertibili). */
double gq_diluted_eps(double net_income, double avg_shares, double other_instruments);
/* Payout ratio: dividendi / reddito netto. */
double gq_dividend_payout_ratio(double dividends, double net_income);
/* Dividend yield in PERCENTUALE: dividendi / prezzo * 100 (canone StdLib). */
double gq_dividend_yield(double dividends, double price);
/* Dividendi per azione: dividendi / azioni. */
double gq_dps(double dividends, double num_shares);
/* Utile per azione: reddito netto / azioni medie. */
double gq_eps(double net_income, double avg_shares);
/* Moltiplicatore del patrimonio: attivo / patrimonio netto. */
double gq_equity_multiplier(double total_assets, double equity);
/* NAV per azione: (attivo - passivo) / azioni in circolazione. */
double gq_nav(double fund_assets, double fund_liabilities, double outstanding_shares);
/* Price-to-book. */
double gq_pb_ratio(double price, double book_value);
/* Price-to-earnings. */
double gq_pe_ratio(double price, double eps);
/* Price-to-sales. */
double gq_ps_ratio(double price, double sales_per_share);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_FUNDAMENTALS_H */
