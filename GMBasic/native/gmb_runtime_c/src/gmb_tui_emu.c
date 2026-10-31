/* ============================================================================
 * gmb_tui_emu.c — emulatore VT (porting EmuCell.cs/EmuGrid.cs/EmuRender.cs/Emulator.cs).
 * Parser semplificato output-only.
 * ============================================================================ */
#include "gmb_tui_emu.h"
#include "gmb_tui_concodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static gmb_emu_cell_t emu_blank(gmb_emu_grid_t* g)
{
    gmb_emu_cell_t c;
    memset(&c, 0, sizeof(c));
    c.ch = ' ';
    c.style = g->default_style;
    return c;
}

void gmb_emu_grid_init(gmb_emu_grid_t* g, int cols, int rows)
{
    memset(g, 0, sizeof(*g));
    if (cols > GMB_EMU_MAX_COLS) cols = GMB_EMU_MAX_COLS;
    if (rows > GMB_EMU_MAX_ROWS) rows = GMB_EMU_MAX_ROWS;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    g->cols = cols;
    g->rows = rows;
    g->auto_wrap = true;
    g->cursor_visible = true;
    g->scroll_top = 0;
    g->scroll_bottom = rows - 1;
    gmb_emu_grid_clear(g);
}

gmb_emu_cell_t* gmb_emu_grid_cell(gmb_emu_grid_t* g, int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= g->cols) x = g->cols - 1;
    if (y >= g->rows) y = g->rows - 1;
    return &g->cells[y][x];
}

void gmb_emu_grid_set_cell(gmb_emu_grid_t* g, int x, int y, uint32_t ch, gmb_emu_style_t style,
                           bool wide, bool cont)
{
    if (x < 0 || x >= g->cols || y < 0 || y >= g->rows) return;
    g->cells[y][x].ch = ch;
    g->cells[y][x].style = style;
    g->cells[y][x].is_wide = wide;
    g->cells[y][x].is_continuation = cont;
}

void gmb_emu_grid_clear(gmb_emu_grid_t* g)
{
    for (int y = 0; y < g->rows; y++)
        for (int x = 0; x < g->cols; x++)
            g->cells[y][x] = emu_blank(g);
}

void gmb_emu_grid_clear_rect(gmb_emu_grid_t* g, int x0, int y0, int x1, int y1)
{
    if (x1 > g->cols - 1) x1 = g->cols - 1;
    if (y1 > g->rows - 1) y1 = g->rows - 1;
    for (int y = (y0 > 0 ? y0 : 0); y <= y1; y++)
        for (int x = (x0 > 0 ? x0 : 0); x <= x1; x++)
            g->cells[y][x] = emu_blank(g);
}

void gmb_emu_grid_resize(gmb_emu_grid_t* g, int cols, int rows)
{
    if (cols > GMB_EMU_MAX_COLS) cols = GMB_EMU_MAX_COLS;
    if (rows > GMB_EMU_MAX_ROWS) rows = GMB_EMU_MAX_ROWS;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    g->cols = cols;
    g->rows = rows;
    gmb_emu_grid_clear(g);
    g->scroll_top = 0;
    g->scroll_bottom = rows - 1;
    if (g->cursor_x > cols - 1) g->cursor_x = cols - 1;
    if (g->cursor_y > rows - 1) g->cursor_y = rows - 1;
    g->alt_active = false;
}

void gmb_emu_grid_scroll_up(gmb_emu_grid_t* g, int top, int bottom, int n)
{
    if (n <= 0 || top >= bottom) return;
    if (n > bottom - top) n = bottom - top;
    for (int y = top + n; y <= bottom; y++)
        for (int x = 0; x < g->cols; x++)
            g->cells[y - n][x] = g->cells[y][x];
    for (int y = bottom - n + 1; y <= bottom; y++)
        for (int x = 0; x < g->cols; x++)
            g->cells[y][x] = emu_blank(g);
}

void gmb_emu_grid_scroll_down(gmb_emu_grid_t* g, int top, int bottom, int n)
{
    if (n <= 0 || top >= bottom) return;
    if (n > bottom - top) n = bottom - top;
    for (int y = bottom; y >= top + n; y--)
        for (int x = 0; x < g->cols; x++)
            g->cells[y][x] = g->cells[y - n][x];
    for (int y = top; y < top + n; y++)
        for (int x = 0; x < g->cols; x++)
            g->cells[y][x] = emu_blank(g);
}

void gmb_emu_grid_insert_chars(gmb_emu_grid_t* g, int y, int at, int n)
{
    if (y < 0 || y >= g->rows || at < 0 || at >= g->cols || n <= 0) return;
    if (n > g->cols - at) n = g->cols - at;
    gmb_emu_cell_t row[GMB_EMU_MAX_COLS];
    gmb_emu_cell_t rot[GMB_EMU_MAX_COLS];
    for (int x = 0; x < g->cols; x++) row[x] = g->cells[y][x];
    for (int x = 0; x < g->cols; x++) rot[(x + n) % g->cols] = row[x];
    for (int x = 0; x < g->cols; x++) g->cells[y][x] = rot[x];
    for (int x = at; x < at + n; x++) g->cells[y][x] = emu_blank(g);
}

void gmb_emu_grid_delete_chars(gmb_emu_grid_t* g, int y, int at, int n)
{
    if (y < 0 || y >= g->rows || at < 0 || at >= g->cols || n <= 0) return;
    if (n > g->cols - at) n = g->cols - at;
    gmb_emu_cell_t row[GMB_EMU_MAX_COLS];
    gmb_emu_cell_t rot[GMB_EMU_MAX_COLS];
    for (int x = 0; x < g->cols; x++) row[x] = g->cells[y][x];
    for (int x = 0; x < g->cols; x++) rot[(x - n + g->cols) % g->cols] = row[x];
    for (int x = 0; x < g->cols; x++) g->cells[y][x] = rot[x];
    for (int x = g->cols - n; x < g->cols; x++) g->cells[y][x] = emu_blank(g);
}

void gmb_emu_grid_new_line(gmb_emu_grid_t* g)
{
    if (g->cursor_y == g->scroll_bottom)
        gmb_emu_grid_scroll_up(g, g->scroll_top, g->scroll_bottom, 1);
    else if (g->cursor_y < g->rows - 1)
        g->cursor_y++;
}

void gmb_emu_grid_carriage_return(gmb_emu_grid_t* g) { g->cursor_x = 0; }

void gmb_emu_grid_set_scroll_region(gmb_emu_grid_t* g, int top1, int bottom1)
{
    int top = top1 > 1 ? top1 : 1;
    int bottom = bottom1 == 0 ? g->rows : bottom1;
    if (top > g->rows) top = g->rows;
    if (bottom > g->rows) bottom = g->rows;
    if (top < bottom)
    {
        g->scroll_top = top - 1;
        g->scroll_bottom = bottom - 1;
        g->cursor_x = 0;
        g->cursor_y = g->origin_mode ? g->scroll_top : 0;
    }
}

void gmb_emu_grid_save_cursor(gmb_emu_grid_t* g)
{
    g->saved_x = g->cursor_x;
    g->saved_y = g->cursor_y;
}

void gmb_emu_grid_restore_cursor(gmb_emu_grid_t* g)
{
    if (g->saved_x < g->cols) g->cursor_x = g->saved_x; else g->cursor_x = g->cols - 1;
    if (g->saved_y < g->rows) g->cursor_y = g->saved_y; else g->cursor_y = g->rows - 1;
}

void gmb_emu_grid_swap_alt_screen(gmb_emu_grid_t* g, bool on)
{
    if (on && !g->alt_active)
    {
        memcpy(g->alt_cells, g->cells, sizeof(g->cells));
        gmb_emu_grid_clear(g);
        g->alt_active = true;
        g->cursor_x = 0;
        g->cursor_y = 0;
    }
    else if (!on && g->alt_active)
    {
        memcpy(g->cells, g->alt_cells, sizeof(g->cells));
        g->alt_active = false;
        g->cursor_x = 0;
        g->cursor_y = 0;
    }
}

/* ===================== Emulator ===================== */

void gmb_emulator_init(gmb_emulator_t* e, int cols, int rows)
{
    memset(e, 0, sizeof(*e));
    gmb_emu_grid_init(&e->grid, cols, rows);
    memset(&e->cur_style, 0, sizeof(e->cur_style));
}

int gmb_emulator_cols(const gmb_emulator_t* e) { return e->grid.cols; }
int gmb_emulator_rows(const gmb_emulator_t* e) { return e->grid.rows; }
void gmb_emulator_cursor(const gmb_emulator_t* e, int* x, int* y)
{
    if (x) *x = e->grid.cursor_x;
    if (y) *y = e->grid.cursor_y;
}

static int emu_region_min_y(gmb_emulator_t* e)
{
    return e->grid.origin_mode ? e->grid.scroll_top : 0;
}

static int emu_region_max_y(gmb_emulator_t* e)
{
    return e->grid.origin_mode ? e->grid.scroll_bottom : e->grid.rows - 1;
}

static int emu_char_width(uint32_t ch)
{
    if (ch < 0x80) return 1;
    /* wide ranges (CJK etc.) semplificati */
    if ((ch >= 0x1100 && ch <= 0x115F) || (ch >= 0x2E80 && ch <= 0x303E) ||
        (ch >= 0x3041 && ch <= 0x33FF) || (ch >= 0x3400 && ch <= 0x4DBF) ||
        (ch >= 0x4E00 && ch <= 0x9FFF) || (ch >= 0xA000 && ch <= 0xA4CF) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) || (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE30 && ch <= 0xFE4F) || (ch >= 0xFF00 && ch <= 0xFF60) ||
        (ch >= 0xFFE0 && ch <= 0xFFE6) || ch == 0x2329 || ch == 0x232A)
        return 2;
    return 1;
}

static void emu_print(gmb_emulator_t* e, uint32_t ch)
{
    gmb_emu_grid_t* g = &e->grid;
    int w = emu_char_width(ch);
    if (w == 0) return;
    if (w == 1)
    {
        if (g->cursor_x < g->cols && g->cursor_y < g->rows)
            gmb_emu_grid_set_cell(g, g->cursor_x, g->cursor_y, ch, e->cur_style, false, false);
        g->cursor_x++;
        if (g->cursor_x >= g->cols)
        {
            if (g->auto_wrap) { g->cursor_x = 0; gmb_emu_grid_new_line(g); }
            else g->cursor_x = g->cols - 1;
        }
        return;
    }
    if (g->cursor_x >= g->cols - 1)
    {
        g->cursor_x = 0;
        gmb_emu_grid_new_line(g);
    }
    if (g->cursor_x < g->cols && g->cursor_y < g->rows)
    {
        gmb_emu_grid_set_cell(g, g->cursor_x, g->cursor_y, ch, e->cur_style, true, false);
        if (g->cursor_x + 1 < g->cols)
            gmb_emu_grid_set_cell(g, g->cursor_x + 1, g->cursor_y, ' ', e->cur_style, false, true);
    }
    g->cursor_x += 2;
    if (g->auto_wrap && g->cursor_x >= g->cols)
    {
        g->cursor_x = 0;
        gmb_emu_grid_new_line(g);
    }
    else if (g->cursor_x >= g->cols)
        g->cursor_x = g->cols - 1;
}

static void emu_execute(gmb_emulator_t* e, uint8_t b)
{
    gmb_emu_grid_t* g = &e->grid;
    switch (b)
    {
        case 0x07: break; /* BEL */
        case 0x08: if (g->cursor_x > 0) g->cursor_x--; break;
        case 0x09:
        {
            int next = (g->cursor_x / 8 + 1) * 8;
            g->cursor_x = next < g->cols - 1 ? next : g->cols - 1;
            break;
        }
        case 0x0A: case 0x0B: case 0x0C:
            gmb_emu_grid_new_line(g);
            break;
        case 0x0D:
            g->cursor_x = 0;
            break;
    }
}

static void emu_erase_display(gmb_emulator_t* e, int mode)
{
    gmb_emu_grid_t* g = &e->grid;
    switch (mode)
    {
        case 0: gmb_emu_grid_clear_rect(g, g->cursor_x, g->cursor_y, g->cols - 1, g->scroll_bottom); break;
        case 1: gmb_emu_grid_clear_rect(g, 0, g->scroll_top, g->cursor_x, g->cursor_y); break;
        case 2: case 3: gmb_emu_grid_clear_rect(g, 0, g->scroll_top, g->cols - 1, g->scroll_bottom); break;
    }
}

static void emu_erase_line(gmb_emulator_t* e, int mode)
{
    gmb_emu_grid_t* g = &e->grid;
    switch (mode)
    {
        case 0: gmb_emu_grid_clear_rect(g, g->cursor_x, g->cursor_y, g->cols - 1, g->cursor_y); break;
        case 1: gmb_emu_grid_clear_rect(g, 0, g->cursor_y, g->cursor_x, g->cursor_y); break;
        case 2: gmb_emu_grid_clear_rect(g, 0, g->cursor_y, g->cols - 1, g->cursor_y); break;
    }
}

static gmb_cell_color_t emu_indexed_to_cell(int i)
{
    gmb_cell_color_t c;
    c.is_default = false;
    if (i < 16)
    {
        switch (i)
        {
            case 0: return GMB_CELLCOLOR_BLACK;
            case 1: return GMB_CELLCOLOR_RED;
            case 2: return GMB_CELLCOLOR_GREEN;
            case 3: return GMB_CELLCOLOR_YELLOW;
            case 4: return GMB_CELLCOLOR_BLUE;
            case 5: return GMB_CELLCOLOR_MAGENTA;
            case 6: return GMB_CELLCOLOR_CYAN;
            case 7: return GMB_CELLCOLOR_WHITE;
            default: return GMB_CELLCOLOR_GREY;
        }
    }
    if (i <= 231)
    {
        int n = i - 16;
        int r = n / 36, gg = n / 6 % 6, b = n % 6;
        c.r = (uint8_t)(r == 0 ? 0 : 55 + 40 * r);
        c.g = (uint8_t)(gg == 0 ? 0 : 55 + 40 * gg);
        c.b = (uint8_t)(b == 0 ? 0 : 55 + 40 * b);
        return c;
    }
    int t = i - 232;
    uint8_t v = (uint8_t)(8 + 10 * t);
    c.r = v; c.g = v; c.b = v;
    return c;
}

static void emu_apply_sgr(gmb_emulator_t* e, int* params, int np)
{
    int i = 0;
    while (i < np)
    {
        int p = params[i];
        if (p == 0) { memset(&e->cur_style, 0, sizeof(e->cur_style)); }
        else if (p == 1) e->cur_style.bold = true;
        else if (p == 2) e->cur_style.dim = true;
        else if (p == 3) e->cur_style.italic = true;
        else if (p == 4) e->cur_style.underline = true;
        else if (p == 5 || p == 6) e->cur_style.blink = true;
        else if (p == 7) e->cur_style.reverse = true;
        else if (p == 22) { e->cur_style.bold = false; e->cur_style.dim = false; }
        else if (p == 23) e->cur_style.italic = false;
        else if (p == 24) e->cur_style.underline = false;
        else if (p == 25) e->cur_style.blink = false;
        else if (p == 27) e->cur_style.reverse = false;
        else if (p == 39) e->cur_style.has_fg = false;
        else if (p == 49) e->cur_style.has_bg = false;
        else if (p >= 30 && p <= 37)
        {
            e->cur_style.has_fg = true;
            e->cur_style.fg = emu_indexed_to_cell(p - 30);
        }
        else if (p >= 90 && p <= 97)
        {
            e->cur_style.has_fg = true;
            e->cur_style.fg = emu_indexed_to_cell(p - 90 + 8);
        }
        else if (p >= 40 && p <= 47)
        {
            e->cur_style.has_bg = true;
            e->cur_style.bg = emu_indexed_to_cell(p - 40);
        }
        else if (p >= 100 && p <= 107)
        {
            e->cur_style.has_bg = true;
            e->cur_style.bg = emu_indexed_to_cell(p - 100 + 8);
        }
        else if (p == 38 || p == 48)
        {
            /* 38;5;n  oppure  38;2;r;g;b */
            if (i + 1 < np && params[i + 1] == 5 && i + 2 < np)
            {
                int idx = params[i + 2];
                if (p == 38) { e->cur_style.has_fg = true; e->cur_style.fg = emu_indexed_to_cell(idx); }
                else { e->cur_style.has_bg = true; e->cur_style.bg = emu_indexed_to_cell(idx); }
                i += 3;
                continue;
            }
            if (i + 1 < np && params[i + 1] == 2 && i + 4 < np)
            {
                gmb_cell_color_t c;
                c.r = (uint8_t)params[i + 2]; c.g = (uint8_t)params[i + 3]; c.b = (uint8_t)params[i + 4];
                c.is_default = false;
                if (p == 38) { e->cur_style.has_fg = true; e->cur_style.fg = c; }
                else { e->cur_style.has_bg = true; e->cur_style.bg = c; }
                i += 5;
                continue;
            }
        }
        i++;
    }
}

/* decodifica UTF-8 di un char */
static int utf8_decode(const char* s, uint32_t* out)
{
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) { *out = c0; return 1; }
    if ((c0 >> 5) == 0x06 && s[1]) { *out = ((c0 & 0x1F) << 6) | ((unsigned char)s[1] & 0x3F); return 2; }
    if ((c0 >> 4) == 0x0E && s[1] && s[2]) { *out = ((c0 & 0x0F) << 12) | (((unsigned char)s[1] & 0x3F) << 6) | ((unsigned char)s[2] & 0x3F); return 3; }
    if ((c0 >> 3) == 0x1E && s[1] && s[2] && s[3]) { *out = ((c0 & 0x07) << 18) | (((unsigned char)s[1] & 0x3F) << 12) | (((unsigned char)s[2] & 0x3F) << 6) | ((unsigned char)s[3] & 0x3F); return 4; }
    *out = c0;
    return 1;
}

/* set mode privati: ?7 autowrap, ?6 origin, ?25 cursor, ?47/?1047/?1049 alt screen */
static void emu_set_private_mode(gmb_emulator_t* e, int mode, bool on)
{
    switch (mode)
    {
        case 7: e->grid.auto_wrap = on; break;
        case 6: e->grid.origin_mode = on; break;
        case 25: e->grid.cursor_visible = on; break;
        case 47: case 1047: case 1049: gmb_emu_grid_swap_alt_screen(&e->grid, on); break;
    }
}

static void emu_soft_reset(gmb_emulator_t* e)
{
    memset(&e->cur_style, 0, sizeof(e->cur_style));
    e->grid.auto_wrap = true;
    e->grid.cursor_visible = true;
    e->grid.origin_mode = false;
    e->grid.cursor_x = 0;
    e->grid.cursor_y = 0;
    e->grid.scroll_top = 0;
    e->grid.scroll_bottom = e->grid.rows - 1;
}

/* applica una Sequence (specchio Emulator.Process) */
static void emu_process_seq(gmb_emulator_t* e, const gmb_sequence_t* seq)
{
    gmb_emu_grid_t* g = &e->grid;
    int miny = g->origin_mode ? g->scroll_top : 0;
    int maxy = g->origin_mode ? g->scroll_bottom : g->rows - 1;
    switch (seq->type)
    {
        case GMB_SEQ_PRINT:
            emu_print(e, (unsigned char)seq->ch);
            break;
        case GMB_SEQ_EXECUTE:
            emu_execute(e, seq->ctrl);
            break;
        case GMB_SEQ_CURSOR_UP:
            g->cursor_y -= seq->n1;
            if (g->cursor_y < miny) g->cursor_y = miny;
            break;
        case GMB_SEQ_CURSOR_DOWN:
            g->cursor_y += seq->n1;
            if (g->cursor_y > maxy) g->cursor_y = maxy;
            break;
        case GMB_SEQ_CURSOR_FORWARD:
            g->cursor_x += seq->n1;
            if (g->cursor_x > g->cols - 1) g->cursor_x = g->cols - 1;
            break;
        case GMB_SEQ_CURSOR_BACKWARD:
            g->cursor_x -= seq->n1;
            if (g->cursor_x < 0) g->cursor_x = 0;
            break;
        case GMB_SEQ_CURSOR_NEXT_LINE:
            g->cursor_x = 0;
            g->cursor_y += seq->n1;
            if (g->cursor_y > maxy) g->cursor_y = maxy;
            break;
        case GMB_SEQ_CURSOR_PREV_LINE:
            g->cursor_x = 0;
            g->cursor_y -= seq->n1;
            if (g->cursor_y < miny) g->cursor_y = miny;
            break;
        case GMB_SEQ_CURSOR_COLUMN:
            g->cursor_x = seq->n1 - 1;
            if (g->cursor_x < 0) g->cursor_x = 0;
            if (g->cursor_x > g->cols - 1) g->cursor_x = g->cols - 1;
            break;
        case GMB_SEQ_CURSOR_LINE:
            g->cursor_y = seq->n1 - 1;
            if (g->cursor_y < 0) g->cursor_y = 0;
            if (g->cursor_y > maxy) g->cursor_y = maxy;
            break;
        case GMB_SEQ_CURSOR_POSITION:
            g->cursor_x = seq->x - 1;
            if (g->cursor_x < 0) g->cursor_x = 0;
            if (g->cursor_x > g->cols - 1) g->cursor_x = g->cols - 1;
            g->cursor_y = seq->y - 1;
            if (g->cursor_y < 0) g->cursor_y = 0;
            if (g->cursor_y > maxy) g->cursor_y = maxy;
            break;
        case GMB_SEQ_ERASE_IN_DISPLAY:
            emu_erase_display(e, (int)seq->n1);
            break;
        case GMB_SEQ_ERASE_IN_LINE:
            emu_erase_line(e, (int)seq->n1);
            break;
        case GMB_SEQ_SCROLL_UP:
            gmb_emu_grid_scroll_up(g, g->scroll_top, g->scroll_bottom, seq->n1);
            break;
        case GMB_SEQ_SCROLL_DOWN:
            gmb_emu_grid_scroll_down(g, g->scroll_top, g->scroll_bottom, seq->n1);
            break;
        case GMB_SEQ_INSERT_LINES:
        {
            int top = g->scroll_top > g->cursor_y ? g->scroll_top : g->cursor_y;
            gmb_emu_grid_scroll_down(g, top, g->scroll_bottom, seq->n1);
            break;
        }
        case GMB_SEQ_DELETE_LINES:
        {
            int top = g->scroll_top > g->cursor_y ? g->scroll_top : g->cursor_y;
            gmb_emu_grid_scroll_up(g, top, g->scroll_bottom, seq->n1);
            break;
        }
        case GMB_SEQ_INSERT_CHARS:
            gmb_emu_grid_insert_chars(g, g->cursor_y, g->cursor_x, seq->n1);
            break;
        case GMB_SEQ_DELETE_CHARS:
            gmb_emu_grid_delete_chars(g, g->cursor_y, g->cursor_x, seq->n1);
            break;
        case GMB_SEQ_SET_SCROLL_REGION:
            gmb_emu_grid_set_scroll_region(g, seq->n1, seq->n2);
            break;
        case GMB_SEQ_SAVE_CURSOR:
            e->saved_style = e->cur_style;
            gmb_emu_grid_save_cursor(g);
            break;
        case GMB_SEQ_RESTORE_CURSOR:
            gmb_emu_grid_restore_cursor(g);
            e->cur_style = e->saved_style;
            break;
        case GMB_SEQ_SET_MODE:
        {
            switch (seq->mode)
            {
                case GMB_TM_AUTOWRAP: g->auto_wrap = true; break;
                case GMB_TM_CURSOR_VISIBLE: g->cursor_visible = true; break;
                case GMB_TM_ORIGIN: g->origin_mode = true; break;
                case GMB_TM_ALTSCREEN: gmb_emu_grid_swap_alt_screen(g, true); break;
            }
            break;
        }
        case GMB_SEQ_RESET_MODE:
        {
            switch (seq->mode)
            {
                case GMB_TM_AUTOWRAP: g->auto_wrap = false; break;
                case GMB_TM_CURSOR_VISIBLE: g->cursor_visible = false; break;
                case GMB_TM_ORIGIN: g->origin_mode = false; break;
                case GMB_TM_ALTSCREEN: gmb_emu_grid_swap_alt_screen(g, false); break;
            }
            break;
        }
        case GMB_SEQ_SOFT_RESET:
            emu_soft_reset(e);
            break;
        case GMB_SEQ_SGR:
        {
            for (int i = 0; i < seq->sgr_count; i++)
            {
                const gmb_sgr_attr_t* a = &seq->sgr[i];
                switch (a->type)
                {
                    case GMB_SA_RESET: memset(&e->cur_style, 0, sizeof(e->cur_style)); break;
                    case GMB_SA_BOLD: e->cur_style.bold = true; break;
                    case GMB_SA_DIM: e->cur_style.dim = true; break;
                    case GMB_SA_ITALIC: e->cur_style.italic = true; break;
                    case GMB_SA_UNDERLINE: e->cur_style.underline = true; break;
                    case GMB_SA_BLINK: case GMB_SA_RAPIDBLINK: e->cur_style.blink = true; break;
                    case GMB_SA_REVERSE: e->cur_style.reverse = true; break;
                    case GMB_SA_NOT_BOLD_OR_DIM: e->cur_style.bold = false; e->cur_style.dim = false; break;
                    case GMB_SA_NOT_ITALIC: e->cur_style.italic = false; break;
                    case GMB_SA_NOT_UNDERLINE: e->cur_style.underline = false; break;
                    case GMB_SA_NOT_BLINK: e->cur_style.blink = false; break;
                    case GMB_SA_NOT_REVERSE: e->cur_style.reverse = false; break;
                    case GMB_SA_DEFAULT_FG: e->cur_style.has_fg = false; break;
                    case GMB_SA_DEFAULT_BG: e->cur_style.has_bg = false; break;
                    case GMB_SA_FG: case GMB_SA_FG_EXTENDED:
                        e->cur_style.has_fg = true;
                        if (a->color.type == GMB_SGRCOL_INDEXED) e->cur_style.fg = emu_indexed_to_cell(a->color.index);
                        else if (a->color.type == GMB_SGRCOL_RGB) { e->cur_style.fg.r = a->color.r; e->cur_style.fg.g = a->color.g; e->cur_style.fg.b = a->color.b; e->cur_style.fg.is_default = false; }
                        break;
                    case GMB_SA_BG: case GMB_SA_BG_EXTENDED:
                        e->cur_style.has_bg = true;
                        if (a->color.type == GMB_SGRCOL_INDEXED) e->cur_style.bg = emu_indexed_to_cell(a->color.index);
                        else if (a->color.type == GMB_SGRCOL_RGB) { e->cur_style.bg.r = a->color.r; e->cur_style.bg.g = a->color.g; e->cur_style.bg.b = a->color.b; e->cur_style.bg.is_default = false; }
                        break;
                    default: break;
                }
            }
            break;
        }
        default:
            break;
    }
}

void gmb_emulator_feed(gmb_emulator_t* e, const char* utf8)
{
    gmb_parser_t parser;
    gmb_parser_init(&parser, true);
    gmb_parser_advance(&parser, utf8, false);
    gmb_sequence_t seqs[GMB_SEQ_MAX];
    int n = gmb_parser_drain(&parser, seqs, GMB_SEQ_MAX);
    for (int i = 0; i < n; i++)
        emu_process_seq(e, &seqs[i]);
}

/* ===================== EmuRender ===================== */

void gmb_emu_render_to_buffer(gmb_emulator_t* e, gmb_screen_buffer_t* buf, bool draw_cursor,
                              int off_x, int off_y)
{
    gmb_emu_grid_t* g = &e->grid;
    int w = g->cols < gmb_screen_width(buf) - off_x ? g->cols : gmb_screen_width(buf) - off_x;
    int h = g->rows < gmb_screen_height(buf) - off_y ? g->rows : gmb_screen_height(buf) - off_y;
    bool draw_cur = draw_cursor && g->cursor_visible;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            gmb_emu_cell_t* cell = &g->cells[y][x];
            if (cell->is_continuation) continue;
            gmb_cell_color_t fg = cell->style.has_fg ? cell->style.fg : GMB_CELLCOLOR_DEFAULT;
            gmb_cell_color_t bg = cell->style.has_bg ? cell->style.bg : GMB_CELLCOLOR_DEFAULT;
            if (cell->style.reverse)
            {
                gmb_cell_color_t t = fg; fg = bg; bg = t;
            }
            if (draw_cur && x == g->cursor_x && y == g->cursor_y)
            {
                gmb_cell_color_t t = fg; fg = bg; bg = t;
            }
            gmb_screen_setchar(buf, x + off_x, y + off_y, cell->ch, fg, bg);
        }
    }
}
