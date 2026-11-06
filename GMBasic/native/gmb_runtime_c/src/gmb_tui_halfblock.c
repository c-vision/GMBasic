/* ============================================================================
 * gmb_tui_halfblock.c — gmb_tui_halfblock. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== HalfBlock ===================== */

void gmb_half_block_canvas_init(gmb_half_block_canvas_t* c, int width, int height, uint32_t fill)
{
    memset(c, 0, sizeof(*c));
    c->width = width;
    c->height = height;
    c->cells = (gmb_half_cell_t*)calloc((size_t)(width * height), sizeof(gmb_half_cell_t));
    gmb_half_block_canvas_clear_char(c, fill);
}
void gmb_half_block_canvas_free(gmb_half_block_canvas_t* c) { free(c->cells); c->cells = NULL; }
void gmb_half_block_canvas_clear_char(gmb_half_block_canvas_t* c, uint32_t fill)
{
    for (int i = 0; i < c->width * c->height; i++)
    {
        memset(&c->cells[i], 0, sizeof(gmb_half_cell_t));
        c->cells[i].ch = fill;
    }
}
void gmb_half_block_canvas_set(gmb_half_block_canvas_t* c, int x, int y, gmb_cell_color_t color)
{
    if (x < 0 || x >= c->width) return;
    if (y < 0 || y >= c->height * 2) return;
    int pos = x + c->width * (y / 2);
    if (pos < 0 || pos >= c->width * c->height) return;
    c->cells[pos].ch = 0;
    c->cells[pos].has_char = false;
    if (y % 2 == 0)
    {
        c->cells[pos].has_top = true;
        c->cells[pos].top_color = color;
    }
    else
    {
        c->cells[pos].has_bottom = true;
        c->cells[pos].bottom_color = color;
    }
}
void gmb_half_block_canvas_print(gmb_half_block_canvas_t* c, int x, int pixel_y, const char* text, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    (void)bg;
    int y_cell = pixel_y / 2;
    int start_pos = x + c->width * y_cell;
    if (y_cell < 0 || y_cell >= c->height) return;
    for (int i = 0; text[i]; i++)
    {
        int idx = start_pos + i;
        if (idx < 0 || idx >= c->width * c->height) break;
        memset(&c->cells[idx], 0, sizeof(gmb_half_cell_t));
        c->cells[idx].ch = (unsigned char)text[i];
        c->cells[idx].has_char = true;
        c->cells[idx].char_color = fg;
    }
}
void gmb_half_block_canvas_render(gmb_half_block_canvas_t* c, gmb_screen_buffer_t* buffer, int offset_x, int offset_y)
{
    for (int i = 0; i < c->width * c->height; i++)
    {
        int cx = offset_x + i % c->width;
        int cy = offset_y + i / c->width;
        if (cx < 0 || cx >= gmb_screen_width(buffer) || cy < 0 || cy >= gmb_screen_height(buffer)) continue;
        gmb_half_cell_t* cell = &c->cells[i];
        if (cell->has_top && cell->has_bottom)
            gmb_screen_setchar(buffer, cx, cy, 0x2580, cell->top_color, cell->bottom_color);
        else if (cell->has_top)
            gmb_screen_setchar(buffer, cx, cy, 0x2580, cell->top_color, GMB_CELLCOLOR_DEFAULT);
        else if (cell->has_bottom)
            gmb_screen_setchar(buffer, cx, cy, 0x2584, cell->bottom_color, GMB_CELLCOLOR_DEFAULT);
        else if (cell->has_char && cell->ch != 0)
            gmb_screen_setchar(buffer, cx, cy, cell->ch, cell->has_char ? cell->char_color : GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
    }
}

/* ===================== Graphics / Base64 ===================== */

