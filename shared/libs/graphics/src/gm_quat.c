/* gm_quat.c — quaternioni. */
#include "gm_quat.h"

#include <math.h>

gm_quat gm_quat_identity(void)
{
    return gm_quat_make(0.0, 0.0, 0.0, 1.0);
}

gm_quat gm_quat_make(gm_real x, gm_real y, gm_real z, gm_real w)
{
    gm_quat q;
    q.x = x;
    q.y = y;
    q.z = z;
    q.w = w;
    return q;
}

gm_quat gm_quat_add(gm_quat a, gm_quat b)
{
    return gm_quat_make(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

gm_quat gm_quat_scale(gm_quat q, gm_real s)
{
    return gm_quat_make(q.x * s, q.y * s, q.z * s, q.w * s);
}

gm_quat gm_quat_neg(gm_quat q)
{
    return gm_quat_make(-q.x, -q.y, -q.z, -q.w);
}

gm_quat gm_quat_mul(gm_quat a, gm_quat b)
{
    return gm_quat_make(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

gm_quat gm_quat_conjugate(gm_quat q)
{
    return gm_quat_make(-q.x, -q.y, -q.z, q.w);
}

gm_real gm_quat_dot(gm_quat a, gm_quat b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

gm_real gm_quat_length(gm_quat q)
{
    return sqrt(gm_quat_dot(q, q));
}

gm_quat gm_quat_normalize(gm_quat q)
{
    gm_real len = gm_quat_length(q);
    if (len == 0.0) return gm_quat_identity();
    return gm_quat_scale(q, 1.0 / len);
}

gm_status gm_quat_inverse(gm_quat q, gm_quat *out)
{
    gm_real n = gm_quat_dot(q, q);
    if (out == NULL) return GM_ERR_ARG;
    if (n == 0.0) return GM_ERR_SINGULAR;
    *out = gm_quat_scale(gm_quat_conjugate(q), 1.0 / n);
    return GM_OK;
}

gm_quat gm_quat_lerp(gm_quat a, gm_quat b, gm_real t)
{
    return gm_quat_normalize(gm_quat_add(gm_quat_scale(a, 1.0 - t),
                                         gm_quat_scale(b, t)));
}

gm_quat gm_quat_slerp(gm_quat a, gm_quat b, gm_real t)
{
    gm_real d = gm_quat_dot(a, b);
    gm_real theta0, theta, s0, s1;
    if (d < 0.0) {
        b = gm_quat_neg(b);
        d = -d;
    }
    if (d > 0.9995) return gm_quat_lerp(a, b, t);
    if (d > 1.0) d = 1.0;
    theta0 = acos(d);
    theta = theta0 * t;
    s0 = sin(theta0 - theta) / sin(theta0);
    s1 = sin(theta) / sin(theta0);
    return gm_quat_add(gm_quat_scale(a, s0), gm_quat_scale(b, s1));
}

gm_quat gm_quat_from_axis_angle(gm_vec3 axis, gm_real radians)
{
    gm_vec3 n = gm_vec3_normalize(axis);
    gm_real half = radians * 0.5;
    gm_real s = sin(half);
    return gm_quat_make(n.x * s, n.y * s, n.z * s, cos(half));
}

gm_mat3 gm_quat_to_mat3(gm_quat q)
{
    gm_real x = q.x, y = q.y, z = q.z, w = q.w;
    gm_mat3 m;
    m.v[0] = 1.0 - 2.0 * (y * y + z * z);
    m.v[1] = 2.0 * (x * y + w * z);
    m.v[2] = 2.0 * (x * z - w * y);
    m.v[3] = 2.0 * (x * y - w * z);
    m.v[4] = 1.0 - 2.0 * (x * x + z * z);
    m.v[5] = 2.0 * (y * z + w * x);
    m.v[6] = 2.0 * (x * z + w * y);
    m.v[7] = 2.0 * (y * z - w * x);
    m.v[8] = 1.0 - 2.0 * (x * x + y * y);
    return m;
}

gm_quat gm_quat_from_mat3(gm_mat3 m)
{
    gm_real m00 = gm_mat3_at(m, 0, 0);
    gm_real m11 = gm_mat3_at(m, 1, 1);
    gm_real m22 = gm_mat3_at(m, 2, 2);
    gm_real tr = m00 + m11 + m22;
    gm_quat q;
    if (tr > 0.0) {
        gm_real s = sqrt(tr + 1.0) * 2.0;
        q.w = 0.25 * s;
        q.x = (gm_mat3_at(m, 2, 1) - gm_mat3_at(m, 1, 2)) / s;
        q.y = (gm_mat3_at(m, 0, 2) - gm_mat3_at(m, 2, 0)) / s;
        q.z = (gm_mat3_at(m, 1, 0) - gm_mat3_at(m, 0, 1)) / s;
    } else if (m00 > m11 && m00 > m22) {
        gm_real s = sqrt(1.0 + m00 - m11 - m22) * 2.0;
        q.w = (gm_mat3_at(m, 2, 1) - gm_mat3_at(m, 1, 2)) / s;
        q.x = 0.25 * s;
        q.y = (gm_mat3_at(m, 0, 1) + gm_mat3_at(m, 1, 0)) / s;
        q.z = (gm_mat3_at(m, 0, 2) + gm_mat3_at(m, 2, 0)) / s;
    } else if (m11 > m22) {
        gm_real s = sqrt(1.0 + m11 - m00 - m22) * 2.0;
        q.w = (gm_mat3_at(m, 0, 2) - gm_mat3_at(m, 2, 0)) / s;
        q.x = (gm_mat3_at(m, 0, 1) + gm_mat3_at(m, 1, 0)) / s;
        q.y = 0.25 * s;
        q.z = (gm_mat3_at(m, 1, 2) + gm_mat3_at(m, 2, 1)) / s;
    } else {
        gm_real s = sqrt(1.0 + m22 - m00 - m11) * 2.0;
        q.w = (gm_mat3_at(m, 1, 0) - gm_mat3_at(m, 0, 1)) / s;
        q.x = (gm_mat3_at(m, 0, 2) + gm_mat3_at(m, 2, 0)) / s;
        q.y = (gm_mat3_at(m, 1, 2) + gm_mat3_at(m, 2, 1)) / s;
        q.z = 0.25 * s;
    }
    return gm_quat_normalize(q);
}

gm_vec3 gm_quat_rotate_vec3(gm_quat q, gm_vec3 v)
{
    gm_vec3 u = gm_vec3_make(q.x, q.y, q.z);
    gm_vec3 t = gm_vec3_scale(gm_vec3_cross(u, v), 2.0);
    return gm_vec3_add(gm_vec3_add(v, gm_vec3_scale(t, q.w)),
                       gm_vec3_cross(u, t));
}

gm_quat gm_quat_from_two_vectors(gm_vec3 from, gm_vec3 to)
{
    gm_vec3 a = gm_vec3_normalize(from);
    gm_vec3 b = gm_vec3_normalize(to);
    gm_real r = gm_vec3_dot(a, b) + 1.0;
    if (r < 1e-12) {
        /* rotazione di 180 gradi: asse ortogonale ad a. */
        gm_vec3 axis;
        if (fabs(a.x) > fabs(a.z)) {
            axis = gm_vec3_make(-a.y, a.x, 0.0);
        } else {
            axis = gm_vec3_make(0.0, -a.z, a.y);
        }
        return gm_quat_from_axis_angle(axis, 3.14159265358979323846);
    }
    return gm_quat_normalize(gm_quat_make(gm_vec3_cross(a, b).x,
                                          gm_vec3_cross(a, b).y,
                                          gm_vec3_cross(a, b).z,
                                          r));
}

/* quaternione che porta l'asse +Z (0,0,1) verso `dir` (da Lib_math). */
gm_quat gm_quat_from_dir(gm_vec3 dir)
{
    gm_vec3 d = gm_vec3_normalize(dir);
    gm_vec3 z = gm_vec3_make(0.0, 0.0, 1.0);
    gm_real dot = gm_vec3_dot(z, d);
    if (dot > 1.0 - 1e-12) return gm_quat_identity();
    if (dot < -1.0 + 1e-12) return gm_quat_from_axis_angle(gm_vec3_make(0.0, 1.0, 0.0), 3.14159265358979323846);
    {
        gm_vec3 axis = gm_vec3_normalize(gm_vec3_cross(z, d));
        gm_real angle = acos(dot > 1.0 ? 1.0 : (dot < -1.0 ? -1.0 : dot));
        return gm_quat_from_axis_angle(axis, angle);
    }
}
