/*
 * gs_curve.h — fit di curve di rendimento (AI/Derivatives/Interest Rate).
 *  - Nelson-Siegel (4 parametri)
 *  - Svensson (6 parametri)
 */
#ifndef GMBSTATS_GS_CURVE_H
#define GMBSTATS_GS_CURVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Fit Nelson-Siegel: y(tau) = b0 + b1*f1 + b2*f2, f1=(1-e^-x)/x, f2=f1-e^-x, x=tau/tau_par.
 * Ritorna 0; *out_rmse = errore RMS. */
int gs_nelson_siegel(const double *maturities, const double *yields, int n,
                     double *out_b0, double *out_b1, double *out_b2,
                     double *out_tau, double *out_rmse);

/* Fit Svensson (NS + secondo termine): 6 parametri. */
int gs_svensson(const double *maturities, const double *yields, int n,
                double *out_b0, double *out_b1, double *out_b2, double *out_b3,
                double *out_tau1, double *out_tau2, double *out_rmse);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_CURVE_H */

/* Vasicek-Fong: discount function ricavata risolvendo P = CF * d (minimi quadrati
 * via equazioni normali). CF: m bond x T periodi (row-major), prices[m], disc[T]. 0/-1. */
int gs_curve_vf_discounts(const double *CF, int m, int T, const double *prices, double *disc);
