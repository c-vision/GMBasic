/*
 * gm_vec.h — vettori 2D/3D/4D (aggiunte dal gap vecmat).
 *
 * Convenzione: componenti nominate (.x/.y/.z/.w). Accesso indicizzato via
 * gm_vecN_get/set. Nessuna union anonima: C99 -pedantic pulito.
 */
#ifndef GMMATH_GM_VEC_H
#define GMMATH_GM_VEC_H

#include "gm_core.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_vec2 { gm_real x, y; } gm_vec2;
typedef struct gm_vec3 { gm_real x, y, z; } gm_vec3;
typedef struct gm_vec4 { gm_real x, y, z, w; } gm_vec4;

/* ---- vec2 ---- */
gm_vec2 gm_vec2_make(gm_real x, gm_real y);
gm_vec2 gm_vec2_zero(void);
gm_vec2 gm_vec2_one(void);
gm_vec2 gm_vec2_splat(gm_real s);
gm_real gm_vec2_get(gm_vec2 v, int i);
gm_vec2 gm_vec2_set(gm_vec2 v, int i, gm_real value);
gm_vec2 gm_vec2_add(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_sub(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_mul(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_div(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_scale(gm_vec2 v, gm_real s);
gm_vec2 gm_vec2_neg(gm_vec2 v);
gm_real gm_vec2_dot(gm_vec2 a, gm_vec2 b);
gm_real gm_vec2_cross(gm_vec2 a, gm_vec2 b);
gm_real gm_vec2_length_sq(gm_vec2 v);
gm_real gm_vec2_length(gm_vec2 v);
/* Norma L1 (Manhattan) e L-infinito (Chebyshev). */
gm_real gm_vec2_norm_l1(gm_vec2 v);
gm_real gm_vec2_norm_inf(gm_vec2 v);
gm_real gm_vec2_distance(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_normalize(gm_vec2 v);
gm_vec2 gm_vec2_lerp(gm_vec2 a, gm_vec2 b, gm_real t);
gm_vec2 gm_vec2_min(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_max(gm_vec2 a, gm_vec2 b);
gm_vec2 gm_vec2_abs(gm_vec2 v);
gm_vec2 gm_vec2_perp(gm_vec2 v);
gm_vec2 gm_vec2_rotate(gm_vec2 v, gm_real radians);
gm_real gm_vec2_angle(gm_vec2 a, gm_vec2 b);

/* ---- vec3 ---- */
gm_vec3 gm_vec3_make(gm_real x, gm_real y, gm_real z);
gm_vec3 gm_vec3_zero(void);
gm_vec3 gm_vec3_one(void);
gm_vec3 gm_vec3_splat(gm_real s);
gm_real gm_vec3_get(gm_vec3 v, int i);
gm_vec3 gm_vec3_set(gm_vec3 v, int i, gm_real value);
gm_vec3 gm_vec3_add(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_sub(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_mul(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_div(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_scale(gm_vec3 v, gm_real s);
gm_vec3 gm_vec3_neg(gm_vec3 v);
gm_real gm_vec3_dot(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_cross(gm_vec3 a, gm_vec3 b);
gm_real gm_vec3_length_sq(gm_vec3 v);
gm_real gm_vec3_length(gm_vec3 v);
gm_real gm_vec3_norm_l1(gm_vec3 v);
gm_real gm_vec3_norm_inf(gm_vec3 v);
gm_real gm_vec3_distance(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_normalize(gm_vec3 v);
gm_vec3 gm_vec3_lerp(gm_vec3 a, gm_vec3 b, gm_real t);
gm_vec3 gm_vec3_min(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_max(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_abs(gm_vec3 v);
gm_real gm_vec3_angle(gm_vec3 a, gm_vec3 b);
gm_vec3 gm_vec3_reflect(gm_vec3 v, gm_vec3 normal);
gm_vec3 gm_vec3_project(gm_vec3 v, gm_vec3 onto);
gm_vec3 gm_vec3_reject(gm_vec3 v, gm_vec3 onto);
/* --- graphics/path-tracing (da Lib_math) --- */
/* Approssimazione di Fresnel di Schlick: f0*(1-f) + f, f = (1-u)^5. */
gm_vec3 gm_vec3_schlick(gm_vec3 f0, gm_real u);
/* Offset di un'origine raggio lungo n (evita auto-intersezione, stile RT Gems). */
gm_vec3 gm_vec3_bias(gm_vec3 v, gm_vec3 n);
/* Clamp della magnitudine a `max`. */
gm_vec3 gm_vec3_clamp_mag(gm_vec3 v, gm_real max);
/* Clamp componente per componente in [lo, hi]. */
gm_vec3 gm_vec3_clamp(gm_vec3 v, gm_real lo, gm_real hi);
gm_vec3 gm_vec3_clamp01(gm_vec3 v);
gm_vec3 gm_vec3_sqrt(gm_vec3 v);
gm_vec3 gm_vec3_pow(gm_vec3 v, gm_real p);
/* Vettore unitario casuale uniforme sulla sfera (rejection sampling). */
gm_vec3 gm_vec3_unit_random(unsigned long long *seed);
/* Colore in [0,1] -> 0x00RRGGBB (alpha = 0xFF). */
uint32_t gm_vec3_to_uint32(gm_vec3 v);

/* ---- vec4 ---- */
gm_vec4 gm_vec4_make(gm_real x, gm_real y, gm_real z, gm_real w);
gm_vec4 gm_vec4_zero(void);
gm_vec4 gm_vec4_one(void);
gm_vec4 gm_vec4_splat(gm_real s);
gm_real gm_vec4_get(gm_vec4 v, int i);
gm_vec4 gm_vec4_set(gm_vec4 v, int i, gm_real value);
gm_vec4 gm_vec4_add(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_sub(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_mul(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_div(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_scale(gm_vec4 v, gm_real s);
gm_vec4 gm_vec4_neg(gm_vec4 v);
gm_real gm_vec4_dot(gm_vec4 a, gm_vec4 b);
gm_real gm_vec4_length_sq(gm_vec4 v);
gm_real gm_vec4_length(gm_vec4 v);
gm_real gm_vec4_norm_l1(gm_vec4 v);
gm_real gm_vec4_norm_inf(gm_vec4 v);
gm_real gm_vec4_distance(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_normalize(gm_vec4 v);
gm_vec4 gm_vec4_lerp(gm_vec4 a, gm_vec4 b, gm_real t);
gm_vec4 gm_vec4_min(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_max(gm_vec4 a, gm_vec4 b);
gm_vec4 gm_vec4_abs(gm_vec4 v);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_VEC_H */
