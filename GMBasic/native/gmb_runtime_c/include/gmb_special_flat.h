/* gmb_special_flat.h — PONTE delle FUNZIONI SPECIALI per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. `CommonLib/Numerics/Special.cs` implementava in C# (serie e MathNet) le stesse
 * funzioni che `gmb_math.c` implementa per VM/C e C99Native: due sorgenti numeriche diverse.
 * Qui il managed chiama le funzioni del runtime condiviso (come `gmb_matrix_flat` e
 * `gmb_ts_flat`): una sola implementazione, stessi valori sulle 4 gambe.
 *
 * CONTRATTO. Solo inoltro, nessuna logica: dominio fuori range → NaN come nei kernel. Nessuna
 * allocazione, nessuna ownership da gestire.
 */
#ifndef GMB_SPECIAL_FLAT_H
#define GMB_SPECIAL_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── scipy.special: nucleo ─────────────────────────────────────────────────────────────────── */
double gmb_spf_gamma(double x);
double gmb_spf_lgamma(double x);
double gmb_spf_beta(double a, double b);
double gmb_spf_erf(double x);
double gmb_spf_erfc(double x);
double gmb_spf_gamma_inc(double a, double x);
double gmb_spf_gamma_incc(double a, double x);
double gmb_spf_beta_inc(double a, double b, double x);
double gmb_spf_digamma(double x);
double gmb_spf_bessel_j(double v, double x);
double gmb_spf_bessel_y(double v, double x);
double gmb_spf_bessel_i(double v, double x);
double gmb_spf_bessel_k(double v, double x);

/* ── inverse e log ────────────────────────────────────────────────────────────────────────── */
double gmb_spf_beta_ln(double a, double b);
double gmb_spf_erf_inv(double x);
double gmb_spf_erfc_inv(double x);
double gmb_spf_gamma_inc_inv(double a, double y);
double gmb_spf_gamma_incc_inv(double a, double y);
double gmb_spf_beta_inc_inv(double a, double b, double y);

/* ── funzioni avanzate (gm_special2) ──────────────────────────────────────────────────────── */
double gmb_spf_zeta(double s);
double gmb_spf_zeta_hurwitz(double s, double a);
double gmb_spf_polygamma(double n, double x);
double gmb_spf_hypergeom_2f1(double a, double b, double c, double x);
double gmb_spf_hypergeom_1f1(double a, double b, double x);
double gmb_spf_lambert_w(double x);
double gmb_spf_mittag_leffler(double alpha, double beta, double x);
double gmb_spf_struve_h(double v, double x);
double gmb_spf_kelvin_ber(double x);
double gmb_spf_kelvin_bei(double x);
double gmb_spf_parabolic_cylinder(double a, double x);

#ifdef __cplusplus
}
#endif

#endif /* GMB_SPECIAL_FLAT_H */
