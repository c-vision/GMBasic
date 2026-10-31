/* ============================================================================
 * gmb_tui_bufferwidgets.c — gmb_tui_bufferwidgets. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== BufferWidgets ===================== */

void gmb_border_box_draw(gmb_border_box_t* b, gmb_screen_buffer_t* buf, gmb_rect_t area)
{
    if (area.w <= 0 || area.h <= 0) return;
    int left = area.x, right = area.x + area.w - 1, top = area.y, bottom = area.y + area.h - 1;
    for (int x = left; x <= right; x++)
    {
        gmb_screen_setchar(buf, x, top, 0x2500, b->border_color, GMB_CELLCOLOR_DEFAULT);
        gmb_screen_setchar(buf, x, bottom, 0x2500, b->border_color, GMB_CELLCOLOR_DEFAULT);
    }
    for (int y = top; y <= bottom; y++)
    {
        gmb_screen_setchar(buf, left, y, 0x2502, b->border_color, GMB_CELLCOLOR_DEFAULT);
        gmb_screen_setchar(buf, right, y, 0x2502, b->border_color, GMB_CELLCOLOR_DEFAULT);
    }
    gmb_screen_setchar(buf, left, top, 0x250C, b->border_color, GMB_CELLCOLOR_DEFAULT);
    gmb_screen_setchar(buf, right, top, 0x2510, b->border_color, GMB_CELLCOLOR_DEFAULT);
    gmb_screen_setchar(buf, left, bottom, 0x2514, b->border_color, GMB_CELLCOLOR_DEFAULT);
    gmb_screen_setchar(buf, right, bottom, 0x2518, b->border_color, GMB_CELLCOLOR_DEFAULT);
    if (b->title[0] && area.w > 2)
    {
        char title[64];
        snprintf(title, sizeof(title), "%s", b->title);
        int avail = area.w - 2;
        if ((int)strlen(title) > avail) title[avail] = '\0';
        int tx = left + 1 + (avail - (int)strlen(title)) / 2;
        if (tx < 0) tx = 0;
        gmb_screen_setstring(buf, tx, top, title, b->border_color, GMB_CELLCOLOR_DEFAULT);
    }
}

void gmb_paragraph_draw(gmb_paragraph_t* p, gmb_screen_buffer_t* buf, gmb_rect_t area)
{
    if (area.w <= 0 || area.h <= 0) return;
    int y = area.y, limit = area.x + area.w, cx = area.x;
    for (int s = 0; s < p->spans_count; s++)
    {
        gmb_rich_span_t* span = &p->spans[s];
        char words[64][64];
        int word_count = 0;
        char copy[512];
        snprintf(copy, sizeof(copy), "%s", span->text);
        char* save = NULL;
        for (char* w = strtok_r(copy, " ", &save); w && word_count < 64; w = strtok_r(NULL, " ", &save))
            snprintf(words[word_count++], 64, "%s", w);
        for (int w = 0; w < word_count; w++)
        {
            char* word = words[w];
            int word_w = (int)strlen(word);
            if (cx + word_w > limit && cx > area.x)
            {
                y++;
                cx = area.x;
                if (y > area.y + area.h - 1) return;
            }
            for (int k = 0; word[k]; k++)
            {
                if (cx >= limit) break;
                gmb_screen_setchar(buf, cx, y, (unsigned char)word[k],
                    span->has_fg ? span->fg : GMB_CELLCOLOR_DEFAULT,
                    span->has_bg ? span->bg : GMB_CELLCOLOR_DEFAULT);
                cx++;
            }
            if (w < word_count - 1)
            {
                if (cx >= limit) { y++; cx = area.x; if (y > area.y + area.h - 1) return; }
                else { gmb_screen_setchar(buf, cx, y, ' ', span->has_fg ? span->fg : GMB_CELLCOLOR_DEFAULT, span->has_bg ? span->bg : GMB_CELLCOLOR_DEFAULT); cx++; }
            }
        }
    }
}

void gmb_bw_list_draw(gmb_bw_list_t* l, gmb_screen_buffer_t* buf, gmb_rect_t area)
{
    if (area.w <= 0 || area.h <= 0 || l->items_count == 0) return;
    int height = area.h;
    int offset = 0;
    if (l->selected >= 0)
    {
        if (l->selected < offset) offset = l->selected;
        else if (l->selected >= offset + height) offset = l->selected + 1 - height;
    }
    for (int row = 0; row < height; row++)
    {
        int idx = offset + row;
        int yy = area.y + row;
        if (idx >= l->items_count) break;
        bool sel = idx == l->selected;
        char s[256];
        snprintf(s, sizeof(s), "%.*s", area.w, l->items[idx]);
        if (sel)
        {
            for (int x = area.x; x < area.x + area.w; x++) gmb_screen_setchar(buf, x, yy, ' ', l->sel_fg, l->sel_bg);
            gmb_screen_setstring(buf, area.x, yy, s, l->sel_fg, l->sel_bg);
        }
        else
            gmb_screen_setstring(buf, area.x, yy, s, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
    }
}

void gmb_bw_tabs_draw(gmb_bw_tabs_t* t, gmb_screen_buffer_t* buf, gmb_rect_t area)
{
    if (area.w <= 0 || area.h <= 0 || t->titles_count == 0) return;
    int x = area.x, limit = area.x + area.w;
    for (int i = 0; i < t->titles_count; i++)
    {
        if (x >= limit) break;
        bool sel = i == t->selected;
        gmb_cell_color_t fg = sel ? t->sel_fg : GMB_CELLCOLOR_WHITE;
        gmb_cell_color_t bg = sel ? t->sel_bg : GMB_CELLCOLOR_BLACK;
        for (int k = 0; t->titles[i][k] && x < limit; k++)
        {
            gmb_screen_setchar(buf, x, area.y, (unsigned char)t->titles[i][k], fg, bg);
            x++;
        }
        if (i + 1 < t->titles_count && x < limit)
        {
            gmb_screen_setchar(buf, x, area.y, ' ', GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK);
            x++;
        }
    }
}

static void bw_table_draw_row(gmb_screen_buffer_t* buf, gmb_rect_t area, char cells[8][128], int cell_count,
                              int x0, int y, int col_w, gmb_cell_color_t border)
{
    gmb_screen_setchar(buf, x0, y, 0x2502, border, GMB_CELLCOLOR_DEFAULT);
    for (int c = 0; c < cell_count; c++)
    {
        int cx = x0 + 1 + c * col_w;
        char s[128];
        snprintf(s, sizeof(s), "%.*s", col_w - 1, cells[c]);
        gmb_screen_setstring(buf, cx, y, s, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
        int cx_end = x0 + 1 + (c + 1) * col_w - 1;
        gmb_screen_setchar(buf, cx_end, y, 0x2502, border, GMB_CELLCOLOR_DEFAULT);
    }
}

void gmb_bw_table_draw(gmb_bw_table_t* t, gmb_screen_buffer_t* buf, gmb_rect_t area)
{
    if (area.w <= 0 || area.h <= 0) return;
    int cols = t->headers_count > t->row_cols ? t->headers_count : t->row_cols;
    if (cols == 0) return;
    int inner_w = area.w - 1;
    int col_w = inner_w / cols;
    if (col_w < 1) col_w = 1;
    int x0 = area.x;
    int y = area.y;
    bw_table_draw_row(buf, area, t->headers, t->headers_count, x0, y, col_w, t->border_color);
    y++;
    if (y > area.y + area.h - 1) return;
    for (int c = 0; c < cols; c++)
    {
        int cx = x0 + 1 + c * col_w;
        gmb_screen_setchar(buf, cx, y, 0x2500, t->border_color, GMB_CELLCOLOR_DEFAULT);
        if (c < cols - 1)
        {
            int cx_end = x0 + 1 + (c + 1) * col_w - 1;
            for (int xx = cx + 1; xx < cx_end; xx++) gmb_screen_setchar(buf, xx, y, 0x2500, t->border_color, GMB_CELLCOLOR_DEFAULT);
            gmb_screen_setchar(buf, cx_end, y, 0x253C, t->border_color, GMB_CELLCOLOR_DEFAULT);
        }
    }
    gmb_screen_setchar(buf, x0, y, 0x251C, t->border_color, GMB_CELLCOLOR_DEFAULT);
    gmb_screen_setchar(buf, x0 + 1 + cols * col_w - 1, y, 0x2524, t->border_color, GMB_CELLCOLOR_DEFAULT);
    y++;
    for (int r = 0; r < t->rows_count; r++)
    {
        if (y > area.y + area.h - 1) break;
        bw_table_draw_row(buf, area, t->rows[r], t->row_cols, x0, y, col_w, t->border_color);
        y++;
    }
}
