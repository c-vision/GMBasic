/* gm_gfx_geom.c — geometria 3D (riscrittura). */
#include "gm_gfx_geom.h"

#include <math.h>

gm_aabb gm_aabb_make(gm_vec3 min, gm_vec3 max)
{
    gm_aabb b;
    b.min = min;
    b.max = max;
    return b;
}

gm_aabb gm_aabb_from_points(const gm_vec3 *pts, int n)
{
    gm_aabb b;
    int i;
    if (pts == NULL || n <= 0) return gm_aabb_make(gm_vec3_zero(), gm_vec3_zero());
    b.min = pts[0]; b.max = pts[0];
    for (i = 1; i < n; i++) {
        b.min = gm_vec3_min(b.min, pts[i]);
        b.max = gm_vec3_max(b.max, pts[i]);
    }
    return b;
}

int gm_aabb_valid(gm_aabb b)
{
    return (b.min.x <= b.max.x && b.min.y <= b.max.y && b.min.z <= b.max.z) ? 1 : 0;
}

gm_aabb gm_aabb_merge(gm_aabb a, gm_aabb b)
{
    return gm_aabb_make(gm_vec3_min(a.min, b.min), gm_vec3_max(a.max, b.max));
}

gm_aabb gm_aabb_transform(gm_aabb b, gm_mat4 m)
{
    gm_vec3 pts[8];
    int i;
    for (i = 0; i < 8; i++) {
        gm_vec3 p = gm_vec3_make((i & 1) ? b.max.x : b.min.x,
                                 (i & 2) ? b.max.y : b.min.y,
                                 (i & 4) ? b.max.z : b.min.z);
        pts[i] = gm_mat4_transform_point(m, p);
    }
    return gm_aabb_from_points(pts, 8);
}

gm_vec3 gm_aabb_center(gm_aabb b)
{
    return gm_vec3_scale(gm_vec3_add(b.min, b.max), 0.5);
}

gm_vec3 gm_aabb_extents(gm_aabb b)
{
    return gm_vec3_sub(b.max, b.min);
}

int gm_aabb_test_point(gm_aabb b, gm_vec3 p)
{
    return (p.x >= b.min.x && p.x <= b.max.x
         && p.y >= b.min.y && p.y <= b.max.y
         && p.z >= b.min.z && p.z <= b.max.z) ? 1 : 0;
}

int gm_aabb_intersect(gm_aabb a, gm_aabb b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x
         && a.min.y <= b.max.y && a.max.y >= b.min.y
         && a.min.z <= b.max.z && a.max.z >= b.min.z) ? 1 : 0;
}

/* ============================== proiezioni ============================== */

gm_mat4 gm_mat4_perspective(gm_real fovy_rad, gm_real aspect, gm_real znear, gm_real zfar)
{
    gm_mat4 m = gm_mat4_zero();
    gm_real f = 1.0 / tan(fovy_rad * 0.5);
    m.v[0] = f / aspect;
    m.v[5] = f;
    m.v[10] = (zfar + znear) / (znear - zfar);
    m.v[11] = -1.0;
    m.v[14] = (2.0 * zfar * znear) / (znear - zfar);
    return m;
}

gm_mat4 gm_mat4_ortho(gm_real left, gm_real right, gm_real bottom, gm_real top,
                      gm_real znear, gm_real zfar)
{
    gm_mat4 m = gm_mat4_identity();
    m.v[0] = 2.0 / (right - left);
    m.v[5] = 2.0 / (top - bottom);
    m.v[10] = -2.0 / (zfar - znear);
    m.v[12] = -(right + left) / (right - left);
    m.v[13] = -(top + bottom) / (top - bottom);
    m.v[14] = -(zfar + znear) / (zfar - znear);
    return m;
}

gm_mat4 gm_mat4_look_at(gm_vec3 eye, gm_vec3 center, gm_vec3 up)
{
    gm_vec3 f = gm_vec3_normalize(gm_vec3_sub(center, eye));
    gm_vec3 s = gm_vec3_normalize(gm_vec3_cross(f, up));
    gm_vec3 u = gm_vec3_cross(s, f);
    gm_mat4 m = gm_mat4_identity();
    m.v[0] = s.x; m.v[4] = s.y; m.v[8] = s.z;
    m.v[1] = u.x; m.v[5] = u.y; m.v[9] = u.z;
    m.v[2] = -f.x; m.v[6] = -f.y; m.v[10] = -f.z;
    m.v[12] = -gm_vec3_dot(s, eye);
    m.v[13] = -gm_vec3_dot(u, eye);
    m.v[14] = gm_vec3_dot(f, eye);
    return m;
}

/* ============================== frustum ============================== */

static gm_vec4 normalize_plane(gm_vec4 p)
{
    gm_real len = sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (len > 0.0) { p.x /= len; p.y /= len; p.z /= len; p.w /= len; }
    return p;
}

gm_frustum gm_frustum_from_matrix(gm_mat4 m)
{
    /* Gribb–Hartmann: righe della matrice (column-major). */
    gm_vec4 r0 = gm_vec4_make(m.v[0], m.v[4], m.v[8], m.v[12]);
    gm_vec4 r1 = gm_vec4_make(m.v[1], m.v[5], m.v[9], m.v[13]);
    gm_vec4 r2 = gm_vec4_make(m.v[2], m.v[6], m.v[10], m.v[14]);
    gm_vec4 r3 = gm_vec4_make(m.v[3], m.v[7], m.v[11], m.v[15]);
    gm_frustum f;
    f.planes[0] = normalize_plane(gm_vec4_add(r3, r0)); /* left */
    f.planes[1] = normalize_plane(gm_vec4_sub(r3, r0)); /* right */
    f.planes[2] = normalize_plane(gm_vec4_add(r3, r1)); /* bottom */
    f.planes[3] = normalize_plane(gm_vec4_sub(r3, r1)); /* top */
    f.planes[4] = normalize_plane(gm_vec4_add(r3, r2)); /* near */
    f.planes[5] = normalize_plane(gm_vec4_sub(r3, r2)); /* far */
    return f;
}

int gm_frustum_test_point(gm_frustum f, gm_vec3 p)
{
    int i;
    for (i = 0; i < 6; i++) {
        gm_vec4 pl = f.planes[i];
        if (pl.x * p.x + pl.y * p.y + pl.z * p.z + pl.w < 0.0) return 0;
    }
    return 1;
}

int gm_frustum_intersect_aabb(gm_frustum f, gm_aabb b)
{
    int i;
    for (i = 0; i < 6; i++) {
        gm_vec4 pl = f.planes[i];
        gm_vec3 pv = gm_vec3_make(pl.x >= 0.0 ? b.max.x : b.min.x,
                                  pl.y >= 0.0 ? b.max.y : b.min.y,
                                  pl.z >= 0.0 ? b.max.z : b.min.z);
        if (pl.x * pv.x + pl.y * pv.y + pl.z * pv.z + pl.w < 0.0) return 0;
    }
    return 1;
}

/* ============================== SRT ============================== */

int gm_mat4_decompose_srt(gm_mat4 m, gm_vec3 *scale, gm_quat *rotation, gm_vec3 *translation)
{
    gm_vec3 c0 = gm_vec3_make(m.v[0], m.v[1], m.v[2]);
    gm_vec3 c1 = gm_vec3_make(m.v[4], m.v[5], m.v[6]);
    gm_vec3 c2 = gm_vec3_make(m.v[8], m.v[9], m.v[10]);
    gm_vec3 t = gm_vec3_make(m.v[12], m.v[13], m.v[14]);
    gm_real sx = gm_vec3_length(c0), sy = gm_vec3_length(c1), sz = gm_vec3_length(c2);
    gm_mat3 rot;
    if (sx == 0.0 || sy == 0.0 || sz == 0.0) return 0;
    rot = gm_mat3_from_cols(gm_vec3_scale(c0, 1.0 / sx),
                            gm_vec3_scale(c1, 1.0 / sy),
                            gm_vec3_scale(c2, 1.0 / sz));
    if (scale) *scale = gm_vec3_make(sx, sy, sz);
    if (rotation) *rotation = gm_quat_from_mat3(rot);
    if (translation) *translation = t;
    return 1;
}

/* ============================== camera (da ZINC) ============================== */

gm_camera gm_camera_make(gm_vec3 position, gm_vec3 target, gm_vec3 up)
{
    gm_camera c;
    gm_vec3 f = gm_vec3_normalize(gm_vec3_sub(target, position));
    gm_vec3 r = gm_vec3_normalize(gm_vec3_cross(f, up));
    c.position = position;
    c.forward = f;
    c.right = r;
    c.up = gm_vec3_cross(r, f);
    c.fov = 1.0471975511965976; /* 60° */
    c.aspect = 1.0;
    c.znear = 0.1;
    c.zfar = 100.0;
    return c;
}

gm_camera gm_camera_perspective(gm_vec3 position, gm_vec3 target, gm_vec3 up,
                                gm_real fov_rad, gm_real aspect, gm_real znear, gm_real zfar)
{
    gm_camera c = gm_camera_make(position, target, up);
    c.fov = fov_rad;
    c.aspect = aspect;
    c.znear = znear;
    c.zfar = zfar;
    return c;
}

gm_mat4 gm_camera_view(const gm_camera *c)
{
    gm_mat4 m = gm_mat4_identity();
    m.v[0] = c->right.x;   m.v[4] = c->right.y;   m.v[8] = c->right.z;
    m.v[1] = c->up.x;      m.v[5] = c->up.y;      m.v[9] = c->up.z;
    m.v[2] = -c->forward.x; m.v[6] = -c->forward.y; m.v[10] = -c->forward.z;
    m.v[12] = -gm_vec3_dot(c->right, c->position);
    m.v[13] = -gm_vec3_dot(c->up, c->position);
    m.v[14] = gm_vec3_dot(c->forward, c->position);
    return m;
}

gm_mat4 gm_camera_projection(const gm_camera *c)
{
    return gm_mat4_perspective(c->fov, c->aspect, c->znear, c->zfar);
}

gm_mat4 gm_camera_view_projection(const gm_camera *c)
{
    return gm_mat4_mul(gm_camera_projection(c), gm_camera_view(c));
}

void gm_camera_orbit(gm_camera *c, gm_vec3 target, gm_real yaw, gm_real pitch, gm_real radius)
{
    gm_vec3 dir;
    gm_vec3 pos;
    dir.x = cos(pitch) * sin(yaw);
    dir.y = sin(pitch);
    dir.z = cos(pitch) * cos(yaw);
    pos = gm_vec3_add(target, gm_vec3_scale(dir, radius));
    *c = gm_camera_make(pos, target, gm_vec3_make(0.0, 1.0, 0.0));
}

void gm_camera_translate(gm_camera *c, gm_vec3 delta)
{
    c->position = gm_vec3_add(c->position, delta);
}

gm_vec3 gm_camera_ndc_ray_dir(const gm_camera *c, gm_real ndc_x, gm_real ndc_y)
{
    gm_real th = tan(0.5 * c->fov);
    gm_vec3 dir = gm_vec3_add(gm_vec3_scale(c->forward, 1.0),
                              gm_vec3_add(gm_vec3_scale(c->right, ndc_x * c->aspect * th),
                                          gm_vec3_scale(c->up, ndc_y * th)));
    return gm_vec3_normalize(dir);
}
