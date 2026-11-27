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

/* Nessuna influenza: due ossi «assenti» e pesi a zero. */
static void skin_blank(gmb_3d_skin_t* s, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        s[i].b0 = GMB_SKIN_NONE; s[i].b1 = GMB_SKIN_NONE;
        s[i].w0 = 0;             s[i].w1 = 0;
    }
}

/* I buffer del skinning, quando la scena usa WEIGHT (M-SKIN): prima erano
 * assenti, quindi la scena non pagava 16 byte per vertice. */
int gmb_3d_scene_skin_touch(void)
{
    if (g_geom.skin != NULL) return GMB_3D_OK;
    if (g_geom.cap_v <= 0 && gmb_3d_scene_reserve_verts(GMB_SCENE_VERTS_INIT) != GMB_3D_OK)
        return GMB_3D_E_ARG;
    size_t c = (size_t)g_geom.cap_v;
    gmb_3d_skin_t* skin = (gmb_3d_skin_t*)malloc(c * sizeof(gmb_3d_skin_t));
    float* sk_pos = (float*)malloc(c * 3 * sizeof(float));
    if (skin == NULL || sk_pos == NULL)
    {
        free(skin); free(sk_pos);
        return buf_fail();
    }
    skin_blank(skin, c);
    g_geom.skin = skin;
    g_geom.sk_pos = sk_pos;
    gmb_3d_note_alloc(2);   /* pesi e posizioni skinnate */
    return GMB_3D_OK;
}

void gmb_3d_scene_skin_clear(int nvert)
{
    if (g_geom.skin == NULL) return;
    if (nvert > g_geom.nv) nvert = g_geom.nv;
    if (nvert > 0) skin_blank(g_geom.skin, (size_t)nvert);
    /* Senza pesi nessun oggetto è più skinnato: la scena torna al percorso di
     * sempre (stessi byte, nessuno scratch letto). */
    for (int i = 0; i < g_geom.nobj; i++) g_geom.obj[i].has_skin = 0;
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
    /* M-SKIN: se la scena usa il skinning, i due buffer crescono con i vertici
     * (una scena senza WEIGHT non li ha e non li alloca). */
    const int want_skin = (g_geom.skin != NULL);
    gmb_3d_skin_t* skin = want_skin ? (gmb_3d_skin_t*)malloc(c * sizeof(gmb_3d_skin_t)) : NULL;
    float* sk_pos = want_skin ? (float*)malloc(c * 3 * sizeof(float)) : NULL;
    if (pos == NULL || uv == NULL || nrm == NULL || nrmw == NULL
        || (want_skin && (skin == NULL || sk_pos == NULL)))
    {
        free(pos); free(uv); free(nrm); free(nrmw); free(skin); free(sk_pos);
        return buf_fail();
    }
    if (used > 0)
    {
        memcpy(pos,  g_geom.pos,   used * 3 * sizeof(float));
        memcpy(uv,   g_geom.uv,    used * 2 * sizeof(float));
        memcpy(nrm,  g_geom.nrm,   used * 3 * sizeof(float));
        memcpy(nrmw, g_geom.nrm_w, used * 3 * sizeof(float));
        if (want_skin) memcpy(skin, g_geom.skin, used * sizeof(gmb_3d_skin_t));
    }
    if (want_skin) skin_blank(skin + used, c - used);   /* coda nuova: nessuna influenza */
    free(g_geom.pos); free(g_geom.uv); free(g_geom.nrm); free(g_geom.nrm_w);
    free(g_geom.skin); free(g_geom.sk_pos);
    g_geom.pos = pos; g_geom.uv = uv; g_geom.nrm = nrm; g_geom.nrm_w = nrmw;
    g_geom.skin = skin; g_geom.sk_pos = sk_pos;
    g_geom.cap_v = cap;
    gmb_3d_note_alloc(want_skin ? 6u : 4u);   /* i buffer dei vertici (più i due del skin) */
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

/* AABB MONDO dell'oggetto `oi`: gli 8 angoli dell'AABB locale, trasformati.
 * Con solo yaw la rotazione è chiusa (il volume non cresce); con pitch/roll
 * delle piste KEY si passa dal modello TRS completo. Il percorso a sola yaw
 * resta quello di sempre: stessi bit, quindi culling e golden invariati. */
static void obj_aabb_world(const gmb_scene_obj_t* o, gmb_3d_aabb_t* b)
{
    float m[16];
    int trs = (o->pitch != 0.0f || o->roll != 0.0f);
    float a = o->yaw * (float)(SC_BUF_PI / 180.0), c = cosf(a), s = sinf(a);
    if (trs) gmb_3d_m4_model_trs(m, o->x, o->y, o->z, o->yaw, o->pitch, o->roll);
    b->min[0] = b->min[1] = b->min[2] = 3.0e38f;
    b->max[0] = b->max[1] = b->max[2] = -3.0e38f;
    for (int k = 0; k < 8; k++)
    {
        float lx = (k & 1) ? o->bmax[0] : o->bmin[0];
        float ly = (k & 2) ? o->bmax[1] : o->bmin[1];
        float lz = (k & 4) ? o->bmax[2] : o->bmin[2];
        float wx, wy, wz;
        if (trs)
        {
            wx = m[0] * lx + m[4] * ly + m[8] * lz + m[12];
            wy = m[1] * lx + m[5] * ly + m[9] * lz + m[13];
            wz = m[2] * lx + m[6] * ly + m[10] * lz + m[14];
        }
        else
        {
            wx = o->x + c * lx + s * lz;
            wy = o->y + ly;
            wz = o->z - s * lx + c * lz;
        }
        if (wx < b->min[0]) b->min[0] = wx;
        if (wx > b->max[0]) b->max[0] = wx;
        if (wy < b->min[1]) b->min[1] = wy;
        if (wy > b->max[1]) b->max[1] = wy;
        if (wz < b->min[2]) b->min[2] = wz;
        if (wz > b->max[2]) b->max[2] = wz;
    }
    b->flags = o->flags;
}

/* Memoria riservata dalla geometria, in byte: 11 float per vertice (posizione,
 * UV, normale, normale mondo), 3 interi + 2 byte per triangolo, un oggetto e i
 * tre scratch dimensionati sugli oggetti (ordine, distanza, AABB mondo). La
 * formula sta qui perché qui stanno i passi: chi aggiunge un buffer lo vede qui.
 * `SCENE.STATS("bytes")` la legge e basta. */
double gmb_3d_scene_bytes(void)
{
    size_t v = (size_t)g_geom.cap_v * 11u * sizeof(float);
    /* M-SKIN: solo se la scena ha pesi (due buffer, 4 + 12 byte per vertice). */
    if (g_geom.skin != NULL)
        v += (size_t)g_geom.cap_v * (sizeof(gmb_3d_skin_t) + 3u * sizeof(float));
    size_t t = (size_t)g_geom.cap_t * (3u * sizeof(int) + 2u);
    size_t o = (size_t)g_geom.cap_o * (sizeof(gmb_scene_obj_t) + sizeof(int) + sizeof(float)
                                       + sizeof(gmb_3d_aabb_t));
    return (double)(v + t + o);
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
        /* Id come SCENE.PICK: gli oggetti sono 1..nobj nell'ordine di
         * dichiarazione, quindi i raggi possono riportarli a BASIC (M-RAYS). */
        g_geom.bounds[n].id = oi + 1;
        n++;
    }
    if (count) *count = n;
    return g_geom.bounds;
}
