/* gm_gfx_raster.c — rasterizzazione software (da SoftwareRenderer). */
#include "gm_gfx_raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gm_status gm_raster_alloc(gm_raster *r, int w, int h)
{
    if (r == NULL || w <= 0 || h <= 0) return GM_ERR_ARG;
    r->w = w; r->h = h;
    r->color = (gm_real *)malloc((size_t)w * (size_t)h * 3 * sizeof(gm_real));
    r->depth = (gm_real *)malloc((size_t)w * (size_t)h * sizeof(gm_real));
    if (!r->color || !r->depth) { gm_raster_free(r); return GM_ERR_NO_MEM; }
    return GM_OK;
}

void gm_raster_free(gm_raster *r)
{
    if (r == NULL) return;
    free(r->color); free(r->depth);
    r->color = NULL; r->depth = NULL; r->w = r->h = 0;
}

void gm_raster_clear(gm_raster *r, gm_real depth)
{
    size_t n, i;
    if (r == NULL || r->color == NULL) return;
    n = (size_t)r->w * (size_t)r->h;
    for (i = 0; i < n; i++) {
        r->depth[i] = depth;
        r->color[i * 3] = 0.0; r->color[i * 3 + 1] = 0.0; r->color[i * 3 + 2] = 0.0;
    }
}

void gm_raster_get(const gm_raster *r, int x, int y, gm_real *rgb)
{
    if (r == NULL || r->color == NULL || x < 0 || y < 0 || x >= r->w || y >= r->h) {
        if (rgb) { rgb[0] = rgb[1] = rgb[2] = 0.0; }
        return;
    }
    {
        size_t i = ((size_t)y * (size_t)r->w + (size_t)x) * 3;
        if (rgb) { rgb[0] = r->color[i]; rgb[1] = r->color[i + 1]; rgb[2] = r->color[i + 2]; }
    }
}

/* edge function (area orientata) */
static gm_real edge(gm_real ax, gm_real ay, gm_real bx, gm_real by, gm_real px, gm_real py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void gm_raster_triangle(gm_raster *r,
                        const gm_real *p0, const gm_real *p1, const gm_real *p2,
                        const gm_real *c0, const gm_real *c1, const gm_real *c2)
{
    int minx, maxx, miny, maxy, x, y, k;
    gm_real area;
    if (r == NULL || r->color == NULL || p0 == NULL || p1 == NULL || p2 == NULL) return;
    area = edge(p0[0], p0[1], p1[0], p1[1], p2[0], p2[1]);
    if (area == 0.0) return;
    minx = (int)floor(fmin(fmin(p0[0], p1[0]), p2[0])); maxx = (int)ceil(fmax(fmax(p0[0], p1[0]), p2[0]));
    miny = (int)floor(fmin(fmin(p0[1], p1[1]), p2[1])); maxy = (int)ceil(fmax(fmax(p0[1], p1[1]), p2[1]));
    if (minx < 0) minx = 0; if (miny < 0) miny = 0;
    if (maxx >= r->w) maxx = r->w - 1;
    if (maxy >= r->h) maxy = r->h - 1;
    for (y = miny; y <= maxy; y++) {
        for (x = minx; x <= maxx; x++) {
            gm_real px = (gm_real)x + 0.5, py = (gm_real)y + 0.5;
            gm_real w0 = edge(p1[0], p1[1], p2[0], p2[1], px, py) / area;
            gm_real w1 = edge(p2[0], p2[1], p0[0], p0[1], px, py) / area;
            gm_real w2 = edge(p0[0], p0[1], p1[0], p1[1], px, py) / area;
            size_t idx;
            if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0) continue;
            idx = (size_t)y * (size_t)r->w + (size_t)x;
            {
                gm_real z = w0 * p0[2] + w1 * p1[2] + w2 * p2[2];
                if (z < r->depth[idx]) {
                    r->depth[idx] = z;
                    for (k = 0; k < 3; k++)
                        r->color[idx * 3 + (size_t)k] = w0 * c0[k] + w1 * c1[k] + w2 * c2[k];
                }
            }
        }
    }
}

int gm_raster_clip_near(const gm_real *tri, gm_real znear, gm_real *out)
{
    /* clipa il triangolo (3 vertici xyz) contro il piano z >= znear (Sutherland-Hodgman) */
    gm_real poly[8][3];
    int np = 0, i;
    if (tri == NULL || out == NULL) return 0;
    for (i = 0; i < 3; i++) { poly[np][0] = tri[i * 3]; poly[np][1] = tri[i * 3 + 1]; poly[np][2] = tri[i * 3 + 2]; np++; }
    {
        gm_real op[8][3];
        int on = 0;
        for (i = 0; i < np; i++) {
            gm_real cx = poly[i][0], cy = poly[i][1], cz = poly[i][2];
            gm_real nx = poly[(i + 1) % np][0], ny = poly[(i + 1) % np][1], nz = poly[(i + 1) % np][2];
            int cin = cz >= znear, nin = nz >= znear;
            if (cin) { op[on][0] = cx; op[on][1] = cy; op[on][2] = cz; on++; }
            if (cin != nin) {
                gm_real t = (znear - cz) / (nz - cz);
                op[on][0] = cx + t * (nx - cx);
                op[on][1] = cy + t * (ny - cy);
                op[on][2] = znear;
                on++;
            }
        }
        for (i = 0; i < on; i++) { out[i * 3] = op[i][0]; out[i * 3 + 1] = op[i][1]; out[i * 3 + 2] = op[i][2]; }
        return on;
    }
}

/* ═══ Texture + triangoli texturizzati (P0 del piano 3D, additive) ═════════════
 * Tutto quanto sta qui sopra resta invariato: gm_raster_triangle è l'oracolo
 * dei MiniTest e non va toccato. */

gm_status gm_tex_alloc(gm_tex *t, int w, int h)
{
    if (t == NULL || w <= 0 || h <= 0) return GM_ERR_ARG;
    t->w = w; t->h = h;
    t->rgba = (unsigned char *)malloc((size_t)w * (size_t)h * 4u);
    if (t->rgba == NULL) { t->w = t->h = 0; return GM_ERR_NO_MEM; }
    memset(t->rgba, 0, (size_t)w * (size_t)h * 4u);
    return GM_OK;
}

void gm_tex_free(gm_tex *t)
{
    if (t == NULL) return;
    free(t->rgba);
    t->rgba = NULL; t->w = t->h = 0;
}

void gm_tex_set(gm_tex *t, int x, int y,
                unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    size_t i;
    if (t == NULL || t->rgba == NULL || x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    i = ((size_t)y * (size_t)t->w + (size_t)x) * 4u;
    t->rgba[i] = r; t->rgba[i + 1] = g; t->rgba[i + 2] = b; t->rgba[i + 3] = a;
}

/* wrap repeat: fmod negativo corretto, poi clamp dell'indice (u=1.0 esatta). */
static int tex_index(gm_real c, int n)
{
    gm_real f = c - floor(c);
    int i = (int)(f * (gm_real)n);
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return i;
}

void gm_tex_sample(const gm_tex *t, gm_real u, gm_real v, int bilinear,
                   gm_real out_rgb[3], gm_real *out_a)
{
    gm_real rgb[3];
    gm_real a = 1.0;
    if (out_rgb == NULL) return;
    rgb[0] = rgb[1] = rgb[2] = 0.0;
    if (t == NULL || t->rgba == NULL || t->w <= 0 || t->h <= 0) {
        out_rgb[0] = out_rgb[1] = out_rgb[2] = 0.0;
        if (out_a) *out_a = 0.0;
        return;
    }
    if (!bilinear) {
        int x = tex_index(u, t->w), y = tex_index(v, t->h);
        const unsigned char *p = t->rgba + ((size_t)y * (size_t)t->w + (size_t)x) * 4u;
        rgb[0] = p[0] / 255.0; rgb[1] = p[1] / 255.0; rgb[2] = p[2] / 255.0; a = p[3] / 255.0;
    } else {
        /* posizione in texel e parte frazionaria: i pesi sono quelli della
         * distanza dal centro del texel, non fx/fy grezzi. */
        gm_real tx = (u - floor(u)) * (gm_real)t->w;
        gm_real ty = (v - floor(v)) * (gm_real)t->h;
        int x0 = (int)tx, y0 = (int)ty, x1, y1, k;
        if (x0 >= t->w) { x0 = t->w - 1; tx = (gm_real)t->w; }
        if (y0 >= t->h) { y0 = t->h - 1; ty = (gm_real)t->h; }
        tx -= (gm_real)x0;
        ty -= (gm_real)y0;
        x1 = (x0 + 1) % t->w;
        y1 = (y0 + 1) % t->h;
        for (k = 0; k < 4; k++) {
            const unsigned char *p00 = t->rgba + ((size_t)y0 * (size_t)t->w + (size_t)x0) * 4u + (size_t)k;
            const unsigned char *p10 = t->rgba + ((size_t)y0 * (size_t)t->w + (size_t)x1) * 4u + (size_t)k;
            const unsigned char *p01 = t->rgba + ((size_t)y1 * (size_t)t->w + (size_t)x0) * 4u + (size_t)k;
            const unsigned char *p11 = t->rgba + ((size_t)y1 * (size_t)t->w + (size_t)x1) * 4u + (size_t)k;
            gm_real top = (gm_real)(*p00) * (1.0 - tx) + (gm_real)(*p10) * tx;
            gm_real bot = (gm_real)(*p01) * (1.0 - tx) + (gm_real)(*p11) * tx;
            gm_real val = (top * (1.0 - ty) + bot * ty) / 255.0;
            if (k < 3) rgb[k] = val; else a = val;
        }
    }
    out_rgb[0] = rgb[0]; out_rgb[1] = rgb[1]; out_rgb[2] = rgb[2];
    if (out_a) *out_a = a;
}

void gm_raster_triangle_tex(gm_raster *r, const gm_tex *t,
                            const gm_real *p0, const gm_real *p1, const gm_real *p2,
                            const gm_real *uv0, const gm_real *uv1, const gm_real *uv2,
                            const gm_real *invw, int filter, int masked)
{
    int minx, maxx, miny, maxy, x, y, k;
    gm_real area;
    gm_real q0 = invw ? invw[0] : 1.0;
    gm_real q1 = invw ? invw[1] : 1.0;
    gm_real q2 = invw ? invw[2] : 1.0;
    if (r == NULL || r->color == NULL || t == NULL || p0 == NULL || p1 == NULL || p2 == NULL)
        return;
    if (uv0 == NULL || uv1 == NULL || uv2 == NULL) return;
    area = edge(p0[0], p0[1], p1[0], p1[1], p2[0], p2[1]);
    if (area == 0.0) return;
    minx = (int)floor(fmin(fmin(p0[0], p1[0]), p2[0]));
    maxx = (int)ceil(fmax(fmax(p0[0], p1[0]), p2[0]));
    miny = (int)floor(fmin(fmin(p0[1], p1[1]), p2[1]));
    maxy = (int)ceil(fmax(fmax(p0[1], p1[1]), p2[1]));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= r->w) maxx = r->w - 1;
    if (maxy >= r->h) maxy = r->h - 1;
    for (y = miny; y <= maxy; y++) {
        for (x = minx; x <= maxx; x++) {
            gm_real px = (gm_real)x + 0.5, py = (gm_real)y + 0.5;
            gm_real w0 = edge(p1[0], p1[1], p2[0], p2[1], px, py) / area;
            gm_real w1 = edge(p2[0], p2[1], p0[0], p0[1], px, py) / area;
            gm_real w2 = edge(p0[0], p0[1], p1[0], p1[1], px, py) / area;
            gm_real z, rgb[3], a = 1.0, q, uu, vv;
            size_t idx;
            if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0) continue;
            idx = (size_t)y * (size_t)r->w + (size_t)x;
            z = w0 * p0[2] + w1 * p1[2] + w2 * p2[2];
            if (z >= r->depth[idx]) continue;
            /* prospettiva: si interpolano u/w e v/w, poi si divide per 1/w */
            q = w0 * q0 + w1 * q1 + w2 * q2;
            if (q == 0.0) continue;
            uu = (w0 * uv0[0] * q0 + w1 * uv1[0] * q1 + w2 * uv2[0] * q2) / q;
            vv = (w0 * uv0[1] * q0 + w1 * uv1[1] * q1 + w2 * uv2[1] * q2) / q;
            gm_tex_sample(t, uu, vv, filter, rgb, &a);
            if (masked && a <= 0.0) continue;
            r->depth[idx] = z;
            for (k = 0; k < 3; k++) r->color[idx * 3 + (size_t)k] = rgb[k];
        }
    }
}

gm_status gm_raster_to_argb(const gm_raster *r, unsigned int *out)
{
    size_t n, i;
    if (r == NULL || out == NULL || r->color == NULL || r->w <= 0 || r->h <= 0) return GM_ERR_ARG;
    n = (size_t)r->w * (size_t)r->h;
    for (i = 0; i < n; i++) {
        unsigned int px = 0;
        int k;
        for (k = 0; k < 3; k++) {
            gm_real v = r->color[i * 3 + (size_t)k];
            int c;
            if (v < 0.0) v = 0.0;
            if (v > 1.0) v = 1.0;
            c = (int)(v * 255.0 + 0.5);
            px = (px << 8) | (unsigned int)c;
        }
        out[i] = px;
    }
    return GM_OK;
}
