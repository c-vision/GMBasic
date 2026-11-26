/* gmb_3d_scene_buf.c — buffer della scena a capacità DINAMICA (P5).
 *
 * Perché esiste: i 200k triangoli dell'accettazione P5 non stanno nei limiti
 * statici del primo blocco (32768 vertici / 16384 triangoli / 512 oggetti).
 * Qui vivono i buffer e la loro crescita; la facciata dei comandi
 * (gmb_3d_cmd.c) li usa attraverso `gmb_3d_scene_geom()`.
 *
 * Contratto (piano §6 P5):
 *  - la crescita è GEOMETRICA (raddoppio) e avviene solo alla DICHIARAZIONE
 *    della geometria (`VERTEX`, `QUAD`, `BOX`, `OBJECT`), mai nel frame:
 *    `RENDER` legge solo i buffer già riservati — scratch dei traslucidi e AABB
 *    mondo compresi — quindi non alloca nulla per frame.
 *  - ogni crescita è tutto-o-niente: si allocano i nuovi buffer, si copia il
 *    contenuto valido e solo allora si liberano i vecchi. Se una malloc
 *    fallisce la scena resta esattamente com'era e l'errore è esplicito.
 *  - `gmb_3d_scene_geom_clear()` azzera i contatori e NON libera: la capacità
 *    acquisita si riusa, quindi un `SCENE RESET` non alloca.
 *  - i tetti (4.194.304 vertici, 2.097.152 triangoli, 65.536 oggetti) esistono
 *    solo per non esaurire la memoria: oltre, errore esplicito.
 *
 * Convenzioni: y = altezza, piano (x, z) (specifica `C-DIVERGE-AXES`).
 */
#include "gmb_3d_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SC_BUF_PI 3.14159265358979

static gmb_3d_scene_geom_t g_geom;

gmb_3d_scene_geom_t* gmb_3d_scene_geom(void) { return &g_geom; }

void gmb_3d_scene_geom_clear(void)
{
    g_geom.nv = g_geom.nt = g_geom.nobj = 0;
    g_geom.cur = -1;
}

static int buf_fail(void)
{
    gmb_3d_set_error("SCENE: memoria esaurita per la geometria della scena");
    return GMB_3D_E_ARG;
}

/* Prossima capacità: parte dal minimo storico, raddoppia finché basta, non
 * supera il tetto (che il chiamante ha già verificato). */
static int buf_next_cap(int cap, int init, int need, int max)
{
    if (cap <= 0) cap = init;
    while (cap < need) cap *= 2;
    if (cap > max) cap = max;
    return cap;
}

int gmb_3d_scene_reserve_verts(int need)
{
    if (need <= g_geom.cap_v) return GMB_3D_OK;
    if (need > GMB_SCENE_VERTS_MAX)
    {
        gmb_3d_set_error("SCENE VERTEX: massimo 4194304 vertici per scena");
        return GMB_3D_E_ARG;
    }
    int cap = buf_next_cap(g_geom.cap_v, GMB_SCENE_VERTS_INIT, need, GMB_SCENE_VERTS_MAX);
    size_t used = (size_t)g_geom.nv, c = (size_t)cap;
    float* pos  = (float*)malloc(c * 3 * sizeof(float));
    float* uv   = (float*)malloc(c * 2 * sizeof(float));
    float* nrm  = (float*)malloc(c * 3 * sizeof(float));
    float* nrmw = (float*)malloc(c * 3 * sizeof(float));
    if (pos == NULL || uv == NULL || nrm == NULL || nrmw == NULL)
    {
        free(pos); free(uv); free(nrm); free(nrmw);
        return buf_fail();
    }
    if (used > 0)
    {
        memcpy(pos,  g_geom.pos,   used * 3 * sizeof(float));
        memcpy(uv,   g_geom.uv,    used * 2 * sizeof(float));
        memcpy(nrm,  g_geom.nrm,   used * 3 * sizeof(float));
        memcpy(nrmw, g_geom.nrm_w, used * 3 * sizeof(float));
    }
    free(g_geom.pos); free(g_geom.uv); free(g_geom.nrm); free(g_geom.nrm_w);
    g_geom.pos = pos; g_geom.uv = uv; g_geom.nrm = nrm; g_geom.nrm_w = nrmw;
    g_geom.cap_v = cap;
    gmb_3d_note_alloc(4);   /* i quattro buffer dei vertici */
    return GMB_3D_OK;
}

int gmb_3d_scene_reserve_tris(int need)
{
    if (need <= g_geom.cap_t) return GMB_3D_OK;
    if (need > GMB_SCENE_TRIS_MAX)
    {
        gmb_3d_set_error("SCENE QUAD: massimo 2097152 triangoli per scena");
        return GMB_3D_E_ARG;
    }
    int cap = buf_next_cap(g_geom.cap_t, GMB_SCENE_TRIS_INIT, need, GMB_SCENE_TRIS_MAX);
    size_t used = (size_t)g_geom.nt, c = (size_t)cap;
    int* idx = (int*)malloc(c * 3 * sizeof(int));
    unsigned char* light = (unsigned char*)malloc(c * sizeof(unsigned char));
    unsigned char* lit = (unsigned char*)malloc(c * sizeof(unsigned char));
    if (idx == NULL || light == NULL || lit == NULL)
    {
        free(idx); free(light); free(lit);
        return buf_fail();
    }
    if (used > 0)
    {
        memcpy(idx,   g_geom.idx,    used * 3 * sizeof(int));
        memcpy(light, g_geom.light,  used * sizeof(unsigned char));
        memcpy(lit,   g_geom.lit_tri, used * sizeof(unsigned char));
    }
    free(g_geom.idx); free(g_geom.light); free(g_geom.lit_tri);
    g_geom.idx = idx; g_geom.light = light; g_geom.lit_tri = lit;
    g_geom.cap_t = cap;
    gmb_3d_note_alloc(3);   /* indici, luce dichiarata, luce calcolata */
    return GMB_3D_OK;
}

/* Con gli oggetti crescono anche i due scratch del RENDER (ordine e distanza
 * dei traslucidi) e la raccolta degli AABB mondo: così il frame trova sempre
 * spazio già riservato, qualunque sia il numero di oggetti. */
int gmb_3d_scene_reserve_objs(int need)
{
    if (need <= g_geom.cap_o) return GMB_3D_OK;
    if (need > GMB_SCENE_OBJS_MAX)
    {
        gmb_3d_set_error("SCENE OBJECT: massimo 65536 oggetti per scena");
        return GMB_3D_E_ARG;
    }
    int cap = buf_next_cap(g_geom.cap_o, GMB_SCENE_OBJS_INIT, need, GMB_SCENE_OBJS_MAX);
    size_t used = (size_t)g_geom.nobj, c = (size_t)cap;
    gmb_scene_obj_t* obj = (gmb_scene_obj_t*)malloc(c * sizeof(gmb_scene_obj_t));
    int* order = (int*)malloc(c * sizeof(int));
    float* dist = (float*)malloc(c * sizeof(float));
    gmb_3d_aabb_t* bounds = (gmb_3d_aabb_t*)malloc(c * sizeof(gmb_3d_aabb_t));
    if (obj == NULL || order == NULL || dist == NULL || bounds == NULL)
    {
        free(obj); free(order); free(dist); free(bounds);
        return buf_fail();
    }
    if (used > 0)
        memcpy(obj, g_geom.obj, used * sizeof(gmb_scene_obj_t));
    free(g_geom.obj); free(g_geom.order); free(g_geom.dist); free(g_geom.bounds);
    g_geom.obj = obj; g_geom.order = order; g_geom.dist = dist; g_geom.bounds = bounds;
    g_geom.cap_o = cap;
    gmb_3d_note_alloc(4);   /* oggetti, ordine, distanza, AABB mondo */
    return GMB_3D_OK;
}

/* AABB MONDO dell'oggetto `oi`: il modello è traslazione + rotazione su y,
 * quindi bastano gli 8 angoli (la rotazione non produce un volume più grande). */
static void obj_aabb_world(const gmb_scene_obj_t* o, gmb_3d_aabb_t* b)
{
    float a = o->yaw * (float)(SC_BUF_PI / 180.0), c = cosf(a), s = sinf(a);
    b->min[0] = b->min[1] = b->min[2] = 3.0e38f;
    b->max[0] = b->max[1] = b->max[2] = -3.0e38f;
    for (int k = 0; k < 8; k++)
    {
        float lx = (k & 1) ? o->bmax[0] : o->bmin[0];
        float ly = (k & 2) ? o->bmax[1] : o->bmin[1];
        float lz = (k & 4) ? o->bmax[2] : o->bmin[2];
        float wx = o->x + c * lx + s * lz;
        float wy = o->y + ly;
        float wz = o->z - s * lx + c * lz;
        if (wx < b->min[0]) b->min[0] = wx;
        if (wx > b->max[0]) b->max[0] = wx;
        if (wy < b->min[1]) b->min[1] = wy;
        if (wy > b->max[1]) b->max[1] = wy;
        if (wz < b->min[2]) b->min[2] = wz;
        if (wz > b->max[2]) b->max[2] = wz;
    }
    b->flags = o->flags;
}

/* Gli AABB mondo di tutti gli oggetti con geometria, in un buffer PERSISTENTE
 * (capacità = oggetti riservati): il corpo e le quote li leggono senza allocare. */
const gmb_3d_aabb_t* gmb_3d_scene_bounds_all(int* count)
{
    if (count) *count = 0;
    if (g_geom.bounds == NULL) return NULL;
    int n = 0;
    for (int oi = 0; oi < g_geom.nobj; oi++)
    {
        const gmb_scene_obj_t* o = &g_geom.obj[oi];
        if (o->nvert <= 0) continue;
        obj_aabb_world(o, &g_geom.bounds[n]);
        n++;
    }
    if (count) *count = n;
    return g_geom.bounds;
}
