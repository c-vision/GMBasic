/* ============================================================================
 * gmb_finance.h — Formule finanziarie (portate da CommonLib.Numerics / Trading).
 * ============================================================================ */
#ifndef GMB_FINANCE_H
#define GMB_FINANCE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Errore dell'ULTIMA funzione finance: "" se ok, altrimenti il messaggio (parità dei
 * throw ArgumentException del C#). Il chiamante (VM/C, C99) lo controlla e solleva. */
const char* gmb_fin_last_error(void);
void gmb_fin_clear_error(void);
void gmb_fin_set_error(const char* msg);

double gmb_fin_pmt(double rate, double nper, double pv, double fv, int type);
double gmb_fin_cumipmt(double rate, double nper, double pv, int start, int end, int type);
double gmb_fin_cumprinc(double rate, double nper, double pv, int start, int end, int type);
double gmb_fin_db(double cost, double salvage, double life, double period, int month);
double gmb_fin_ddb(double cost, double salvage, double life, double period, double factor);
double gmb_fin_rate(double nper, double pmt, double pv, double fv, int type, double guess);
double gmb_fin_bs_call(double s, double k, double t, double r, double v);
double gmb_fin_bs_put(double s, double k, double t, double r, double v);
double gmb_fin_merton_call(double s, double k, double t, double r, double q, double v);
double gmb_fin_merton_put(double s, double k, double t, double r, double q, double v);
double gmb_fin_delta(double s, double k, double t, double r, double v, int is_put);
double gmb_fin_gamma(double s, double k, double t, double r, double v);
double gmb_fin_vega(double s, double k, double t, double r, double v);
double gmb_fin_theta(double s, double k, double t, double r, double v, int is_put);
double gmb_fin_rho(double s, double k, double t, double r, double v, int is_put);
double gmb_fin_vanna(double s, double k, double t, double r, double v);
double gmb_fin_volga(double s, double k, double t, double r, double v);
double gmb_fin_speed(double s, double k, double t, double r, double v);
double gmb_fin_zomma(double s, double k, double t, double r, double v);
double gmb_fin_charm(double s, double k, double t, double r, double v, int is_put);
double gmb_fin_color(double s, double k, double t, double r, double v);
double gmb_trd_stock_const_growth(double nextDividend, double rate, double growth);
double gmb_trd_capm(double riskFreeRate, double beta, double marketReturn);
double gmb_trd_bond_equiv_yield(double faceValue, double bondPrice, double daysToMaturity);
double gmb_trd_current_yield(double annualCoupons, double bondPrice);
double gmb_trd_ytm(double couponPayment, double faceValue, double price, double years);
double gmb_trd_zc_bond_value(double faceValue, double rate, double time);
double gmb_trd_zc_bond_yield(double faceValue, double price, double time);
double gmb_trd_bond_price(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency);
double gmb_trd_bond_yield(double oa_settle, double oa_mat, double rate, double pr, double redemption, int frequency);
double gmb_fin_binomial(double s, double k, double t, double r, double v, int steps, int is_put, int is_american);
double gmb_trd_dps(double dividends, double numShares);
double gmb_trd_eps(double netIncome, double avgShares);
double gmb_trd_nav(double fundAssets, double fundLiabilities, double outstandingShares);
double gmb_trd_returns_kurtosis(const double* returns, int n);
double gmb_fin_nper_ext(double rate, double pmt, double pv, double fv, int type);
double gmb_fin_price_simple(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency);
double gmb_trd_equity_multiplier(double totalAssets, double equity);
double gmb_trd_total_return(const double* r, int n);
double gmb_trd_annualized_return(const double* r, int n, int ppy);
double gmb_trd_sharpe(const double* r, int n, double rf, int ppy);
double gmb_trd_sortino(const double* r, int n, double rf, int ppy);
double gmb_trd_max_drawdown(const double* r, int n);
double gmb_trd_beta(const double* r, const double* b, int n);
double gmb_trd_alpha(const double* r, const double* b, int n, double rf, int ppy);
double gmb_trd_pe_ratio(double price, double eps);
double gmb_trd_dividend_yield(double dps, double price);
double gmb_trd_var(const double* r, int n, double conf);
double gmb_trd_cvar(const double* r, int n, double conf);
/* PVAR/CVAR su storico jagged (righe per asset, uniformi): appiattisce row-major e applica
 * gmb_trd_var/gmb_trd_cvar (parità IL FlattenReturns / VM/C gmb_bf_194). */
double gmb_trd_portfolio_var_jagged(double** rows, int rows_n, int cols, double conf);
double gmb_trd_portfolio_cvar_jagged(double** rows, int rows_n, int cols, double conf);
double gmb_trd_downside_dev(const double* r, int n, double threshold);
double gmb_trd_ann_vol(const double* r, int n, int ppy);
double gmb_trd_omega(const double* r, int n, double threshold);
double gmb_trd_ulcer(const double* r, int n);
double gmb_trd_portfolio_stddev(double w1, double s1, double w2, double s2, double corr);
double gmb_trd_book_value(double equity, double shares);
double gmb_trd_stock_zero_growth(double dividend, double rate);
double gmb_trd_preferred_stock_value(double dividend, double rate);
double gmb_trd_expected_return(const double* w, const double* r, int n);
int gmb_trd_is_long_only(const double* w, int n);
double gmb_trd_portfolio_sharpe(const double* w, const double* r, int n);
double gmb_fin_days360(double oa_start, double oa_end, int basis);
double gmb_fin_bond_price(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency, int basis);
double gmb_fin_bond_yield(double oa_settle, double oa_mat, double price, double redemption, double rate, int frequency, int basis);
double gmb_fin_duration(double oa_settle, double oa_mat, double coupon, double yld, int frequency, int basis, int modified);

/* Utility date OADate → giorno giuliano → Y/M/D (condivise) */
int gmb_oa_to_jdn(double oa);
void gmb_jdn_to_ymd(int jdn, int* y, int* m, int* d);
int gmb_ymd_to_jdn(int y, int m, int d);

/* Funzioni vettoriali: VM estrae la lista e delega qui */
double gmb_fin_npv(double rate, const double* cf, int n);
double gmb_fin_mnpv(double rate, const double* cf, int n);
double gmb_fin_irr(const double* cf, int n, double guess);
double gmb_fin_mirr(const double* cf, int n, double fr, double rr);
double gmb_fin_xirr(const double* cf, const double* dates, int n, double guess);
double gmb_fin_fv(double r, double n, double pmt, double pv, int type);
double gmb_fin_fvext(double r, double n, double pmt, double pv, int type);
double gmb_fin_pvext(double r, double n, double pmt, double fv, int type);
double gmb_fin_sln(double cost, double salvage, double life);
double gmb_fin_syd(double cost, double salvage, double life, double period);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FINANCE_H */

double gmb_fin_yearfrac(double oa_start, double oa_end, int basis);
double gmb_fin_xnpv(double rate, const double* cf, const double* oa_dates, int n);
