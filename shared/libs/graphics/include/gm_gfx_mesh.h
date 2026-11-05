/*
 * gm_gfx_mesh.h — generazione di mesh (da c3dlas meshgen).
 *
 * Sfera UV, box, piano, cilindro: vertici + indici triangolari.
 */
#ifndef GMMATH_GM_GFX_MESH_H
#define GMMATH_GM_GFX_MESH_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_mesh {
    int nv;          /* numero vertici */
    gm_vec3 *pos;    /* vertici */
    int nt;          /* numero triangoli */
    int *tri;        /* indici (3 per triangolo) */
} gm_mesh;

gm_mesh gm_mesh_uv_sphere(int slices, int stacks, gm_real radius);
gm_mesh gm_mesh_box(gm_real sx, gm_real sy, gm_real sz);
gm_mesh gm_mesh_plane(int nx, int ny, gm_real sx, gm_real sy);
gm_mesh gm_mesh_cylinder(int slices, gm_real radius, gm_real height);
void gm_mesh_free(gm_mesh *m);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_MESH_H */
