/* gm_vec.c — vettori 2D/3D/4D. */
#include "gm_vec.h"

#include <math.h>

/* ============================== vec2 ============================== */

gm_vec2 gm_vec2_make(gm_real x, gm_real y)
{
    gm_vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

gm_vec2 gm_vec2_zero(void)
{
    return gm_vec2_make(0.0, 0.0);
}

gm_vec2 gm_vec2_one(void)
{
    return gm_vec2_make(1.0, 1.0);
}

gm_vec2 gm_vec2_splat(gm_real s)
{
    return gm_vec2_make(s, s);
}

gm_real gm_vec2_get(gm_vec2 v, int i)
{
    if (i == 0) return v.x;
    if (i == 1) return v.y;
    return 0.0;
}

gm_vec2 gm_vec2_set(gm_vec2 v, int i, gm_real value)
{
    if (i == 0) v.x = value;
    else if (i == 1) v.y = value;
    return v;
}

gm_vec2 gm_vec2_add(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_make(a.x + b.x, a.y + b.y);
}

gm_vec2 gm_vec2_sub(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_make(a.x - b.x, a.y - b.y);
}

gm_vec2 gm_vec2_mul(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_make(a.x * b.x, a.y * b.y);
}

gm_vec2 gm_vec2_div(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_make(a.x / b.x, a.y / b.y);
}

gm_vec2 gm_vec2_scale(gm_vec2 v, gm_real s)
{
    return gm_vec2_make(v.x * s, v.y * s);
}

gm_vec2 gm_vec2_neg(gm_vec2 v)
{
    return gm_vec2_make(-v.x, -v.y);
}

gm_real gm_vec2_dot(gm_vec2 a, gm_vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

gm_real gm_vec2_cross(gm_vec2 a, gm_vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

gm_real gm_vec2_length_sq(gm_vec2 v)
{
    return v.x * v.x + v.y * v.y;
}

gm_real gm_vec2_length(gm_vec2 v)
{
    return sqrt(gm_vec2_length_sq(v));
}

gm_real gm_vec2_distance(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_length(gm_vec2_sub(a, b));
}

gm_vec2 gm_vec2_normalize(gm_vec2 v)
{
    gm_real len = gm_vec2_length(v);
    if (len == 0.0) return gm_vec2_zero();
    return gm_vec2_scale(v, 1.0 / len);
}

gm_vec2 gm_vec2_lerp(gm_vec2 a, gm_vec2 b, gm_real t)
{
    return gm_vec2_make(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

gm_vec2 gm_vec2_min(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_make(fmin(a.x, b.x), fmin(a.y, b.y));
}

gm_vec2 gm_vec2_max(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_make(fmax(a.x, b.x), fmax(a.y, b.y));
}

gm_vec2 gm_vec2_abs(gm_vec2 v)
{
    return gm_vec2_make(fabs(v.x), fabs(v.y));
}

gm_vec2 gm_vec2_perp(gm_vec2 v)
{
    return gm_vec2_make(-v.y, v.x);
}

gm_vec2 gm_vec2_rotate(gm_vec2 v, gm_real radians)
{
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    return gm_vec2_make(c * v.x - s * v.y, s * v.x + c * v.y);
}

gm_real gm_vec2_angle(gm_vec2 a, gm_vec2 b)
{
    gm_real d = gm_vec2_dot(a, b);
    gm_real l = gm_vec2_length(a) * gm_vec2_length(b);
    if (l == 0.0) return 0.0;
    d /= l;
    if (d > 1.0) d = 1.0;
    if (d < -1.0) d = -1.0;
    return acos(d);
}

/* ============================== vec3 ============================== */

gm_vec3 gm_vec3_make(gm_real x, gm_real y, gm_real z)
{
    gm_vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

gm_vec3 gm_vec3_zero(void)
{
    return gm_vec3_make(0.0, 0.0, 0.0);
}

gm_vec3 gm_vec3_one(void)
{
    return gm_vec3_make(1.0, 1.0, 1.0);
}

gm_vec3 gm_vec3_splat(gm_real s)
{
    return gm_vec3_make(s, s, s);
}

gm_real gm_vec3_get(gm_vec3 v, int i)
{
    if (i == 0) return v.x;
    if (i == 1) return v.y;
    if (i == 2) return v.z;
    return 0.0;
}

gm_vec3 gm_vec3_set(gm_vec3 v, int i, gm_real value)
{
    if (i == 0) v.x = value;
    else if (i == 1) v.y = value;
    else if (i == 2) v.z = value;
    return v;
}

gm_vec3 gm_vec3_add(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

gm_vec3 gm_vec3_sub(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

gm_vec3 gm_vec3_mul(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(a.x * b.x, a.y * b.y, a.z * b.z);
}

gm_vec3 gm_vec3_div(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(a.x / b.x, a.y / b.y, a.z / b.z);
}

gm_vec3 gm_vec3_scale(gm_vec3 v, gm_real s)
{
    return gm_vec3_make(v.x * s, v.y * s, v.z * s);
}

gm_vec3 gm_vec3_neg(gm_vec3 v)
{
    return gm_vec3_make(-v.x, -v.y, -v.z);
}

gm_real gm_vec3_dot(gm_vec3 a, gm_vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

gm_vec3 gm_vec3_cross(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x);
}

gm_real gm_vec3_length_sq(gm_vec3 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

gm_real gm_vec3_length(gm_vec3 v)
{
    return sqrt(gm_vec3_length_sq(v));
}

/* norme L1 / L-infinito (da Numerical-Library: manhattan_norm). */
gm_real gm_vec2_norm_l1(gm_vec2 v) { return fabs(v.x) + fabs(v.y); }
gm_real gm_vec2_norm_inf(gm_vec2 v) { gm_real m = fabs(v.x); if (fabs(v.y) > m) m = fabs(v.y); return m; }
gm_real gm_vec3_norm_l1(gm_vec3 v) { return fabs(v.x) + fabs(v.y) + fabs(v.z); }
gm_real gm_vec3_norm_inf(gm_vec3 v)
{
    gm_real m = fabs(v.x);
    if (fabs(v.y) > m) m = fabs(v.y);
    if (fabs(v.z) > m) m = fabs(v.z);
    return m;
}
gm_real gm_vec4_norm_l1(gm_vec4 v) { return fabs(v.x) + fabs(v.y) + fabs(v.z) + fabs(v.w); }
gm_real gm_vec4_norm_inf(gm_vec4 v)
{
    gm_real m = fabs(v.x);
    if (fabs(v.y) > m) m = fabs(v.y);
    if (fabs(v.z) > m) m = fabs(v.z);
    if (fabs(v.w) > m) m = fabs(v.w);
    return m;
}

gm_real gm_vec3_distance(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_length(gm_vec3_sub(a, b));
}

gm_vec3 gm_vec3_normalize(gm_vec3 v)
{
    gm_real len = gm_vec3_length(v);
    if (len == 0.0) return gm_vec3_zero();
    return gm_vec3_scale(v, 1.0 / len);
}

gm_vec3 gm_vec3_lerp(gm_vec3 a, gm_vec3 b, gm_real t)
{
    return gm_vec3_make(a.x + (b.x - a.x) * t,
                        a.y + (b.y - a.y) * t,
                        a.z + (b.z - a.z) * t);
}

gm_vec3 gm_vec3_min(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z));
}

gm_vec3 gm_vec3_max(gm_vec3 a, gm_vec3 b)
{
    return gm_vec3_make(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z));
}

gm_vec3 gm_vec3_abs(gm_vec3 v)
{
    return gm_vec3_make(fabs(v.x), fabs(v.y), fabs(v.z));
}

gm_real gm_vec3_angle(gm_vec3 a, gm_vec3 b)
{
    gm_real d = gm_vec3_dot(a, b);
    gm_real l = gm_vec3_length(a) * gm_vec3_length(b);
    if (l == 0.0) return 0.0;
    d /= l;
    if (d > 1.0) d = 1.0;
    if (d < -1.0) d = -1.0;
    return acos(d);
}

gm_vec3 gm_vec3_reflect(gm_vec3 v, gm_vec3 normal)
{
    /* r = v - 2 (v·n) n, con n assunto normalizzato. */
    gm_real d = gm_vec3_dot(v, normal);
    return gm_vec3_sub(v, gm_vec3_scale(normal, 2.0 * d));
}

gm_vec3 gm_vec3_project(gm_vec3 v, gm_vec3 onto)
{
    gm_real d = gm_vec3_length_sq(onto);
    if (d == 0.0) return gm_vec3_zero();
    return gm_vec3_scale(onto, gm_vec3_dot(v, onto) / d);
}

gm_vec3 gm_vec3_reject(gm_vec3 v, gm_vec3 onto)
{
    return gm_vec3_sub(v, gm_vec3_project(v, onto));
}

/* ============================== vec4 ============================== */

gm_vec4 gm_vec4_make(gm_real x, gm_real y, gm_real z, gm_real w)
{
    gm_vec4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

gm_vec4 gm_vec4_zero(void)
{
    return gm_vec4_make(0.0, 0.0, 0.0, 0.0);
}

gm_vec4 gm_vec4_one(void)
{
    return gm_vec4_make(1.0, 1.0, 1.0, 1.0);
}

gm_vec4 gm_vec4_splat(gm_real s)
{
    return gm_vec4_make(s, s, s, s);
}

gm_real gm_vec4_get(gm_vec4 v, int i)
{
    if (i == 0) return v.x;
    if (i == 1) return v.y;
    if (i == 2) return v.z;
    if (i == 3) return v.w;
    return 0.0;
}

gm_vec4 gm_vec4_set(gm_vec4 v, int i, gm_real value)
{
    if (i == 0) v.x = value;
    else if (i == 1) v.y = value;
    else if (i == 2) v.z = value;
    else if (i == 3) v.w = value;
    return v;
}

gm_vec4 gm_vec4_add(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_make(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

gm_vec4 gm_vec4_sub(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_make(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

gm_vec4 gm_vec4_mul(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_make(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

gm_vec4 gm_vec4_div(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_make(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

gm_vec4 gm_vec4_scale(gm_vec4 v, gm_real s)
{
    return gm_vec4_make(v.x * s, v.y * s, v.z * s, v.w * s);
}

gm_vec4 gm_vec4_neg(gm_vec4 v)
{
    return gm_vec4_make(-v.x, -v.y, -v.z, -v.w);
}

gm_real gm_vec4_dot(gm_vec4 a, gm_vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

gm_real gm_vec4_length_sq(gm_vec4 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

gm_real gm_vec4_length(gm_vec4 v)
{
    return sqrt(gm_vec4_length_sq(v));
}

gm_real gm_vec4_distance(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_length(gm_vec4_sub(a, b));
}

gm_vec4 gm_vec4_normalize(gm_vec4 v)
{
    gm_real len = gm_vec4_length(v);
    if (len == 0.0) return gm_vec4_zero();
    return gm_vec4_scale(v, 1.0 / len);
}

gm_vec4 gm_vec4_lerp(gm_vec4 a, gm_vec4 b, gm_real t)
{
    return gm_vec4_make(a.x + (b.x - a.x) * t,
                        a.y + (b.y - a.y) * t,
                        a.z + (b.z - a.z) * t,
                        a.w + (b.w - a.w) * t);
}

gm_vec4 gm_vec4_min(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_make(fmin(a.x, b.x), fmin(a.y, b.y),
                        fmin(a.z, b.z), fmin(a.w, b.w));
}

gm_vec4 gm_vec4_max(gm_vec4 a, gm_vec4 b)
{
    return gm_vec4_make(fmax(a.x, b.x), fmax(a.y, b.y),
                        fmax(a.z, b.z), fmax(a.w, b.w));
}

gm_vec4 gm_vec4_abs(gm_vec4 v)
{
    return gm_vec4_make(fabs(v.x), fabs(v.y), fabs(v.z), fabs(v.w));
}

/* ---- graphics/path-tracing (da Lib_math) ---- */
gm_vec3 gm_vec3_schlick(gm_vec3 f0, gm_real u)
{
    gm_real t = 1.0 - u, f = t * t * t * t * t;
    return gm_vec3_add(gm_vec3_scale(f0, 1.0 - f), gm_vec3_splat(f));
}

gm_vec3 gm_vec3_bias(gm_vec3 v, gm_vec3 n)
{
    gm_vec3 a = gm_vec3_abs(v);
    gm_real m = a.x;
    gm_real bias;
    if (a.y > m) m = a.y;
    if (a.z > m) m = a.z;
    bias = m * 1e-7;                 /* G_EPSILON */
    if (bias < 1e-7) bias = 1e-7;    /* B_EPSILON */
    return gm_vec3_add(v, gm_vec3_scale(n, bias));
}

gm_vec3 gm_vec3_clamp_mag(gm_vec3 v, gm_real max)
{
    gm_real len_sq = gm_vec3_length_sq(v);
    if (len_sq > max * max && len_sq > 0.0)
        return gm_vec3_scale(v, max / sqrt(len_sq));
    return v;
}

gm_vec3 gm_vec3_clamp(gm_vec3 v, gm_real lo, gm_real hi)
{
    gm_vec3 r;
    r.x = v.x < lo ? lo : (v.x > hi ? hi : v.x);
    r.y = v.y < lo ? lo : (v.y > hi ? hi : v.y);
    r.z = v.z < lo ? lo : (v.z > hi ? hi : v.z);
    return r;
}

gm_vec3 gm_vec3_clamp01(gm_vec3 v) { return gm_vec3_clamp(v, 0.0, 1.0); }
gm_vec3 gm_vec3_sqrt(gm_vec3 v) { return gm_vec3_make(sqrt(v.x), sqrt(v.y), sqrt(v.z)); }
gm_vec3 gm_vec3_pow(gm_vec3 v, gm_real p) { return gm_vec3_make(pow(v.x, p), pow(v.y, p), pow(v.z, p)); }

gm_vec3 gm_vec3_unit_random(unsigned long long *seed)
{
    unsigned long long s = seed ? *seed : 88172645463325252ULL;
    gm_vec3 r;
    for (;;) {
        gm_real x, y, z, len_sq;
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        x = (gm_real)(s >> 11) / 9007199254740992.0 * 2.0 - 1.0;
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        y = (gm_real)(s >> 11) / 9007199254740992.0 * 2.0 - 1.0;
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        z = (gm_real)(s >> 11) / 9007199254740992.0 * 2.0 - 1.0;
        len_sq = x * x + y * y + z * z;
        if (len_sq <= 1.0 && len_sq >= 1e-8) {
            gm_real inv = 1.0 / sqrt(len_sq);
            r = gm_vec3_make(x * inv, y * inv, z * inv);
            break;
        }
    }
    if (seed) *seed = s;
    return r;
}

uint32_t gm_vec3_to_uint32(gm_vec3 v)
{
    unsigned int r = (unsigned int)(v.x * 255.0 + 0.5);
    unsigned int g = (unsigned int)(v.y * 255.0 + 0.5);
    unsigned int b = (unsigned int)(v.z * 255.0 + 0.5);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
