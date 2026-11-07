/* gm_quant_bonds.h — obbligazioni (namespace BONDS). */
#ifndef GMBQUANT_BONDS_H
#define GMBQUANT_BONDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Prezzo (dirty) di un'obbligazione cedolare. Date = OADate (giorni dal 1899-12-30).
 * model 0 = Trading.Bonds (cedole reali), model 1 = Numerics.Price (basis 0). */
double gq_bond_price(double settle_oa, double maturity_oa, double rate, double yld,
                     double redemption, int frequency, int model);
/* Rendimento a scadenza dal prezzo (stesso modello di gq_bond_price). */
double gq_bond_yield(double settle_oa, double maturity_oa, double rate, double price,
                     double redemption, int frequency, int model);
/* Rendimento equivalente obbligazionario: ((F-P)/P) * (365/giorni). */
double gq_bond_equiv_yield(double face_value, double bond_price, double days_to_maturity);
/* Current yield: cedole annuali / prezzo. */
double gq_current_yield(double annual_coupons, double bond_price);
/* Duration (a cedole intere); modified=1 -> duration modificata. */
double gq_duration(double settle_oa, double maturity_oa, double coupon_rate, double yld,
                   int frequency, int basis, int modified);
/* Prezzo con periodi interi (modello semplice). */
double gq_price_simple(double settle_oa, double maturity_oa, double rate, double yld,
                       double redemption, int frequency);
/* YTM approssimato: (C + (F-P)/anni) / ((F+P)/2). */
double gq_ytm(double coupon_payment, double face_value, double price, double years);
/* Zero-coupon: valore attuale e rendimento implicito. */
double gq_zcb_value(double face_value, double rate, double time);
double gq_zcb_yield(double face_value, double price, double time);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_BONDS_H */
