/* gm_gfx_ray.c — intersezioni raggio (riscrittura). */
#include "gm_gfx_ray.h"

#include <math.h>

gm_ray gm_ray_make(gm_vec3 origin, gm_vec3 dir)
{
    gm_ray r;
    r.origin = origin;
    r.dir = dir;
    return r;
}

gm_vec3 gm_ray_at(gm_ray r, gm_real t)
{
    return gm_vec3_add(r.origin, gm_vec3_scale(r.dir, t));
}

int gm_ray_sphere(gm_ray r, gm_vec3 center, gm_real radius, gm_real *t)
{
    gm_vec3 oc = gm_vec3_sub(r.origin, center);
    gm_real a = gm_vec3_dot(r.dir, r.dir);
    gm_real b = 2.0 * gm_vec3_dot(oc, r.dir);
    gm_real c = gm_vec3_dot(oc, oc) - radius * radius;
    gm_real disc, sq, t0, t1;
    if (a == 0.0) return 0;
    disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return 0;
    sq = sqrt(disc);
    t0 = (-b - sq) / (2.0 * a);
    t1 = (-b + sq) / (2.0 * a);
    if (t0 > 0.0) { if (t) *t = t0; return 1; }
    if (t1 > 0.0) { if (t) *t = t1; return 1; }
    return 0;
}

int gm_ray_plane(gm_ray r, gm_vec3 point, gm_vec3 normal, gm_real *t)
{
    gm_real denom = gm_vec3_dot(normal, r.dir);
    gm_real tt;
    if (denom == 0.0) return 0;
    tt = gm_vec3_dot(gm_vec3_sub(point, r.origin), normal) / denom;
    if (tt < 0.0) return 0;
    if (t) *t = tt;
    return 1;
}

int gm_ray_triangle(gm_ray r, gm_vec3 v0, gm_vec3 v1, gm_vec3 v2,
                    gm_real *t, gm_real *u, gm_real *v)
{
    gm_vec3 e1 = gm_vec3_sub(v1, v0);
    gm_vec3 e2 = gm_vec3_sub(v2, v0);
    gm_vec3 p = gm_vec3_cross(r.dir, e2);
    gm_real det = gm_vec3_dot(e1, p);
    gm_vec3 tv, q;
    gm_real inv, uu, vv, tt;
    if (fabs(det) < 1e-15) return 0;
    inv = 1.0 / det;
    tv = gm_vec3_sub(r.origin, v0);
    uu = gm_vec3_dot(tv, p) * inv;
    if (uu < 0.0 || uu > 1.0) return 0;
    q = gm_vec3_cross(tv, e1);
    vv = gm_vec3_dot(r.dir, q) * inv;
    if (vv < 0.0 || uu + vv > 1.0) return 0;
    tt = gm_vec3_dot(e2, q) * inv;
    if (tt < 0.0) return 0;
    if (t) *t = tt;
    if (u) *u = uu;
    if (v) *v = vv;
    return 1;
}

int gm_ray_aabb(gm_ray r, gm_vec3 bmin, gm_vec3 bmax, gm_real *tmin, gm_real *tmax)
{
    gm_real lo = -1e300, hi = 1e300;
    int axis;
    for (axis = 0; axis < 3; axis++) {
        gm_real o = gm_vec3_get(r.origin, axis);
        gm_real d = gm_vec3_get(r.dir, axis);
        gm_real mn = gm_vec3_get(bmin, axis);
        gm_real mx = gm_vec3_get(bmax, axis);
        if (fabs(d) < 1e-300) {
            if (o < mn || o > mx) return 0;
        } else {
            gm_real t0 = (mn - o) / d;
            gm_real t1 = (mx - o) / d;
            gm_real tmp;
            if (t0 > t1) { tmp = t0; t0 = t1; t1 = tmp; }
            if (t0 > lo) lo = t0;
            if (t1 < hi) hi = t1;
            if (lo > hi) return 0;
        }
    }
    if (hi < 0.0) return 0;
    if (tmin) *tmin = lo;
    if (tmax) *tmax = hi;
    return 1;
}

int gm_ray_box(gm_ray r, gm_vec3 bmin, gm_vec3 bmax, gm_real *t)
{
    gm_real lo, hi;
    if (!gm_ray_aabb(r, bmin, bmax, &lo, &hi)) return 0;
    if (t) *t = (lo > 0.0) ? lo : hi;
    return 1;
}

gm_vec3 gm_ray_reflect(gm_vec3 v, gm_vec3 n)
{
    gm_real d = gm_vec3_dot(v, n);
    return gm_vec3_sub(v, gm_vec3_scale(n, 2.0 * d));
}

int gm_ray_refract(gm_vec3 v, gm_vec3 n, gm_real eta, gm_vec3 *out)
{
    gm_vec3 uv = gm_vec3_normalize(v);
    gm_real dt = gm_vec3_dot(uv, n);
    gm_real k = 1.0 - eta * eta * (1.0 - dt * dt);
    if (k < 0.0) return 0; /* riflessione totale interna */
    if (out) {
        *out = gm_vec3_sub(gm_vec3_scale(uv, eta),
                           gm_vec3_scale(n, eta * dt + sqrt(k)));
    }
    return 1;
}

/* ---- cilindro / cono (da miniRT) ---- */

static int solve_quad(gm_real a, gm_real b, gm_real c, gm_real *r0, gm_real *r1)
{
    gm_real disc, sq, q;
    if (fabs(a) < 1e-12) {
        if (fabs(b) < 1e-12) return 0;
        *r0 = -c / b;
        return 1;
    }
    disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return 0;
    sq = sqrt(disc);
    q = -0.5 * (b + (b >= 0.0 ? sq : -sq));
    *r0 = q / a;
    *r1 = (q != 0.0) ? c / q : q / a;
    return 2;
}

int gm_ray_cylinder(gm_ray r, gm_vec3 base, gm_vec3 axis, gm_real radius,
                    gm_real height, gm_real *t)
{
    gm_vec3 d = r.dir, o = gm_vec3_sub(r.origin, base);
    gm_real da = gm_vec3_dot(d, axis), oa = gm_vec3_dot(o, axis);
    gm_vec3 dp = gm_vec3_sub(d, gm_vec3_scale(axis, da));
    gm_vec3 op = gm_vec3_sub(o, gm_vec3_scale(axis, oa));
    gm_real a = gm_vec3_dot(dp, dp);
    gm_real b = 2.0 * gm_vec3_dot(op, dp);
    gm_real c = gm_vec3_dot(op, op) - radius * radius;
    gm_real roots[2], best = 0.0;
    int n, i, hit = 0;
    n = solve_quad(a, b, c, &roots[0], &roots[1]);
    for (i = 0; i < n; i++) {
        gm_real tt = roots[i], y = oa + tt * da;
        if (tt > 1e-9 && y >= -1e-9 && y <= height + 1e-9 && (!hit || tt < best)) { best = tt; hit = 1; }
    }
    if (fabs(da) > 1e-12) {
        int ci;
        for (ci = 0; ci < 2; ci++) {
            gm_real yc = ci ? height : 0.0, tt = (yc - oa) / da, rr;
            gm_vec3 p, pp;
            if (tt <= 1e-9) continue;
            p = gm_vec3_add(o, gm_vec3_scale(d, tt));
            pp = gm_vec3_sub(p, gm_vec3_scale(axis, gm_vec3_dot(p, axis)));
            rr = gm_vec3_dot(pp, pp);
            if (rr <= radius * radius + 1e-9 && (!hit || tt < best)) { best = tt; hit = 1; }
        }
    }
    if (hit) *t = best;
    return hit;
}

int gm_ray_cone(gm_ray r, gm_vec3 apex, gm_vec3 axis, gm_real radius,
                gm_real height, gm_real *t)
{
    gm_vec3 d = r.dir, O = gm_vec3_sub(r.origin, apex);
    gm_real k = radius / height;
    gm_real dy = gm_vec3_dot(d, axis), oy = gm_vec3_dot(O, axis);
    gm_real dd = gm_vec3_dot(d, d), Od = gm_vec3_dot(O, d), OO = gm_vec3_dot(O, O);
    gm_real a = (dd - dy * dy) - k * k * dy * dy;
    gm_real b = 2.0 * (Od - oy * dy * (1.0 + k * k));
    gm_real c = (OO - oy * oy) - k * k * oy * oy;
    gm_real roots[2], best = 0.0;
    int n, i, hit = 0;
    n = solve_quad(a, b, c, &roots[0], &roots[1]);
    for (i = 0; i < n; i++) {
        gm_real tt = roots[i], y = oy + tt * dy;
        if (tt > 1e-9 && y >= -1e-9 && y <= height + 1e-9 && (!hit || tt < best)) { best = tt; hit = 1; }
    }
    if (fabs(dy) > 1e-12) {
        gm_real tt = (height - oy) / dy, rr;
        gm_vec3 p, pp;
        if (tt > 1e-9) {
            p = gm_vec3_add(O, gm_vec3_scale(d, tt));
            pp = gm_vec3_sub(p, gm_vec3_scale(axis, gm_vec3_dot(p, axis)));
            rr = gm_vec3_dot(pp, pp);
            if (rr <= radius * radius + 1e-9 && (!hit || tt < best)) { best = tt; hit = 1; }
        }
    }
    if (hit) *t = best;
    return hit;
}

gm_vec3 gm_cylinder_normal(gm_vec3 base, gm_vec3 axis, gm_vec3 p)
{
    gm_vec3 v = gm_vec3_sub(p, base);
    gm_vec3 perp = gm_vec3_sub(v, gm_vec3_scale(axis, gm_vec3_dot(v, axis)));
    return gm_vec3_normalize(perp);
}

gm_vec3 gm_cone_normal(gm_vec3 apex, gm_vec3 axis, gm_real k, gm_vec3 p)
{
    gm_vec3 v = gm_vec3_sub(p, apex);
    gm_real y = gm_vec3_dot(v, axis);
    gm_vec3 perp = gm_vec3_sub(v, gm_vec3_scale(axis, y));
    return gm_vec3_normalize(gm_vec3_sub(perp, gm_vec3_scale(axis, k * k * y)));
}

gm_vec3 gm_shade_phong(gm_vec3 p, gm_vec3 n, gm_vec3 view_dir, gm_vec3 light_pos,
                       gm_vec3 light_color, gm_vec3 obj_color,
                       gm_real ka, gm_real kd, gm_real ks, gm_real shininess)
{
    gm_vec3 L = gm_vec3_normalize(gm_vec3_sub(light_pos, p));
    gm_vec3 H = gm_vec3_normalize(gm_vec3_add(L, gm_vec3_normalize(view_dir)));
    gm_real ndl = gm_vec3_dot(n, L);
    gm_real ndh = gm_vec3_dot(n, H);
    gm_vec3 ambient, diffuse, specular;
    if (ndl < 0.0) ndl = 0.0;
    if (ndh < 0.0) ndh = 0.0;
    ambient = gm_vec3_scale(obj_color, ka);
    diffuse = gm_vec3_scale(gm_vec3_mul(obj_color, light_color), kd * ndl);
    specular = gm_vec3_scale(light_color, ks * pow(ndh, shininess));
    return gm_vec3_add(gm_vec3_add(ambient, diffuse), specular);
}

/* ---- raycasting DDA su griglia (da cub3d42) ---- */
int gm_raycast_dda(const int *grid, int rows, int cols, gm_real pos_x, gm_real pos_y,
                   gm_real dir_x, gm_real dir_y, int max_steps, gm_raycast_hit *hit)
{
    int map_x, map_y, step_x, step_y, side = 0, k;
    gm_real ddx, ddy, sdx, sdy, perp;
    if (!grid || !hit || rows <= 0 || cols <= 0) return 0;
    map_x = (int)floor(pos_x);
    map_y = (int)floor(pos_y);
    ddx = (dir_x == 0.0) ? 1e30 : fabs(1.0 / dir_x);
    ddy = (dir_y == 0.0) ? 1e30 : fabs(1.0 / dir_y);
    if (dir_x < 0.0) { step_x = -1; sdx = (pos_x - map_x) * ddx; }
    else { step_x = 1; sdx = (map_x + 1 - pos_x) * ddx; }
    if (dir_y < 0.0) { step_y = -1; sdy = (pos_y - map_y) * ddy; }
    else { step_y = 1; sdy = (map_y + 1 - pos_y) * ddy; }
    hit->hit = 0;
    for (k = 0; k < max_steps; k++) {
        if (sdx < sdy) { sdx += ddx; map_x += step_x; side = 0; }
        else { sdy += ddy; map_y += step_y; side = 1; }
        if (map_x < 0 || map_x >= cols || map_y < 0 || map_y >= rows) return 0;
        if (grid[(size_t)map_y * cols + map_x] != 0) {
            if (side == 0)
                perp = (map_x - pos_x + (1 - step_x) / 2.0) / dir_x;
            else
                perp = (map_y - pos_y + (1 - step_y) / 2.0) / dir_y;
            hit->hit = 1;
            hit->map_x = map_x;
            hit->map_y = map_y;
            hit->side = side;
            hit->perp_dist = perp;
            {
                gm_real wx = (side == 0) ? pos_y + perp * dir_y : pos_x + perp * dir_x;
                wx -= floor(wx);
                hit->wall_x = wx;
            }
            return 1;
        }
    }
    return 0;
}

/* ===================== Volume rendering (da f2-nerf / NeRF) ===================== */

void gm_gfx_volume_render(int n, const gm_real *sigma, const gm_real *rgb,
                          const gm_real *dt, gm_real *out_rgb, gm_real *out_acc)
{
    gm_real T = 1.0, acc = 0.0;
    int i;
    if (!out_rgb) return;
    out_rgb[0] = out_rgb[1] = out_rgb[2] = 0.0;
    if (n < 1 || !sigma || !rgb || !dt) { if (out_acc) *out_acc = 0.0; return; }
    for (i = 0; i < n; i++) {
        gm_real alpha = 1.0 - exp(-sigma[i] * dt[i]);
        gm_real w = T * alpha;
        out_rgb[0] += w * rgb[i * 3 + 0];
        out_rgb[1] += w * rgb[i * 3 + 1];
        out_rgb[2] += w * rgb[i * 3 + 2];
        acc += w;
        T *= (1.0 - alpha);
        if (T < 1e-6) break;
    }
    if (out_acc) *out_acc = acc;
}
