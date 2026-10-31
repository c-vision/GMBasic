/* gmb_graphics_tilemap.c — tilemap (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int gmb_gfx_tilemap_create(const char* tileset, int cell_w, int cell_h, int cols, int rows)
{
    gmb_gfx_init();
    if (!tileset || cols <= 0 || rows <= 0) return -1;
    for (int i = 0; i < 32; i++)
        if (!g_gfx.tilemaps[i].in_use)
        {
            int tex = gmb_gfx_load_texture_sheet(tileset, cell_w > 0 ? cell_w : 16, cell_h > 0 ? cell_h : 16);
            if (tex < 0) return -1;
            int id = g_gfx.next_sprite_id++;
            if (id >= GMB_GFX_MAX_SPRITES) return -1;
            gmb_gfx_sprite_t* s = &g_gfx.sprites[id];
            s->tex_id = tex;
            s->x = 0; s->y = 0;
            s->visible = 0; /* contenitore texture: SYNC non deve disegnare il tileset */
            s->priority = 0;
            s->in_use = 1;
            g_gfx.tilemaps[i].in_use = 1;
            g_gfx.tilemaps[i].sprite_handle = id;
            g_gfx.tilemaps[i].cols = cols;
            g_gfx.tilemaps[i].rows = rows;
            g_gfx.tilemaps[i].cell_w = cell_w > 0 ? cell_w : 16;
            g_gfx.tilemaps[i].cell_h = cell_h > 0 ? cell_h : 16;
            g_gfx.tilemaps[i].grid = (unsigned short*)calloc((size_t)cols * rows, sizeof(unsigned short));
            return id;
        }
    return -1;
}

int gmb_gfx_tilemap_from_texture(int texture_id, int cell_w, int cell_h, int cols, int rows)
{
    gmb_gfx_init();
    if (texture_id < 0 || cols <= 0 || rows <= 0) return -1;
    if (texture_id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[texture_id].in_use) return -1;
    for (int i = 0; i < 32; i++)
        if (!g_gfx.tilemaps[i].in_use)
        {
            int id = g_gfx.next_sprite_id++;
            gmb_gfx_sprite_t* s;
            if (id >= GMB_GFX_MAX_SPRITES) return -1;
            s = &g_gfx.sprites[id];
            s->tex_id = texture_id;
            s->x = 0; s->y = 0;
            s->visible = 0; /* contenitore texture: SYNC non deve disegnare il tileset */ s->priority = 0;
            s->rotation = 0; s->scale_x = 1; s->scale_y = 1;
            s->has_matrix = 0;
            s->in_use = 1;
            g_gfx.tilemaps[i].in_use = 1;
            g_gfx.tilemaps[i].sprite_handle = id;
            g_gfx.tilemaps[i].cols = cols;
            g_gfx.tilemaps[i].rows = rows;
            g_gfx.tilemaps[i].cell_w = cell_w > 0 ? cell_w : 16;
            g_gfx.tilemaps[i].cell_h = cell_h > 0 ? cell_h : 16;
            g_gfx.tilemaps[i].offset_x = 0;
            g_gfx.tilemaps[i].offset_y = 0;
            g_gfx.tilemaps[i].grid = (unsigned short*)calloc((size_t)cols * rows, sizeof(unsigned short));
            return id;
        }
    return -1;
}

void gmb_gfx_tilemap_set(int handle, int col, int row, int tile)
{
    for (int i = 0; i < 32; i++)
    {
        struct gmb_gfx_tilemap* t = &g_gfx.tilemaps[i];
        if (t->in_use && t->sprite_handle == handle)
        {
            if (col >= 0 && col < t->cols && row >= 0 && row < t->rows)
                t->grid[(size_t)row * t->cols + col] = (unsigned short)tile;
            return;
        }
    }
}

void gmb_gfx_tilemap_draw(int handle, int offset_x, int offset_y)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    for (int i = 0; i < 32; i++)
    {
        struct gmb_gfx_tilemap* t = &g_gfx.tilemaps[i];
        if (!t->in_use || t->sprite_handle != handle) continue;
        int ox = offset_x, oy = offset_y;
        if (offset_x == 0 && offset_y == 0 && (t->offset_x != 0 || t->offset_y != 0))
        { ox = t->offset_x; oy = t->offset_y; }
        gmb_gfx_image_t* ts = NULL;
        if (t->sprite_handle > 0 && t->sprite_handle < GMB_GFX_MAX_SPRITES && g_gfx.sprites[t->sprite_handle].tex_id > 0)
            ts = &g_gfx.textures[g_gfx.sprites[t->sprite_handle].tex_id];
        if (!ts || !ts->pixels) return;
        int per_row = ts->w / t->cell_w;
        if (per_row <= 0) per_row = 1;
        /* Coordinate/celle LOGICHE → NATIVE (renderScale): la pagina è nativa.
         * Senza, su Retina la mappa restava nel quarto in alto a sinistra. */
        float scf = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
        int cw = (int)lround((double)t->cell_w * scf), ch = (int)lround((double)t->cell_h * scf);
        if (cw < 1) cw = 1;
        if (ch < 1) ch = 1;
        ox = (int)lround((double)ox * scf);
        oy = (int)lround((double)oy * scf);
        for (int r = 0; r < t->rows; r++)
            for (int c = 0; c < t->cols; c++)
            {
                int tile = t->grid[(size_t)r * t->cols + c];
                int fx = (tile % per_row) * t->cell_w;
                int fy = (tile / per_row) * t->cell_h;
                int dx = ox + c * cw;
                int dy = oy + r * ch;
                for (int yy = 0; yy < ch; yy++)
                {
                    int sy = fy + yy * t->cell_h / ch;   /* nearest */
                    for (int xx = 0; xx < cw; xx++)
                    {
                        int sx = fx + xx * t->cell_w / cw;
                        uint32_t pix = ts->pixels[(size_t)sy * ts->w + sx];
                        if (((unsigned char)(pix >> 24)) < 8) continue;
                        put_pixel(p, dx + xx, dy + yy, pix & 0xFFFFFF);
                    }
                }
            }
        return;
    }
}

void gmb_gfx_tilemap_offset(int handle, int dx, int dy)
{
    for (int i = 0; i < 32; i++)
    {
        struct gmb_gfx_tilemap* t = &g_gfx.tilemaps[i];
        if (t->in_use && t->sprite_handle == handle) { t->offset_x = dx; t->offset_y = dy; return; }
    }
}

int gmb_gfx_tilemap_offset_x(int handle)
{
    for (int i = 0; i < 32; i++)
        if (g_gfx.tilemaps[i].in_use && g_gfx.tilemaps[i].sprite_handle == handle)
            return g_gfx.tilemaps[i].offset_x;
    return 0;
}

int gmb_gfx_tilemap_offset_y(int handle)
{
    for (int i = 0; i < 32; i++)
        if (g_gfx.tilemaps[i].in_use && g_gfx.tilemaps[i].sprite_handle == handle)
            return g_gfx.tilemaps[i].offset_y;
    return 0;
}

void gmb_gfx_tilemap_scroll(int handle, int dx, int dy)
{
    for (int i = 0; i < 32; i++)
    {
        struct gmb_gfx_tilemap* t = &g_gfx.tilemaps[i];
        if (t->in_use && t->sprite_handle == handle)
        {
            t->offset_x += dx;
            t->offset_y += dy;
            return;
        }
    }
}

/* ============================ API CANONICA (TILEMAP LOAD/SET/...) ============================
 * Il BASIC canonico non usa handle: `TILEMAP LOAD texId, tileW, tileH, tilesPerRow, mapW, mapH
 * [, tiles()]`, poi SET row,col,tile / OFFSET dx,dy / SCROLL dx,dy / DRAW senza handle.
 * Il runtime tiene la tilemap CORRENTE (una per volta, come il motore C#). */
static int g_cur_tilemap = -1;

int gmb_gfx_tilemap_load_tex(int tex_id, int cell_w, int cell_h, int per_row, int map_w, int map_h,
                             const double* tiles, int ntiles)
{
    (void)per_row;
    int h = gmb_gfx_tilemap_from_texture(tex_id, cell_w, cell_h, map_w, map_h);
    if (h <= 0) return h;
    g_cur_tilemap = h;
    if (tiles && ntiles > 0)
    {
        for (int i = 0; i < 32; i++)
        {
            struct gmb_gfx_tilemap* t = &g_gfx.tilemaps[i];
            if (!t->in_use || t->sprite_handle != h) continue;
            int n = t->cols * t->rows;
            if (ntiles < n) n = ntiles;
            for (int k = 0; k < n; k++) t->grid[k] = (unsigned short)(int)tiles[k];
            break;
        }
    }
    return h;
}

void gmb_gfx_tilemap_set_cur(int row, int col, int tile) { gmb_gfx_tilemap_set(g_cur_tilemap, col, row, tile); }
void gmb_gfx_tilemap_offset_cur(int dx, int dy) { gmb_gfx_tilemap_offset(g_cur_tilemap, dx, dy); }
void gmb_gfx_tilemap_scroll_cur(int dx, int dy) { gmb_gfx_tilemap_scroll(g_cur_tilemap, dx, dy); }
void gmb_gfx_tilemap_draw_cur(void) { gmb_gfx_tilemap_draw(g_cur_tilemap, 0, 0); }
