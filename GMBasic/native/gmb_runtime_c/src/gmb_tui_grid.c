/* ============================================================================
 * gmb_tui_grid.c — Grid (porting del runtime C, specchio ClipperLib).
 *
 * MODELLO UNICO: una griglia è un gmb_widget_t (W_GRID); celle in `cells`
 * (riga-major), colonne in `cols`, riga scelta in `ival`, cursore in
 * cursor_row/cursor_col.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool grid_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* g = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int rows = g->cols > 0 ? (g->cells_count + g->cols - 1) / g->cols : 0;
    switch (key)
    {
        case GMB_K_UP: if (g->cursor_row > 0) g->cursor_row--; return true;
        case GMB_K_DOWN: if (g->cursor_row < rows - 1) g->cursor_row++; return true;
        case GMB_K_LEFT: if (g->cursor_col > 0) g->cursor_col--; return true;
        case GMB_K_RIGHT: if (g->cursor_col < g->cols - 1) g->cursor_col++; return true;
        case GMB_K_HOME: g->cursor_col = 0; return true;
        case GMB_K_END: g->cursor_col = g->cols - 1; return true;
        case GMB_K_ENTER:
            g->has_selected = 1;
            g->ival = g->cursor_row;
            g->sel_col = g->cursor_col;
            return true;
    }
    return false;
}
static bool grid_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* g = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    g->cursor_row = y - c->y;
    int col_width = c->width / (g->cols > 0 ? g->cols : 1);
    if (col_width < 1) col_width = 1;
    g->cursor_col = (x - c->x) / (col_width + 1);
    if (g->cursor_col < 0) g->cursor_col = 0;
    if (g->cursor_col >= g->cols) g->cursor_col = g->cols - 1;
    g->has_selected = 1;
    g->ival = g->cursor_row;
    g->sel_col = g->cursor_col;
    return true;
}
void gmb_grid_init(gmb_grid_t* g, int x, int y, int w, int h)
{
    gmb_widget_t* m = (gmb_widget_t*)g;
    memset(m, 0, sizeof(*m));
    gmb_control_init(&m->base, "", x, y, w, h, gmb_widget_draw_cells, grid_key);
    m->base.draw_px_fn = gmb_widget_draw_px;
    m->base.ctrl_kind = GMB_CK_GRID;
    m->base.handle_click_fn = grid_click;
    m->base.user = m;
    m->kind = W_GRID;
    m->cols = 1;
}
void gmb_grid_set_cols(gmb_grid_t* g, int cols) { g->cols = cols > 0 ? cols : 1; }
void gmb_grid_set_cells(gmb_grid_t* g, const char* const* cells, int count)
{
    g->cells_count = count < 128 ? count : 128;
    for (int i = 0; i < g->cells_count && cells; i++)
        snprintf(g->cells[i], sizeof(g->cells[i]), "%s", cells[i] ? cells[i] : "");
}
int gmb_grid_render_line(gmb_grid_t* g, int r, char* out, int cap)
{
    int cols = g->cols > 0 ? g->cols : 1;
    int col_w = g->base.width / cols;
    if (col_w < 1) col_w = 1;
    int o = 0;
    for (int c = 0; c < cols; c++)
    {
        int idx = r * cols + c;
        const char* val = idx < g->cells_count ? g->cells[idx] : "";
        int vl = (int)strlen(val);
        for (int i = 0; i < vl && o < cap - 1; i++) out[o++] = val[i];
        for (int i = vl; i < col_w && o < cap - 1; i++) out[o++] = ' ';
        if (c < cols - 1 && o < cap - 1) out[o++] = ' ';
    }
    if (o > g->base.width) o = g->base.width;
    else for (int i = o; i < g->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
