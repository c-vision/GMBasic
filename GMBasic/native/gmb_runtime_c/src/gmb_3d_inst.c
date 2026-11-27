/* gmb_3d_inst.c — instancing per materiale (P5, M-INST, piano §4.3).
 *
 *  - `SCENE INSTANCE o, src` dichiara che l'oggetto `o` (1..nobj, ordine di
 *    dichiarazione come SCENE.PICK) disegna la GEOMETRIA e il MATERIALE (flag,
 *    luce base) dell'oggetto `src`, con la trasformazione (posizione, yaw,
 *    pitch, roll) e la tinta di `o`: geometria e materiale si dichiarano UNA
 *    volta e valgono per N collocazioni.
 *
 * Perché un modulo a parte: la regola «quale geometria e quale materiale» deve
 * stare in UN solo posto, letto dal disegno; la facciata dei comandi
 * (`gmb_3d_cmd.c`) è al limite delle 1200 righe (§0quater).
 *
 * Un solo dato: `inst_src` (0 = nessuna istanza, altrimenti 1..nobj). Oltre al
 * campo c'è UNA sola funzione che lo legge (`inst_src_of`), e le due risposte
 * che servono al disegno — geometria e materiale — sono due indici 0-based:
 * niente accessor per flag e luce, niente copie del materiale.
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

/* Sorgente dell'istanza, 1-based; 0 = nessuna (o `oi` fuori scena).
 * L'invariante 1..nobj e «diversa da oi» la stabilisce gmb_scene_instance: chi
 * legge non la ricontrolla. */
static int inst_src_of(int oi)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    return (oi < 0 || oi >= g->nobj) ? 0 : g->obj[oi].inst_src;
}

int64_t gmb_scene_instance(int64_t o, int64_t src)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (o < 1 || o > g->nobj) return inst_err("SCENE INSTANCE: oggetto inesistente (1..nobj)");
    if (src < 1 || src > g->nobj) return inst_err("SCENE INSTANCE: sorgente inesistente (1..nobj)");

    /* L'ultima dichiarazione vince (è un setter, non una pista). */
    gmb_scene_obj_t* ob = &g->obj[o - 1];
    ob->inst_src = (src == o) ? 0 : (int)src;
    /* La geometria ha UNA sola sorgente: azzerare `lod_mesh` basta, `lod_dist`
     * si legge solo quando c'è una mesh (gmb_3d_lod_source). */
    ob->lod_mesh = 0;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_inst_geom(int oi, const float eye[3])
{
    int s = inst_src_of(oi);
    return s ? s - 1 : gmb_3d_lod_source(oi, eye);   /* un livello solo */
}

int gmb_3d_inst_mat(int oi)
{
    int s = inst_src_of(oi);
    return s ? s - 1 : oi;
}
