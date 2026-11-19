/*
 * gm_gfx_sector.h — settori e portali (modello W3D), rendering software.
 *
 * Un settore è un poligono CONVESSO con quota di pavimento e di soffitto,
 * delimitato da pareti (segmenti 2D). Una parete con un settore dietro è un
 * portale: il raggio ci passa attraverso, la finestra di righe si restringe
 * alla sua apertura e il disegno prosegue nel settore dietro.
 *
 * Il renderer traccia UN raggio 2D per colonna di schermo (nessun BSP, nessun
 * ordinamento di pareti): trova la parete più vicina del settore corrente,
 * disegna pavimento e soffitto fino a lì, poi parete solida (la colonna
 * finisce) oppure fascia di parete attorno al varco + ricorsione nel portale.
 * È la semantica di docs/3D-Specifica-Comportamenti.md §2.2 (W-COLUMN, W-DEPTH,
 * W-SHEAR) più la luce di W-LIGHT (settore × lampade).
 *
 * Colore in word 0x00RRGGBB e depth float per pixel: il chiamante passa i due
 * buffer e la suite non alloca nulla per frame (decisione §11 del piano).
 *
 * Dipendenze: gm_core per i tipi, gm_gfx_raster solo per gm_tex (RGBA8).
 */
#ifndef GMMATH_GM_GFX_SECTOR_H
#define GMMATH_GM_GFX_SECTOR_H

#include "gm_core.h"
#include "gm_gfx_raster.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GM_SECTOR_MAX_PORTALS  64   /* portali attraversati da un raggio */
#define GM_SECTOR_MAX_LAMPS     8   /* lampade per mondo (W-LIGHT) */
#define GM_SECTOR_SKY           1   /* il soffitto è cielo, mappato per angolo */
#define GM_SECTOR_W_MOVE_BLOCK  2   /* la parete ferma il movimento (W-FLAGS) */
#define GM_SECTOR_W_TRANS      16   /* parete traslucida: media con ciò che sta dietro */

/* Lampada puntiforme: schiarisce, falloff lineare a zero su `radius` (W-LIGHT:
 * niente luci colorate, niente ombre). */
typedef struct gm_sector_lamp {
    gm_real x, y, z;
    gm_real radius, level;
} gm_sector_lamp;

/* Una parete va elencata in senso ANTIORARIO attorno al settore (vista
 * dall'alto): la texture scorre da p0 a p1. Un portale è elencato in entrambi
 * i settori con gli stessi estremi e il proprio `to`. `scroll_u`/`scroll_v`
 * sono l'offset della picture (SCROLL, unità nel mondo). */
typedef struct gm_sector_wall {
    gm_real x0, y0, x1, y1;   /* estremi nel mondo */
    int     to;               /* settore dietro: -1 = parete solida */
    int     tex;              /* texture della parete, -1 = colore pieno */
    int     flags;            /* GM_SECTOR_W_* */
    gm_real bottom, top;      /* quote della parete; per un portale: l'apertura */
    gm_real scroll_u, scroll_v;
} gm_sector_wall;

typedef struct gm_sector {
    gm_real floor_z, ceil_z;
    int    *walls;  int nwalls;   /* indici dentro gm_sector_world.walls */
    int     tex_floor, tex_ceil;  /* -1 = colore pieno */
    int     flags;                /* GM_SECTOR_SKY */
    gm_real light;                /* moltiplicatore 0..1 del settore (LIGHTFX) */
    gm_real scroll_x, scroll_y;   /* offset delle x,y del mondo sui piani (SCROLL) */
} gm_sector;

typedef struct gm_sector_world {
    gm_sector      *sectors;  int nsectors;
    gm_sector_wall *walls;    int nwalls;
    const gm_tex   *tex;      int ntex;    /* condivise; 1 unità = 1 texel */
    int             tex_sky;               /* -1 = gradiente */
    gm_real         fog;                   /* 0 = nessun oscuramento per distanza */
    unsigned int    fog_rgb;
    gm_sector_lamp  lamps[GM_SECTOR_MAX_LAMPS];
    int             nlamps;
} gm_sector_world;

typedef struct gm_sector_camera {
    gm_real x, y, z;   /* occhio nel mondo */
    gm_real angle;     /* radianti, 0 = +x */
    gm_real pitch;     /* radianti: shear dell'orizzonte, non tilt */
    gm_real fov;       /* radianti, orizzontale */
} gm_sector_camera;

/* Rende il mondo in `argb` (w*h, 0x00RRGGBB) e `depth` (w*h, distanza lungo
 * la direzione di vista; 0 = pixel non scritto). Nessuna allocazione.
 * Ritorna GM_OK oppure GM_ERR_ARG su mondo/camera/buffer incoerenti. */
gm_status gm_sector_render(const gm_sector_world *w, const gm_sector_camera *cam,
                           unsigned int *argb, float *depth, int w_px, int h_px);

/* Punto (x,y) dentro il poligono del settore `sec` (in pianta, la quota non
 * conta). È il test unico: lo usano camera, authoring e collisione. */
int gm_sector_contains(const gm_sector_world *w, int sec, gm_real x, gm_real y);

/* Settore che contiene (x,y) alla quota z; -1 se nessuno. */
int gm_sector_at(const gm_sector_world *w, gm_real x, gm_real y, gm_real z);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_SECTOR_H */
