/* gmb_graphics_sprite.c — sprite (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================ Sprite ================================ */

int gmb_gfx_sprite_load(const char* path)
{
    gmb_gfx_init();
    /* Anche in null mode lo stato sprite e' software (PNG decodificato, nessuna SDL): serve a rilevare le
     * collisioni al SYNC (ON SPRITE COLLISION) con id e rettangoli veri, uguali a quelli della grafica reale. */
    if (!path) return -1;
    if (g_gfx.next_sprite_id >= GMB_GFX_MAX_SPRITES) return -1;
    int id = g_gfx.next_sprite_id++;
    g_gfx.sprites[id].tex_id = -1;
    g_gfx.sprites[id].x = 0;
    g_gfx.sprites[id].y = 0;
    g_gfx.sprites[id].visible = 1;
    g_gfx.sprites[id].priority = 0;
    g_gfx.sprites[id].rotation = 0;
    g_gfx.sprites[id].scale_x = 1;
    g_gfx.sprites[id].scale_y = 1;
    g_gfx.sprites[id].in_use = 1;
    int tex = gmb_gfx_load_texture(path);
    g_gfx.sprites[id].tex_id = tex;
    return id;
}

void gmb_gfx_sprite_set_position(int id, float x, float y)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    g_gfx.sprites[id].x = x;
    g_gfx.sprites[id].y = y;
}

void gmb_gfx_sprite_set_visible(int id, int visible)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    g_gfx.sprites[id].visible = visible;
}

void gmb_gfx_sprite_set_priority(int id, int priority)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    g_gfx.sprites[id].priority = priority;
}

void gmb_gfx_sprite_set_rotation(int id, double degrees)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    g_gfx.sprites[id].rotation = degrees;
}

void gmb_gfx_sprite_set_scale(int id, double sx, double sy)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    g_gfx.sprites[id].scale_x = sx;
    g_gfx.sprites[id].scale_y = sy;
}

void gmb_gfx_sprite_draw(int id)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    gmb_gfx_sprite_t* s = &g_gfx.sprites[id];
    if (!s->visible) return;
    if (s->tex_id < 0) return;
    if (s->has_matrix)
    {
        /* Matrice affine 2x3 (a,b,c,d,e,f): decomposizione in rotazione+scala
         * (draw_texture supporta rot/scale; la traslazione resta s->x/s->y). */
        double a = s->matrix[0], b = s->matrix[1], c = s->matrix[2], d = s->matrix[3];
        double sx = sqrt(a * a + b * b);
        double sy = sqrt(c * c + d * d);
        double rot = atan2(b, a) * 180.0 / 3.14159265358979323846;
        if (sx <= 0) sx = 1;
        if (sy <= 0) sy = 1;
        gmb_gfx_draw_texture(s->tex_id, (int)s->x, (int)s->y, rot, sx, sy);
        return;
    }
    gmb_gfx_draw_texture(s->tex_id, (int)s->x, (int)s->y, s->rotation, s->scale_x, s->scale_y);
}

int gmb_gfx_sprite_def(int id, int w, int h, const int* pixels)
{
    uint32_t* px;
    int tex;
    gmb_gfx_sprite_t* s;
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || w <= 0 || h <= 0 || !pixels) return -1;
    gmb_gfx_init();
    px = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    if (!px) return -1;
    for (int i = 0; i < w * h; i++)
    {
        /* Contratto IGraphicsService.SpriteDef: 0 = trasparente, altrimenti 0xRRGGBB opaco.
         * Formato texture del motore: 0xAARRGGBB (png_decode). */
        unsigned int rgb = (unsigned int)pixels[i] & 0xFFFFFFu;
        px[i] = (rgb == 0) ? 0u : (0xFF000000u | rgb);
    }
    tex = gmb_gfx_register_rgba(px, w, h);
    if (tex < 0) { free(px); return -1; }
    s = &g_gfx.sprites[id];
    memset(s, 0, sizeof(*s));
    s->tex_id = tex;
    s->visible = 1;
    s->scale_x = 1;
    s->scale_y = 1;
    s->in_use = 1;
    if (id >= g_gfx.next_sprite_id) g_gfx.next_sprite_id = id + 1;
    return id;
}

void gmb_gfx_sprite_set_matrix(int id, const double* m6)
{
    gmb_gfx_sprite_t* s;
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !m6 || !g_gfx.sprites[id].in_use) return;
    s = &g_gfx.sprites[id];
    for (int i = 0; i < 6; i++) s->matrix[i] = m6[i];
    s->has_matrix = 1;
}

int gmb_gfx_sprite_count(void)
{
    int n = 0;
    for (int i = 0; i < GMB_GFX_MAX_SPRITES; i++) if (g_gfx.sprites[i].in_use) n++;
    return n;
}

/* ============================ Sprite pixel / collisioni ============================ */

void gmb_gfx_sprite_get_pixels(int src_id, int dst_id)
{
    if (src_id <= 0 || src_id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[src_id].in_use) return;
    if (dst_id <= 0 || dst_id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[dst_id].in_use) return;
    int s = g_gfx.sprites[src_id].tex_id;
    int d = g_gfx.sprites[dst_id].tex_id;
    if (s <= 0 || s >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[s].in_use) return;
    if (d <= 0 || d >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[d].in_use) return;
    gmb_gfx_image_t* ds = &g_gfx.textures[s];
    gmb_gfx_image_t* dd = &g_gfx.textures[d];
    int w = ds->w < dd->w ? ds->w : dd->w;
    int h = ds->h < dd->h ? ds->h : dd->h;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            dd->pixels[(size_t)yy * dd->w + xx] = ds->pixels[(size_t)yy * ds->w + xx];
}

/* SPRITE GETPIXELS → array IMAGEGET [w, h, indici...]. Ritorna il numero di elementi. */
int gmb_gfx_sprite_pixels_to_array(int src_id, int* out, int max)
{
    if (gmb_gfx_null_mode()) {
        /* dummy VALIDO 1x1 bianco (parity NullGraphicsService.SpriteGetPixelsGpm) */
        (void)src_id;
        if (!out || max < 3) return 0;
        out[0] = 1; out[1] = 1; out[2] = 0x00FFFFFF;
        return 3;
    }
    if (src_id <= 0 || src_id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[src_id].in_use) return 0;
    int s = g_gfx.sprites[src_id].tex_id;
    if (s <= 0 || s >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[s].in_use) return 0;
    gmb_gfx_image_t* t = &g_gfx.textures[s];
    if (!t->pixels || max < 2 + t->w * t->h) return 0;
    out[0] = t->w;
    out[1] = t->h;
    {
        extern int gmb_gfx_find_color_index(uint32_t argb, int check_alpha);
        for (int yy = 0; yy < t->h; yy++)
            for (int xx = 0; xx < t->w; xx++)
            {
                /* Parità CommonGraphics.GetSpriteImagePixels: alpha<128 → -1 (trasparente),
                 * altrimenti indice palette più vicino (mai packed RGB → overflow SHORT). */
                uint32_t pix = t->pixels[(size_t)yy * t->w + xx];
                out[2 + yy * t->w + xx] = gmb_gfx_find_color_index(pix, 1);
            }
    }
    return 2 + t->w * t->h;
}

int gmb_gfx_collide(int id1, int id2)
{
    if (id1 <= 0 || id1 >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id1].in_use) return 0;
    if (id2 <= 0 || id2 >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id2].in_use) return 0;
    gmb_gfx_sprite_t* a = &g_gfx.sprites[id1];
    gmb_gfx_sprite_t* b = &g_gfx.sprites[id2];
    gmb_gfx_image_t *ta = NULL, *tb = NULL;
    if (a->tex_id > 0 && a->tex_id < GMB_GFX_MAX_TEXTURES && g_gfx.textures[a->tex_id].in_use) ta = &g_gfx.textures[a->tex_id];
    if (b->tex_id > 0 && b->tex_id < GMB_GFX_MAX_TEXTURES && g_gfx.textures[b->tex_id].in_use) tb = &g_gfx.textures[b->tex_id];
    if (!ta || !tb) return 0;
    float ax = a->x, ay = a->y, aw = (float)ta->w, ah = (float)ta->h;
    float bx = b->x, by = b->y, bw = (float)tb->w, bh = (float)tb->h;
    if (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by) return -1;
    return 0;
}
