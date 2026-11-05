/*
 * gm_gfx_image.h — immagini (riscrittura, dominio immagini).
 *
 * Immagini 2D a canale singolo (row-major) e volumi 3D: convoluzione generica
 * e separabile, Gaussiana, Sobel, ricampionamento bilineare, up/downsample 2x.
 */
#ifndef GMMATH_GM_GFX_IMAGE_H
#define GMMATH_GM_GFX_IMAGE_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_image { int w, h; gm_real *data; } gm_image;
typedef struct gm_image3 { int w, h, d; gm_real *data; } gm_image3;

/* ---- 2D ---- */
gm_status gm_image_alloc(gm_image *im, int w, int h);
void gm_image_free(gm_image *im);
gm_image gm_image_clone(gm_image im);
gm_real gm_image_get(gm_image im, int x, int y);
void gm_image_set(gm_image im, int x, int y, gm_real v);
void gm_image_fill(gm_image im, gm_real v);
gm_real gm_image_min(gm_image im);
gm_real gm_image_max(gm_image im);
gm_real gm_image_absmax(gm_image im);
void gm_image_scale(gm_image im, gm_real s);
/* out = a - b (stesse dimensioni). */
void gm_image_sub(gm_image a, gm_image b, gm_image out);
/* convoluzione 2D generica con kernel kx×ky (ancorato al centro). */
void gm_image_convolve(gm_image in, const gm_real *kernel, int kx, int ky, gm_image out);
/* convoluzione separabile: righe con kx[nx], colonne con ky[ny]. */
void gm_image_convolve_sep(gm_image in, const gm_real *kx, int nx,
                           const gm_real *ky, int ny, gm_image out);
/* blur gaussiano (costruisce il kernel da sigma). */
void gm_image_gaussian(gm_image in, gm_real sigma, gm_image out);
/* gradiente di Sobel. */
void gm_image_sobel(gm_image in, gm_image gx, gm_image gy);
/* ricampionamento bilineare a nw×nh. */
gm_image gm_image_resample(gm_image in, int nw, int nh);
/* up/downsample 2x (downsample = media 2x2). */
gm_image gm_image_upsample2(gm_image in);
gm_image gm_image_downsample2(gm_image in);

/* ---- 3D ---- */
gm_status gm_image3_alloc(gm_image3 *im, int w, int h, int d);
void gm_image3_free(gm_image3 *im);
gm_real gm_image3_get(gm_image3 im, int x, int y, int z);
void gm_image3_set(gm_image3 im, int x, int y, int z, gm_real v);
void gm_image3_fill(gm_image3 im, gm_real v);
/* convoluzione separabile 3D con lo stesso kernel 1D su x, y, z. */
void gm_image3_convolve_sep(gm_image3 in, const gm_real *k, int n, gm_image3 out);

/* ---- trasformazioni geometriche (da bitmap-editor) ---- */
gm_image gm_image_transpose(gm_image in);
/* rotazione di k*90° in senso antiorario (k mod 4). */
gm_image gm_image_rotate90(gm_image in, int k);
/* rotazione di un angolo arbitrario attorno al centro (bilineare, bg fuori). */
gm_image gm_image_rotate(gm_image in, gm_real radians, gm_real bg);
gm_status gm_image_flip_h(gm_image in, gm_image out);
gm_status gm_image_flip_v(gm_image in, gm_image out);

/* ---- punti/istogramma ---- */
gm_status gm_image_threshold(gm_image in, gm_real t, gm_real lo, gm_real hi, gm_image out);
gm_status gm_image_normalize(gm_image in, gm_real lo, gm_real hi, gm_image out);
gm_status gm_image_histogram(gm_image in, int nbins, gm_real lo, gm_real hi, gm_real *counts);
gm_status gm_image_hist_equalize(gm_image in, int nbins, gm_image out);
gm_status gm_image_median3(gm_image in, gm_image out);

/* ---- disegno (in-place) ---- */
void gm_image_draw_line(gm_image im, int x0, int y0, int x1, int y1, gm_real v);
void gm_image_draw_rect(gm_image im, int x0, int y0, int x1, int y1, gm_real v, int fill);
void gm_image_draw_circle(gm_image im, int cx, int cy, int r, gm_real v, int fill);
void gm_image_draw_triangle(gm_image im, int ax, int ay, int bx, int by, int cx, int cy,
                            gm_real v, int fill);

/* Poisson image editing / seamless clone (da cnnvisualizer).
 * Incolla il gradiente di `source` nella regione `mask`!=0 mantenendo i valori
 * di `target` fuori dalla maschera (Dirichlet). target/source/out: nx*ny
 * (griglia con passo 1). Risolve ∇²f = ∇²g su Ω con Gauss-Seidel (maxit iter).
 * out deve essere preallocato nx*ny. */
gm_status gm_image_poisson_blend(const gm_real *target, const gm_real *source,
                                 const unsigned char *mask, int nx, int ny,
                                 gm_real *out, int maxit);

/* Warping all'indietro con interpolazione bilineare (da MEMC-Net / flownet2).
 * in: c*h*w (CHW); flow_x, flow_y: h*w (offset per pixel). Per ogni pixel
 * (y,x): sorgente = (y+flow_y, x+flow_x); campionamento bilineare, 0 fuori.
 * out: c*h*w. */
gm_status gm_image_warp_bilinear(gm_real *out, const gm_real *in, int c, int h, int w,
                                 const gm_real *flow_x, const gm_real *flow_y);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_IMAGE_H */
