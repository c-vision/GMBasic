/*
 * ofx_gfx.c — shim FFI del dominio grafica (`libgmbgraphics`): colore e geometria 2D.
 *
 * L'API C di gmbgraphics usa `gm_real` (double) e struct by-value (`gm_vec2`, `gm_circle`,
 * `gm_aabb2`): qui tutto viaggia in scalari/array che l'FFI sa passare.
 *
 * Convenzioni:
 *  - colori in componenti 0..255 (out() = 0..255), H in [0,360), S/V/L in [0,1];
 *  - i poligoni sono array piatti x0,y0,x1,y1,…; `n` = NUMERO DI VALORI (pari),
 *    il numero di vertici è n/2;
 *  - i predicati ritornano 1 = dentro, 0 = fuori;
 *  - le funzioni con out() ritornano 0 = ok, -1 = argomenti non validi.
 */
#include "gm_color.h"
#include "gm_gfx_shape.h"

#include <math.h>
#include <stdlib.h>

static double c8(double v) { return v / 255.0; }

static double c255(double v)
{
    double x = v * 255.0;
    if (x < 0.0) return 0.0;
    if (x > 255.0) return 255.0;
    return floor(x + 0.5);
}

/* Converte l'array piatto in vertici gm_vec2. Caller: free. */
static gm_vec2* pts_from(const double* p, int nvalues, int* out_n)
{
    int n = (nvalues > 0) ? nvalues / 2 : 0;
    *out_n = n;
    if (n <= 0 || !p) return NULL;
    gm_vec2* v = (gm_vec2*)malloc(sizeof(gm_vec2) * (size_t)n);
    if (!v) { *out_n = 0; return NULL; }
    for (int i = 0; i < n; i++) { v[i].x = p[2 * i]; v[i].y = p[2 * i + 1]; }
    return v;
}

/* ── colore ─────────────────────────────────────────────────────────────── */

int ofx_gfx_rgb2hsv(double r, double g, double b, double* out)
{
    double h, s, v;
    if (!out) return -1;
    gm_color_rgb2hsv(c8(r), c8(g), c8(b), &h, &s, &v);
    out[0] = h; out[1] = s; out[2] = v;
    return 0;
}

int ofx_gfx_hsv2rgb(double h, double s, double v, double* out)
{
    double r, g, b;
    if (!out) return -1;
    gm_color_hsv2rgb(h, s, v, &r, &g, &b);
    out[0] = c255(r); out[1] = c255(g); out[2] = c255(b);
    return 0;
}

int ofx_gfx_rgb2hsl(double r, double g, double b, double* out)
{
    double h, s, l;
    if (!out) return -1;
    gm_color_rgb2hsl(c8(r), c8(g), c8(b), &h, &s, &l);
    out[0] = h; out[1] = s; out[2] = l;
    return 0;
}

int ofx_gfx_hsl2rgb(double h, double s, double l, double* out)
{
    double r, g, b;
    if (!out) return -1;
    gm_color_hsl2rgb(h, s, l, &r, &g, &b);
    out[0] = c255(r); out[1] = c255(g); out[2] = c255(b);
    return 0;
}

int ofx_gfx_rgb2cmyk(double r, double g, double b, double* out)
{
    double c, m, y, k;
    if (!out) return -1;
    gm_color_rgb2cmyk(c8(r), c8(g), c8(b), &c, &m, &y, &k);
    out[0] = c; out[1] = m; out[2] = y; out[3] = k;
    return 0;
}

int ofx_gfx_cmyk2rgb(double c, double m, double y, double k, double* out)
{
    double r, g, b;
    if (!out) return -1;
    gm_color_cmyk2rgb(c, m, y, k, &r, &g, &b);
    out[0] = c255(r); out[1] = c255(g); out[2] = c255(b);
    return 0;
}

int ofx_gfx_mix(double r1, double g1, double b1, double r2, double g2, double b2,
                double t, double* out)
{
    double r, g, b;
    if (!out) return -1;
    gm_color_mix(c8(r1), c8(g1), c8(b1), c8(r2), c8(g2), c8(b2), t, &r, &g, &b);
    out[0] = c255(r); out[1] = c255(g); out[2] = c255(b);
    return 0;
}

int ofx_gfx_lighten(double r, double g, double b, double amt, double* out)
{
    double ro, go, bo;
    if (!out) return -1;
    gm_color_lighten(c8(r), c8(g), c8(b), amt, &ro, &go, &bo);
    out[0] = c255(ro); out[1] = c255(go); out[2] = c255(bo);
    return 0;
}

int ofx_gfx_darken(double r, double g, double b, double amt, double* out)
{
    double ro, go, bo;
    if (!out) return -1;
    gm_color_darken(c8(r), c8(g), c8(b), amt, &ro, &go, &bo);
    out[0] = c255(ro); out[1] = c255(go); out[2] = c255(bo);
    return 0;
}

int ofx_gfx_saturate(double r, double g, double b, double amt, double* out)
{
    double ro, go, bo;
    if (!out) return -1;
    gm_color_saturate(c8(r), c8(g), c8(b), amt, &ro, &go, &bo);
    out[0] = c255(ro); out[1] = c255(go); out[2] = c255(bo);
    return 0;
}

/* ── geometria 2D / hit-test ────────────────────────────────────────────── */

int ofx_gfx_point_in_polygon(double x, double y, const double* pts, int nvalues)
{
    int n = 0;
    gm_vec2* v = pts_from(pts, nvalues, &n);
    if (!v || n < 3) { free(v); return 0; }
    gm_vec2 p = { x, y };
    int inside = gm_shape_point_in_polygon(p, v, n);
    free(v);
    return inside;
}

int ofx_gfx_point_in_circle(double x, double y, double cx, double cy, double r)
{
    gm_vec2 c = { cx, cy };
    gm_circle cir = gm_circle_make(c, r);
    gm_vec2 p = { x, y };
    return gm_shape_circle_point(cir, p);
}

int ofx_gfx_point_in_rect(double x, double y, double x1, double y1, double x2, double y2)
{
    double ax = x1, bx = x2, ay = y1, by = y2;
    if (ax > bx) { double t = ax; ax = bx; bx = t; }
    if (ay > by) { double t = ay; ay = by; by = t; }
    gm_vec2 lo = { ax, ay }, hi = { bx, by };
    gm_aabb2 box = gm_aabb2_make(lo, hi);
    gm_vec2 p = { x, y };
    return gm_shape_point_in_aabb2(box, p);
}

double ofx_gfx_polygon_area(const double* pts, int nvalues)
{
    int n = 0;
    gm_vec2* v = pts_from(pts, nvalues, &n);
    if (!v || n < 3) { free(v); return 0.0; }
    double a = gm_shape_polygon_area(v, n);
    free(v);
    return a;
}

int ofx_gfx_polygon_centroid(const double* pts, int nvalues, double* out)
{
    int n = 0;
    if (!out) return -1;
    gm_vec2* v = pts_from(pts, nvalues, &n);
    if (!v || n < 3) { free(v); return -1; }
    gm_vec2 c = gm_shape_polygon_centroid(v, n);
    free(v);
    out[0] = c.x; out[1] = c.y;
    return 0;
}

int ofx_gfx_closest_point_segment(double px, double py, double ax, double ay,
                                  double bx, double by, double* out)
{
    if (!out) return -1;
    gm_vec2 p = { px, py }, a = { ax, ay }, b = { bx, by };
    gm_vec2 c = gm_shape_closest_point_segment(p, a, b);
    out[0] = c.x; out[1] = c.y;
    return 0;
}

int ofx_gfx_point_in_triangle(double x, double y, double ax, double ay,
                              double bx, double by, double cx, double cy)
{
    gm_vec2 p = { x, y }, a = { ax, ay }, b = { bx, by }, c = { cx, cy };
    return gm_shape_point_in_triangle(a, b, c, p);
}

/* ── immagini float 2D (gfx2d.inc) ─────────────────────────────────────────
 * L'API gmbgraphics passa `gm_image` per valore: qui si avvolgono i buffer del chiamante
 * (row-major, w*h double) e si copiano i risultati negli array out(). Nessuna allocazione
 * nascosta: l'unica alloca sono i temporanei delle funzioni che ritornano gm_image. */

#include "gm_gfx_image.h"

static gm_image img_wrap(const double* data, int w, int h)
{
    gm_image im;
    im.w = w;
    im.h = h;
    im.data = (gm_real*)data;   /* solo per operazioni in-place */
    return im;
}

int ofx_gfx_img_convolve(const double* in, int w, int h, const double* k, int kx, int ky, double* out)
{
    if (!in || !k || !out || w <= 0 || h <= 0 || kx <= 0 || ky <= 0) return -1;
    gm_image a = img_wrap(in, w, h), b = img_wrap(out, w, h);
    gm_image_convolve(a, k, kx, ky, b);
    return 0;
}

int ofx_gfx_img_convolve_sep(const double* in, int w, int h, const double* kx, int nx,
                             const double* ky, int ny, double* out)
{
    if (!in || !kx || !ky || !out || w <= 0 || h <= 0 || nx <= 0 || ny <= 0) return -1;
    gm_image a = img_wrap(in, w, h), b = img_wrap(out, w, h);
    gm_image_convolve_sep(a, kx, nx, ky, ny, b);
    return 0;
}

int ofx_gfx_img_gaussian(const double* in, int w, int h, double sigma, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    gm_image a = img_wrap(in, w, h), b = img_wrap(out, w, h);
    gm_image_gaussian(a, sigma, b);
    return 0;
}

int ofx_gfx_img_sobel(const double* in, int w, int h, double* gx, double* gy)
{
    if (!in || !gx || !gy || w <= 0 || h <= 0) return -1;
    gm_image a = img_wrap(in, w, h), b = img_wrap(gx, w, h), c = img_wrap(gy, w, h);
    gm_image_sobel(a, b, c);
    return 0;
}

int ofx_gfx_img_resample(const double* in, int w, int h, int nw, int nh, double* out)
{
    if (!in || !out || w <= 0 || h <= 0 || nw <= 0 || nh <= 0) return -1;
    gm_image r = gm_image_resample(img_wrap(in, w, h), nw, nh);
    if (!r.data) return -1;
    for (long i = 0; i < (long)nw * nh; i++) out[i] = r.data[i];
    gm_image_free(&r);
    return 0;
}

int ofx_gfx_img_transpose(const double* in, int w, int h, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    gm_image r = gm_image_transpose(img_wrap(in, w, h));
    if (!r.data) return -1;
    for (long i = 0; i < (long)w * h; i++) out[i] = r.data[i];
    gm_image_free(&r);
    return 0;
}

int ofx_gfx_img_rotate90(const double* in, int w, int h, int k, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    gm_image r = gm_image_rotate90(img_wrap(in, w, h), k);
    if (!r.data) return -1;
    for (long i = 0; i < (long)w * h; i++) out[i] = r.data[i];
    gm_image_free(&r);
    return 0;
}

int ofx_gfx_img_rotate(const double* in, int w, int h, double radians, double bg, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    gm_image r = gm_image_rotate(img_wrap(in, w, h), radians, bg);
    if (!r.data) return -1;
    for (long i = 0; i < (long)w * h; i++) out[i] = r.data[i];
    gm_image_free(&r);
    return 0;
}

int ofx_gfx_img_flip_h(const double* in, int w, int h, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    return gm_image_flip_h(img_wrap(in, w, h), img_wrap(out, w, h));
}

int ofx_gfx_img_flip_v(const double* in, int w, int h, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    return gm_image_flip_v(img_wrap(in, w, h), img_wrap(out, w, h));
}

int ofx_gfx_img_threshold(const double* in, int w, int h, double t, double lo, double hi, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    return gm_image_threshold(img_wrap(in, w, h), t, lo, hi, img_wrap(out, w, h));
}

int ofx_gfx_img_normalize(const double* in, int w, int h, double lo, double hi, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    return gm_image_normalize(img_wrap(in, w, h), lo, hi, img_wrap(out, w, h));
}

int ofx_gfx_img_histogram(const double* in, int w, int h, int nbins, double lo, double hi, double* counts)
{
    if (!in || !counts || w <= 0 || h <= 0 || nbins <= 0) return -1;
    return gm_image_histogram(img_wrap(in, w, h), nbins, lo, hi, counts);
}

int ofx_gfx_img_hist_equalize(const double* in, int w, int h, int nbins, double* out)
{
    if (!in || !out || w <= 0 || h <= 0 || nbins <= 0) return -1;
    return gm_image_hist_equalize(img_wrap(in, w, h), nbins, img_wrap(out, w, h));
}

int ofx_gfx_img_median3(const double* in, int w, int h, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    return gm_image_median3(img_wrap(in, w, h), img_wrap(out, w, h));
}

int ofx_gfx_img_draw_line(double* d, int w, int h, int x0, int y0, int x1, int y1, double v)
{
    if (!d || w <= 0 || h <= 0) return -1;
    gm_image_draw_line(img_wrap(d, w, h), x0, y0, x1, y1, v);
    return 0;
}

int ofx_gfx_img_draw_rect_fill(double* d, int w, int h, int x0, int y0, int x1, int y1, double v)
{
    if (!d || w <= 0 || h <= 0) return -1;
    gm_image_draw_rect(img_wrap(d, w, h), x0, y0, x1, y1, v, 1);
    return 0;
}

int ofx_gfx_img_draw_rect_stroke(double* d, int w, int h, int x0, int y0, int x1, int y1, double v)
{
    if (!d || w <= 0 || h <= 0) return -1;
    gm_image_draw_rect(img_wrap(d, w, h), x0, y0, x1, y1, v, 0);
    return 0;
}

int ofx_gfx_img_draw_circle(double* d, int w, int h, int cx, int cy, int r, double v, int fill)
{
    if (!d || w <= 0 || h <= 0) return -1;
    gm_image_draw_circle(img_wrap(d, w, h), cx, cy, r, v, fill);
    return 0;
}

int ofx_gfx_img_warp(const double* in, int c, int h, int w, const double* fx, const double* fy, double* out)
{
    if (!in || !fx || !fy || !out || c <= 0 || h <= 0 || w <= 0) return -1;
    return gm_image_warp_bilinear(out, in, c, h, w, fx, fy);
}

int ofx_gfx_img_poisson(const double* target, const double* source, const double* mask,
                        int nx, int ny, double* out, int maxit)
{
    if (!target || !source || !mask || !out || nx <= 0 || ny <= 0) return -1;
    /* mask BASIC = double 0/1; il blend vuole unsigned char: conversione esplicita. */
    size_t n = (size_t)nx * (size_t)ny;
    unsigned char* m = (unsigned char*)malloc(n);
    if (!m) return -1;
    for (size_t i = 0; i < n; i++) m[i] = (mask[i] != 0.0) ? 1u : 0u;
    int rc = gm_image_poisson_blend(target, source, m, nx, ny, out, maxit);
    free(m);
    return rc;
}

/* ── geometria 2D completa (gfxshape.inc) ─────────────────────────────────── */

static gm_aabb2 box2_in(const double* b)
{
    gm_aabb2 a;
    a.min.x = b[0]; a.min.y = b[1];
    a.max.x = b[2]; a.max.y = b[3];
    return a;
}
static void box2_out(gm_aabb2 a, double* out)
{
    out[0] = a.min.x; out[1] = a.min.y; out[2] = a.max.x; out[3] = a.max.y;
}
static gm_vec2 v2(const double* p) { gm_vec2 v; v.x = p[0]; v.y = p[1]; return v; }
static void v2_out(gm_vec2 v, double* out) { out[0] = v.x; out[1] = v.y; }
static gm_ray2 ray2_in(const double* r)
{
    gm_ray2 x;
    x.origin.x = r[0]; x.origin.y = r[1];
    x.dir.x = r[2]; x.dir.y = r[3];
    return x;
}

double ofx_gfx_shape_dist(const double* a, const double* b)
{
    if (!a || !b) return 0.0;
    return gm_shape_distance(v2(a), v2(b));
}

int ofx_gfx_shape_aabb_from_points(const double* pts, int nvalues, double* out)
{
    if (!pts || !out || nvalues < 2) return -1;
    int n = nvalues / 2;
    gm_vec2* v = (gm_vec2*)malloc(sizeof(gm_vec2) * (size_t)n);
    if (!v) return -1;
    for (int i = 0; i < n; i++) v[i] = v2(pts + 2 * i);
    gm_aabb2 b = gm_aabb2_from_points(v, n);
    free(v);
    box2_out(b, out);
    return 0;
}

int ofx_gfx_shape_aabb_merge(const double* a, const double* b, double* out)
{ if (!a || !b || !out) return -1; box2_out(gm_aabb2_merge(box2_in(a), box2_in(b)), out); return 0; }
int ofx_gfx_shape_aabb_center(const double* a, double* out)
{ if (!a || !out) return -1; v2_out(gm_aabb2_center(box2_in(a)), out); return 0; }
int ofx_gfx_shape_aabb_extents(const double* a, double* out)
{ if (!a || !out) return -1; v2_out(gm_aabb2_extents(box2_in(a)), out); return 0; }
int ofx_gfx_shape_aabb_valid(const double* a)
{ if (!a) return 0; return gm_aabb2_valid(box2_in(a)); }

int ofx_gfx_shape_closest_ray(const double* ray, const double* p, double* out)
{ if (!ray || !p || !out) return -1; v2_out(gm_shape_closest_point_ray(ray2_in(ray), v2(p)), out); return 0; }
int ofx_gfx_shape_closest_aabb(const double* box, const double* p, double* out)
{ if (!box || !p || !out) return -1; v2_out(gm_shape_closest_point_aabb2(box2_in(box), v2(p)), out); return 0; }
int ofx_gfx_shape_closest_circle(double cx, double cy, double r, const double* p, double* out)
{
    if (!p || !out) return -1;
    gm_vec2 c = { cx, cy };
    gm_circle cir = gm_circle_make(c, r);
    v2_out(gm_shape_closest_point_circle(cir, v2(p)), out);
    return 0;
}
int ofx_gfx_shape_closest_aabb_aabb(const double* a, const double* b, double* out)
{ if (!a || !b || !out) return -1; v2_out(gm_shape_closest_point_aabb2_aabb2(box2_in(a), box2_in(b)), out); return 0; }

int ofx_gfx_shape_circle_circle(const double* c1, double r1, const double* c2, double r2)
{
    if (!c1 || !c2) return 0;
    gm_vec2 a = { c1[0], c1[1] }, b = { c2[0], c2[1] };
    return gm_shape_circle_circle(gm_circle_make(a, r1), gm_circle_make(b, r2));
}
int ofx_gfx_shape_aabb_aabb(const double* a, const double* b)
{ if (!a || !b) return 0; return gm_shape_aabb2_aabb2(box2_in(a), box2_in(b)); }
int ofx_gfx_shape_aabb_circle(const double* a, double cx, double cy, double r)
{
    if (!a) return 0;
    gm_vec2 c = { cx, cy };
    return gm_shape_aabb2_circle(box2_in(a), gm_circle_make(c, r));
}

int ofx_gfx_shape_ray2_circle(const double* ray, double cx, double cy, double r, double* t)
{
    if (!ray || !t) return -1;
    gm_vec2 c = { cx, cy };
    gm_circle cir = gm_circle_make(c, r);
    gm_real tt = 0;
    int hit = gm_shape_ray2_circle(ray2_in(ray), cir, &tt);
    t[0] = tt;
    return hit;
}
int ofx_gfx_shape_ray2_aabb(const double* ray, const double* box, double* t)
{
    if (!ray || !box || !t) return -1;
    gm_real tmin = 0, tmax = 0;
    int hit = gm_shape_ray2_aabb2(ray2_in(ray), box2_in(box), &tmin, &tmax);
    t[0] = tmin; t[1] = tmax;
    return hit;
}
int ofx_gfx_shape_ray2_segment(const double* ray, const double* a, const double* b, double* t)
{
    if (!ray || !a || !b || !t) return -1;
    gm_real tt = 0;
    int hit = gm_shape_ray2_segment(ray2_in(ray), v2(a), v2(b), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_shape_barycentric(const double* a, const double* b, const double* c,
                              const double* p, double* out)
{
    if (!a || !b || !c || !p || !out) return -1;
    gm_real wa = 0, wb = 0, wc = 0;
    int rc = gm_shape_barycentric(v2(a), v2(b), v2(c), v2(p), &wa, &wb, &wc);
    out[0] = wa; out[1] = wb; out[2] = wc;
    return rc;
}

int ofx_gfx_shape_point_in_oriented_rect(const double* center, const double* u, const double* v,
                                         double half_u, double half_v, const double* p)
{
    if (!center || !u || !v || !p) return 0;
    return gm_shape_point_in_oriented_rect(v2(center), v2(u), v2(v), half_u, half_v, v2(p));
}

int ofx_gfx_shape_point_in_quad(const double* a, const double* b, const double* c,
                                const double* d, const double* p)
{
    if (!a || !b || !c || !d || !p) return 0;
    return gm_shape_point_in_quad(v2(a), v2(b), v2(c), v2(d), v2(p));
}

double ofx_gfx_shape_hex_area(double side) { return gm_hex_area(side); }
double ofx_gfx_shape_hex_long(double side) { return gm_hex_long_diagonal(side); }
double ofx_gfx_shape_hex_short(double side) { return gm_hex_short_diagonal(side); }
double ofx_gfx_shape_hex_inner(double side) { return gm_hex_inner_radius(side); }

double ofx_gfx_shape_polygon_signed_area(const double* pts, int nvalues)
{
    int n = 0;
    gm_vec2* v = pts_from(pts, nvalues, &n);
    if (!v || n < 3) { free(v); return 0.0; }
    double a = gm_shape_polygon_signed_area(v, n);
    free(v);
    return a;
}

double ofx_gfx_shape_chamfer(const double* A, int nvalues_a, const double* B, int nvalues_b)
{
    if (!A || !B || nvalues_a < 3 || nvalues_b < 3) return 0.0;
    return gm_shape_chamfer(A, nvalues_a / 3, B, nvalues_b / 3);
}
double ofx_gfx_shape_hausdorff(const double* A, int nvalues_a, const double* B, int nvalues_b)
{
    if (!A || !B || nvalues_a < 3 || nvalues_b < 3) return 0.0;
    return gm_shape_hausdorff(A, nvalues_a / 3, B, nvalues_b / 3);
}

/* ── superficie: colormap e heatmap (gfxcolor.inc / gfx2d.inc) ───────────── */

#include "gm_gfx_bitmap.h"
#include "gm_gfx_surface.h"

int ofx_gfx_colormap(double t, double* out)
{
    unsigned char r = 0, g = 0, b = 0;
    if (!out) return -1;
    gm_gfx_colormap(t, &r, &g, &b);
    out[0] = r; out[1] = g; out[2] = b;
    return 0;
}

/* Heatmap: z[nx*ny] -> out() = nx*ny colori impacchettati (0x40000000|RGB), pronti per
 * CANVAS SET (stesso formato di GRAPHICS.RGB dopo il packing). */
int ofx_gfx_heatmap(const double* z, int nx, int ny, double zmin, double zmax, double* out)
{
    if (!z || !out || nx <= 0 || ny <= 0) return -1;
    /* gm_gfx_heatmap libera out->data se non-NULL: il bitmap va azzerato (stack garbage
     * causava free() su un puntatore casuale -> SIGABRT). */
    gm_bitmap bm = { 0, 0, NULL };
    if (gm_gfx_heatmap(z, nx, ny, zmin, zmax, &bm) != 0) return -1;
    for (int i = 0; i < bm.w * bm.h; i++)
    {
        unsigned char r = bm.data[3 * i + 0];
        unsigned char g = bm.data[3 * i + 1];
        unsigned char b = bm.data[3 * i + 2];
        out[i] = (double)(0x40000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
    }
    gm_bitmap_free(&bm);
    return 0;
}

/* Volume 3D: convoluzione separabile con lo stesso kernel 1D sui tre assi. */
#include "gm_gfx_image.h"
int ofx_gfx_img3_convolve_sep(const double* in, int w, int h, int d,
                              const double* k, int n, double* out)
{
    if (!in || !k || !out || w <= 0 || h <= 0 || d <= 0 || n <= 0) return -1;
    gm_image3 a, b;
    a.w = w; a.h = h; a.d = d; a.data = (gm_real*)in;
    b.w = w; b.h = h; b.d = d; b.data = (gm_real*)out;
    gm_image3_convolve_sep(a, k, n, b);
    return 0;
}
