/* gmb_3d_sector.c — binding del renderer di settori sul target 3D (P2).
 *
 * Qui sta solo il ponte: lo stato del mondo è in gmb_3d_io.c, la matematica e
 * la rasterizzazione in gm_gfx_sector (suite). Nessuna seconda copia del
 * renderer, nessuna allocazione per frame: il target è quello di P1.
 */
#include "gmb_3d_internal.h"

int gmb_3d_world_render(const gmb_3d_world_t* w, const gmb_3d_camera_t* cam,
                        gmb_3d_target_t* t)
{
    if (!w || !cam)
    {
        gmb_3d_set_error("world render: mondo o camera mancanti");
        return GMB_3D_E_ARG;
    }
    if (!t || !t->argb || !t->depth || t->w <= 0 || t->h <= 0)
    {
        gmb_3d_set_error("world render: target non allocato");
        return GMB_3D_E_ARG;
    }
    if (gmb_3d_present_mode() == GMB_3D_MODE_NULL)
    {
        gmb_3d_set_error("");   /* T-NULL: lo stato avanza, il disegno no */
        return GMB_3D_OK;
    }

    gm_sector_camera c;
    c.x = (gm_real)cam->x;   c.y = (gm_real)cam->y;   c.z = (gm_real)cam->z;
    c.angle = (gm_real)cam->angle;
    c.pitch = (gm_real)cam->pitch;
    c.fov = (gm_real)(cam->fov > 0.0f ? cam->fov : 1.5707964f);
    c.far = (gm_real)(cam->far > 0.0f ? cam->far : 4096.0f);

    if (gm_sector_render(&w->sw, &c, t->argb, t->depth, t->w, t->h) != GM_OK)
    {
        gmb_3d_set_error("world render: geometria non valida");
        return GMB_3D_E_ARG;
    }
    gmb_3d_set_error("");
    return GMB_3D_OK;
}
