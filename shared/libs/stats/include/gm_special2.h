/*
 * gm_special2.h — funzioni speciali avanzate (riscrittura).
 */
#ifndef GMMATH_GM_SPECIAL2_H
#define GMMATH_GM_SPECIAL2_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

gm_real gm_hypergeom_2f1(gm_real a, gm_real b, gm_real c, gm_real x);
gm_real gm_hypergeom_1f1(gm_real a, gm_real b, gm_real x);
gm_real gm_zeta(gm_real s);
gm_real gm_zeta_hurwitz(gm_real s, gm_real a);
gm_real gm_polygamma(int n, gm_real x);
gm_real gm_lambert_w(gm_real x);
gm_real gm_mittag_leffler(gm_real alpha, gm_real beta, gm_real x);
gm_real gm_struve_h(gm_real v, gm_real x);
gm_real gm_kelvin_ber(gm_real x);
gm_real gm_kelvin_bei(gm_real x);
/* Funzione parabolica cilindrica U(a,x). */
gm_real gm_parabolic_cylinder(gm_real a, gm_real x);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_SPECIAL2_H */
