/*
 * gm_gfx_raster.h — rasterizzazione software (da SoftwareRenderer).
 *
 * Z-buffer, rasterizzazione di triangoli con interpolazione baricentrica di
 * colore/profondità, clipping near-plane. RGB float, nessuna dipendenza esterna.
 */
#ifndef GMMATH_GM_GFX_RASTER_H
#define GMMATH_GM_GFX_RASTER_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Framebuffer RGB (3 float/pixel) + z-buffer. */
typedef struct gm_raster {
    int w, h;
    gm_real *color; /* w*h*3 */
    gm_real *depth; /* w*h */
} gm_raster;

gm_status gm_raster_alloc(gm_raster *r, int w, int h);
void gm_raster_free(gm_raster *r);
void gm_raster_clear(gm_raster *r, gm_real depth);
void gm_raster_get(const gm_raster *r, int x, int y, gm_real *rgb);
/* Rasterizza un triangolo (screen space): p = (x,y,z), c = (r,g,b). */
void gm_raster_triangle(gm_raster *r,
                        const gm_real *p0, const gm_real *p1, const gm_real *p2,
                        const gm_real *c0, const gm_real *c1, const gm_real *c2);

/* Clipping near-plane (z >= znear) di un triangolo (9 valori). Ritorna il numero
 * di vertici del poligono risultante (0..4), in out (12 valori: 4 vertici x,y,z). */
int gm_raster_clip_near(const gm_real *tri, gm_real znear, gm_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_RASTER_H */
