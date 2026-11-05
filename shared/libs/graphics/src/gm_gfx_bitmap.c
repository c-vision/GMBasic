/* gm_gfx_bitmap.c — bitmap RGB a 3 canali (da bitmap-editor). */
#include "gm_gfx_bitmap.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gm_status gm_bitmap_alloc(gm_bitmap *bm, int w, int h)
{
    if (bm == NULL || w <= 0 || h <= 0) return GM_ERR_ARG;
    bm->w = w; bm->h = h;
    bm->data = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
    if (!bm->data) { bm->w = bm->h = 0; return GM_ERR_NO_MEM; }
    return GM_OK;
}

void gm_bitmap_free(gm_bitmap *bm)
{
    if (bm == NULL) return;
    free(bm->data);
    bm->data = NULL; bm->w = bm->h = 0;
}

void gm_bitmap_fill(gm_bitmap *bm, unsigned char r, unsigned char g, unsigned char b)
{
    size_t n, i;
    if (bm == NULL || bm->data == NULL) return;
    n = (size_t)bm->w * (size_t)bm->h;
    for (i = 0; i < n; i++) { bm->data[i * 3] = r; bm->data[i * 3 + 1] = g; bm->data[i * 3 + 2] = b; }
}

void gm_bitmap_get(const gm_bitmap *bm, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b)
{
    if (bm == NULL || bm->data == NULL || x < 0 || y < 0 || x >= bm->w || y >= bm->h) {
        if (r) *r = 0; if (g) *g = 0; if (b) *b = 0;
        return;
    }
    {
        size_t i = ((size_t)y * (size_t)bm->w + (size_t)x) * 3;
        if (r) *r = bm->data[i];
        if (g) *g = bm->data[i + 1];
        if (b) *b = bm->data[i + 2];
    }
}

void gm_bitmap_set(gm_bitmap *bm, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    size_t i;
    if (bm == NULL || bm->data == NULL || x < 0 || y < 0 || x >= bm->w || y >= bm->h) return;
    i = ((size_t)y * (size_t)bm->w + (size_t)x) * 3;
    bm->data[i] = r; bm->data[i + 1] = g; bm->data[i + 2] = b;
}

void gm_bitmap_grayscale_naive(gm_bitmap *bm)
{
    size_t n, i;
    if (bm == NULL || bm->data == NULL) return;
    n = (size_t)bm->w * (size_t)bm->h;
    for (i = 0; i < n; i++) {
        unsigned char g = (unsigned char)(((int)bm->data[i * 3] + bm->data[i * 3 + 1] + bm->data[i * 3 + 2]) / 3);
        bm->data[i * 3] = bm->data[i * 3 + 1] = bm->data[i * 3 + 2] = g;
    }
}

void gm_bitmap_grayscale_luminance(gm_bitmap *bm)
{
    size_t n, i;
    if (bm == NULL || bm->data == NULL) return;
    n = (size_t)bm->w * (size_t)bm->h;
    for (i = 0; i < n; i++) {
        gm_real y = 0.299 * bm->data[i * 3] + 0.587 * bm->data[i * 3 + 1] + 0.114 * bm->data[i * 3 + 2];
        unsigned char g = (unsigned char)(y + 0.5);
        bm->data[i * 3] = bm->data[i * 3 + 1] = bm->data[i * 3 + 2] = g;
    }
}

static unsigned char clamp255(gm_real v)
{
    if (v < 0.0) return 0;
    if (v > 255.0) return 255;
    return (unsigned char)(v + 0.5);
}

gm_status gm_bitmap_convolve3(gm_bitmap *bm, const int *kernel, gm_real divider)
{
    unsigned char *copy;
    int x, y, c;
    if (bm == NULL || bm->data == NULL || kernel == NULL || divider == 0.0) return GM_ERR_ARG;
    copy = (unsigned char *)malloc((size_t)bm->w * (size_t)bm->h * 3);
    if (!copy) return GM_ERR_NO_MEM;
    memcpy(copy, bm->data, (size_t)bm->w * (size_t)bm->h * 3);
    for (y = 1; y < bm->h - 1; y++)
        for (x = 1; x < bm->w - 1; x++)
            for (c = 0; c < 3; c++) {
                gm_real sum = 0.0;
                int ky, kx;
                for (ky = -1; ky <= 1; ky++)
                    for (kx = -1; kx <= 1; kx++) {
                        size_t idx = ((size_t)(y + ky) * (size_t)bm->w + (size_t)(x + kx)) * 3 + (size_t)c;
                        sum += (gm_real)copy[idx] * (gm_real)kernel[(ky + 1) * 3 + (kx + 1)];
                    }
                bm->data[((size_t)y * (size_t)bm->w + (size_t)x) * 3 + (size_t)c] = clamp255(sum / divider);
            }
    free(copy);
    return GM_OK;
}

void gm_bitmap_brightness_contrast(gm_bitmap *bm, gm_real brightness, gm_real contrast)
{
    size_t n, i;
    if (bm == NULL || bm->data == NULL) return;
    n = (size_t)bm->w * (size_t)bm->h * 3;
    for (i = 0; i < n; i++)
        bm->data[i] = clamp255(((gm_real)bm->data[i] - 128.0) * contrast + 128.0 + brightness);
}

gm_status gm_bitmap_to_image(const gm_bitmap *bm, gm_image *out)
{
    int x, y;
    if (bm == NULL || bm->data == NULL || out == NULL) return GM_ERR_ARG;
    if (gm_image_alloc(out, bm->w, bm->h) != GM_OK) return GM_ERR_NO_MEM;
    for (y = 0; y < bm->h; y++)
        for (x = 0; x < bm->w; x++) {
            size_t i = ((size_t)y * (size_t)bm->w + (size_t)x) * 3;
            gm_real lum = 0.299 * bm->data[i] + 0.587 * bm->data[i + 1] + 0.114 * bm->data[i + 2];
            out->data[(size_t)y * (size_t)out->w + (size_t)x] = lum;
        }
    return GM_OK;
}
