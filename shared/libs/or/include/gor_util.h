/*
 * gor_util.h — supporto OR: code, inventario, MCDM, MDP, simulazione.
 *
 * Teoria delle code (M/M/1, M/M/c, Little), inventario (EOQ/EPQ/newsvendor),
 * MCDM (AHP, TOPSIS), MDP (value/policy iteration), Monte Carlo.
 */
#ifndef GMBORTOOLS_GOR_UTIL_H
#define GMBORTOOLS_GOR_UTIL_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- teoria delle code ---- */
/* M/M/1: ritorna rho; L, Lq, W, Wq se non NULL. */
gor_real gor_queue_mm1(gor_real lambda, gor_real mu, gor_real *L, gor_real *Lq,
                       gor_real *W, gor_real *Wq);
/* M/M/c: ritorna rho; L, Lq, W, Wq se non NULL. */
gor_real gor_queue_mmc(gor_real lambda, gor_real mu, int c, gor_real *L, gor_real *Lq,
                       gor_real *W, gor_real *Wq);
/* Little's law: L = lambda * W. */
gor_real gor_littles_law(gor_real lambda, gor_real W);

/* ---- inventario ---- */
gor_real gor_eoq(gor_real demand, gor_real setup, gor_real holding);      /* Q* */
gor_real gor_epq(gor_real demand, gor_real setup, gor_real holding, gor_real rate); /* Q* con produzione */
gor_real gor_newsvendor(gor_real cu, gor_real co, gor_real mean, gor_real std); /* quantità ottima */
gor_real gor_safety_stock(gor_real z, gor_real sigma);

/* ---- MCDM ---- */
/* AHP: matrice di confronto n×n -> pesi (autovettore principale, normalizzato). */
gor_status gor_ahp(int n, const gor_real *pairwise, gor_real *weights, gor_real *consistency);
/* TOPSIS: X m×n (alternative×criteri), w[n], benefit[n] (1=beneficio, 0=costo).
 * scores[m] = vicinanza relativa (0..1, 1=migliore). */
gor_status gor_topsis(int m, int n, const gor_real *X, const gor_real *w,
                      const int *benefit, gor_real *scores);

/* ---- MDP ---- */
/* Value iteration: P[ns*na*ns], R[ns*na], gamma. V[ns], policy[ns]. Ritorna iterazioni. */
int gor_mdp_value_iteration(int ns, int na, const gor_real *P, const gor_real *R,
                            gor_real gamma, gor_real tol, int maxit, gor_real *V, int *policy);
int gor_mdp_policy_iteration(int ns, int na, const gor_real *P, const gor_real *R,
                             gor_real gamma, int maxit, gor_real *V, int *policy);

/* ---- simulazione ---- */
/* Monte Carlo: f(x, ctx) valutata su n campioni uniformi in [lo,hi]^d.
 * Ritorna la media; *std (se non NULL) = deviazione standard. */
typedef gor_real (*gor_util_fn)(const gor_real *x, int d, void *ctx);
gor_real gor_montecarlo(gor_util_fn f, void *ctx, int d, const gor_real *lo, const gor_real *hi,
                        int n, unsigned long seed, gor_real *std);

/* CVaR (conditional value-at-risk) al livello alpha su `returns` (perdite).
 * Ritorna il CVaR; *var (se non NULL) = VaR. */
gor_real gor_cvar(int n, const gor_real *returns, gor_real alpha, gor_real *var);

/* Simulazione a eventi discreti di una coda M/M/1. Ritorna il tempo medio di
 * attesa in coda (Wq); *util (se non NULL) = utilizzazione. */
gor_real gor_des_mm1(gor_real lambda, gor_real mu, int ncust, unsigned long seed, gor_real *util);

/* Chance constraint: per un vincolo a'x <= b con coefficienti aleatori
 * a_i ~ N(mean_i, sigma_i), il vincolo probabilistico P(a'x <= b) >= 1-alpha
 * diventa mean'x + z_{1-alpha} * sqrt(Σ (sigma_i x_i)²) <= b.
 * Ritorna il termine di sicurezza z * ||sigma ⊙ x||; *shift (se non NULL) = z. */
gor_real gor_chance_constraint(int n, const gor_real *sigma, const gor_real *x,
                               gor_real alpha, gor_real *shift);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_UTIL_H */
