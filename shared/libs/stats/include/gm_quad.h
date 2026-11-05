/*
 * gm_quad.h — quadratura numerica (riscrittura).
 */
#ifndef GMMATH_GM_QUAD_H
#define GMMATH_GM_QUAD_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gm_real (*gm_fn1)(gm_real, void *);

gm_real gm_quad_trapezoid(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n);
gm_real gm_quad_midpoint(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n);
gm_real gm_quad_simpson(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n);
/* Gauss-Legendre a n nodi (n>=1). */
gm_real gm_quad_gauss_legendre(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n);
/* Gauss-Kronrod G7-K15; errore stimato in *err (può essere NULL). */
gm_real gm_quad_gauss_kronrod(gm_fn1 f, void *ctx, gm_real a, gm_real b, gm_real *err);
/* Romberg con tolleranza tol e maxit livelli. */
gm_real gm_quad_romberg(gm_fn1 f, void *ctx, gm_real a, gm_real b, gm_real tol, int maxit);
/* Nodi e pesi di Gauss-Legendre (n nodi). */
gm_status gm_gauss_legendre_nodes(int n, gm_real *x, gm_real *w);
/* Nodi e pesi di Gauss-Hermite (n nodi, integrale su R con peso e^{-x^2}):
 * int f(x) e^{-x^2} dx ~ sum w_i f(x_i). Da estimating-dynamic-corporate
 * (#176, nessuna licenza -> clean-room; metodo di Numerical Recipes). */
gm_status gm_gauss_hermite_nodes(int n, gm_real *x, gm_real *w);

/* Quadratura tanh-sinh (double-exponential, Takahasi-Mori): robusta per
 * integrandi con singolarita' agli estremi. x = tanh((pi/2) sinh(t)),
 * w = (pi/2) cosh(t)/cosh^2(u); trapezoidale adattivo (raddoppio dei nodi).
 * abs_acc = accuratezza assoluta (es. 1e-10), max_evals = limite valutazioni. */
gm_real gm_quad_tanh_sinh(gm_fn1 f, void *ctx, gm_real a, gm_real b,
                          gm_real abs_acc, int max_evals);

/* Variante tanh-sinh "complement": l'integranda riceve (x, d) dove d = distanza
 * firmata all'estremo piu' vicino (gestisce singolarita' agli estremi: es.
 * 1/sqrt(x) scrivendola come 1/sqrt(d) con d=x vicino 0). */
gm_real gm_quad_tanh_sinh_c(gm_real (*f)(gm_real x, gm_real d, void *), void *ctx,
                            gm_real a, gm_real b, gm_real abs_acc, int max_evals);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_QUAD_H */
