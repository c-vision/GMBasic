/*
 * gs_math.h — metodi da AI/Math non ancora coperti:
 *  - Bayesian optimization: acquisition EI/PI/UCB + passo GP;
 *  - stima MLE di distribuzioni + log-verosimiglianza + AIC/BIC;
 *  - gambler's ruin (De Moivre): probabilita' e durata attesa;
 *  - inferenza causale: propensity score, ATE via IPW e matching.
 */
#ifndef GMBSTATS_GS_MATH_H
#define GMBSTATS_GS_MATH_H

#include "gc_core.h"
#include "gs_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Bayesian optimization ---- */
/* Expected Improvement (massimizzazione): incumbent best, jitter xi. */
double gs_bo_ei(double mu, double sd, double best, double xi);
/* Probability of Improvement. */
double gs_bo_pi(double mu, double sd, double best, double xi);
/* Upper Confidence Bound: mu + kappa*sd. */
double gs_bo_ucb(double mu, double sd, double kappa);
/* Passo BO: fit GP su (x,y), valuta la UCB sui candidati; ritorna l'indice
 * (in [0,m)) con UCB massima e ne scrive il valore in *out_val. */
int gs_bo_next(const double *x, const double *y, int n,
               const double *cand, int m, double l2, double noise, double kappa,
               int *out_idx, double *out_val);

/* ---- TPE (Tree-structured Parzen Estimator, Bergstra et al. 2011) ---- */
typedef gc_real (*gs_tpe_fn)(const gc_real *x, void *ctx);
/* Minimizza f su box [lo,hi] (d dim). n_init punti casuali, poi n_iter proposte
 * TPE (miscela di gaussiane su buoni/cattivi, massimizza l(x)/g(x)). Scrive il
 * miglior x in out_x[d] e il valore in out_f. Ritorna 0/-1. */
int gs_tpe(gs_tpe_fn f, void *ctx, const gc_real *lo, const gc_real *hi, int d,
           int n_iter, int n_init, long seed, gc_real *out_x, gc_real *out_f);


/* ---- Gambler's ruin (De Moivre) ---- */
/* P(rovina: raggiungere 0 prima di N) partendo da i, vittoria +1 con prob. p. */
double gs_gambler_ruin_prob(double i, double N, double p);
/* Durata attesa fino all'assorbimento (0 o N). */
double gs_gambler_ruin_duration(double i, double N, double p);

/* ---- Inferenza causale ---- */
/* Propensity score via regressione logistica (X n*p row-major, t in {0,1}). */
int gs_propensity_score(const double *X, const double *t, int n, int p, double *out_ps);
/* ATE via inverse probability weighting. */
double gs_ate_ipw(const double *t, const double *y, const double *ps, int n);
/* ATE via nearest-neighbor matching sul propensity score. */
double gs_ate_matching(const double *t, const double *y, const double *ps, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_MATH_H */
