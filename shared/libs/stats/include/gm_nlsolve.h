/*
 * gm_nlsolve.h — sistemi di equazioni non lineari (riscrittura).
 *
 * Newton multidimensionale (Jacobiano numerico o analitico), Broyden,
 * punto fisso, continuation.
 */
#ifndef GMMATH_GM_NLSOLVE_H
#define GMMATH_GM_NLSOLVE_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* f: R^n -> R^n, callback F(x, fx, n, ctx). */
typedef void (*gm_nl_fn)(const gm_real *x, gm_real *fx, int n, void *ctx);

/* Newton con Jacobiano per differenze finite. x viene aggiornato. */
gm_status gm_newton_system(gm_nl_fn f, void *ctx, gm_real *x, int n,
                           int maxit, gm_real tol, int *iters);
/* Newton con Jacobiano analitico J(x, J, n, ctx) (n×n row-major). */
gm_status gm_newton_jacobian(gm_nl_fn f, void (*J)(const gm_real *, gm_real *, int, void *),
                             void *ctx, gm_real *x, int n, int maxit, gm_real tol, int *iters);
/* Broyden (quasi-Newton, Jacobiano approssimato). */
gm_status gm_broyden_system(gm_nl_fn f, void *ctx, gm_real *x, int n,
                            int maxit, gm_real tol, int *iters);
/* Punto fisso: x_{k+1} = g(x_k). g: R^n -> R^n. */
gm_status gm_fixed_point(gm_nl_fn g, void *ctx, gm_real *x, int n,
                         int maxit, gm_real tol, int *iters);
/* Continuation (omotoppia): risolve f(x)=0 partendo da f0(x)=0 con blending. */
gm_status gm_continuation(gm_nl_fn f, gm_nl_fn f0, void *ctx, gm_real *x, int n,
                          int steps, int maxit, gm_real tol, int *iters);

/* Steffensen (radice scalare, derivative-free; convergenza quadratica su
 * radici semplici): x_{k+1} = x_k - f(x_k)^2 / (f(x_k + f(x_k)) - f(x_k)).
 * Ritorna la radice (o INFINITY se non converge entro maxit); iters opzionale
 * (iterazioni usate, -1 su fallimento). Derivata non richiesta. */
gm_real gm_steffensen(gm_real (*f)(gm_real, void *), void *ctx,
                      gm_real x0, gm_real tol, int maxit, int *iters);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_NLSOLVE_H */
