/* gmb_3d_inst.c — instancing per materiale (P5, M-INST, piano §4.3).
 *
 *  - `SCENE INSTANCE o, src` dichiara che l'oggetto `o` (1..nobj, ordine di
 *    dichiarazione come SCENE.PICK) disegna la GEOMETRIA e il MATERIALE (flag
 *    1/2/16, luce base) dell'oggetto `src`, con la trasformazione (posizione,
 *    yaw, pitch, roll) e la tinta di `o`: geometria e materiale si dichiarano
 *    UNA volta e valgono per N collocazioni.
 *
 * Perché un modulo a parte: la regola «quale geometria e quale materiale» deve
 * stare in UN solo posto, letto dal disegno; la facciata dei comandi
 * (`gmb_3d_cmd.c`) è già al limite delle 1200 righe (§0quater).
 *
 * Scelte dichiarate (specifica M-INST):
 *  - UN livello solo: la sorgente non riapplica la propria istanza né il proprio
 *    LOD, quindi non ci sono catene e nessun ciclo da inseguire;
 *  - `INSTANCE` e `LOD` si contendono la stessa scelta («quale geometria»):
 *    l'ultima dichiarazione vince e azzera l'altra;
 *  - `src == o` riporta l'oggetto alla propria geometria, come `LOD o, d, o`;
 *  - è SOLO una scelta di disegno: PICK (identità di `o`), AABB mondo, quote,
 *    raggi e collisione restano quelli della geometria DICHIARATA di `o`;
 *  - il culling del frustum guarda la geometria disegnata (come il LOD);
 *  - nessuna allocazione: un intero nell'oggetto, letto dal frame.
 */
#include "gmb_3d_internal.h"

static int inst_err(const char* msg)
{
    gmb_3d_set_error(msg);
    return GMB_3D_E_ARG;
}

/* Indice (0-based) della sorgente dichiarata, -1 se l'oggetto non è un'istanza.
 * Fuori intervallo (impossibile: la dichiarazione valida) vale come «se stessa». */
static int inst_source(int oi, const gmb_3d_scene_geom_t* g)
{
    if (oi < 0 || oi >= g->nobj) return -1;
    int s = g->obj[oi].inst_src;
    if (s < 1 || s > g->nobj || s - 1 == oi) return -1;
    return s - 1;
}

int64_t gmb_scene_instance(int64_t o, int64_t src)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (o < 1 || o > g->nobj) return inst_err("SCENE INSTANCE: oggetto inesistente (1..nobj)");
    if (src < 1 || src > g->nobj) return inst_err("SCENE INSTANCE: sorgente inesistente (1..nobj)");

    /* L'ultima dichiarazione vince (è un setter, non una pista). */
    gmb_scene_obj_t* ob = &g->obj[o - 1];
    ob->inst_src = (src == o) ? 0 : (int)src;
    /* La geometria ha UNA sola sorgente: INSTANCE sostituisce l'eventuale LOD. */
    ob->lod_mesh = 0;
    ob->lod_dist = 0.0f;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_inst_geom(int oi, const float eye[3])
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    int s = inst_source(oi, g);
    if (s >= 0) return s;                 /* un livello solo: la sorgente è sé stessa */
    return gmb_3d_lod_source(oi, eye);
}

unsigned char gmb_3d_inst_flags(int oi)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (oi < 0 || oi >= g->nobj) return 0;
    int s = inst_source(oi, g);
    return s >= 0 ? g->obj[s].flags : g->obj[oi].flags;
}

unsigned char gmb_3d_inst_light(int oi)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (oi < 0 || oi >= g->nobj) return 255;
    int s = inst_source(oi, g);
    return s >= 0 ? g->obj[s].light : g->obj[oi].light;
}
