/*
 * gm_special.h — funzioni speciali (SciPy special), wrapper moderno del legacy.
 *
 * Espone come API gm_* le funzioni speciali già presenti nel codice GMBasic
 * (erf/erfc/inverse, gamma/lgamma/digamma, beta/incomplete, Bessel).
 */
#ifndef GMMATH_GM_SPECIAL_H
#define GMMATH_GM_SPECIAL_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Errore e sue inverse. */
gm_real gm_special_erf(gm_real x);
gm_real gm_special_erfc(gm_real x);
gm_real gm_special_erfinv(gm_real x);
gm_real gm_special_erfcinv(gm_real x);

/* Gamma. */
gm_real gm_special_gamma(gm_real x);
gm_real gm_special_lgamma(gm_real x);
gm_real gm_special_digamma(gm_real x);
/* P(a,x) e Q(a,x) incomplete gamma regolarizzate; inverse di P/Q. */
gm_real gm_special_gammainc(gm_real a, gm_real x);
gm_real gm_special_gammaincc(gm_real a, gm_real x);
gm_real gm_special_gammaincinv(gm_real a, gm_real y);
gm_real gm_special_gammainccinv(gm_real a, gm_real y);

/* Beta. */
gm_real gm_special_beta(gm_real a, gm_real b);
gm_real gm_special_betaln(gm_real a, gm_real b);
/* I_x(a,b) incomplete beta regolarizzata e la sua inversa. */
gm_real gm_special_betainc(gm_real a, gm_real b, gm_real x);
gm_real gm_special_betaincinv(gm_real a, gm_real b, gm_real y);

/* Bessel (ordine v reale). */
gm_real gm_special_besselj(gm_real v, gm_real x);
gm_real gm_special_bessely(gm_real v, gm_real x);
gm_real gm_special_besseli(gm_real v, gm_real x);
gm_real gm_special_besselk(gm_real v, gm_real x);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_SPECIAL_H */
