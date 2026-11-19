/* gmb_3d_internal.h — quanto i moduli 3D si dicono fra loro, mai fuori.
 *
 * gmb_3d.h espone il mondo come handle opaco: chi lo usa non vede né gm_* né
 * la forma dell'array di settori. Qui stanno la definizione e il setter
 * dell'errore (l'ultimo errore vive in gmb_3d_frame.c, una sola copia).
 */
#ifndef GMB_3D_INTERNAL_H
#define GMB_3D_INTERNAL_H

#include "gmb_3d.h"
#include "gm_gfx_sector.h"

struct gmb_3d_world {
    gm_sector_world sw;   /* geometria + texture (gm_tex* dentro `tex`) */
    gm_tex* tex;          /* buffer delle texture, posseduto dal mondo */
    int ntex;
};

/* Imposta gmb_3d_last_error(). Vuoto = errore azzerato. */
void gmb_3d_set_error(const char* msg);

#endif /* GMB_3D_INTERNAL_H */
