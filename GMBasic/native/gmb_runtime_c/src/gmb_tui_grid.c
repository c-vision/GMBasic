/* ============================================================================
 * gmb_tui_grid.c — gmb_tui_grid. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== Grid ===================== */

static void grid_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_grid_t* g = (gmb_grid_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sf, &sb);
    /* Surface gfx: base rounded + highlight riga cursore + testo nativo. */
    for (int r = 0; r < c->height; r++)
    {
        char line[256];
        gmb_grid_render_line(g, r, line, (int)sizeof(line));
        if (r == g->cursor_row) gmb_screen_setstring(buf, c->x, c->y + r, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + r, line, fg, bg);
    }
}
static bool grid_key(void* u, gmb_control_t* c, int key)
{
    gmb_grid_t* g = (gmb_grid_t*)u;
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
            g->has_selected = true;
            g->sel_row = g->cursor_row;
            g->sel_col = g->cursor_col;
            return true;
    }
    return false;
}
static bool grid_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_grid_t* g = (gmb_grid_t*)u;
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
    g->has_selected = true;
    g->sel_row = g->cursor_row;
    g->sel_col = g->cursor_col;
    return true;
}
static void grid_draw_px(void* u, gmb_control_t* c)
{
    gmb_grid_t* g = (gmb_grid_t*)u;
    static char _buf[32][256]; static const char* _it[32]; int cols = g->cols > 0 ? g->cols : 1; int rows = g->cells_count / cols; int n = 0;
    for (int r = 0; r < rows && n < 32; r++) { int o = 0; for (int cc = 0; cc < cols && o < 250; cc++) o += snprintf(_buf[n] + o, (size_t)(256 - o), "%s%s", cc ? " | " : "", g->cells[r * cols + cc]); _it[n] = _buf[n]; n++; }
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, _it, n, g->sel_row, 0, 0, NULL, 0, NULL, 0);
}
void gmb_grid_init(gmb_grid_t* g, int x, int y, int w, int h)
{
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->ctrl, "", x, y, w, h, grid_draw, grid_key);
    g->ctrl.draw_px_fn = grid_draw_px;
    g->ctrl.ctrl_kind = GMB_CK_GRID;
    g->ctrl.handle_click_fn = grid_click;
    g->ctrl.user = g;
    g->cols = 1;
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
    int col_w = g->ctrl.width / cols;
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
    if (o > g->ctrl.width) o = g->ctrl.width;
    else for (int i = o; i < g->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
