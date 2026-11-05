/*
 * gm_decomp2.h — decomposizioni avanzate (da detectum).
 *
 * Hessenberg, Schur (reale), COD (complete orthogonal decomposition),
 * Aasen (LTL^T), solver LTL^T, BVLS (least squares con vincoli di box),
 * radice quadrata di matrice quasi-triangolare.
 */
#ifndef GMMATH_GM_DECOMP2_H
#define GMMATH_GM_DECOMP2_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* H = Q^T A Q (Hessenberg superiore), A = Q H Q^T. Q (n×n) ortogonale. */
gm_status gm_linalg_hessenberg(int n, const gm_real *A, gm_real *H, gm_real *Q);

/* A = Q T Q^T, T quasi-triangolare superiore (Schur reale), Q ortogonale. */
gm_status gm_linalg_schur(int n, const gm_real *A, gm_real *T, gm_real *Q);

/* A = U T V^T, U (m×m), V (n×n) ortogonali, T (m×n) con solo il blocco
 * r×r in alto a sinistra (r = rank). */
gm_status gm_linalg_cod(int m, int n, const gm_real *A, gm_real *U,
                        gm_real *T, gm_real *V, int *rank);

/* P A P^T = L T L^T, T tridiagonale simmetrica, L unit lower triangular.
 * piv (n) = permutazione. */
gm_status gm_linalg_aasen(int n, const gm_real *A, gm_real *L, gm_real *T, int *piv);

/* Risolve P A P^T x = b usando la fattorizzazione di Aasen. */
gm_status gm_linalg_aasen_solve(int n, const gm_real *L, const gm_real *T,
                                const int *piv, const gm_real *b, gm_real *x);

/* min ||C x - d||^2  s.t.  lower <= x <= upper  (Lawson-Hanson). */
gm_status gm_linalg_bvls(int m, int n, const gm_real *C, const gm_real *d,
                         const gm_real *lower, const gm_real *upper, gm_real *x);

/* S^2 = T, S quasi-triangolare superiore (T quasi-triangolare con autovalori
 * reali positivi). */
gm_status gm_mat_sqrt_quasitriu(int n, const gm_real *T, gm_real *S);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_DECOMP2_H */
