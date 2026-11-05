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
