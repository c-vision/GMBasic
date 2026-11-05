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
