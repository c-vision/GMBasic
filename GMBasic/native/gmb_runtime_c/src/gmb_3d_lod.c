/* gmb_3d_lod.c — livello di dettaglio per oggetto (P5, M-LOD, piano §4.3).
 *
 *  - `SCENE LOD o, dist, mesh` dichiara il LOD dell'oggetto `o` (1..nobj,
 *    ordine di dichiarazione come SCENE.PICK): quando la distanza fra l'occhio
 *    della camera e l'origine dell'oggetto supera `dist` unità mondo, il frame
 *    disegna la GEOMETRIA dell'oggetto `mesh` al posto della sua, con la
 *    trasformazione (posizione/yaw/pitch/roll) e la tinta di `o`.
 *
 * Perché un modulo a parte: la regola «quale geometria» deve stare in UN solo
 * posto, letto dal disegno; la facciata dei comandi (`gmb_3d_cmd.c`) è già al
 * limite delle 1200 righe (§0quater) e non cresce.
 *
 * Scelte dichiarate (specifica M-LOD):
 *  - UN livello solo: la mesh sostitutiva non riapplica il proprio LOD, quindi
 *    non ci sono catene né cicli da inseguire;
 *  - `mesh` è un oggetto come gli altri e continua a disegnarsi per conto suo
 *    (tipicamente lo si tiene lontano, o fuori dal frustum);
 *  - il LOD è SOLO visivo: PICK, AABB mondo, quote, raggi e collisione restano
 *    quelli della geometria dichiarata di `o` (M-LOD-VIS) — così le golden e i
 *    raggi delle scene senza LOD non cambiano e un cambio di dettaglio non
 *    sposta il mondo sotto i piedi del giocatore;
 *  - nessuna allocazione: due campi nell'oggetto, letti dal frame.
 */
#include "gmb_3d_internal.h"

static int lod_err(const char* msg)
{
    gmb_3d_set_error(msg);
    return GMB_3D_E_ARG;
}

int64_t gmb_scene_lod(int64_t o, double dist, int64_t mesh)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (o < 1 || o > g->nobj) return lod_err("SCENE LOD: oggetto inesistente (1..nobj)");
    if (mesh < 1 || mesh > g->nobj) return lod_err("SCENE LOD: mesh inesistente (1..nobj)");
    if (!(dist >= 0.0) || dist > 1.0e9) return lod_err("SCENE LOD: distanza non valida (>= 0)");

    /* L'ultima dichiarazione vince (è un setter, non una pista). */
    gmb_scene_obj_t* ob = &g->obj[o - 1];
    ob->lod_mesh = (int)mesh;
    ob->lod_dist = (float)dist;
    /* La geometria ha UNA sola sorgente: LOD sostituisce l'eventuale istanza. */
    ob->inst_src = 0;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_lod_source(int oi, const float eye[3])
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (oi < 0 || oi >= g->nobj) return oi;

    const gmb_scene_obj_t* o = &g->obj[oi];
    /* 0 = nessun LOD; `mesh == o` è un no-op dichiarato (niente sostituzione). */
    if (o->lod_mesh < 1 || o->lod_mesh > g->nobj || o->lod_mesh - 1 == oi) return oi;

    const float dx = o->x - eye[0], dy = o->y - eye[1], dz = o->z - eye[2];
    const float d2 = dx * dx + dy * dy + dz * dz;
    /* «Oltre» è stretto: alla distanza esatta vale ancora la geometria piena. */
    if (d2 <= o->lod_dist * o->lod_dist) return oi;
    return o->lod_mesh - 1;   /* un livello solo: la mesh non riapplica il suo LOD */
}
