/*
 * gm_vecf.h — vettori a singola precisione (f32): vec2f/3f/4f (da libsunflower).
 *
 * Stessa API dei vettori double (gm_vec*), ma in float. Utili per grafica/ML
 * dove f32 è lo standard.
 */
#ifndef GMMATH_GM_VECF_H
#define GMMATH_GM_VECF_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_vec2f { float x, y; } gm_vec2f;
typedef struct gm_vec3f { float x, y, z; } gm_vec3f;
typedef struct gm_vec4f { float x, y, z, w; } gm_vec4f;

/* costruzione */
gm_vec2f gm_vec2f_make(float x, float y);
gm_vec2f gm_vec2f_splat(float s);
gm_vec2f gm_vec2f_from_array(const float *a);
gm_vec3f gm_vec3f_make(float x, float y, float z);
gm_vec3f gm_vec3f_splat(float s);
gm_vec3f gm_vec3f_from_array(const float *a);
gm_vec4f gm_vec4f_make(float x, float y, float z, float w);
gm_vec4f gm_vec4f_splat(float s);
gm_vec4f gm_vec4f_from_array(const float *a);

/* operazioni componente per componente */
gm_vec2f gm_vec2f_add(gm_vec2f a, gm_vec2f b);
gm_vec2f gm_vec2f_sub(gm_vec2f a, gm_vec2f b);
gm_vec2f gm_vec2f_mul(gm_vec2f a, gm_vec2f b);
gm_vec2f gm_vec2f_div(gm_vec2f a, gm_vec2f b);
gm_vec2f gm_vec2f_mul_f32(gm_vec2f a, float s);
gm_vec2f gm_vec2f_div_f32(gm_vec2f a, float s);
float    gm_vec2f_dot(gm_vec2f a, gm_vec2f b);

gm_vec3f gm_vec3f_add(gm_vec3f a, gm_vec3f b);
gm_vec3f gm_vec3f_sub(gm_vec3f a, gm_vec3f b);
gm_vec3f gm_vec3f_mul(gm_vec3f a, gm_vec3f b);
gm_vec3f gm_vec3f_div(gm_vec3f a, gm_vec3f b);
gm_vec3f gm_vec3f_mul_f32(gm_vec3f a, float s);
gm_vec3f gm_vec3f_div_f32(gm_vec3f a, float s);
gm_vec3f gm_vec3f_cross(gm_vec3f a, gm_vec3f b);
float    gm_vec3f_dot(gm_vec3f a, gm_vec3f b);
float    gm_vec3f_length(gm_vec3f a);
gm_vec3f gm_vec3f_normalize(gm_vec3f a);

gm_vec4f gm_vec4f_add(gm_vec4f a, gm_vec4f b);
gm_vec4f gm_vec4f_sub(gm_vec4f a, gm_vec4f b);
gm_vec4f gm_vec4f_mul(gm_vec4f a, gm_vec4f b);
gm_vec4f gm_vec4f_div(gm_vec4f a, gm_vec4f b);
gm_vec4f gm_vec4f_mul_f32(gm_vec4f a, float s);
gm_vec4f gm_vec4f_div_f32(gm_vec4f a, float s);
float    gm_vec4f_dot(gm_vec4f a, gm_vec4f b);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_VECF_H */
