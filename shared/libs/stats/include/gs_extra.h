/*
 * gs_extra.h — metodi statistici dal repo OPTIMAL777/ACI
 * (InformationGeometry, ConvexAnalysis, StatisticalMechanics, TopologicalDataAnalysis,
 * PhaseTransitions).
 */
#ifndef GMBSTATS_GS_EXTRA_H
#define GMBSTATS_GS_EXTRA_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Matrice di informazione di Fisher: F = (1/T) Σ s_t s_t' (scores T x P). */
int gs_fisher_info(const double *scores, int n_obs, int n_par, double *out_F);

/* Natural gradient: F^{-1} grad (F: n x n, grad: n). */
int gs_natural_gradient(const double *grad, const double *F, int n, double *out);

/* Efficienza di Cramer-Rao: inv_fisher / varianza (<= 1). */
double gs_cramer_rao_efficiency(double variance, double inv_fisher);

/* Moreau envelope: env(x) = inf_y f(y) + (x-y)^2/(2 lambda). */
int gs_moreau_envelope(const double *f, double x0, double dx, int n, double lam, double *out);

/* Prox del l1 (soft-thresholding): sign(v)*max(|v|-lambda, 0). */
int gs_soft_threshold(const double *v, int n, double lambda, double *out);

/* Distribuzione di Maxwell-Boltzmann: out_i ∝ exp(-v_i^2/(2 kT)), normalizzata. */
int gs_maxwell_boltzmann(const double *v, int n, double kT, double *out);

/* Persistenza 0-dimensionale di una funzione 1D (sublevel sets). */
int gs_persistence_1d(const double *f, int n, double *out_birth, double *out_death, int *out_n);

/* Distanza bottleneck (1D) tra due diagrammi di persistenza (stessa cardinalita'). */
double gs_bottleneck_distance(const double *b1, const double *d1, int n1,
                              const double *b2, const double *d2, int n2);

/* Energia libera di Landau: F = a/2 phi^2 + b/4 phi^4. */
double gs_landau_free_energy(double a, double b, double phi);


/* VaR Cornish-Fisher con skew/kurtosi. */
double gs_cornish_fisher_var(double mu, double sigma, double skew, double kurt, double conf);

#ifdef __cplusplus
}
#endif

#endif
