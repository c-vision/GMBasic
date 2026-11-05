/*
 * gm_gfx_surface.c — visualizzazione di superfici (IV surface, heatmap, mesh).
 * Riscrittura C99 clean-room da AI/Sources (IV surface, clean-room).
 */
#include "gm_gfx_surface.h"

#include <stdlib.h>

void gm_gfx_colormap(gm_real t, unsigned char *r, unsigned char *g, unsigned char *b)
{
    static const double ctrl[4][3] = {
        { 0.0, 0.0, 1.0 },   /* blu */
        { 0.0, 1.0, 1.0 },   /* ciano */
        { 1.0, 1.0, 0.0 },   /* giallo */
        { 1.0, 0.0, 0.0 }    /* rosso */
    };
    double tt = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t) * 3.0;
    int seg = (int)tt;
    double u, rc, gc, bc;
    if (seg > 2) seg = 2;
    u = tt - seg;
    rc = ctrl[seg][0] + u * (ctrl[seg + 1][0] - ctrl[seg][0]);
    gc = ctrl[seg][1] + u * (ctrl[seg + 1][1] - ctrl[seg][1]);
    bc = ctrl[seg][2] + u * (ctrl[seg + 1][2] - ctrl[seg][2]);
    if (r) *r = (unsigned char)(rc * 255.0 + 0.5);
    if (g) *g = (unsigned char)(gc * 255.0 + 0.5);
    if (b) *b = (unsigned char)(bc * 255.0 + 0.5);
}

gm_status gm_gfx_heatmap(const gm_real *z, int nx, int ny, gm_real zmin, gm_real zmax,
                         gm_bitmap *out)
{
    int i, j;
    if (!z || !out || nx < 1 || ny < 1) return GM_ERR_ARG;
    if (out->data) gm_bitmap_free(out);
    if (gm_bitmap_alloc(out, nx, ny) != GM_OK) return GM_ERR_NO_MEM;
    if (zmax <= zmin) zmax = zmin + 1.0;
    for (i = 0; i < nx; i++)
        for (j = 0; j < ny; j++) {
            unsigned char r, g, b;
            gm_real t = (z[i * ny + j] - zmin) / (zmax - zmin);
            gm_gfx_colormap(t, &r, &g, &b);
            gm_bitmap_set(out, i, j, r, g, b);
        }
    return GM_OK;
}

gm_status gm_mesh_surface(const gm_real *z, int nx, int ny, gm_real sx, gm_real sy,
                          gm_real zscale, gm_mesh *m)
{
    int nv, nt, i, j, idx = 0, t = 0;
    if (!z || !m || nx < 2 || ny < 2) return GM_ERR_ARG;
    nv = nx * ny;
    nt = 2 * (nx - 1) * (ny - 1);
    m->nv = nv;
    m->nt = nt;
    m->pos = (gm_vec3 *)malloc((size_t)nv * sizeof(gm_vec3));
    m->tri = (int *)malloc((size_t)nt * 3 * sizeof(int));
    if (!m->pos || !m->tri) {
        free(m->pos); free(m->tri);
        m->pos = NULL; m->tri = NULL; m->nv = 0; m->nt = 0;
        return GM_ERR_NO_MEM;
    }
    for (i = 0; i < nx; i++)
        for (j = 0; j < ny; j++) {
            m->pos[idx].x = (nx > 1 ? sx * i / (nx - 1) : 0.0);
            m->pos[idx].y = (ny > 1 ? sy * j / (ny - 1) : 0.0);
            m->pos[idx].z = z[i * ny + j] * zscale;
            idx++;
        }
    for (i = 0; i < nx - 1; i++)
        for (j = 0; j < ny - 1; j++) {
            int a = i * ny + j, b = i * ny + j + 1;
            int c = (i + 1) * ny + j, d = (i + 1) * ny + j + 1;
            m->tri[t++] = a; m->tri[t++] = c; m->tri[t++] = b;
            m->tri[t++] = b; m->tri[t++] = c; m->tri[t++] = d;
        }
    return GM_OK;
}