/*
 * gm_integral.h — equazioni integrali (Fredholm/Volterra, seconda specie).
 */
#ifndef GMMATH_GM_INTEGRAL_H
#define GMMATH_GM_INTEGRAL_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fredholm: u(x) = f(x) + ∫_a^b K(x,t) u(t) dt.
 * Nyström con regola del trapezio su n nodi. */
gm_status gm_fredholm2(gm_real (*K)(gm_real, gm_real, void *), void *kctx,
                       gm_real (*f)(gm_real, void *), void *fctx,
                       gm_real a, gm_real b, int n, gm_real *u);
/* Volterra: u(x) = f(x) + ∫_a^x K(x,t) u(t) dt (avanzamento in avanti). */
gm_status gm_volterra2(gm_real (*K)(gm_real, gm_real, void *), void *kctx,
                       gm_real (*f)(gm_real, void *), void *fctx,
                       gm_real a, gm_real b, int n, gm_real *u);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_INTEGRAL_H */
