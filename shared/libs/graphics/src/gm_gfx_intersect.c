/* gm_gfx_intersect.c — test di intersezione 3D/2D (da c3dlas). */
#include "gm_gfx_intersect.h"

#include <math.h>

int gm_intersect_sphere_sphere(gm_vec3 c1, gm_real r1, gm_vec3 c2, gm_real r2)
{
    gm_real d = gm_vec3_length_sq(gm_vec3_sub(c1, c2));
    gm_real rr = r1 + r2;
    return d <= rr * rr;
}

int gm_intersect_sphere_box(gm_vec3 c, gm_real r, gm_vec3 bmin, gm_vec3 bmax)
{
    gm_real d2 = 0.0;
    int i;
    for (i = 0; i < 3; i++) {
        gm_real v = gm_vec3_get(c, i);
        gm_real lo = gm_vec3_get(bmin, i), hi = gm_vec3_get(bmax, i);
        if (v < lo) { gm_real d = lo - v; d2 += d * d; }
        else if (v > hi) { gm_real d = v - hi; d2 += d * d; }
    }
    return d2 <= r * r;
}

int gm_intersect_sphere_plane(gm_vec3 c, gm_real r, gm_vec3 pp, gm_vec3 n, gm_real *dist)
{
    gm_real d = gm_vec3_dot(gm_vec3_sub(c, pp), n);
    if (dist) *dist = d;
    return fabs(d) <= r;
}

int gm_intersect_segment_plane(gm_vec3 a, gm_vec3 b, gm_vec3 pp, gm_vec3 n, gm_real *t)
{
    gm_vec3 ab = gm_vec3_sub(b, a);
    gm_real denom = gm_vec3_dot(n, ab);
    gm_real ta;
    if (denom == 0.0) return 0;
    ta = gm_vec3_dot(gm_vec3_sub(pp, a), n) / denom;
    if (ta >= 0.0 && ta <= 1.0) { if (t) *t = ta; return 1; }
    return 0;
}

int gm_intersect_segment_sphere(gm_vec3 a, gm_vec3 b, gm_vec3 c, gm_real r, gm_real *t)
{
    gm_vec3 d = gm_vec3_sub(b, a);
    gm_vec3 f = gm_vec3_sub(a, c);
    gm_real A = gm_vec3_dot(d, d);
    gm_real B = 2.0 * gm_vec3_dot(f, d);
    gm_real C = gm_vec3_dot(f, f) - r * r;
    gm_real disc, s, t0, t1;
    if (A == 0.0) return 0;
    disc = B * B - 4.0 * A * C;
    if (disc < 0.0) return 0;
    s = sqrt(disc);
    t0 = (-B - s) / (2.0 * A);
    t1 = (-B + s) / (2.0 * A);
    if (t0 >= 0.0 && t0 <= 1.0) { if (t) *t = t0; return 1; }
    if (t1 >= 0.0 && t1 <= 1.0) { if (t) *t = t1; return 1; }
    return 0;
}

int gm_intersect_segment_triangle(gm_vec3 a, gm_vec3 b, gm_vec3 v0, gm_vec3 v1, gm_vec3 v2, gm_real *t)
{
    gm_vec3 dir = gm_vec3_sub(b, a);
    gm_vec3 e1 = gm_vec3_sub(v1, v0);
    gm_vec3 e2 = gm_vec3_sub(v2, v0);
    gm_vec3 p = gm_vec3_cross(dir, e2);
    gm_real det = gm_vec3_dot(e1, p);
    gm_vec3 sv, q;
    gm_real inv, u, v, tt;
    if (fabs(det) < 1e-12) return 0;
    inv = 1.0 / det;
    sv = gm_vec3_sub(a, v0);
    u = gm_vec3_dot(sv, p) * inv;
    if (u < 0.0 || u > 1.0) return 0;
    q = gm_vec3_cross(sv, e1);
    v = gm_vec3_dot(dir, q) * inv;
    if (v < 0.0 || u + v > 1.0) return 0;
    tt = gm_vec3_dot(e2, q) * inv;
    if (tt >= 0.0 && tt <= 1.0) { if (t) *t = tt; return 1; }
    return 0;
}

int gm_intersect_triangle_triangle(const gm_vec3 *t1, const gm_vec3 *t2)
{
    /* intersezione se un lato di t2 interseca t1 o viceversa (test dei bordi) */
    int i, j;
    if (t1 == NULL || t2 == NULL) return 0;
    for (i = 0; i < 3; i++) {
        gm_vec3 a = t2[i], b = t2[(i + 1) % 3];
        if (gm_intersect_segment_triangle(a, b, t1[0], t1[1], t1[2], NULL)) return 1;
    }
    for (i = 0; i < 3; i++) {
        gm_vec3 a = t1[i], b = t1[(i + 1) % 3];
        if (gm_intersect_segment_triangle(a, b, t2[0], t2[1], t2[2], NULL)) return 1;
    }
    (void)j;
    return 0;
}

int gm_intersect_circle_circle(gm_vec2 c1, gm_real r1, gm_vec2 c2, gm_real r2)
{
    gm_real d = gm_vec2_length_sq(gm_vec2_sub(c1, c2));
    gm_real rr = r1 + r2;
    return d <= rr * rr;
}

int gm_intersect_segment_circle(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_real r, gm_real *t)
{
    gm_vec2 d = gm_vec2_sub(b, a);
    gm_vec2 f = gm_vec2_sub(a, c);
    gm_real A = gm_vec2_dot(d, d);
    gm_real B = 2.0 * gm_vec2_dot(f, d);
    gm_real C = gm_vec2_dot(f, f) - r * r;
    gm_real disc, s, t0, t1;
    if (A == 0.0) return 0;
    disc = B * B - 4.0 * A * C;
    if (disc < 0.0) return 0;
    s = sqrt(disc);
    t0 = (-B - s) / (2.0 * A);
    t1 = (-B + s) / (2.0 * A);
    if (t0 >= 0.0 && t0 <= 1.0) { if (t) *t = t0; return 1; }
    if (t1 >= 0.0 && t1 <= 1.0) { if (t) *t = t1; return 1; }
    return 0;
}
