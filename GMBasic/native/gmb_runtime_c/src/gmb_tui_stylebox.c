/* ============================================================================
 * gmb_tui_stylebox.c — gmb_tui_stylebox. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== StyleBox ===================== */

gmb_border_style_t gmb_border_style_default(void)
{
    gmb_border_style_t b;
    memset(&b, 0, sizeof(b));
    b.top_left = 0x250C; b.top_right = 0x2510;
    b.bottom_left = 0x2514; b.bottom_right = 0x2518;
    b.horizontal = 0x2500; b.vertical = 0x2502;
    return b;
}
gmb_border_style_t gmb_border_style_thick(void)
{
    gmb_border_style_t b = gmb_border_style_default();
    b.top_left = 0x250F; b.top_right = 0x2513;
    b.bottom_left = 0x2517; b.bottom_right = 0x251B;
    b.horizontal = 0x2501; b.vertical = 0x2503;
    return b;
}
gmb_border_style_t gmb_border_style_round(void)
{
    gmb_border_style_t b = gmb_border_style_default();
    b.top_left = 0x256D; b.top_right = 0x256E;
    b.bottom_left = 0x2570; b.bottom_right = 0x256F;
    return b;
}
gmb_border_style_t gmb_border_style_with_fg(gmb_border_style_t b, gmb_cell_color_t c)
{ b.has_fg = true; b.fg = c; return b; }
gmb_border_style_t gmb_border_style_with_bg(gmb_border_style_t b, gmb_cell_color_t c)
{ b.has_bg = true; b.bg = c; return b; }

gmb_box_style_t gmb_box_style_new(void)
{
    gmb_box_style_t s;
    memset(&s, 0, sizeof(s));
    s.align = GMB_SALIGN_LEFT;
    return s;
}
gmb_box_style_t gmb_box_style_with_fg(gmb_box_style_t s, gmb_cell_color_t c)
{ s.has_fg = true; s.fg = c; return s; }
gmb_box_style_t gmb_box_style_with_bg(gmb_box_style_t s, gmb_cell_color_t c)
{ s.has_bg = true; s.bg = c; return s; }
gmb_box_style_t gmb_box_style_with_padding_all(gmb_box_style_t s, int all)
{ s.pad_left = s.pad_right = s.pad_top = s.pad_bottom = all; return s; }
gmb_box_style_t gmb_box_style_with_border(gmb_box_style_t s, gmb_border_style_t b)
{ s.has_border = true; s.border = b; return s; }
gmb_box_style_t gmb_box_style_with_align(gmb_box_style_t s, int align)
{ s.align = align; return s; }

int gmb_text_measurer_measure(const char* text, int max_width)
{
    if (!text || max_width <= 0) return 0;
    int lines = 0;
    char copy[4096];
    snprintf(copy, sizeof(copy), "%s", text);
    char* line = copy;
    while (line)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        int rl = (int)strlen(line);
        if (rl == 0) { lines++; }
        else lines += (rl + max_width - 1) / max_width;
        if (!nl) break;
        line = nl + 1;
    }
    return lines;
}

int gmb_text_measurer_prepare_text(const char* text, int max_width, char out[][256], int cap)
{
    if (max_width <= 0 || !text) return 0;
    char copy[4096];
    snprintf(copy, sizeof(copy), "%s", text);
    char* line = copy;
    int n = 0;
    while (line && n < cap)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        int rl = (int)strlen(line);
        if (rl == 0) { snprintf(out[n++], 256, ""); }
        else
        {
            int pos = 0;
            while (pos < rl && n < cap)
            {
                int len = (rl - pos) < max_width ? (rl - pos) : max_width;
                snprintf(out[n++], 256, "%.*s", len, line + pos);
                pos += len;
            }
        }
        if (!nl) break;
        line = nl + 1;
    }
    return n;
}

void gmb_text_measurer_render_boxed(gmb_box_style_t style, char lines[][256], int line_count,
                                    gmb_screen_buffer_t* buffer, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    gmb_cell_color_t fg = style.has_fg ? style.fg : GMB_CELLCOLOR_WHITE;
    gmb_cell_color_t bg = style.has_bg ? style.bg : GMB_CELLCOLOR_BLACK;
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            gmb_screen_setchar(buffer, col, row, ' ', fg, bg);
    int bsize = style.has_border ? 1 : 0;
    if (style.has_border)
    {
        gmb_border_style_t b = style.border;
        gmb_cell_color_t bfg = b.has_fg ? b.fg : fg;
        gmb_cell_color_t bbg = b.has_bg ? b.bg : bg;
        int last_x = x + w - 1;
        int last_y = y + h - 1;
        gmb_screen_setchar(buffer, x, y, b.top_left, bfg, bbg);
        for (int col = x + 1; col < last_x; col++) gmb_screen_setchar(buffer, col, y, b.horizontal, bfg, bbg);
        if (w > 1) gmb_screen_setchar(buffer, last_x, y, b.top_right, bfg, bbg);
        gmb_screen_setchar(buffer, x, last_y, b.bottom_left, bfg, bbg);
        for (int col = x + 1; col < last_x; col++) gmb_screen_setchar(buffer, col, last_y, b.horizontal, bfg, bbg);
        if (w > 1) gmb_screen_setchar(buffer, last_x, last_y, b.bottom_right, bfg, bbg);
        for (int row = y + 1; row < last_y; row++)
        {
            gmb_screen_setchar(buffer, x, row, b.vertical, bfg, bbg);
            if (w > 1) gmb_screen_setchar(buffer, last_x, row, b.vertical, bfg, bbg);
        }
    }
    int content_x = x + bsize + style.pad_left;
    int content_y = y + bsize + style.pad_top;
    int content_w = w - 2 * bsize - style.pad_left - style.pad_right;
    int content_h = h - 2 * bsize - style.pad_top - style.pad_bottom;
    if (content_w <= 0 || content_h <= 0) return;
    int n = line_count < content_h ? line_count : content_h;
    for (int i = 0; i < n; i++)
    {
        int llen = (int)strlen(lines[i]);
        if (llen > content_w) llen = content_w;
        int pad = content_w - llen;
        int left_pad = 0;
        if (style.align == GMB_SALIGN_CENTER) left_pad = pad / 2;
        else if (style.align == GMB_SALIGN_RIGHT) left_pad = pad;
        char line[256];
        snprintf(line, sizeof(line), "%.*s", llen, lines[i]);
        gmb_screen_setstring(buffer, content_x + left_pad, content_y + i, line, fg, bg);
    }
}
