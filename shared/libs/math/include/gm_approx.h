/*
 * gm_approx.h — approssimazione di funzioni (riscrittura).
 *
 * Differenze divise di Newton, interpolazione di Hermite, nodi di Chebyshev,
 * algoritmo di Remez per l'approssimazione minimax, polinomi di Legendre.
 */
#ifndef GMMATH_GM_APPROX_H
#define GMMATH_GM_APPROX_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gm_real (*gm_approx_fn)(gm_real, void *);

/* Valuta il polinomio interpolante di Newton in t. */
gm_real gm_newton_divided(const gm_real *x, const gm_real *y, int n, gm_real t);
/* Coefficienti di Hermite (2n) da valori e derivate prime nei nodi. */
gm_status gm_hermite_interp(const gm_real *x, const gm_real *y, const gm_real *dy,
                            int n, gm_real *coef);
/* Nodi di Chebyshev di prima specie su [a,b]. */
gm_status gm_chebyshev_nodes(int n, gm_real a, gm_real b, gm_real *x);
/* Approssimazione minimax (Remez): coef[0..n] di grado n. */
gm_status gm_remez(gm_approx_fn f, void *ctx, gm_real a, gm_real b, int n,
                   gm_real *coef, int maxit);
/* Valuta P_n(x) (Legendre). */
gm_real gm_legendre_eval(int n, gm_real x);

/* Leggere associati P_l^m(x) (l>=0, 0<=m<=l), SENZA fattore di Condon-Shortley
 * (convenzione delle armoniche sferiche reali). */
gm_real gm_assoc_legendre(int l, int m, gm_real x);
/* Armonica sferica REALE Y_l^m(theta, phi) (convenzione e3nn): m=-l..l,
 * theta = colatitudine [0,pi], phi = azimut [0,2pi). Ortonormale su S². */
gm_real gm_real_sh(int l, int m, gm_real theta, gm_real phi);
/* Riempie out[(l+1)^2] con tutte le Y_l^m (ordine e3nn: l=0; l=1: m=-1,0,1; ...). */
gm_status gm_real_sh_all(int lmax, gm_real theta, gm_real phi, gm_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_APPROX_H */
