/*
 * gm_gfx_surface.h — visualizzazione di superfici (volatilità implicita e
 * campi di altezza): colormap, heatmap RGB e mesh a campo di altezza.
 *
 * Da AI/Sources (IV surface / surface visualization, clean-room): le superfici
 * IV (strike × maturity) costruite con gm_interp2_bilinear/gmf_iv_surface
 * possono essere renderizzate come heatmap (bitmap RGB) o come mesh 3D.
 */
#ifndef GMMATH_GM_GFX_SURFACE_H
#define GMMATH_GM_GFX_SURFACE_H

#include "gm_core.h"
#include "gm_vec.h"
#include "gm_gfx_bitmap.h"
#include "gm_gfx_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Colormap jet-like: t in [0,1] -> RGB (blu -> ciano -> verde -> giallo ->
 * rosso), interpolazione lineare a tratti su 4 punti di controllo. */
void gm_gfx_colormap(gm_real t, unsigned char *r, unsigned char *g, unsigned char *b);

/* Heatmap: griglia z[nx*ny] (row-major, z[i*ny + j]) -> bitmap RGB nx×ny con
 * colormap, normalizzata tra zmin e zmax. Il bitmap viene (ri)allocato. */
gm_status gm_gfx_heatmap(const gm_real *z, int nx, int ny, gm_real zmin, gm_real zmax,
                         gm_bitmap *out);

/* Mesh a campo di altezza: vertici su griglia nx×ny su [0,sx]×[0,sy] con
 * altezza z[i*ny+j]*zscale, due triangoli per cella. Richiede nx, ny >= 2. */
gm_status gm_mesh_surface(const gm_real *z, int nx, int ny, gm_real sx, gm_real sy,
                          gm_real zscale, gm_mesh *m);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_SURFACE_H */