/*
 * gm_veccalc.h — calcolo vettoriale numerico su R^3 (nuovo, gap vector-calculus).
 *
 * Operatori sui campi (divergenza, rotore, gradiente, laplaciano) per differenze
 * centrali, predicati solenoidale/irrotazionale, frame di Frenet-Serret (TNB +
 * curvatura + torsione) di una curva parametrica, lunghezza d'arco, integrali di
 * linea (campo scalare e lavoro) e flusso su patch parametrica. Integrazione via
 * gm_integrate_adaptive (Simpson adattiva). C99 puro, sole dipendenze libc/libm.
 */
#ifndef GMMATH_GM_VECCALC_H
#define GMMATH_GM_VECCALC_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Campo vettoriale F: R^3 -> R^3. */
typedef gm_vec3 (*gm_vc_field)(gm_real x, gm_real y, gm_real z, void *ctx);
/* Campo scalare f: R^3 -> R. */
typedef gm_real (*gm_vc_scalar)(gm_real x, gm_real y, gm_real z, void *ctx);
/* Curva parametrica r: R -> R^3. */
typedef gm_vec3 (*gm_vc_curve)(gm_real t, void *ctx);
/* Patch parametrica S: R^2 -> R^3. */
typedef gm_vec3 (*gm_vc_patch)(gm_real u, gm_real v, void *ctx);

/* Divergenza div F per differenze centrali di passo h. */
gm_real gm_vc_divergence(gm_vc_field F, void *ctx,
                         gm_real x, gm_real y, gm_real z, gm_real h);
/* Rotore (curl F) per differenze centrali di passo h. */
gm_vec3 gm_vc_curl(gm_vc_field F, void *ctx,
                   gm_real x, gm_real y, gm_real z, gm_real h);
/* Gradiente di un campo scalare per differenze centrali di passo h. */
gm_vec3 gm_vc_gradient(gm_vc_scalar f, void *ctx,
                       gm_real x, gm_real y, gm_real z, gm_real h);
/* Laplaciano di un campo scalare (stencil a 7 punti di passo h). */
gm_real gm_vc_laplacian(gm_vc_scalar f, void *ctx,
                        gm_real x, gm_real y, gm_real z, gm_real h);
/* 1 se |div F| <= tol (campo solenoidale), altrimenti 0. */
int gm_vc_is_solenoidal(gm_vc_field F, void *ctx,
                        gm_real x, gm_real y, gm_real z, gm_real h, gm_real tol);
/* 1 se |curl F| <= tol (campo irrotazionale), altrimenti 0. */
int gm_vc_is_irrotational(gm_vc_field F, void *ctx,
                          gm_real x, gm_real y, gm_real z, gm_real h, gm_real tol);

/* Frame di Frenet-Serret in t (derivate per differenze centrali; h di
 * riferimento ~= 1e-4: il passo cresce con l'ordine, 10h e 100h+Richardson):
 * T = r'/|r'|, kappa = |r'x r''|/|r'|^3, B = (r'x r'')/|r'x r''|, N = B x T,
 * tau = ((r'x r'') . r''')/|r'x r''|^2.
 * Curva rettilinea (kappa <= 1e-9, sotto il rumore delle differenze finite):
 * kappa = tau = 0, N = B = 0, ritorna GM_OK.
 * Ritorna GM_ERR_ARG se r/T/N/B/kappa/tau sono NULL, h == 0 o |r'| == 0. */
gm_status gm_vc_tnb(gm_vc_curve r, void *ctx, gm_real t, gm_real h,
                    gm_vec3 *T, gm_vec3 *N, gm_vec3 *B,
                    gm_real *kappa, gm_real *tau);

/* Lunghezza d'arco L = integrale di |r'(t)| su [a,b] (tol = Simpson adattiva). */
gm_real gm_vc_arc_length(gm_vc_curve r, void *ctx,
                         gm_real a, gm_real b, gm_real tol);
/* Integrale di linea di un campo scalare: integrale di f(r(t))|r'(t)| su [a,b]. */
gm_real gm_vc_line_scalar(gm_vc_scalar f, void *fctx,
                          gm_vc_curve r, void *rctx,
                          gm_real a, gm_real b, gm_real tol);
/* Lavoro: integrale di linea di un campo vettoriale, integrale di F(r).r' su [a,b]. */
gm_real gm_vc_line_work(gm_vc_field F, void *fctx,
                        gm_vc_curve r, void *rctx,
                        gm_real a, gm_real b, gm_real tol);
/* Flusso su patch: doppio integrale di F(S) . (Su x Sv) su
 * [u0,u1]x[v0,v1] (Simpson adattiva annidata, tol per livello). */
gm_real gm_vc_flux(gm_vc_field F, void *fctx,
                   gm_vc_patch S, void *sctx,
                   gm_real u0, gm_real u1, gm_real v0, gm_real v1, gm_real tol);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_VECCALC_H */
