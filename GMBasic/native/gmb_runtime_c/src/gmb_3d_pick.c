/* gmb_3d_pick.c — SCENE PICK: buffer di ID per pixel, ADDITIVO e spento di default.
 *
 * Il colore e la profondita' del target restano quelli di sempre: il buffer di id
 * e' un terzo array, allocato SOLO quando il programma lo accende (`SCENE PICK on`).
 * Con il pick spento non si alloca e non si scrive niente, quindi le scene che non
 * lo usano restano byte per byte identiche (stessa regola di SCENE AA).
 *
 * Chi scrive: il rasterizzatore (`gmb_3d_frame.c`), dopo che il pixel ha vinto il
 * depth test, con l'id del materiale (l'ordine di dichiarazione degli OBJECT,
 * 1..256: 0 e' riservato al fondo). Chi legge: `gmb_scene_pick(x, y)`.
 *
 * Perche' un file a parte: gmb_3d_cmd.c e' gia' vicino al limite di 1200 righe e
 * il gate `tools/sh/check_file_size.sh` vale anche per i domini. Lo STATO resta uno
 * solo: la scena vive in gmb_3d_cmd.c, qui si tiene solo il flag e si passa il
 * target con `gmb_scene_target_ref()` (interno, non esportato nell'API).
 */
#include "gmb_3d_internal.h"

/* 0 = spento (default). Sopravvive a RESET, come gli altri flag di scena. */
static int g_pick_on = 0;

int gmb_scene_pick_active(void)
{
    return g_pick_on;
}

/* Chiamata da VIEW e RENDER: se il pick e' acceso, il buffer di id deve esistere
 * ed essere della dimensione del target. Idempotente e senza allocazioni quando
 * la dimensione non cambia (gmb_3d_target_ids riusa). */
int gmb_scene_pick_attach(gmb_3d_target_t* t)
{
    if (!g_pick_on || t == NULL || t->argb == NULL) return GMB_3D_OK;
    return gmb_3d_target_ids(t, 1);
}

int64_t gmb_scene_pick_enable(int64_t on)
{
    gmb_3d_target_t* t = gmb_scene_target_ref();
    g_pick_on = (on != 0);
    /* Buffer non allocato (manca VIEW) = si ricorda solo il flag: VIEW/RENDER lo
     * allocano appena c'e' un target. */
    if (t->argb == NULL) { gmb_3d_set_error(""); return GMB_3D_OK; }

    int rc = gmb_3d_target_ids(t, g_pick_on);
    if (rc != GMB_3D_OK)
    {
        gmb_3d_set_error("SCENE PICK: buffer di id non allocabile");
        return rc;
    }
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_pick(int64_t x, int64_t y)
{
    const gmb_3d_target_t* t = gmb_scene_target_ref();
    /* Senza target non esiste nemmeno il buffer, e la sentinella e' quella di
     * SCENE.PIXEL: -1 = assente / fuori, in tutto il dominio. Il -2 e' riservato
     * al pick SPENTO su un target che esiste, cosi' i due errori di scrittura si
     * distinguono: "manca SCENE PICK on" (-2) da "manca SCENE VIEW" (-1). */
    if (t->argb == NULL) return -1;                    /* nessun target */
    if (t->id == NULL) return -2;                      /* buffer spento */
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return -1;
    return (int64_t)t->id[(size_t)y * (size_t)t->w + (size_t)x];
}
