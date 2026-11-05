/* gm_gfx_image.c — immagini 2D/3D (riscrittura). */
#include "gm_gfx_image.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define IM_AT(im, x, y) ((im).data[(size_t)(y) * (size_t)(im).w + (size_t)(x)])

gm_status gm_image_alloc(gm_image *im, int w, int h)
{
    if (im == NULL || w <= 0 || h <= 0) return GM_ERR_ARG;
    im->w = w; im->h = h;
    im->data = (gm_real *)malloc((size_t)w * (size_t)h * sizeof(gm_real));
    if (!im->data) { im->w = im->h = 0; return GM_ERR_NO_MEM; }
    return GM_OK;
}

void gm_image_free(gm_image *im)
{
    if (im == NULL) return;
    free(im->data);
    im->data = NULL; im->w = im->h = 0;
}

gm_image gm_image_clone(gm_image im)
{
    gm_image out;
    out.w = im.w; out.h = im.h; out.data = NULL;
    if (im.data != NULL && gm_image_alloc(&out, im.w, im.h) == GM_OK)
        memcpy(out.data, im.data, (size_t)im.w * (size_t)im.h * sizeof(gm_real));
    return out;
}

gm_real gm_image_get(gm_image im, int x, int y)
{
    if (x < 0) x = 0; if (x >= im.w) x = im.w - 1;
    if (y < 0) y = 0; if (y >= im.h) y = im.h - 1;
    return IM_AT(im, x, y);
}

void gm_image_set(gm_image im, int x, int y, gm_real v)
{
    if (x < 0 || x >= im.w || y < 0 || y >= im.h) return;
    IM_AT(im, x, y) = v;
}

void gm_image_fill(gm_image im, gm_real v)
{
    size_t n = (size_t)im.w * (size_t)im.h, i;
    for (i = 0; i < n; i++) im.data[i] = v;
}

gm_real gm_image_min(gm_image im)
{
    size_t n = (size_t)im.w * (size_t)im.h, i;
    gm_real m;
    if (n == 0) return 0.0;
    m = im.data[0];
    for (i = 1; i < n; i++) if (im.data[i] < m) m = im.data[i];
    return m;
}

gm_real gm_image_max(gm_image im)
{
    size_t n = (size_t)im.w * (size_t)im.h, i;
    gm_real m;
    if (n == 0) return 0.0;
    m = im.data[0];
    for (i = 1; i < n; i++) if (im.data[i] > m) m = im.data[i];
    return m;
}

gm_real gm_image_absmax(gm_image im)
{
    size_t n = (size_t)im.w * (size_t)im.h, i;
    gm_real m = 0.0;
    for (i = 0; i < n; i++) { gm_real v = fabs(im.data[i]); if (v > m) m = v; }
    return m;
}

void gm_image_scale(gm_image im, gm_real s)
{
    size_t n = (size_t)im.w * (size_t)im.h, i;
    for (i = 0; i < n; i++) im.data[i] *= s;
}

void gm_image_sub(gm_image a, gm_image b, gm_image out)
{
    size_t n = (size_t)a.w * (size_t)a.h, i;
    for (i = 0; i < n; i++) out.data[i] = a.data[i] - b.data[i];
}

void gm_image_convolve(gm_image in, const gm_real *kernel, int kx, int ky, gm_image out)
{
    int x, y, i, j;
    int cx = kx / 2, cy = ky / 2;
    for (y = 0; y < in.h; y++) {
        for (x = 0; x < in.w; x++) {
            gm_real s = 0.0;
            for (j = 0; j < ky; j++)
                for (i = 0; i < kx; i++)
                    s += kernel[(size_t)j * (size_t)kx + (size_t)i]
                       * gm_image_get(in, x + i - cx, y + j - cy);
            IM_AT(out, x, y) = s;
        }
    }
}

void gm_image_convolve_sep(gm_image in, const gm_real *kx, int nx,
                           const gm_real *ky, int ny, gm_image out)
{
    gm_image tmp;
    int x, y, i, j;
    int cx = nx / 2, cy = ny / 2;
    if (gm_image_alloc(&tmp, in.w, in.h) != GM_OK) return;
    /* righe */
    for (y = 0; y < in.h; y++) {
        for (x = 0; x < in.w; x++) {
            gm_real s = 0.0;
            for (i = 0; i < nx; i++) s += kx[i] * gm_image_get(in, x + i - cx, y);
            IM_AT(tmp, x, y) = s;
        }
    }
    /* colonne */
    for (y = 0; y < in.h; y++) {
        for (x = 0; x < in.w; x++) {
            gm_real s = 0.0;
            for (j = 0; j < ny; j++) s += ky[j] * gm_image_get(tmp, x, y + j - cy);
            IM_AT(out, x, y) = s;
        }
    }
    gm_image_free(&tmp);
}

void gm_image_gaussian(gm_image in, gm_real sigma, gm_image out)
{
    int radius, i, n;
    gm_real *k;
    gm_real sum = 0.0;
    if (sigma <= 0.0) { memcpy(out.data, in.data, (size_t)in.w * (size_t)in.h * sizeof(gm_real)); return; }
    radius = (int)ceil(3.0 * sigma);
    n = 2 * radius + 1;
    k = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!k) return;
    for (i = 0; i < n; i++) {
        gm_real d = (gm_real)(i - radius);
        k[i] = exp(-(d * d) / (2.0 * sigma * sigma));
        sum += k[i];
    }
    for (i = 0; i < n; i++) k[i] /= sum;
    gm_image_convolve_sep(in, k, n, k, n, out);
    free(k);
}

void gm_image_sobel(gm_image in, gm_image gx, gm_image gy)
{
    static const gm_real sx[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
    static const gm_real sy[9] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};
    gm_image_convolve(in, sx, 3, 3, gx);
    gm_image_convolve(in, sy, 3, 3, gy);
}

gm_image gm_image_resample(gm_image in, int nw, int nh)
{
    gm_image out;
    int x, y;
    out.w = nw; out.h = nh; out.data = NULL;
    if (gm_image_alloc(&out, nw, nh) != GM_OK) return out;
    for (y = 0; y < nh; y++) {
        gm_real fy = (nh > 1) ? (gm_real)y * (gm_real)(in.h - 1) / (gm_real)(nh - 1) : 0.0;
        int y0 = (int)floor(fy), y1 = (y0 + 1 < in.h) ? y0 + 1 : y0;
        gm_real wy = fy - y0;
        for (x = 0; x < nw; x++) {
            gm_real fx = (nw > 1) ? (gm_real)x * (gm_real)(in.w - 1) / (gm_real)(nw - 1) : 0.0;
            int x0 = (int)floor(fx), x1 = (x0 + 1 < in.w) ? x0 + 1 : x0;
            gm_real wx = fx - x0;
            gm_real v00 = IM_AT(in, x0, y0), v10 = IM_AT(in, x1, y0);
            gm_real v01 = IM_AT(in, x0, y1), v11 = IM_AT(in, x1, y1);
            gm_real top = v00 + (v10 - v00) * wx;
            gm_real bot = v01 + (v11 - v01) * wx;
            IM_AT(out, x, y) = top + (bot - top) * wy;
        }
    }
    return out;
}

gm_image gm_image_upsample2(gm_image in)
{
    return gm_image_resample(in, in.w * 2, in.h * 2);
}

gm_image gm_image_downsample2(gm_image in)
{
    gm_image out;
    int x, y;
    out.w = in.w / 2; out.h = in.h / 2; out.data = NULL;
    if (out.w <= 0 || out.h <= 0) return out;
    if (gm_image_alloc(&out, out.w, out.h) != GM_OK) return out;
    for (y = 0; y < out.h; y++) {
        for (x = 0; x < out.w; x++) {
            gm_real s = IM_AT(in, 2 * x, 2 * y) + IM_AT(in, 2 * x + 1, 2 * y)
                      + IM_AT(in, 2 * x, 2 * y + 1) + IM_AT(in, 2 * x + 1, 2 * y + 1);
            IM_AT(out, x, y) = 0.25 * s;
        }
    }
    return out;
}

/* ============================== 3D ============================== */

#define IM3_AT(im, x, y, z) ((im).data[(((size_t)(z) * (size_t)(im).h) + (size_t)(y)) * (size_t)(im).w + (size_t)(x)])

gm_status gm_image3_alloc(gm_image3 *im, int w, int h, int d)
{
    if (im == NULL || w <= 0 || h <= 0 || d <= 0) return GM_ERR_ARG;
    im->w = w; im->h = h; im->d = d;
    im->data = (gm_real *)malloc((size_t)w * (size_t)h * (size_t)d * sizeof(gm_real));
    if (!im->data) { im->w = im->h = im->d = 0; return GM_ERR_NO_MEM; }
    return GM_OK;
}

void gm_image3_free(gm_image3 *im)
{
    if (im == NULL) return;
    free(im->data);
    im->data = NULL; im->w = im->h = im->d = 0;
}

gm_real gm_image3_get(gm_image3 im, int x, int y, int z)
{
    if (x < 0) x = 0; if (x >= im.w) x = im.w - 1;
    if (y < 0) y = 0; if (y >= im.h) y = im.h - 1;
    if (z < 0) z = 0; if (z >= im.d) z = im.d - 1;
    return IM3_AT(im, x, y, z);
}

void gm_image3_set(gm_image3 im, int x, int y, int z, gm_real v)
{
    if (x < 0 || x >= im.w || y < 0 || y >= im.h || z < 0 || z >= im.d) return;
    IM3_AT(im, x, y, z) = v;
}

void gm_image3_fill(gm_image3 im, gm_real v)
{
    size_t n = (size_t)im.w * (size_t)im.h * (size_t)im.d, i;
    for (i = 0; i < n; i++) im.data[i] = v;
}

void gm_image3_convolve_sep(gm_image3 in, const gm_real *k, int n, gm_image3 out)
{
    int x, y, z, i, c = n / 2;
    gm_image3 tmp;
    if (gm_image3_alloc(&tmp, in.w, in.h, in.d) != GM_OK) return;
    /* asse x */
    for (z = 0; z < in.d; z++)
        for (y = 0; y < in.h; y++)
            for (x = 0; x < in.w; x++) {
                gm_real s = 0.0;
                for (i = 0; i < n; i++) s += k[i] * gm_image3_get(in, x + i - c, y, z);
                IM3_AT(tmp, x, y, z) = s;
            }
    /* asse y (da tmp a out) */
    for (z = 0; z < in.d; z++)
        for (y = 0; y < in.h; y++)
            for (x = 0; x < in.w; x++) {
                gm_real s = 0.0;
                for (i = 0; i < n; i++) s += k[i] * gm_image3_get(tmp, x, y + i - c, z);
                IM3_AT(out, x, y, z) = s;
            }
    /* asse z (da out a tmp, poi copia) */
    for (z = 0; z < in.d; z++)
        for (y = 0; y < in.h; y++)
            for (x = 0; x < in.w; x++) {
                gm_real s = 0.0;
                for (i = 0; i < n; i++) s += k[i] * gm_image3_get(out, x, y, z + i - c);
                IM3_AT(tmp, x, y, z) = s;
            }
    memcpy(out.data, tmp.data, (size_t)in.w * (size_t)in.h * (size_t)in.d * sizeof(gm_real));
    gm_image3_free(&tmp);
}

/* ============================== trasformazioni geometriche ============================== */

gm_image gm_image_transpose(gm_image in)
{
    gm_image out;
    int x, y;
    out.w = in.h; out.h = in.w; out.data = NULL;
    if (gm_image_alloc(&out, in.h, in.w) != GM_OK) return out;
    for (y = 0; y < in.h; y++)
        for (x = 0; x < in.w; x++)
            out.data[(size_t)x * (size_t)out.w + (size_t)y] = in.data[(size_t)y * (size_t)in.w + (size_t)x];
    return out;
}

gm_image gm_image_rotate90(gm_image in, int k)
{
    gm_image out;
    int x, y;
    k = ((k % 4) + 4) % 4;
    if (k == 0) return gm_image_clone(in);
    if (k == 2) {
        out.w = in.w; out.h = in.h; out.data = NULL;
        if (gm_image_alloc(&out, in.w, in.h) != GM_OK) return out;
        for (y = 0; y < in.h; y++)
            for (x = 0; x < in.w; x++)
                IM_AT(out, x, y) = IM_AT(in, in.w - 1 - x, in.h - 1 - y);
        return out;
    }
    /* k == 1 o 3: dimensioni scambiate */
    out.w = in.h; out.h = in.w; out.data = NULL;
    if (gm_image_alloc(&out, in.h, in.w) != GM_OK) return out;
    for (y = 0; y < out.h; y++)
        for (x = 0; x < out.w; x++) {
            gm_real v;
            if (k == 1) v = IM_AT(in, y, in.h - 1 - x);          /* 90° antiorario */
            else        v = IM_AT(in, in.w - 1 - y, x);          /* 270° antiorario */
            IM_AT(out, x, y) = v;
        }
    return out;
}

gm_image gm_image_rotate(gm_image in, gm_real radians, gm_real bg)
{
    gm_image out;
    int x, y;
    gm_real c = cos(radians), s = sin(radians);
    gm_real cx = 0.5 * (gm_real)(in.w - 1), cy = 0.5 * (gm_real)(in.h - 1);
    out.w = in.w; out.h = in.h; out.data = NULL;
    if (gm_image_alloc(&out, in.w, in.h) != GM_OK) return out;
    for (y = 0; y < in.h; y++)
        for (x = 0; x < in.w; x++) {
            gm_real dx = (gm_real)x - cx, dy = (gm_real)y - cy;
            gm_real sx = c * dx + s * dy + cx;   /* rotazione inversa */
            gm_real sy = -s * dx + c * dy + cy;
            if (sx < 0.0 || sy < 0.0 || sx > (gm_real)(in.w - 1) || sy > (gm_real)(in.h - 1)) {
                IM_AT(out, x, y) = bg;
            } else {
                int x0 = (int)floor(sx), y0 = (int)floor(sy);
                gm_real fx = sx - (gm_real)x0, fy = sy - (gm_real)y0;
                gm_real v00 = IM_AT(in, x0, y0);
                gm_real v10 = IM_AT(in, x0 + 1 <= in.w - 1 ? x0 + 1 : x0, y0);
                gm_real v01 = IM_AT(in, x0, y0 + 1 <= in.h - 1 ? y0 + 1 : y0);
                gm_real v11 = IM_AT(in, x0 + 1 <= in.w - 1 ? x0 + 1 : x0, y0 + 1 <= in.h - 1 ? y0 + 1 : y0);
                IM_AT(out, x, y) = (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10
                                 + (1 - fx) * fy * v01 + fx * fy * v11;
            }
        }
    return out;
}

gm_status gm_image_flip_h(gm_image in, gm_image out)
{
    int x, y;
    if (in.w != out.w || in.h != out.h || in.data == NULL || out.data == NULL) return GM_ERR_ARG;
    for (y = 0; y < in.h; y++)
        for (x = 0; x < in.w; x++)
            IM_AT(out, x, y) = IM_AT(in, in.w - 1 - x, y);
    return GM_OK;
}

gm_status gm_image_flip_v(gm_image in, gm_image out)
{
    int x, y;
    if (in.w != out.w || in.h != out.h || in.data == NULL || out.data == NULL) return GM_ERR_ARG;
    for (y = 0; y < in.h; y++)
        for (x = 0; x < in.w; x++)
            IM_AT(out, x, y) = IM_AT(in, x, in.h - 1 - y);
    return GM_OK;
}

/* ============================== punti/istogramma ============================== */

gm_status gm_image_threshold(gm_image in, gm_real t, gm_real lo, gm_real hi, gm_image out)
{
    size_t n, i;
    if (in.w != out.w || in.h != out.h || in.data == NULL || out.data == NULL) return GM_ERR_ARG;
    n = (size_t)in.w * (size_t)in.h;
    for (i = 0; i < n; i++) out.data[i] = (in.data[i] >= t) ? hi : lo;
    return GM_OK;
}

gm_status gm_image_normalize(gm_image in, gm_real lo, gm_real hi, gm_image out)
{
    gm_real mn, mx, rng;
    size_t n, i;
    if (in.w != out.w || in.h != out.h || in.data == NULL || out.data == NULL) return GM_ERR_ARG;
    mn = gm_image_min(in); mx = gm_image_max(in);
    rng = mx - mn;
    n = (size_t)in.w * (size_t)in.h;
    if (rng == 0.0) { for (i = 0; i < n; i++) out.data[i] = lo; return GM_OK; }
    for (i = 0; i < n; i++) out.data[i] = lo + (in.data[i] - mn) * (hi - lo) / rng;
    return GM_OK;
}

gm_status gm_image_histogram(gm_image in, int nbins, gm_real lo, gm_real hi, gm_real *counts)
{
    size_t n, i;
    int b;
    if (in.data == NULL || counts == NULL || nbins <= 0 || hi <= lo) return GM_ERR_ARG;
    for (b = 0; b < nbins; b++) counts[b] = 0.0;
    n = (size_t)in.w * (size_t)in.h;
    for (i = 0; i < n; i++) {
        int k = (int)((in.data[i] - lo) / (hi - lo) * (gm_real)nbins);
        if (k < 0) k = 0;
        if (k >= nbins) k = nbins - 1;
        counts[k] += 1.0;
    }
    return GM_OK;
}

gm_status gm_image_hist_equalize(gm_image in, int nbins, gm_image out)
{
    gm_real *counts, cdf, mn, mx;
    size_t n, i;
    int b;
    if (in.w != out.w || in.h != out.h || in.data == NULL || out.data == NULL || nbins <= 0)
        return GM_ERR_ARG;
    mn = gm_image_min(in); mx = gm_image_max(in);
    if (mx == mn) { gm_image_fill(out, mn); return GM_OK; }
    counts = (gm_real *)malloc((size_t)nbins * sizeof(gm_real));
    if (!counts) return GM_ERR_NO_MEM;
    gm_image_histogram(in, nbins, mn, mx, counts);
    n = (size_t)in.w * (size_t)in.h;
    cdf = 0.0;
    for (b = 0; b < nbins; b++) {
        gm_real next = cdf + counts[b];
        counts[b] = cdf;
        cdf = next;
    }
    for (i = 0; i < n; i++) {
        int k = (int)((in.data[i] - mn) / (mx - mn) * (gm_real)nbins);
        if (k < 0) k = 0;
        if (k >= nbins) k = nbins - 1;
        out.data[i] = mn + (mx - mn) * (counts[k] / (gm_real)n);
    }
    free(counts);
    return GM_OK;
}

gm_status gm_image_median3(gm_image in, gm_image out)
{
    int x, y;
    if (in.w != out.w || in.h != out.h || in.data == NULL || out.data == NULL) return GM_ERR_ARG;
    for (y = 0; y < in.h; y++)
        for (x = 0; x < in.w; x++) {
            gm_real w[9];
            int i, j, k;
            for (i = -1; i <= 1; i++)
                for (j = -1; j <= 1; j++)
                    w[(i + 1) * 3 + (j + 1)] = gm_image_get(in, x + i, y + j);
            for (i = 0; i < 9; i++)
                for (j = i + 1; j < 9; j++)
                    if (w[j] < w[i]) { gm_real t = w[i]; w[i] = w[j]; w[j] = t; }
            k = 4;
            IM_AT(out, x, y) = w[k];
        }
    return GM_OK;
}

/* ============================== disegno ============================== */

void gm_image_draw_line(gm_image im, int x0, int y0, int x1, int y1, gm_real v)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gm_image_set(im, x0, y0, v);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

void gm_image_draw_rect(gm_image im, int x0, int y0, int x1, int y1, gm_real v, int fill)
{
    int x, y;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (fill) {
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++) gm_image_set(im, x, y, v);
    } else {
        gm_image_draw_line(im, x0, y0, x1, y0, v);
        gm_image_draw_line(im, x1, y0, x1, y1, v);
        gm_image_draw_line(im, x1, y1, x0, y1, v);
        gm_image_draw_line(im, x0, y1, x0, y0, v);
    }
}

void gm_image_draw_circle(gm_image im, int cx, int cy, int r, gm_real v, int fill)
{
    int x, y;
    if (fill) {
        for (y = -r; y <= r; y++)
            for (x = -r; x <= r; x++)
                if (x * x + y * y <= r * r) gm_image_set(im, cx + x, cy + y, v);
    } else {
        int xr = r, yr = 0, err = 1 - r;
        while (xr >= yr) {
            gm_image_set(im, cx + xr, cy + yr, v);
            gm_image_set(im, cx + yr, cy + xr, v);
            gm_image_set(im, cx - yr, cy + xr, v);
            gm_image_set(im, cx - xr, cy + yr, v);
            gm_image_set(im, cx - xr, cy - yr, v);
            gm_image_set(im, cx - yr, cy - xr, v);
            gm_image_set(im, cx + yr, cy - xr, v);
            gm_image_set(im, cx + xr, cy - yr, v);
            yr++;
            if (err < 0) err += 2 * yr + 1;
            else { xr--; err += 2 * (yr - xr) + 1; }
        }
    }
}

static int tri_sign(int ax, int ay, int bx, int by, int cx, int cy)
{
    return (ax - cx) * (by - cy) - (bx - cx) * (ay - cy);
}

void gm_image_draw_triangle(gm_image im, int ax, int ay, int bx, int by, int cx, int cy,
                            gm_real v, int fill)
{
    int minx = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
    int maxx = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
    int miny = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
    int maxy = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
    int x, y;
    if (fill) {
        for (y = miny; y <= maxy; y++)
            for (x = minx; x <= maxx; x++) {
                int d1 = tri_sign(x, y, ax, ay, bx, by);
                int d2 = tri_sign(x, y, bx, by, cx, cy);
                int d3 = tri_sign(x, y, cx, cy, ax, ay);
                int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                if (!(has_neg && has_pos)) gm_image_set(im, x, y, v);
            }
    } else {
        gm_image_draw_line(im, ax, ay, bx, by, v);
        gm_image_draw_line(im, bx, by, cx, cy, v);
        gm_image_draw_line(im, cx, cy, ax, ay, v);
    }
}

/* ===================== Poisson image editing (da cnnvisualizer) ===================== */

gm_status gm_image_poisson_blend(const gm_real *target, const gm_real *source,
                                 const unsigned char *mask, int nx, int ny,
                                 gm_real *out, int maxit)
{
    int i, x, y, it;
    if (!target || !source || !mask || !out || nx < 3 || ny < 3) return GM_ERR_ARG;
    if (maxit < 1) maxit = 500;
    for (i = 0; i < nx * ny; i++) out[i] = target[i];
    for (it = 0; it < maxit; it++) {
        for (y = 1; y < ny - 1; y++)
            for (x = 1; x < nx - 1; x++) {
                int idx = y * nx + x;
                gm_real lap_g, s;
                if (!mask[idx]) continue;
                lap_g = source[idx - 1] + source[idx + 1] + source[idx - nx] + source[idx + nx] - 4.0 * source[idx];
                s = out[idx - 1] + out[idx + 1] + out[idx - nx] + out[idx + nx] - lap_g;
                out[idx] = 0.25 * s;
            }
    }
    return GM_OK;
}

/* ===================== Warping bilineare (da MEMC-Net) ===================== */

gm_status gm_image_warp_bilinear(gm_real *out, const gm_real *in, int c, int h, int w,
                                 const gm_real *flow_x, const gm_real *flow_y)
{
    int ch, y, x;
    if (!out || !in || !flow_x || !flow_y || c < 1 || h < 1 || w < 1) return GM_ERR_ARG;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            gm_real x2 = (gm_real)x + flow_x[y * w + x];
            gm_real y2 = (gm_real)y + flow_y[y * w + x];
            if (x2 >= 0.0 && y2 >= 0.0 && x2 < (gm_real)w && y2 < (gm_real)h) {
                int xl = (int)x2, yt = (int)y2;
                int xr = (xl + 1 < w) ? xl + 1 : w - 1;
                int yb = (yt + 1 < h) ? yt + 1 : h - 1;
                gm_real al = x2 - xl, be = y2 - yt;
                for (ch = 0; ch < c; ch++) {
                    const gm_real *p = in + (size_t)ch * h * w;
                    gm_real TL = p[yt * w + xl], TR = p[yt * w + xr];
                    gm_real BL = p[yb * w + xl], BR = p[yb * w + xr];
                    out[((size_t)ch * h + y) * w + x] =
                        (1 - al) * (1 - be) * TL + al * (1 - be) * TR
                      + (1 - al) * be * BL + al * be * BR;
                }
            } else {
                for (ch = 0; ch < c; ch++) out[((size_t)ch * h + y) * w + x] = 0.0;
            }
        }
    return GM_OK;
}
