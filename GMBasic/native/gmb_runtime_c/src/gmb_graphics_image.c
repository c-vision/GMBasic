/* gmb_graphics_image.c — image/bload (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================ IMAGEGET / IMAGEPUT ============================ */

int gmb_gfx_image_get(int x1, int y1, int x2, int y2, int* out, int max)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || !out) return 0;
    /* Coordinate BASIC = LOGICHE → NATIVE (renderScale). La pagina è nativa: senza
     * questa conversione su Retina si catturava solo il quarto in alto a sinistra
     * (IMAGEGET 11x10 logico → 11x10 nativo = mezzo sprite). */
    { float sc = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
      if (sc != 1.0f) {
          x1 = (int)lround((double)x1 * sc);
          y1 = (int)lround((double)y1 * sc);
          x2 = (int)lround((double)(x2 + 1) * sc) - 1;
          y2 = (int)lround((double)(y2 + 1) * sc) - 1;
      } }
    int w = x2 - x1 + 1, h = y2 - y1 + 1;
    if (w <= 0 || h <= 0) return 0;
    if (max < 2 + w * h) return 0;
    out[0] = w;
    out[1] = h;
    /* Finestra reale: gli sprite sono composti dalla GPU e non sono nella pagina →
     * composizione offscreen pagina+sprite, così IMAGEGET li include. */
    uint32_t* cap = (uint32_t*)malloc((size_t)p->w * (size_t)p->h * sizeof(uint32_t));
    if (cap && gmb_gfx_present_capture(cap, p->w, p->h) != 0) { free(cap); cap = NULL; }
    const uint32_t* src = cap ? cap : p->pixels;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
        {
            int sx = x1 + xx, sy = y1 + yy;
            if (sx < 0 || sy < 0 || sx >= p->w || sy >= p->h) { out[2 + yy * w + xx] = 0; continue; }
            uint32_t px = src[(size_t)sy * p->w + sx];
            extern int gmb_gfx_find_color_index(uint32_t argb, int check_alpha);
            out[2 + yy * w + xx] = gmb_gfx_find_color_index(px, 0);
        }
    free(cap);
    return 2 + w * h;
}

void gmb_gfx_image_put(int x, int y, const int* img, int n)
{
    gmb_gfx_image_put_ex(x, y, img, n, "PSET");
}

void gmb_gfx_image_put_ex(int x, int y, const int* img, int n, const char* op)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || !img || n < 2) return;
    int w = img[0], h = img[1];
    if (w <= 0 || h <= 0 || n < 2 + w * h) return;
    gmb_gfx_map(&x, &y);
    /* Posizione LOGICA → NATIVA (lo sprite è già nativo, catturato con la stessa
     * scala): senza, su Retina gli sprite finivano nel quarto in alto a sinistra. */
    gmb_gfx_scale_pt(&x, &y);
    int mode = 0; // 0=PSET,1=AND,2=OR,3=XOR,4=PRESET
    if (op) {
        if (op[0]=='A' || op[0]=='a') mode = 1;
        else if (op[0]=='O' || op[0]=='o') mode = 2;
        else if (op[0]=='X' || op[0]=='x') mode = 3;
        else if (op[0]=='P' || op[0]=='p') {
            if (op[1]=='R' || op[1]=='r') mode = 4;
            else mode = 0;
        }
    }
    /* Fast path PSET: sprite interamente in-bounds e nessun viewport → scrittura diretta
       ai pixel senza i check per-pixel di put_pixel (benchmark sprite pesanti). */
    if (mode == 0 && !g_gfx.view_set && x >= 0 && y >= 0 && x + w <= p->w && y + h <= p->h)
    {
        size_t row = (size_t)p->w;
        for (int yy = 0; yy < h; yy++)
        {
            uint32_t* dst = p->pixels + (size_t)(y + yy) * row + x;
            const int* srcrow = img + 2 + yy * w;
            for (int xx = 0; xx < w; xx++)
            {
                int idx = srcrow[xx];
                if (idx >= 0)
                    dst[xx] = (idx >= g_gfx.palette_size) ? (uint32_t)(idx & 0xFFFFFF) : palette_rgb(idx);
            }
        }
        return;
    }
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
        {
            int idx = img[2 + yy * w + xx];
            if (idx < 0) continue;
            uint32_t src = (idx >= g_gfx.palette_size) ? (uint32_t)(idx & 0xFFFFFF) : palette_rgb(idx);
            if (mode == 0) {
                put_pixel(p, x + xx, y + yy, src);
            } else if (mode == 4) {
                // PRESET = invert (come QBASIC PSET vs PRESET)
                put_pixel(p, x + xx, y + yy, (~src) & 0xFFFFFF);
            } else {
                int px = x + xx, py = y + yy;
                if (px < 0 || py < 0 || px >= p->w || py >= p->h) continue;
                uint32_t dst = p->pixels[(size_t)py * p->w + px] & 0xFFFFFF;
                uint32_t out = src & 0xFFFFFF;
                if (mode == 1) out = dst & src;
                else if (mode == 2) out = dst | src;
                else if (mode == 3) out = dst ^ src;
                p->pixels[(size_t)py * p->w + px] = (p->pixels[(size_t)py * p->w + px] & 0xFF000000) | out;
            }
        }
}

/* IMAGEPUTS: UNO sprite (spriteData = [w,h,pixel...]) disegnato a OGNI coppia
 * (x,y) di positions (parità C# ImagePutsInternal: spriteData è un singolo
 * sprite, positions.Length/2 posizioni). Prima si avanzava di 2+w*h dentro
 * spriteData per ogni posizione: dopo il primo sprite w/h erano spazzatura →
 * un solo sprite disegnato (benchmark PutsVsTextures software). */
void gmb_gfx_image_puts_n(const int* sprites, int sprites_len, const float* positions, int count)
{
    if (!sprites || !positions || count <= 0 || sprites_len < 2) return;
    int w = sprites[0], h = sprites[1];
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return;
    if ((long)2 + (long)w * h > sprites_len) return;
    for (int i = 0; i < count; i++)
        gmb_gfx_image_put((int)positions[i * 2], (int)positions[i * 2 + 1], sprites, 2 + w * h);
}

/* Legacy (emitter C99Native): stessa semantica, con hardening anti-overrun. */
void gmb_gfx_image_puts(const int* sprites, const float* positions, int count)
{
    if (!sprites || !positions || count <= 0) return;
    int w = sprites[0], h = sprites[1];
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return;
    for (int i = 0; i < count; i++)
        gmb_gfx_image_put((int)positions[i * 2], (int)positions[i * 2 + 1], sprites, 2 + w * h);
}

/* Sprite pixel/collide spostato in gmb_graphics_sprite.c */

/* BSAVE: cattura un'area della pagina attiva (buffer BASIC ignorato dal riferimento). */
void gmb_gfx_bsave(int x1, int y1, int x2, int y2)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    /* Coordinate LOGICHE → NATIVE (renderScale), come IMAGEGET: la pagina è nativa. */
    { float s = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
      if (s != 1.0f) {
          x1 = (int)lround((double)x1 * s);
          y1 = (int)lround((double)y1 * s);
          x2 = (int)lround((double)(x2 + 1) * s) - 1;
          y2 = (int)lround((double)(y2 + 1) * s) - 1;
      } }
    int w = x2 - x1 + 1, h = y2 - y1 + 1;
    if (w <= 0 || h <= 0) return;
    if (x1 < 0) x1 = 0; if (y1 < 0) y1 = 0;
    if (x2 >= p->w) x2 = p->w - 1; if (y2 >= p->h) y2 = p->h - 1;
    w = x2 - x1 + 1; h = y2 - y1 + 1;
    free(g_gfx.bsave_pixels);
    g_gfx.bsave_pixels = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    g_gfx.bsave_w = w;
    g_gfx.bsave_h = h;
    if (!g_gfx.bsave_pixels) { g_gfx.bsave_w = g_gfx.bsave_h = 0; return; }
    /* Finestra reale: includi gli sprite GPU (composizione offscreen). */
    uint32_t* cap = (uint32_t*)malloc((size_t)p->w * (size_t)p->h * sizeof(uint32_t));
    if (cap && gmb_gfx_present_capture(cap, p->w, p->h) != 0) { free(cap); cap = NULL; }
    const uint32_t* src = cap ? cap : p->pixels;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            g_gfx.bsave_pixels[(size_t)yy * w + xx] = src[(size_t)(y1 + yy) * p->w + (x1 + xx)];
    free(cap);
}

/* BLOAD: ridisegna l'ultima area BSAVE a (x, y). */
void gmb_gfx_bload(int x, int y)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || !g_gfx.bsave_pixels) return;
    { float s = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
      if (s != 1.0f) { x = (int)lround((double)x * s); y = (int)lround((double)y * s); } }
    for (int yy = 0; yy < g_gfx.bsave_h; yy++)
        for (int xx = 0; xx < g_gfx.bsave_w; xx++)
            put_pixel(p, x + xx, y + yy, g_gfx.bsave_pixels[(size_t)yy * g_gfx.bsave_w + xx]);
}

/* collide spostato in gmb_graphics_sprite.c */

/* Chart spostato in gmb_graphics_chart.c per evitare monolite */

/* Tilemap spostato in gmb_graphics_tilemap.c per evitare monolite */

