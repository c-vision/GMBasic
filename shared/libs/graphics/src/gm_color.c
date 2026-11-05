/*
 * gm_color.c — conversioni RGB/HSV/HSL (da nurnn).
 */
#include "gm_color.h"
#include <math.h>

void gm_color_rgb2hsv(gm_real r, gm_real g, gm_real b,
                      gm_real *h, gm_real *s, gm_real *v)
{
    gm_real mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    gm_real mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    gm_real d = mx - mn;
    if (v) *v = mx;
    if (s) *s = (mx <= 0.0) ? 0.0 : d / mx;
    if (h) {
        gm_real hh = 0.0;
        if (d > 1e-12) {
            if (mx == r) hh = 60.0 * fmod(((g - b) / d), 6.0);
            else if (mx == g) hh = 60.0 * (((b - r) / d) + 2.0);
            else hh = 60.0 * (((r - g) / d) + 4.0);
        }
        if (hh < 0.0) hh += 360.0;
        *h = hh;
    }
}

void gm_color_hsv2rgb(gm_real h, gm_real s, gm_real v,
                      gm_real *r, gm_real *g, gm_real *b)
{
    gm_real c, x, m, hh;
    int sec;
    if (s < 0) s = 0; if (s > 1) s = 1;
    if (v < 0) v = 0;
    c = v * s;
    hh = h / 60.0;
    hh = hh - 6.0 * floor(hh / 6.0);
    sec = (int)floor(hh);
    x = c * (1.0 - fabs(fmod(hh, 2.0) - 1.0));
    switch (sec) {
        case 0: if (r) *r = c; if (g) *g = x; if (b) *b = 0; break;
        case 1: if (r) *r = x; if (g) *g = c; if (b) *b = 0; break;
        case 2: if (r) *r = 0; if (g) *g = c; if (b) *b = x; break;
        case 3: if (r) *r = 0; if (g) *g = x; if (b) *b = c; break;
        case 4: if (r) *r = x; if (g) *g = 0; if (b) *b = c; break;
        default: if (r) *r = c; if (g) *g = 0; if (b) *b = x; break;
    }
    m = v - c;
    if (r) *r += m; if (g) *g += m; if (b) *b += m;
}

void gm_color_rgb2hsl(gm_real r, gm_real g, gm_real b,
                      gm_real *h, gm_real *s, gm_real *l)
{
    gm_real mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    gm_real mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    gm_real d = mx - mn, L = 0.5 * (mx + mn);
    if (l) *l = L;
    if (s) *s = (d <= 1e-12) ? 0.0 : d / (1.0 - fabs(2.0 * L - 1.0));
    if (h) {
        gm_real hh = 0.0;
        if (d > 1e-12) {
            if (mx == r) hh = 60.0 * fmod(((g - b) / d), 6.0);
            else if (mx == g) hh = 60.0 * (((b - r) / d) + 2.0);
            else hh = 60.0 * (((r - g) / d) + 4.0);
        }
        if (hh < 0.0) hh += 360.0;
        *h = hh;
    }
}

void gm_color_hsl2rgb(gm_real h, gm_real s, gm_real l,
                      gm_real *r, gm_real *g, gm_real *b)
{
    gm_real c, x, m, hh;
    int sec;
    if (s < 0) s = 0; if (s > 1) s = 1;
    c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    hh = h / 60.0;
    hh = hh - 6.0 * floor(hh / 6.0);
    sec = (int)floor(hh);
    x = c * (1.0 - fabs(fmod(hh, 2.0) - 1.0));
    switch (sec) {
        case 0: if (r) *r = c; if (g) *g = x; if (b) *b = 0; break;
        case 1: if (r) *r = x; if (g) *g = c; if (b) *b = 0; break;
        case 2: if (r) *r = 0; if (g) *g = c; if (b) *b = x; break;
        case 3: if (r) *r = 0; if (g) *g = x; if (b) *b = c; break;
        case 4: if (r) *r = x; if (g) *g = 0; if (b) *b = c; break;
        default: if (r) *r = c; if (g) *g = 0; if (b) *b = x; break;
    }
    m = l - 0.5 * c;
    if (r) *r += m; if (g) *g += m; if (b) *b += m;
}

void gm_color_rgb2cmyk(gm_real r, gm_real g, gm_real b,
                       gm_real *c, gm_real *m, gm_real *y, gm_real *k)
{
    gm_real mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    gm_real kk = 1.0 - mx;
    if (kk >= 1.0) { if (c) *c = 0; if (m) *m = 0; if (y) *y = 0; if (k) *k = 1; return; }
    if (c) *c = (1.0 - r - kk) / (1.0 - kk);
    if (m) *m = (1.0 - g - kk) / (1.0 - kk);
    if (y) *y = (1.0 - b - kk) / (1.0 - kk);
    if (k) *k = kk;
}

void gm_color_cmyk2rgb(gm_real c, gm_real m, gm_real y, gm_real k,
                       gm_real *r, gm_real *g, gm_real *b)
{
    if (r) *r = (1.0 - c) * (1.0 - k);
    if (g) *g = (1.0 - m) * (1.0 - k);
    if (b) *b = (1.0 - y) * (1.0 - k);
}

void gm_color_mix(gm_real r1, gm_real g1, gm_real b1,
                  gm_real r2, gm_real g2, gm_real b2, gm_real t,
                  gm_real *r, gm_real *g, gm_real *b)
{
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    if (r) *r = r1 + t * (r2 - r1);
    if (g) *g = g1 + t * (g2 - g1);
    if (b) *b = b1 + t * (b2 - b1);
}

void gm_color_lighten(gm_real r, gm_real g, gm_real b, gm_real amt,
                      gm_real *ro, gm_real *go, gm_real *bo)
{
    gm_real h, s, l, nl;
    gm_color_rgb2hsl(r, g, b, &h, &s, &l);
    nl = l + amt;
    if (nl < 0.0) nl = 0.0;
    if (nl > 1.0) nl = 1.0;
    gm_color_hsl2rgb(h, s, nl, ro, go, bo);
}

void gm_color_darken(gm_real r, gm_real g, gm_real b, gm_real amt,
                     gm_real *ro, gm_real *go, gm_real *bo)
{
    gm_color_lighten(r, g, b, -amt, ro, go, bo);
}

void gm_color_saturate(gm_real r, gm_real g, gm_real b, gm_real amt,
                       gm_real *ro, gm_real *go, gm_real *bo)
{
    gm_real h, s, l, ns;
    gm_color_rgb2hsl(r, g, b, &h, &s, &l);
    ns = s + amt;
    if (ns < 0.0) ns = 0.0;
    if (ns > 1.0) ns = 1.0;
    gm_color_hsl2rgb(h, ns, l, ro, go, bo);
}
