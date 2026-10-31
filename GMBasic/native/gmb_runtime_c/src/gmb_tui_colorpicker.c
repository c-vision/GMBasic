/* ============================================================================
 * gmb_tui_colorpicker.c — gmb_tui_colorpicker. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== ColorPicker ===================== */

static void colorpicker_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_color_picker_t* p = (gmb_color_picker_t*)u;
    (void)c;
    int rows = gmb_color_picker_row_count(p);
    for (int r = 0; r < c->height; r++)
    {
        int pr = p->top_row + r;
        if (pr >= rows) break;
        for (int cc = 0; cc < p->columns && cc < c->width; cc++)
        {
            int idx = pr * p->columns + cc;
            if (idx >= 256) break;
            gmb_cell_color_t swatch = p->palette[idx];
            if (idx == p->selected_index)
            {
                int lum = swatch.r * 299 + swatch.g * 587 + swatch.b * 114;
                gmb_cell_color_t mfg = lum > 140000 ? GMB_CELLCOLOR_BLACK : GMB_CELLCOLOR_WHITE;
                gmb_screen_setchar(buf, c->x + cc, c->y + r, 0x25CF, mfg, swatch);
            }
            else
            {
                gmb_screen_setchar(buf, c->x + cc, c->y + r, 0x2588, swatch, swatch);
            }
        }
    }
}
static bool colorpicker_key(void* u, gmb_control_t* c, int key)
{
    gmb_color_picker_t* p = (gmb_color_picker_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (p->selected_index >= p->columns) p->selected_index -= p->columns;
            return true;
        case GMB_K_DOWN:
            if (p->selected_index + p->columns < 256) p->selected_index += p->columns;
            return true;
        case GMB_K_LEFT:
            if (p->selected_index > 0) p->selected_index--;
            return true;
        case GMB_K_RIGHT:
            if (p->selected_index < 255) p->selected_index++;
            return true;
        case GMB_K_HOME: p->selected_index = 0; return true;
        case GMB_K_END: p->selected_index = 255; return true;
        case GMB_K_PGUP:
            p->selected_index -= p->columns * c->height;
            if (p->selected_index < 0) p->selected_index = 0;
            return true;
        case GMB_K_PGDN:
            p->selected_index += p->columns * c->height;
            if (p->selected_index > 255) p->selected_index = 255;
            return true;
    }
    return false;
}
static bool colorpicker_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_color_picker_t* p = (gmb_color_picker_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int row = (y - c->y) + p->top_row;
    int col = x - c->x;
    if (col >= 0 && col < p->columns)
    {
        int idx = row * p->columns + col;
        if (idx < 256) p->selected_index = idx;
    }
    return true;
}
static void colorpicker_draw_px(void* u, gmb_control_t* c)
{
    gmb_color_picker_t* p = (gmb_color_picker_t*)u;
    unsigned cols[256]; int total = (p->columns > 0 ? p->columns : 8) * 8; if (total > 256) total = 256;
    for (int i = 0; i < total; i++) cols[i] = ((unsigned)p->palette[i].r << 16) | ((unsigned)p->palette[i].g << 8) | p->palette[i].b;
    gmb_px_draw_ex(GMB_PX_COLORPICKER, c->x, c->y, c->width, c->height, NULL, NULL, 0, p->selected_index, 0, 0, NULL, 0, cols, total);
}
void gmb_color_picker_init(gmb_color_picker_t* p, int x, int y, int w, int h)
{
    gmb_control_init(&p->ctrl, "", x, y, w, h, colorpicker_draw, colorpicker_key);
    p->ctrl.draw_px_fn = colorpicker_draw_px;
    p->ctrl.ctrl_kind = GMB_CK_COLORPICKER;
    p->ctrl.handle_click_fn = colorpicker_click;
    p->ctrl.user = p;
    p->columns = 16;
    for (int i = 0; i < 256; i++)
    {
        int rr = i / 36 * 85; if (rr > 255) rr = 255;
        int gg = i / 6 % 6 * 51; if (gg > 255) gg = 255;
        int bb = i % 6 * 51; if (bb > 255) bb = 255;
        p->palette[i].r = (uint8_t)rr;
        p->palette[i].g = (uint8_t)gg;
        p->palette[i].b = (uint8_t)bb;
        p->palette[i].is_default = false;
    }
}
int gmb_color_picker_row_count(gmb_color_picker_t* p)
{
    return (256 + p->columns - 1) / p->columns;
}
const gmb_cell_color_t* gmb_color_picker_cell_at(gmb_color_picker_t* p, int row, int col)
{
    int idx = row * p->columns + col;
    return idx < 256 ? &p->palette[idx] : NULL;
}
bool gmb_color_picker_is_cell_selected(gmb_color_picker_t* p, int row, int col)
{
    return row * p->columns + col == p->selected_index;
}
int gmb_color_picker_render_line(gmb_color_picker_t* p, int row, char* out, int cap)
{
    int o = 0;
    for (int c = 0; c < p->columns && c < p->ctrl.width; c++)
    {
        int idx = (p->top_row + row) * p->columns + c;
        out[o++] = idx < 256 ? 0xE2 : ' ';
        if (idx < 256) { out[o++] = 0x96; out[o++] = 0x88; }
    }
    for (int i = o; i < p->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
