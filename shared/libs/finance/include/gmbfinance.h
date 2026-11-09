/* gmbfinance.h — API della lib `finance` (shared/libs/finance).
 *
 * Formule finanziarie pure (PMT/FV/PV/NPER/RATE, ammortamenti, NPV/IRR/MIRR/XIRR/XNPV,
 * DAYS360/YEARFRAC). Self-contained: nessuna dipendenza esterna.
 * Simboli con prefisso ofx_fin_ per non collidere col runtime.
 *
 * Date: OADate (giorni dal 1899-12-30), come il tipo DATETIME del linguaggio.
 */
#ifndef GMBFINANCE_H
#define GMBFINANCE_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- utility date (JDN) --- */
int ofx_oa_to_jdn(double oa);
void ofx_jdn_to_ymd(int jdn, int* y, int* m, int* d);
int ofx_ymd_to_jdn(int y, int m, int d);

/* --- rendite --- */
double ofx_fin_pmt(double rate, double nper, double pv, double fv, int type);
double ofx_fin_fv(double r, double n, double pmt, double pv, int type);
double ofx_fin_fvext(double r, double n, double pmt, double pv, int type);
double ofx_fin_pvext(double r, double n, double pmt, double fv, int type);
double ofx_fin_nper_ext(double rate, double pmt, double pv, double fv, int type);
double ofx_fin_rate(double nper, double pmt, double pv, double fv, int type, double guess);
double ofx_fin_cumipmt(double rate, double nper, double pv, int start, int end, int type);
double ofx_fin_cumprinc(double rate, double nper, double pv, int start, int end, int type);

/* --- ammortamenti --- */
double ofx_fin_sln(double cost, double salvage, double life);
double ofx_fin_syd(double cost, double salvage, double life, double period);
double ofx_fin_db(double cost, double salvage, double life, double period, int month);
double ofx_fin_ddb(double cost, double salvage, double life, double period, double factor);

/* --- flussi di cassa --- */
double ofx_fin_npv(double rate, const double* cf, int n);
double ofx_fin_mnpv(double rate, const double* cf, int n);
double ofx_fin_irr(const double* cf, int n, double guess);
double ofx_fin_mirr(const double* cf, int n, double fr, double rr);
double ofx_fin_xirr(const double* cf, const double* dates, int n, double guess);
double ofx_fin_xnpv(double rate, const double* cf, const double* oa_dates, int n);

/* --- date --- */
double ofx_fin_days360(double oa_start, double oa_end, int basis);
double ofx_fin_yearfrac(double oa_start, double oa_end, int basis);

#ifdef __cplusplus
}
#endif

#endif /* GMBFINANCE_H */
