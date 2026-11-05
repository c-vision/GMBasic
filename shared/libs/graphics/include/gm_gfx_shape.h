/*
 * gm_gfx_shape.h — geometria 2D per grafica (da RLMath, bitmap-editor).
 *
 * Raggio 2D, cerchio, AABB 2D: punto più vicino, test di intersezione,
 * appartenenza a triangolo (baricentrico).
 */
#ifndef GMMATH_GM_GFX_SHAPE_H
#define GMMATH_GM_GFX_SHAPE_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_ray2 { gm_vec2 origin, dir; } gm_ray2;
typedef struct gm_circle { gm_vec2 center; gm_real radius; } gm_circle;
typedef struct gm_aabb2 { gm_vec2 min, max; } gm_aabb2;

gm_ray2 gm_ray2_make(gm_vec2 origin, gm_vec2 dir);
gm_vec2 gm_ray2_at(gm_ray2 r, gm_real t);
gm_circle gm_circle_make(gm_vec2 center, gm_real radius);

gm_aabb2 gm_aabb2_make(gm_vec2 min, gm_vec2 max);
gm_aabb2 gm_aabb2_from_points(const gm_vec2 *pts, int n);
gm_aabb2 gm_aabb2_merge(gm_aabb2 a, gm_aabb2 b);
gm_vec2 gm_aabb2_center(gm_aabb2 b);
gm_vec2 gm_aabb2_extents(gm_aabb2 b);
int gm_aabb2_valid(gm_aabb2 b);

/* distanza euclidea tra due punti. */
gm_real gm_shape_distance(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_shape_closest_point_segment(gm_vec2 p, gm_vec2 a, gm_vec2 b);
gm_vec2 gm_shape_closest_point_ray(gm_ray2 r, gm_vec2 p);
gm_vec2 gm_shape_closest_point_aabb2(gm_aabb2 box, gm_vec2 p);
gm_vec2 gm_shape_closest_point_circle(gm_circle c, gm_vec2 p);
gm_vec2 gm_shape_closest_point_aabb2_aabb2(gm_aabb2 a, gm_aabb2 b);

/* test di appartenenza (1 = dentro/colpito). */
int gm_shape_point_in_triangle(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_vec2 p);
int gm_shape_point_in_aabb2(gm_aabb2 box, gm_vec2 p);
int gm_shape_circle_point(gm_circle c, gm_vec2 p);
int gm_shape_circle_circle(gm_circle a, gm_circle b);
int gm_shape_aabb2_point(gm_aabb2 b, gm_vec2 p);
int gm_shape_aabb2_aabb2(gm_aabb2 a, gm_aabb2 b);
int gm_shape_aabb2_circle(gm_aabb2 b, gm_circle c);

/* intersezioni raggio: 1 se colpito, *t = parametro più piccolo >= 0. */
int gm_shape_ray2_circle(gm_ray2 r, gm_circle c, gm_real *t);
int gm_shape_ray2_aabb2(gm_ray2 r, gm_aabb2 b, gm_real *tmin, gm_real *tmax);
int gm_shape_ray2_segment(gm_ray2 r, gm_vec2 a, gm_vec2 b, gm_real *t);

/* Coordinate baricentriche di p rispetto al triangolo (a,b,c).
 * wa+wb+wc = 1. Ritorna 0 se il triangolo è degenere. (da bitmap-editor) */
int gm_shape_barycentric(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_vec2 p,
                         gm_real *wa, gm_real *wb, gm_real *wc);
/* Punto in un rettangolo ORIENTATO: centro, assi u/v (ortonormali), semi-lati.
 * Ritorna 1 se dentro/on-bordo. (da bitmap-editor point_in_rect) */
int gm_shape_point_in_oriented_rect(gm_vec2 center, gm_vec2 u, gm_vec2 v,
                                    gm_real half_u, gm_real half_v, gm_vec2 p);
/* Punto in un rettangolo definito da 4 vertici (A,B,C,D in ordine).
 * Ritorna 1 se dentro. (da bitmap-editor) */
int gm_shape_point_in_quad(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_vec2 d, gm_vec2 p);

/* Geometria dell'esagono regolare (da c3dlas). */
gm_real gm_hex_area(gm_real side);
gm_real gm_hex_long_diagonal(gm_real side);
gm_real gm_hex_short_diagonal(gm_real side);
gm_real gm_hex_inner_radius(gm_real side);

/* Poligoni semplici 2D (da EmircanKartal marine-resource).
 * point_in_polygon: ray casting (even-odd); 1 = dentro.
 * signed_area: formula di Gauss (shoelace), >0 se antiorario.
 * area: valore assoluto. centroid: baricentro dell'area. */
int gm_shape_point_in_polygon(gm_vec2 p, const gm_vec2 *poly, int n);
gm_real gm_shape_polygon_signed_area(const gm_vec2 *poly, int n);
gm_real gm_shape_polygon_area(const gm_vec2 *poly, int n);
gm_vec2 gm_shape_polygon_centroid(const gm_vec2 *poly, int n);

/* Chamfer distance (da GeoRT): media delle distanze minime al quadrato
 * bidirezionali tra due point cloud A (n×3) e B (m×3):
 * mean_a(min_b d²) + mean_b(min_a d²). */
gm_real gm_shape_chamfer(const gm_real *A, int n, const gm_real *B, int m);
/* Hausdorff distance: max delle distanze minime bidirezionali (A n×3, B m×3). */
gm_real gm_shape_hausdorff(const gm_real *A, int n, const gm_real *B, int m);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_SHAPE_H */
