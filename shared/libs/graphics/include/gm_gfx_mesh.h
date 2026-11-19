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

/* --- Attributi opzionali (normali + UV) -------------------------------------
 * Stanno in un blocco SEPARATO perché `gm_mesh` ha 4 campi ma viene costruita
 * anche da altri domini (`gm_mesh_surface` in gmbgfx-surface): allungarla
 * lascerebbe non inizializzati i campi nuovi per chi non passa da questi
 * generatori, e `gm_mesh_free` libererebbe puntatori spazzatura. Con un blocco
 * separato l'aggiunta è additiva al 100%: le mesh esistenti restano identiche
 * (stessi conteggi, stessi vertici) e chi non usa le texture ignora il blocco. */
typedef struct gm_mesh_attrs {
    gm_vec3 *nrm;  /* nv normali unitarie */
    gm_real *uv;   /* nv*2 coordinate (u,v) */
} gm_mesh_attrs;

gm_mesh_attrs gm_mesh_attrs_none(void);
void gm_mesh_attrs_free(gm_mesh_attrs *a);
/* 1 se nrm e uv sono allocati e nv > 0: usare il blocco con la mesh che lo ha
 * prodotto (stesso nv), non con un'altra. */
int gm_mesh_attrs_valid(const gm_mesh_attrs *a, int nv);

/* Normali lisce (media pesata per area) di una mesh qualsiasi; nrm ha m->nv
 * elementi ed è scritto interamente. Non modifica m. */
gm_status gm_mesh_normals(const gm_mesh *m, gm_vec3 *nrm);

/* Generatori con attributi: riempiono m e a. La mesh si libera con
 * gm_mesh_free, gli attributi con gm_mesh_attrs_free (entrambi, sempre).
 * UV in [0,1], normali unitarie, stesse convenzioni di avvolgimento dei
 * generatori senza attributi. */
gm_status gm_mesh_box_uv(gm_real sx, gm_real sy, gm_real sz, gm_mesh *m, gm_mesh_attrs *a);
gm_status gm_mesh_plane_uv(int nx, int ny, gm_real sx, gm_real sy, gm_mesh *m, gm_mesh_attrs *a);
gm_status gm_mesh_sphere_uv(int slices, int stacks, gm_real radius, gm_mesh *m, gm_mesh_attrs *a);
gm_status gm_mesh_cylinder_uv(int slices, gm_real radius, gm_real height, gm_mesh *m, gm_mesh_attrs *a);
void gm_mesh_free(gm_mesh *m);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_MESH_H */
