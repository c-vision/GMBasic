/*
 * gm_eig.h — autovalori/autovettori avanzati (riscrittura).
 *
 * Forma di Hessenberg, algoritmo QR (shift di Wilkinson), cerchi di
 * Gershgorin, metodo delle potenze.
 */
#ifndef GMMATH_GM_EIG_H
#define GMMATH_GM_EIG_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Riduce A (n×n row-major) in forma di Hessenberg superiore (in-place). */
gm_status gm_hessenberg(int n, gm_real *A);
/* Autovalori generali reali/complessi: wr reali, wi immaginari. */
gm_status gm_eig_qr(int n, const gm_real *A, gm_real *wr, gm_real *wi);
/* Raggio del cerchio di Gershgorin della riga i: Σ_{j≠i}|A_ij|. */
gm_real gm_gershgorin_radius(int n, const gm_real *A, int i);
/* Autovalore dominante e autovettore (metodo delle potenze). */
gm_status gm_eig_power(int n, const gm_real *A, gm_real *lambda, gm_real *v,
                       int iters, gm_real tol);
/* Power iteration con shift: autovalore più vicino a sigma. */
gm_status gm_eig_shifted_power(int n, const gm_real *A, gm_real sigma, gm_real *lambda,
                               gm_real *v, int iters, gm_real tol);
/* Inverse power iteration: autovalore più vicino a sigma (risolve (A-sigma I)). */
gm_status gm_eig_inverse_power(int n, const gm_real *A, gm_real sigma, gm_real *lambda,
                               gm_real *v, int iters, gm_real tol);
/* Rayleigh quotient iteration. */
gm_status gm_eig_rayleigh(int n, const gm_real *A, gm_real *lambda, gm_real *v,
                          int iters, gm_real tol);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_EIG_H */
