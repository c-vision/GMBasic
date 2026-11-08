/* ============================================================================
 * gmb_tui_colorpicker.c — ColorPicker (porting del runtime C, specchio ClipperLib).
 *
 * MODELLO UNICO: un colorpicker è un gmb_widget_t (W_COLORPICKER); palette
 * logica in `palette`, colonne in `columns`, prima riga in `top_row`, cella
 * scelta in `ival`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool colorpicker_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* p = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int sel = (int)p->ival;
    switch (key)
    {
        case GMB_K_UP:
            if (sel >= p->columns) sel -= p->columns;
            break;
        case GMB_K_DOWN:
            if (sel + p->columns < 256) sel += p->columns;
            break;
        case GMB_K_LEFT:
            if (sel > 0) sel--;
            break;
        case GMB_K_RIGHT:
            if (sel < 255) sel++;
            break;
        case GMB_K_HOME: sel = 0; break;
        case GMB_K_END: sel = 255; break;
        case GMB_K_PGUP:
            sel -= p->columns * c->height;
            if (sel < 0) sel = 0;
            break;
        case GMB_K_PGDN:
            sel += p->columns * c->height;
            if (sel > 255) sel = 255;
            break;
        default: return false;
    }
    p->ival = sel;
    return true;
}
static bool colorpicker_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* p = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int row = (y - c->y) + p->top_row;
    int col = x - c->x;
    if (col >= 0 && col < p->columns)
    {
        int idx = row * p->columns + col;
        if (idx < 256) p->ival = idx;
    }
    return true;
}
void gmb_color_picker_init(gmb_color_picker_t* p, int x, int y, int w, int h)
{
    gmb_widget_t* g = (gmb_widget_t*)p;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, h, gmb_widget_draw_cells, colorpicker_key);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_COLORPICKER;
    g->base.handle_click_fn = colorpicker_click;
    g->base.user = g;
    g->kind = W_COLORPICKER;
    g->columns = 16;
    for (int i = 0; i < 256; i++)
    {
        int rr = i / 36 * 85; if (rr > 255) rr = 255;
        int gg = i / 6 % 6 * 51; if (gg > 255) gg = 255;
        int bb = i % 6 * 51; if (bb > 255) bb = 255;
        g->palette[i].r = (uint8_t)rr;
        g->palette[i].g = (uint8_t)gg;
        g->palette[i].b = (uint8_t)bb;
        g->palette[i].is_default = false;
    }
    /* Swatch del renderer grafico (primi 16): stessa palette di default della
     * GUI — il modello è uno, qui si popola ciò che il renderer usa. */
    { unsigned int def[8] = { 0xFF000000u,0xFFFFFFFFu,0xFFFF0000u,0xFF00C000u,0xFF3060FFu,0xFFFFD400u,0xFFFF00FFu,0xFF00E5FFu };
      for (int k = 0; k < 8; k++) g->colors[g->ncolors++] = def[k]; }
}
int gmb_color_picker_row_count(gmb_color_picker_t* p)
{
    gmb_widget_t* g = (gmb_widget_t*)p;
    return (256 + g->columns - 1) / (g->columns > 0 ? g->columns : 1);
}
const gmb_cell_color_t* gmb_color_picker_cell_at(gmb_color_picker_t* p, int row, int col)
{
    gmb_widget_t* g = (gmb_widget_t*)p;
    int idx = row * g->columns + col;
    return idx < 256 ? &g->palette[idx] : NULL;
}
bool gmb_color_picker_is_cell_selected(gmb_color_picker_t* p, int row, int col)
{
    gmb_widget_t* g = (gmb_widget_t*)p;
    return row * g->columns + col == (int)g->ival;
}
int gmb_color_picker_render_line(gmb_color_picker_t* p, int row, char* out, int cap)
{
    gmb_widget_t* g = (gmb_widget_t*)p;
    int o = 0;
    for (int c = 0; c < g->columns && c < g->base.width; c++)
    {
        int idx = (g->top_row + row) * g->columns + c;
        out[o++] = idx < 256 ? 0xE2 : ' ';
        if (idx < 256) { out[o++] = 0x96; out[o++] = 0x88; }
    }
    for (int i = o; i < g->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
