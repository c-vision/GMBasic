/* gmb_3d_scene_tex.c — texture procedurali della scena (SCENE TEXTURE celle).
 *
 * Perché un file a parte: `gmb_3d_cmd.c` sta sotto le 1200 righe (§0quater) e la
 * generazione della scacchiera non tocca nessuno stato della scena oltre alla
 * texture corrente, che si sostituisce con l'unica sede `gmb_3d_scene_set_texture`.
 */
#include "gmb_3d_internal.h"

#include <stdlib.h>

int64_t gmb_scene_checker(int64_t cells)
{
    if (cells < 1 || cells > 64)
    {
        gmb_3d_set_error("SCENE TEXTURE: celle deve essere tra 1 e 64");
        return GMB_3D_E_ARG;
    }
    const int cell = 8;
    int size = (int)cells * cell;
    unsigned char* buf = (unsigned char*)malloc((size_t)size * (size_t)size * 4u);
    if (buf == NULL) return GMB_3D_E_NOMEM;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
        {
            unsigned char v = (((x / cell) + (y / cell)) & 1) ? 255u : 96u;
            unsigned char* p = buf + ((size_t)y * (size_t)size + (size_t)x) * 4u;
            p[0] = v; p[1] = v; p[2] = v; p[3] = 255u;
        }
    gmb_3d_scene_set_texture(buf, size, size);
    gmb_3d_set_error("");
    return GMB_3D_OK;
}
