/*
 * gm_deriv.h — differenziazione numerica (riscrittura).
 */
#ifndef GMMATH_GM_DERIV_H
#define GMMATH_GM_DERIV_H

#include "gm_core.h"
#include "gm_quad.h" /* gm_fn1 */

#ifdef __cplusplus
extern "C" {
#endif

gm_real gm_deriv_forward(gm_fn1 f, void *ctx, gm_real x, gm_real h);
gm_real gm_deriv_backward(gm_fn1 f, void *ctx, gm_real x, gm_real h);
gm_real gm_deriv_central(gm_fn1 f, void *ctx, gm_real x, gm_real h);
gm_real gm_deriv_second(gm_fn1 f, void *ctx, gm_real x, gm_real h);
/* Estrapolazione di Richardson (levels livelli) sulla differenza centrale. */
gm_real gm_deriv_richardson(gm_fn1 f, void *ctx, gm_real x, gm_real h, int levels);
/* Gradiente per differenze centrali di f: R^n -> R. */
void gm_grad_fd(gm_real (*f)(const gm_real *, int, void *), void *ctx,
                const gm_real *x, int n, gm_real *g, gm_real h);
/* Hessiano (n×n row-major) per differenze centrali. */
void gm_hessian_fd(gm_real (*f)(const gm_real *, int, void *), void *ctx,
                   const gm_real *x, int n, gm_real *H, gm_real h);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_DERIV_H */
