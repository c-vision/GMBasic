/* gm_gfx_shape.c — geometria 2D per grafica (da RLMath, bitmap-editor). */
#include "gm_gfx_shape.h"

#include <math.h>

gm_ray2 gm_ray2_make(gm_vec2 origin, gm_vec2 dir)
{
    gm_ray2 r;
    r.origin = origin;
    r.dir = dir;
    return r;
}

gm_vec2 gm_ray2_at(gm_ray2 r, gm_real t)
{
    return gm_vec2_make(r.origin.x + t * r.dir.x, r.origin.y + t * r.dir.y);
}

gm_circle gm_circle_make(gm_vec2 center, gm_real radius)
{
    gm_circle c;
    c.center = center;
    c.radius = radius;
    return c;
}

gm_aabb2 gm_aabb2_make(gm_vec2 min, gm_vec2 max)
{
    gm_aabb2 b;
    b.min = min;
    b.max = max;
    return b;
}

gm_aabb2 gm_aabb2_from_points(const gm_vec2 *pts, int n)
{
    gm_aabb2 b;
    int i;
    if (pts == NULL || n <= 0) return gm_aabb2_make(gm_vec2_zero(), gm_vec2_zero());
    b.min = b.max = pts[0];
    for (i = 1; i < n; i++) {
        if (pts[i].x < b.min.x) b.min.x = pts[i].x;
        if (pts[i].y < b.min.y) b.min.y = pts[i].y;
        if (pts[i].x > b.max.x) b.max.x = pts[i].x;
        if (pts[i].y > b.max.y) b.max.y = pts[i].y;
    }
    return b;
}

gm_aabb2 gm_aabb2_merge(gm_aabb2 a, gm_aabb2 b)
{
    return gm_aabb2_make(gm_vec2_min(a.min, b.min), gm_vec2_max(a.max, b.max));
}

gm_vec2 gm_aabb2_center(gm_aabb2 b)
{
    return gm_vec2_scale(gm_vec2_add(b.min, b.max), 0.5);
}

gm_vec2 gm_aabb2_extents(gm_aabb2 b)
{
    return gm_vec2_scale(gm_vec2_sub(b.max, b.min), 0.5);
}

int gm_aabb2_valid(gm_aabb2 b)
{
    return (b.min.x <= b.max.x) && (b.min.y <= b.max.y);
}

gm_real gm_shape_distance(gm_vec2 a, gm_vec2 b)
{
    return gm_vec2_distance(a, b);
}

gm_vec2 gm_shape_closest_point_segment(gm_vec2 p, gm_vec2 a, gm_vec2 b)
{
    gm_vec2 ab = gm_vec2_sub(b, a);
    gm_real t = gm_vec2_dot(gm_vec2_sub(p, a), ab);
    gm_real d = gm_vec2_length_sq(ab);
    if (d == 0.0) return a;
    t /= d;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return gm_vec2_add(a, gm_vec2_scale(ab, t));
}

gm_vec2 gm_shape_closest_point_ray(gm_ray2 r, gm_vec2 p)
{
    gm_real t = gm_vec2_dot(gm_vec2_sub(p, r.origin), r.dir);
    gm_real d = gm_vec2_length_sq(r.dir);
    if (d != 0.0) t /= d;
    if (t < 0.0) t = 0.0;
    return gm_ray2_at(r, t);
}

gm_vec2 gm_shape_closest_point_aabb2(gm_aabb2 box, gm_vec2 p)
{
    return gm_vec2_make(p.x < box.min.x ? box.min.x : (p.x > box.max.x ? box.max.x : p.x),
                        p.y < box.min.y ? box.min.y : (p.y > box.max.y ? box.max.y : p.y));
}

gm_vec2 gm_shape_closest_point_circle(gm_circle c, gm_vec2 p)
{
    gm_vec2 d = gm_vec2_sub(p, c.center);
    gm_real len = gm_vec2_length(d);
    if (len == 0.0) return gm_vec2_make(c.center.x + c.radius, c.center.y);
    return gm_vec2_add(c.center, gm_vec2_scale(d, c.radius / len));
}

gm_vec2 gm_shape_closest_point_aabb2_aabb2(gm_aabb2 a, gm_aabb2 b)
{
    gm_vec2 ca = gm_aabb2_center(a), cb = gm_aabb2_center(b);
    return gm_shape_closest_point_aabb2(b, ca.x == cb.x && ca.y == cb.y ? cb : ca);
}

int gm_shape_point_in_triangle(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_vec2 p)
{
    /* coordinate baricentriche (metodo dei segni d'area) */
    gm_real d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    gm_real d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    gm_real d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    int has_neg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
    int has_pos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
    return !(has_neg && has_pos);
}

int gm_shape_point_in_aabb2(gm_aabb2 box, gm_vec2 p)
{
    return p.x >= box.min.x && p.x <= box.max.x && p.y >= box.min.y && p.y <= box.max.y;
}

int gm_shape_circle_point(gm_circle c, gm_vec2 p)
{
    return gm_vec2_length_sq(gm_vec2_sub(p, c.center)) <= c.radius * c.radius;
}

int gm_shape_circle_circle(gm_circle a, gm_circle b)
{
    gm_real r = a.radius + b.radius;
    return gm_vec2_length_sq(gm_vec2_sub(a.center, b.center)) <= r * r;
}

int gm_shape_aabb2_point(gm_aabb2 b, gm_vec2 p)
{
    return gm_shape_point_in_aabb2(b, p);
}

int gm_shape_aabb2_aabb2(gm_aabb2 a, gm_aabb2 b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y;
}

int gm_shape_aabb2_circle(gm_aabb2 b, gm_circle c)
{
    gm_vec2 q = gm_shape_closest_point_aabb2(b, c.center);
    return gm_vec2_length_sq(gm_vec2_sub(q, c.center)) <= c.radius * c.radius;
}

int gm_shape_ray2_circle(gm_ray2 r, gm_circle c, gm_real *t)
{
    gm_vec2 oc = gm_vec2_sub(r.origin, c.center);
    gm_real a = gm_vec2_dot(r.dir, r.dir);
    gm_real b = 2.0 * gm_vec2_dot(oc, r.dir);
    gm_real cc = gm_vec2_dot(oc, oc) - c.radius * c.radius;
    gm_real disc, s, t0, t1;
    if (a == 0.0) return 0;
    disc = b * b - 4.0 * a * cc;
    if (disc < 0.0) return 0;
    s = sqrt(disc);
    t0 = (-b - s) / (2.0 * a);
    t1 = (-b + s) / (2.0 * a);
    if (t0 >= 0.0) { if (t) *t = t0; return 1; }
    if (t1 >= 0.0) { if (t) *t = t1; return 1; }
    return 0;
}

int gm_shape_ray2_aabb2(gm_ray2 r, gm_aabb2 b, gm_real *tmin, gm_real *tmax)
{
    /* slab method (branchless, da RLMath / Tavianator) */
    gm_real t1, t2, tminv = -1e300, tmaxv = 1e300;
    int i;
    for (i = 0; i < 2; i++) {
        gm_real o = (i == 0) ? r.origin.x : r.origin.y;
        gm_real d = (i == 0) ? r.dir.x : r.dir.y;
        gm_real mn = (i == 0) ? b.min.x : b.min.y;
        gm_real mx = (i == 0) ? b.max.x : b.max.y;
        if (d == 0.0) {
            if (o < mn || o > mx) return 0;
        } else {
            t1 = (mn - o) / d;
            t2 = (mx - o) / d;
            if (t1 > t2) { gm_real tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tminv) tminv = t1;
            if (t2 < tmaxv) tmaxv = t2;
            if (tminv > tmaxv) return 0;
        }
    }
    if (tmaxv < 0.0) return 0;
    if (tmin) *tmin = tminv;
    if (tmax) *tmax = tmaxv;
    return 1;
}

int gm_shape_ray2_segment(gm_ray2 r, gm_vec2 a, gm_vec2 b, gm_real *t)
{
    gm_vec2 e = gm_vec2_sub(b, a);
    gm_real denom = r.dir.x * e.y - r.dir.y * e.x;
    gm_vec2 diff;
    gm_real s, u;
    if (denom == 0.0) return 0;
    diff = gm_vec2_sub(a, r.origin);
    s = (diff.x * e.y - diff.y * e.x) / denom;
    u = (diff.x * r.dir.y - diff.y * r.dir.x) / denom;
    if (s >= 0.0 && u >= 0.0 && u <= 1.0) { if (t) *t = s; return 1; }
    return 0;
}

/* ============================== baricentriche (da bitmap-editor) ============================== */

int gm_shape_barycentric(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_vec2 p,
                         gm_real *wa, gm_real *wb, gm_real *wc)
{
    gm_real bx = b.x - a.x, by = b.y - a.y;
    gm_real cx = c.x - a.x, cy = c.y - a.y;
    gm_real px = p.x - a.x, py = p.y - a.y;
    gm_real d = bx * cy - cx * by;
    if (d == 0.0) return 0;
    if (wa) *wa = (px * (by - cy) + py * (cx - bx) + bx * cy - cx * by) / d;
    if (wb) *wb = (px * cy - py * cx) / d;
    if (wc) *wc = (py * bx - px * by) / d;
    return 1;
}

int gm_shape_point_in_oriented_rect(gm_vec2 center, gm_vec2 u, gm_vec2 v,
                                    gm_real half_u, gm_real half_v, gm_vec2 p)
{
    gm_real du = (p.x - center.x) * u.x + (p.y - center.y) * u.y;
    gm_real dv = (p.x - center.x) * v.x + (p.y - center.y) * v.y;
    return (du >= -half_u && du <= half_u && dv >= -half_v && dv <= half_v) ? 1 : 0;
}

int gm_shape_point_in_quad(gm_vec2 a, gm_vec2 b, gm_vec2 c, gm_vec2 d, gm_vec2 p)
{
    /* quadrilatero convesso: dentro se dai lati del quadrilatero i segni sono concordi */
    gm_real s1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    gm_real s2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
    gm_real s3 = (d.x - c.x) * (p.y - c.y) - (d.y - c.y) * (p.x - c.x);
    gm_real s4 = (a.x - d.x) * (p.y - d.y) - (a.y - d.y) * (p.x - d.x);
    int neg = (s1 < 0) || (s2 < 0) || (s3 < 0) || (s4 < 0);
    int pos = (s1 > 0) || (s2 > 0) || (s3 > 0) || (s4 > 0);
    return !(neg && pos);
}

/* ============================== esagono regolare (da c3dlas) ============================== */

gm_real gm_hex_area(gm_real side) { return 1.5 * 1.7320508075688772 * side * side; }
gm_real gm_hex_long_diagonal(gm_real side) { return 2.0 * side; }
gm_real gm_hex_short_diagonal(gm_real side) { return 1.7320508075688772 * side; }
gm_real gm_hex_inner_radius(gm_real side) { return 0.8660254037844386 * side; }

/* ===================== Poligoni 2D (da marine-resource) ===================== */

int gm_shape_point_in_polygon(gm_vec2 p, const gm_vec2 *poly, int n)
{
    int i, inside = 0;
    if (!poly || n < 3) return 0;
    for (i = 0; i < n; i++) {
        gm_vec2 a = poly[i], b = poly[(i + 1) % n];
        if (((a.y > p.y) != (b.y > p.y))) {
            gm_real xint = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
            if (p.x < xint) inside = !inside;
        }
    }
    return inside;
}

gm_real gm_shape_polygon_signed_area(const gm_vec2 *poly, int n)
{
    gm_real s = 0.0;
    int i;
    if (!poly || n < 3) return 0.0;
    for (i = 0; i < n; i++) {
        gm_vec2 a = poly[i], b = poly[(i + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5 * s;
}

gm_real gm_shape_polygon_area(const gm_vec2 *poly, int n)
{
    gm_real a = gm_shape_polygon_signed_area(poly, n);
    return a < 0.0 ? -a : a;
}

gm_vec2 gm_shape_polygon_centroid(const gm_vec2 *poly, int n)
{
    gm_vec2 c = {0.0, 0.0};
    gm_real a2 = 0.0;
    int i;
    if (!poly || n < 3) return c;
    for (i = 0; i < n; i++) {
        gm_vec2 p0 = poly[i], p1 = poly[(i + 1) % n];
        gm_real cr = p0.x * p1.y - p1.x * p0.y;
        a2 += cr;
        c.x += (p0.x + p1.x) * cr;
        c.y += (p0.y + p1.y) * cr;
    }
    if (a2 == 0.0) return c;
    c.x /= (3.0 * a2);
    c.y /= (3.0 * a2);
    return c;
}

/* ===================== Chamfer / Hausdorff (da GeoRT) ===================== */

static gm_real nn_d2(const gm_real *a, const gm_real *B, int m)
{
    gm_real best = 1e300;
    int j;
    for (j = 0; j < m; j++) {
        gm_real dx = a[0]-B[j*3+0], dy = a[1]-B[j*3+1], dz = a[2]-B[j*3+2];
        gm_real d = dx*dx + dy*dy + dz*dz;
        if (d < best) best = d;
    }
    return best;
}

gm_real gm_shape_chamfer(const gm_real *A, int n, const gm_real *B, int m)
{
    gm_real sa = 0.0, sb = 0.0;
    int i;
    if (!A || !B || n < 1 || m < 1) return 0.0;
    for (i = 0; i < n; i++) sa += nn_d2(A + i*3, B, m);
    for (i = 0; i < m; i++) sb += nn_d2(B + i*3, A, n);
    return sa / (gm_real)n + sb / (gm_real)m;
}

gm_real gm_shape_hausdorff(const gm_real *A, int n, const gm_real *B, int m)
{
    gm_real ma = 0.0, mb = 0.0;
    int i;
    if (!A || !B || n < 1 || m < 1) return 0.0;
    for (i = 0; i < n; i++) { gm_real d = nn_d2(A + i*3, B, m); if (d > ma) ma = d; }
    for (i = 0; i < m; i++) { gm_real d = nn_d2(B + i*3, A, n); if (d > mb) mb = d; }
    { gm_real d1 = sqrt(ma), d2 = sqrt(mb); return d1 > d2 ? d1 : d2; }
}
