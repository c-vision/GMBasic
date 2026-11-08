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
double gmb_fin_binomial(double s, double k, double t, double r, double v, int steps, int is_put, int is_american);
double gmb_fin_nper_ext(double rate, double pmt, double pv, double fv, int type);
double gmb_fin_price_simple(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency);
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
