/* gm_vecf.c — vettori f32 (da libsunflower). */
#include "gm_vecf.h"

#include <math.h>

gm_vec2f gm_vec2f_make(float x, float y) { gm_vec2f v; v.x = x; v.y = y; return v; }
gm_vec2f gm_vec2f_splat(float s) { return gm_vec2f_make(s, s); }
gm_vec2f gm_vec2f_from_array(const float *a) { return gm_vec2f_make(a[0], a[1]); }
gm_vec3f gm_vec3f_make(float x, float y, float z) { gm_vec3f v; v.x = x; v.y = y; v.z = z; return v; }
gm_vec3f gm_vec3f_splat(float s) { return gm_vec3f_make(s, s, s); }
gm_vec3f gm_vec3f_from_array(const float *a) { return gm_vec3f_make(a[0], a[1], a[2]); }
gm_vec4f gm_vec4f_make(float x, float y, float z, float w) { gm_vec4f v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
gm_vec4f gm_vec4f_splat(float s) { return gm_vec4f_make(s, s, s, s); }
gm_vec4f gm_vec4f_from_array(const float *a) { return gm_vec4f_make(a[0], a[1], a[2], a[3]); }

gm_vec2f gm_vec2f_add(gm_vec2f a, gm_vec2f b) { return gm_vec2f_make(a.x + b.x, a.y + b.y); }
gm_vec2f gm_vec2f_sub(gm_vec2f a, gm_vec2f b) { return gm_vec2f_make(a.x - b.x, a.y - b.y); }
gm_vec2f gm_vec2f_mul(gm_vec2f a, gm_vec2f b) { return gm_vec2f_make(a.x * b.x, a.y * b.y); }
gm_vec2f gm_vec2f_div(gm_vec2f a, gm_vec2f b) { return gm_vec2f_make(a.x / b.x, a.y / b.y); }
gm_vec2f gm_vec2f_mul_f32(gm_vec2f a, float s) { return gm_vec2f_make(a.x * s, a.y * s); }
gm_vec2f gm_vec2f_div_f32(gm_vec2f a, float s) { return gm_vec2f_make(a.x / s, a.y / s); }
float gm_vec2f_dot(gm_vec2f a, gm_vec2f b) { return a.x * b.x + a.y * b.y; }

gm_vec3f gm_vec3f_add(gm_vec3f a, gm_vec3f b) { return gm_vec3f_make(a.x + b.x, a.y + b.y, a.z + b.z); }
gm_vec3f gm_vec3f_sub(gm_vec3f a, gm_vec3f b) { return gm_vec3f_make(a.x - b.x, a.y - b.y, a.z - b.z); }
gm_vec3f gm_vec3f_mul(gm_vec3f a, gm_vec3f b) { return gm_vec3f_make(a.x * b.x, a.y * b.y, a.z * b.z); }
gm_vec3f gm_vec3f_div(gm_vec3f a, gm_vec3f b) { return gm_vec3f_make(a.x / b.x, a.y / b.y, a.z / b.z); }
gm_vec3f gm_vec3f_mul_f32(gm_vec3f a, float s) { return gm_vec3f_make(a.x * s, a.y * s, a.z * s); }
gm_vec3f gm_vec3f_div_f32(gm_vec3f a, float s) { return gm_vec3f_make(a.x / s, a.y / s, a.z / s); }
gm_vec3f gm_vec3f_cross(gm_vec3f a, gm_vec3f b)
{
    return gm_vec3f_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float gm_vec3f_dot(gm_vec3f a, gm_vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float gm_vec3f_length(gm_vec3f a) { return sqrtf(gm_vec3f_dot(a, a)); }
gm_vec3f gm_vec3f_normalize(gm_vec3f a)
{
    float len = gm_vec3f_length(a);
    return (len > 0.0f) ? gm_vec3f_div_f32(a, len) : a;
}

gm_vec4f gm_vec4f_add(gm_vec4f a, gm_vec4f b) { return gm_vec4f_make(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
gm_vec4f gm_vec4f_sub(gm_vec4f a, gm_vec4f b) { return gm_vec4f_make(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
gm_vec4f gm_vec4f_mul(gm_vec4f a, gm_vec4f b) { return gm_vec4f_make(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
gm_vec4f gm_vec4f_div(gm_vec4f a, gm_vec4f b) { return gm_vec4f_make(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
gm_vec4f gm_vec4f_mul_f32(gm_vec4f a, float s) { return gm_vec4f_make(a.x * s, a.y * s, a.z * s, a.w * s); }
gm_vec4f gm_vec4f_div_f32(gm_vec4f a, float s) { return gm_vec4f_make(a.x / s, a.y / s, a.z / s, a.w / s); }
float gm_vec4f_dot(gm_vec4f a, gm_vec4f b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
