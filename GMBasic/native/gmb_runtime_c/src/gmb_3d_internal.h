/* gmb_3d_internal.h — quanto i moduli 3D si dicono fra loro, mai fuori.
 *
 * gmb_3d.h espone il mondo come handle opaco: chi lo usa non vede né gm_* né
 * la forma dell'array di settori. Qui stanno la definizione, lo stato di tempo
 * per settore (T-*) e il setter dell'errore (l'ultimo errore vive in
 * gmb_3d_frame.c, una sola copia).
 */
#ifndef GMB_3D_INTERNAL_H
#define GMB_3D_INTERNAL_H

#include "gmb_3d.h"
#include "gm_gfx_sector.h"

/* Passo fisso di simulazione (T-FIXED): un'unità sola, uguale per il load e
 * per chi chiama TICK. */
#define GMB_3D_DEFAULT_STEP (1.0 / 60.0)

/* Stato di animazione di un settore: mover (SECMOVE), animazione (ANIM),
 * scroll (SCROLL) e luce dinamica (LIGHTFX). Il documento non lo contiene —
 * lo avvia il programma — quindi vive solo qui e si azzera al load. La luce
 * corrente invece sta nel settore (`light`): una sola copia, letta dal
 * renderer e da LIGHTNOW. */
typedef struct gmb_3d_sector_rt {
    int     moving;                  /* mover attivo */
    gm_real floor_to, ceil_to, speed;
    int     has_ceil;
    int     anim_first, anim_count, anim_ceil;   /* anim_ceil: il settore aveva un soffitto */
    gm_real anim_fps, anim_phase;
    int     scroll_planes, scroll_walls;
    gm_real scroll_u, scroll_v;
    int     fx_mode;                 /* 0 spento, 1 flicker, 2 pulse, 3 strobe */
    gm_real fx_amount, fx_rate, fx_phase, fx_value;
    unsigned int fx_rng;
} gmb_3d_sector_rt;

struct gmb_3d_world {
    gm_sector_world sw;   /* geometria, texture e lampade (`sw.tex`, `sw.ntex`) */
    gm_tex* tex;          /* buffer delle texture, posseduto dal mondo */
    gmb_3d_sector_rt* rt; /* uno per settore, posseduto dal mondo */
    gm_real step;         /* passo fisso (T-FIXED), default GMB_3D_DEFAULT_STEP */
    gm_real accum, time;
    unsigned long ticks;
};

/* Imposta gmb_3d_last_error(). Vuoto = errore azzerato. */
void gmb_3d_set_error(const char* msg);

#endif /* GMB_3D_INTERNAL_H */
