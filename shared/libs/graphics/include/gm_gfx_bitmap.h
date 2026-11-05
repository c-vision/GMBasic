/*
 * gm_gfx_bitmap.h — bitmap RGB a 3 canali (da bitmap-editor).
 *
 * Grayscale (naive/luminanza), convoluzione 3×3 per canale con clamp [0,255] e
 * bordo invariato (kernel crop), brightness/contrast, conversione a immagine a
 * canale singolo. Le funzioni sono operazioni matematiche su bitmap.
 */
#ifndef GMMATH_GM_GFX_BITMAP_H
#define GMMATH_GM_GFX_BITMAP_H

#include "gm_core.h"
#include "gm_gfx_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bitmap RGB: 3 byte per pixel (r,g,b), row-major. */
typedef struct gm_bitmap { int w, h; unsigned char *data; } gm_bitmap;

gm_status gm_bitmap_alloc(gm_bitmap *bm, int w, int h);
void gm_bitmap_free(gm_bitmap *bm);
void gm_bitmap_fill(gm_bitmap *bm, unsigned char r, unsigned char g, unsigned char b);
void gm_bitmap_get(const gm_bitmap *bm, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b);
void gm_bitmap_set(gm_bitmap *bm, int x, int y, unsigned char r, unsigned char g, unsigned char b);

/* Grayscale naive: grigio = (r+g+b)/3 (in-place). */
void gm_bitmap_grayscale_naive(gm_bitmap *bm);
/* Grayscale luminanza: 0.299r + 0.587g + 0.114b (in-place). */
void gm_bitmap_grayscale_luminance(gm_bitmap *bm);

/* Convoluzione 3×3 (kernel int[9], per canale), divider, clamp [0,255],
 * bordo invariato (kernel crop). */
gm_status gm_bitmap_convolve3(gm_bitmap *bm, const int *kernel, gm_real divider);
/* Brightness/contrast: out = clamp((in-128)*contrast + 128 + brightness). */
void gm_bitmap_brightness_contrast(gm_bitmap *bm, gm_real brightness, gm_real contrast);

/* Da RGB a immagine a canale singolo (luminanza, [0,255]). */
gm_status gm_bitmap_to_image(const gm_bitmap *bm, gm_image *out);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_BITMAP_H */
