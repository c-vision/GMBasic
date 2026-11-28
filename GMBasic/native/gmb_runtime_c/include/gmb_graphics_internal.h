/* gmb_graphics_internal.h — condiviso tra i moduli split di gmb_graphics.c
 * Non esposto a BASIC: solo per i file interni del runtime.
 */
#ifndef GMB_GRAPHICS_INTERNAL_H
#define GMB_GRAPHICS_INTERNAL_H

#include "gmb_graphics.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define GMB_GFX_MAX_PAGES 8
#define GMB_GFX_MAX_CANVASES 32
#define GMB_GFX_MAX_PALETTE 256
#define GMB_GFX_MAX_TEXTURES 512
#define GMB_GFX_MAX_SPRITES 1024

typedef struct gmb_gfx_image {
    uint32_t* pixels;
    int w, h;
    int cell_w, cell_h;
    int frames;
    int in_use;
} gmb_gfx_image_t;

typedef struct gmb_gfx_sprite {
    int tex_id;
    float x, y;
    int visible;
    int priority;
    double rotation;
    double scale_x, scale_y;
    double matrix[6];   /* 2x3 (a,b,c,d,e,f); has_matrix=1 -> applicata in draw */
    int has_matrix;
    int in_use;
} gmb_gfx_sprite_t;

typedef struct gmb_gfx_page {
    uint32_t* pixels;
    int w, h;
    int in_use;
} gmb_gfx_page_t;

typedef struct gmb_gfx_ctx {
    int init;
    int screen_on;
    int mode;
    int width, height;
    int active_page;
    int visible_page;
    gmb_gfx_page_t pages[GMB_GFX_MAX_PAGES];
    /* Canvas offscreen (BASIC: CANVAS NEW/USE/...). Handle = indice+1 (1..32), 0 = schermo.
     * Il canvas attivo dirotta active_page(): le primitive disegnano lì, senza VIEW/WINDOW
     * né render-scale (coordinate 1:1), così il contenuto è riproducibile e salvabile in PNG. */
    gmb_gfx_page_t canvases[GMB_GFX_MAX_CANVASES];
    int canvas_active;
    unsigned char palette[GMB_GFX_MAX_PALETTE][3];
    int palette_size;
    int fg, bg;
    int cursor_x, cursor_y;
    int cursor_visible;   /* default 1 */
    int cursor_shape;     /* 0=block 1=underline 2=bar */
    int attribute;        /* attributi testo (1=bold 2=dim 4=underline 8=reverse) */
    int font_size;
    unsigned char* font_data;
    size_t font_len;
    gmb_gfx_image_t textures[GMB_GFX_MAX_TEXTURES];
    int next_tex_id;
    gmb_gfx_sprite_t sprites[GMB_GFX_MAX_SPRITES];
    int next_sprite_id;
    int vx1, vy1, vx2, vy2;
    int view_set;
    double wx1, wy1, wx2, wy2;
    int window_set;
    /* I chart vivono nel modulo autonomo `gmb_chart` (native/gmb_chart), tenuti per
     * handle dall'adapter (gmb_graphics_chart.c). Nessuna struttura chart in g_gfx. */
    struct gmb_gfx_tilemap { int in_use; int sprite_handle; int cols, rows; int cell_w, cell_h; int offset_x, offset_y; unsigned short* grid; } tilemaps[32];
    uint32_t* bsave_pixels;
    int bsave_w, bsave_h;
} gmb_gfx_ctx_t;

extern gmb_gfx_ctx_t g_gfx;
extern int g_font_loaded;
struct stbtt_fontinfo;
extern struct stbtt_fontinfo g_font_info;

uint32_t palette_rgb(int idx);
uint32_t resolve_color(int color);
void put_pixel(gmb_gfx_page_t* p, int x, int y, uint32_t c);
void gmb_gfx_map(int* x, int* y);
/* Coordinata LOGICA → NATIVA (renderScale): da usare nelle primitive dopo gmb_gfx_map,
 * perché le pagine sono allocate a logico*scale. */
void gmb_gfx_scale_pt(int* x, int* y);
/* Scala effettiva del target: 1.0 quando è attivo un canvas (coordinate 1:1). */
float gmb_gfx_scale_eff(void);
gmb_gfx_page_t* active_page(void);
unsigned char* read_whole_file(const char* path, size_t* len);
/* F3 multimodale (2026-09-11): codec PNG condiviso col dominio immagini (gmb_image.c).
 * Unica implementazione (§0bis): texture/screenshot e immagini BASIC usano le stesse. */
int png_decode(const unsigned char* data, size_t len, uint32_t** out_px, int* out_w, int* out_h);
/* Registra una texture RGBA gia' decodificata (proprieta' trasferita al runtime). */
int gmb_gfx_register_rgba(uint32_t* px, int w, int h);
int save_png_rgb24(const char* path, const uint32_t* px, int w, int h);

/* Rotazione/scala EFFETTIVE di uno sprite: matrice affine 2x3 decomposta (rot = atan2(b,a),
 * scale = |(a,b)|/|(c,d)|), altrimenti rotazione/scala esplicite. Unica sede della
 * decomposizione: disegno singolo, collisione per pixel e disposizione dei gruppi. */
void gmb_gfx_sprite_rot_scale(const gmb_gfx_sprite_t* s, double* rot, double* sx, double* sy);

/* ---- Gruppi di sprite (gmb_graphics_sprite_group.c) ----
 * La testa comanda il blocco; i membri sono disposti in griglia rigida (posizione,
 * rotazione, scala e matrice della testa) e le loro impostazioni si salvano alla creazione
 * per essere ripristinate allo scioglimento. */
int gmb_gfx_group_head_of(int id);      /* 0 = non in gruppo; == id se e' la testa */
int gmb_gfx_group_is_member(int id);
int gmb_gfx_group_size(int head, int* cols, int* rows);   /* 0 = non e' testa di gruppo */
int gmb_gfx_group_relayout(int head);
int gmb_gfx_group_draw_block(int head); /* 1 = ha disegnato il blocco, 0 = non e' testa */
int gmb_gfx_group_reset(void);

/* ---- Movimento a tick (gmb_graphics_sprite_motion.c) ---- */
void gmb_gfx_motion_cancel(int id);
void gmb_gfx_motion_reset(void);

#endif
