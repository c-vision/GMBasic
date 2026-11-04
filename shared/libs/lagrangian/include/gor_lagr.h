/*
 * gor_lagr.h — rilassamento lagrangiano con ottimizzazione subgradiente.
 *
 * Da seydou-coulibaly/Location_problem_FLP (SSCFLP): per problemi della forma
 *   min f(x)  s.t.  A x >= b,  x in X
 * si rilassano i vincoli A x >= b con moltiplicatori lambda >= 0 e si risolve
 * il subproblema su X. I moltiplicatori sono aggiornati col subgradiente.
 */
#ifndef GMBORTOOLS_GOR_LAGR_H
#define GMBORTOOLS_GOR_LAGR_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Risolve il subproblema rilassato
 *   min_{x in X}  f(x) + sum_i lambda_i * (b_i - (A x)_i)
 * e scrive la soluzione in x[nx]. Ritorna il valore ottimo del subproblema. */
typedef gor_real (*gor_lagr_sub)(const gor_real *lambda, int m, void *ctx, gor_real *x);

/* Rilassamento lagrangiano + subgradiente. A (m×nx row-major), b[m].
 * `ub` = miglior upper bound noto (per il passo); <=0 per stimarlo dal primo
 * subproblema. Aggiorna lambda[m] e best_x[nx]. Ritorna il miglior lower bound. */
gor_real gor_lagr_subgradient(int m, int nx, const gor_real *A, const gor_real *b,
                              gor_lagr_sub sub, void *ctx, int maxit, gor_real ub,
                              gor_real *lambda, gor_real *best_x);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_LAGR_H */
