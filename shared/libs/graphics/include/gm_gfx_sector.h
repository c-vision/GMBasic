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
 * finisce) oppure gradini + ricorsione nel portale. È la semantica di
 * docs/3D-Specifica-Comportamenti.md §2.2 (W-COLUMN, W-DEPTH, W-SHEAR).
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

#define GM_SECTOR_MAX_PORTALS 64   /* portali attraversati da un raggio */
#define GM_SECTOR_MAX_WALLS   64   /* pareti per settore */
#define GM_SECTOR_SKY         1    /* il soffitto è cielo, mappato per angolo */

/* Una parete va elencata in senso ANTIORARIO attorno al settore (vista
 * dall'alto): la texture scorre da p0 a p1. Un portale è elencato in entrambi
 * i settori con gli stessi estremi e il proprio `to`. */
typedef struct gm_sector_wall {
    gm_real x0, y0, x1, y1;   /* estremi nel mondo */
    int     to;               /* settore dietro: -1 = parete solida */
    int     tex;              /* texture della parete, -1 = colore pieno */
    gm_real bottom, top;      /* quote della parete; per un portale: l'apertura */
} gm_sector_wall;

typedef struct gm_sector {
    gm_real floor_z, ceil_z;
    int    *walls;  int nwalls;   /* indici dentro gm_sector_world.walls */
    int     tex_floor, tex_ceil;  /* -1 = colore pieno */
    int     flags;                /* GM_SECTOR_SKY */
} gm_sector;

typedef struct gm_sector_world {
    gm_sector      *sectors;  int nsectors;
    gm_sector_wall *walls;    int nwalls;
    const gm_tex   *tex;      int ntex;    /* condivise; 1 unità = 1 texel */
    int             tex_sky;               /* -1 = gradiente */
    gm_real         fog;                   /* 0 = nessun oscuramento per distanza */
    unsigned int    fog_rgb;
} gm_sector_world;

typedef struct gm_sector_camera {
    gm_real x, y, z;   /* occhio nel mondo */
    gm_real angle;     /* radianti, 0 = +x */
    gm_real pitch;     /* radianti: shear dell'orizzonte, non tilt */
    gm_real fov;       /* radianti, orizzontale */
    gm_real far;       /* oltre: colore di fondo */
} gm_sector_camera;

/* Rende il mondo in `argb` (w*h, 0x00RRGGBB) e `depth` (w*h, distanza lungo
 * la direzione di vista; 0 = pixel non scritto). Nessuna allocazione.
 * Ritorna GM_OK oppure GM_ERR_ARG su mondo/camera/buffer incoerenti. */
gm_status gm_sector_render(const gm_sector_world *w, const gm_sector_camera *cam,
                           unsigned int *argb, float *depth, int w_px, int h_px);

/* Settore che contiene (x,y) alla quota z; -1 se nessuno. Serve a camera,
 * authoring e collisione: non tocca il renderer. */
int gm_sector_at(const gm_sector_world *w, gm_real x, gm_real y, gm_real z);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_SECTOR_H */
