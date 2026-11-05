/*
 * gm_gfx_intersect.h — test di intersezione 3D/2D (da c3dlas).
 */
#ifndef GMMATH_GM_GFX_INTERSECT_H
#define GMMATH_GM_GFX_INTERSECT_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 3D */
int gm_intersect_sphere_sphere(gm_vec3 c1, gm_real r1, gm_vec3 c2, gm_real r2);
int gm_intersect_sphere_box(gm_vec3 c, gm_real r, gm_vec3 bmin, gm_vec3 bmax);
/* Sfera-piano: ritorna 1 se interseca; *dist = distanza con segno centro-piano. */
int gm_intersect_sphere_plane(gm_vec3 c, gm_real r, gm_vec3 pp, gm_vec3 n, gm_real *dist);
/* Segmento-piano: 1 se colpito, *t in [0,1]. */
int gm_intersect_segment_plane(gm_vec3 a, gm_vec3 b, gm_vec3 pp, gm_vec3 n, gm_real *t);
/* Segmento-sfera: 1 se colpito, *t in [0,1] più vicino. */
int gm_intersect_segment_sphere(gm_vec3 a, gm_vec3 b, gm_vec3 c, gm_real r, gm_real *t);
/* Segmento-triangolo (Möller–Trumbore): 1 se colpito, *t in [0,1]. */
int gm_intersect_segment_triangle(gm_vec3 a, gm_vec3 b, gm_vec3 v0, gm_vec3 v1, gm_vec3 v2, gm_real *t);
/* Triangolo-triangolo (test di intersezione, SAT semplificato via segmenti). */
int gm_intersect_triangle_triangle(const gm_vec3 *t1, const gm_vec3 *t2);

/* 2D */
int gm_intersect_circle_circle(gm_vec2 c1, gm_real r1, gm_vec2 c2, gm_real r2);
/* Segmento-cerchio: 1 se colpito, *t in [0,1]. */
int gm_intersect_segment_circle(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_real r, gm_real *t);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_INTERSECT_H */
