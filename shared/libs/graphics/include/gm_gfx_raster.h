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

/* --- Texture + triangoli texturizzati (P0 del piano 3D, additive) -------------
 * Il raster per colore-per-vertice qui sopra resta com'è (e resta l'oracolo dei
 * MiniTest); queste funzioni aggiungono ciò che serve a un motore 3D con mesh:
 * campionamento di texture, UV corretti in prospettiva, alpha-test e uscita nel
 * formato del framebuffer di GMBasic. */

/* Texture RGBA8, riga 0 in ALTO (convenzione delle immagini), canali 0..255. */
typedef struct gm_tex {
    int w, h;
    unsigned char *rgba; /* w*h*4 */
} gm_tex;

gm_status gm_tex_alloc(gm_tex *t, int w, int h);
void gm_tex_free(gm_tex *t);
void gm_tex_set(gm_tex *t, int x, int y,
                unsigned char r, unsigned char g, unsigned char b, unsigned char a);
/* Campiona in (u,v) con WRAP repeat (le coordinate fuori [0,1] si avvolgono).
 * bilinear != 0: interpolazione bilineare, altrimenti nearest.
 * out_rgb in 0..1, *out_a in 0..1 (out_a può essere NULL). */
void gm_tex_sample(const gm_tex *t, gm_real u, gm_real v, int bilinear,
                   gm_real out_rgb[3], gm_real *out_a);

/* Triangolo texturizzato in screen space.
 * p = (x,y,z) per vertice (stessa convenzione di gm_raster_triangle: z piccolo =
 * vicino), uv = (u,v) per vertice, invw = 1/w per vertice (opzionale, NULL =
 * interpolazione affine). Con invw si ottiene l'UV corretto in prospettiva.
 * filter != 0: bilineare. masked != 0: i texel con alpha == 0 sono scartati
 * (alpha-test, per foglie/reti/recinzioni); senza masked l'alpha è ignorato.
 * Profondità e colore sono scritti come in gm_raster_triangle. */
void gm_raster_triangle_tex(gm_raster *r, const gm_tex *t,
                            const gm_real *p0, const gm_real *p1, const gm_real *p2,
                            const gm_real *uv0, const gm_real *uv1, const gm_real *uv2,
                            const gm_real *invw, int filter, int masked);

/* Converte il target in pixel 0x00RRGGBB (clamp, arrotondamento): è il formato
 * del framebuffer di GMBasic, quindi il ponte verso il present SDL2. out ha
 * w*h elementi. */
gm_status gm_raster_to_argb(const gm_raster *r, unsigned int *out);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_RASTER_H */
