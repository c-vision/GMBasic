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
