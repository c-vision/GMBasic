/*
 * gm_color.h — conversioni di spazio colore (da nixers-projects/urnn).
 *
 * RGB <-> HSV e RGB <-> HSL. Componenti RGB in [0,1]; H in [0,360); S,V,L in
 * [0,1]. Utile per palette, ricing, analisi di immagini.
 */
#ifndef GMMATH_GM_COLOR_H
#define GMMATH_GM_COLOR_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RGB -> HSV. h in [0,360), s,v in [0,1]. */
void gm_color_rgb2hsv(gm_real r, gm_real g, gm_real b,
                      gm_real *h, gm_real *s, gm_real *v);
/* HSV -> RGB. */
void gm_color_hsv2rgb(gm_real h, gm_real s, gm_real v,
                      gm_real *r, gm_real *g, gm_real *b);
/* RGB -> HSL. h in [0,360), s,l in [0,1]. */
void gm_color_rgb2hsl(gm_real r, gm_real g, gm_real b,
                      gm_real *h, gm_real *s, gm_real *l);
/* HSL -> RGB. */
void gm_color_hsl2rgb(gm_real h, gm_real s, gm_real l,
                      gm_real *r, gm_real *g, gm_real *b);

/* RGB -> CMYK (tutti in [0,1]). */
void gm_color_rgb2cmyk(gm_real r, gm_real g, gm_real b,
                       gm_real *c, gm_real *m, gm_real *y, gm_real *k);
/* CMYK -> RGB (tutti in [0,1]). */
void gm_color_cmyk2rgb(gm_real c, gm_real m, gm_real y, gm_real k,
                       gm_real *r, gm_real *g, gm_real *b);

/* Interpolazione lineare tra due colori, t in [0,1] (clampato). */
void gm_color_mix(gm_real r1, gm_real g1, gm_real b1,
                  gm_real r2, gm_real g2, gm_real b2, gm_real t,
                  gm_real *r, gm_real *g, gm_real *b);
/* Aumenta la luminosità HSL di amt (clampata a [0,1]). */
void gm_color_lighten(gm_real r, gm_real g, gm_real b, gm_real amt,
                      gm_real *ro, gm_real *go, gm_real *bo);
/* Riduce la luminosità HSL di amt (lighten con -amt). */
void gm_color_darken(gm_real r, gm_real g, gm_real b, gm_real amt,
                     gm_real *ro, gm_real *go, gm_real *bo);
/* Aumenta la saturazione HSL di amt (clampata a [0,1]). */
void gm_color_saturate(gm_real r, gm_real g, gm_real b, gm_real amt,
                       gm_real *ro, gm_real *go, gm_real *bo);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_COLOR_H */
