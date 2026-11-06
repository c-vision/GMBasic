/* ============================================================================
 * gmb_tui_spreadsheet.c — gmb_tui_spreadsheet. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== SpreadsheetModel ===================== */

gmb_column_width_t gmb_column_width_auto(void) { gmb_column_width_t c = { GMB_CWK_AUTO, 0, 0 }; return c; }
gmb_column_width_t gmb_column_width_min(int m) { gmb_column_width_t c = { GMB_CWK_MIN, m, 0 }; return c; }
gmb_column_width_t gmb_column_width_max(int mx) { gmb_column_width_t c = { GMB_CWK_MAX, mx, 0 }; return c; }
gmb_column_width_t gmb_column_width_bound(int mn, int mx) { gmb_column_width_t c = { GMB_CWK_BOUND, mn, mx }; return c; }
gmb_column_width_t gmb_column_width_fixed(int w) { gmb_column_width_t c = { GMB_CWK_FIXED, w, 0 }; return c; }
int gmb_column_width_resolve(gmb_column_width_t cw, int max_width)
{
    switch (cw.kind)
    {
        case GMB_CWK_AUTO: return max_width;
        case GMB_CWK_MIN: return cw.a > max_width ? cw.a : max_width;
        case GMB_CWK_MAX: return cw.a < max_width ? cw.a : max_width;
        case GMB_CWK_BOUND:
        {
            int lo = cw.a, hi = cw.a + cw.b;
            if (max_width < lo) return lo;
            if (max_width > hi) return hi;
            return max_width;
        }
        case GMB_CWK_FIXED: return cw.a;
        default: return max_width;
    }
}
void gmb_column_def_init(gmb_column_def_t* c, const char* title, gmb_column_width_t width)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->title, sizeof(c->title), "%s", title ? title : "");
    c->width = width;
    c->alignment = GMB_HALIGN_LEFT;
}
int gmb_column_def_resolved_width(gmb_column_def_t* c, int max_width) { return gmb_column_width_resolve(c->width, max_width); }

void gmb_cell_cursor_init(gmb_cell_cursor_t* c)
{
    memset(c, 0, sizeof(*c));
    memset(c, 0, sizeof(*c));
}
void gmb_cell_cursor_move_left(gmb_cell_cursor_t* c, int n, int rows, int cols)
{
    (void)rows; (void)cols;
    c->col -= n;
    if (c->col < 0) c->col = 0;
}
void gmb_cell_cursor_move_right(gmb_cell_cursor_t* c, int n, int rows, int cols)
{
    (void)rows;
    c->col += n;
    if (cols == 0) c->col = 0;
    else if (c->col >= cols) c->col = cols - 1;
}
void gmb_cell_cursor_move_up(gmb_cell_cursor_t* c, int n, int rows, int cols)
{
    (void)cols;
    c->row -= n;
    if (c->row < 0) c->row = 0;
}
void gmb_cell_cursor_move_down(gmb_cell_cursor_t* c, int n, int rows, int cols)
{
    (void)cols;
    c->row += n;
    if (rows == 0) c->row = 0;
    else if (c->row >= rows) c->row = rows - 1;
}
void gmb_cell_cursor_toggle_select(gmb_cell_cursor_t* c)
{
    int r = c->row, col = c->col;
    if (r >= 0 && r < 64 && col >= 0 && col < 32)
    {
        c->has_selected[r][col] = !c->has_selected[r][col];
        c->selected[r][col] = c->has_selected[r][col];
    }
    c->column_select = false;
}
void gmb_cell_cursor_column_select_all(gmb_cell_cursor_t* c, int rows)
{
    for (int r = 0; r < 64; r++)
        for (int col = 0; col < 32; col++)
        {
            c->has_selected[r][col] = false;
            c->selected[r][col] = false;
        }
    int col = c->col;
    for (int r = 0; r < rows && r < 64; r++)
    {
        c->has_selected[r][col] = true;
        c->selected[r][col] = true;
    }
    c->column_select = true;
}
bool gmb_cell_cursor_is_selected(gmb_cell_cursor_t* c, int r, int col)
{
    return (r >= 0 && r < 64 && col >= 0 && col < 32) ? c->has_selected[r][col] : false;
}
