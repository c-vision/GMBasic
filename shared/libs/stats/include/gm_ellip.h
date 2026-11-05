/*
 * gm_ellip.h — integrali/funzioni ellittiche e Airy (riscrittura, da ccmath).
 */
#ifndef GMMATH_GM_ELLIP_H
#define GMMATH_GM_ELLIP_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Integrali ellittici (Carlson RF/RD). */
gm_real gm_ellip_rf(gm_real x, gm_real y, gm_real z);
gm_real gm_ellip_rd(gm_real x, gm_real y, gm_real z);
/* Completi di prima/seconda specie, parametro k (modulo). */
gm_real gm_ellip_K(gm_real k);
gm_real gm_ellip_E(gm_real k);
/* Incompleti: F(phi,k), E(phi,k). */
gm_real gm_ellip_F(gm_real phi, gm_real k);
gm_real gm_ellip_Einc(gm_real phi, gm_real k);
/* Jacobi: ampiezza e sn/cn/dn. */
gm_real gm_ellip_am(gm_real u, gm_real k);
gm_real gm_ellip_sn(gm_real u, gm_real k);
gm_real gm_ellip_cn(gm_real u, gm_real k);
gm_real gm_ellip_dn(gm_real u, gm_real k);
/* Nome q(k). */
gm_real gm_ellip_nome(gm_real k);

/* Airy Ai/Bi e derivate. */
gm_real gm_airy_ai(gm_real x);
gm_real gm_airy_bi(gm_real x);
gm_real gm_airy_ai_prime(gm_real x);
gm_real gm_airy_bi_prime(gm_real x);

/* Bessel sferiche j_n, y_n. */
gm_real gm_bessel_sph_j(int n, gm_real x);
gm_real gm_bessel_sph_y(int n, gm_real x);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_ELLIP_H */
