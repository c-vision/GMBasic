/* ============================================================================
 * gmb_tui_boxdraw.c — gmb_tui_boxdraw. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== BoxDrawing ===================== */

static uint32_t box_normal_table[16] = {
    0x20,   /* 0000: spazio */
    0x2502, /* 0001: up */
    0x2500, /* 0010: right */
    0x2514, /* 0011: up+right */
    0x2500, /* 0100: left */
    0x2518, /* 0101: up+left */
    0x2500, /* 0110: left+right */
    0x2534, /* 0111: up+left+right */
    0x2502, /* 1000: down */
    0x2502, /* 1001: up+down */
    0x250C, /* 1010: right+down */
    0x251C, /* 1011: up+right+down */
    0x2510, /* 1100: left+down */
    0x2524, /* 1101: up+left+down */
    0x252C, /* 1110: left+right+down */
    0x253C  /* 1111: cross */
};
static uint32_t box_thick_table[16] = {
    0x20,
    0x2503, 0x2501, 0x2517, 0x2501, 0x251B, 0x2501, 0x253B,
    0x2503, 0x2503, 0x250F, 0x2523, 0x2513, 0x252B, 0x2533,
    0x254B
};
static uint32_t box_doubled_table[16] = {
    0x20,
    0x2551, 0x2550, 0x255A, 0x2550, 0x255D, 0x2550, 0x2569,
    0x2551, 0x2551, 0x2554, 0x2560, 0x2557, 0x2563, 0x2566,
    0x256C
};

static gmb_box_char_t box_make(int up, int right, int left, int down)
{
    gmb_box_char_t b;
    b.up = up; b.right = right; b.left = left; b.down = down;
    b.style = GMB_BS_PLAIN;
    b.ascii = false;
    return b;
}

gmb_box_char_t gmb_box_char_empty(void) { return box_make(-1, -1, -1, -1); }
gmb_box_char_t gmb_box_char_vertical(int w) { return box_make(w, -1, -1, w); }
gmb_box_char_t gmb_box_char_horizontal(int w) { return box_make(-1, w, w, -1); }
gmb_box_char_t gmb_box_char_cross(int w) { return box_make(w, w, w, w); }
gmb_box_char_t gmb_box_char_upper_left(int w) { return box_make(-1, w, -1, w); }
gmb_box_char_t gmb_box_char_upper_right(int w) { return box_make(-1, -1, w, w); }
gmb_box_char_t gmb_box_char_lower_left(int w) { return box_make(w, w, -1, -1); }
gmb_box_char_t gmb_box_char_lower_right(int w) { return box_make(w, -1, w, -1); }
gmb_box_char_t gmb_box_char_up_tee(int w) { return box_make(-1, w, w, w); }
gmb_box_char_t gmb_box_char_right_tee(int w) { return box_make(w, -1, w, w); }
gmb_box_char_t gmb_box_char_left_tee(int w) { return box_make(w, w, -1, w); }
gmb_box_char_t gmb_box_char_down_tee(int w) { return box_make(w, w, w, -1); }

gmb_box_char_t gmb_box_char_thin(void) { gmb_box_char_t b = box_make(0, 0, 0, 0); return b; }
gmb_box_char_t gmb_box_char_thick(void) { gmb_box_char_t b = box_make(1, 1, 1, 1); return b; }
gmb_box_char_t gmb_box_char_round(void) { gmb_box_char_t b = box_make(0, 0, 0, 0); b.style = GMB_BS_CURVED; return b; }

gmb_box_char_t gmb_box_char_with_style(gmb_box_char_t b, int style) { b.style = style; return b; }
gmb_box_char_t gmb_box_char_with_up(gmb_box_char_t b, int w) { b.up = w; return b; }
gmb_box_char_t gmb_box_char_with_right(gmb_box_char_t b, int w) { b.right = w; return b; }
gmb_box_char_t gmb_box_char_with_left(gmb_box_char_t b, int w) { b.left = w; return b; }
gmb_box_char_t gmb_box_char_with_down(gmb_box_char_t b, int w) { b.down = w; return b; }

uint32_t gmb_box_char_to_char(gmb_box_char_t b)
{
    int bit = 0;
    if (b.up >= 0) bit |= 1;
    if (b.right >= 0) bit |= 2;
    if (b.left >= 0) bit |= 4;
    if (b.down >= 0) bit |= 8;
    int w = 0;
    if (b.up > w) w = b.up;
    if (b.right > w) w = b.right;
    if (b.left > w) w = b.left;
    if (b.down > w) w = b.down;
    if (w == 2) return box_doubled_table[bit];
    if (w == 1) return box_thick_table[bit];
    if (b.style == GMB_BS_CURVED)
    {
        switch (bit)
        {
            case 0x3: return 0x256F; /* up+right ╯ */
            case 0x6: return 0x2570; /* left+right ╰ */
            case 0xA: return 0x256D; /* right+down ╭ */
            case 0xC: return 0x256E; /* left+down ╮ */
            default: break;
        }
    }
    return box_normal_table[bit];
}

unsigned char gmb_box_char_to_ascii(gmb_box_char_t b)
{
    int bit = 0;
    if (b.up >= 0) bit |= 1;
    if (b.right >= 0) bit |= 2;
    if (b.left >= 0) bit |= 4;
    if (b.down >= 0) bit |= 8;
    int ones = 0;
    if (bit & 1) ones++;
    if (bit & 2) ones++;
    if (bit & 4) ones++;
    if (bit & 8) ones++;
    if (bit == 0) return ' ';
    if (ones == 1)
    {
        if (bit & 1) return '|';
        if (bit & 8) return '|';
        return '-';
    }
    if (ones == 2)
    {
        if ((bit & 1) && (bit & 8)) return '|';
        if ((bit & 2) && (bit & 4)) return '-';
        return '.';
    }
    return '+';
}

void gmb_box_drawing_draw_box(gmb_screen_buffer_t* buffer, int x, int y, int w, int h,
                              gmb_box_char_t style, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (w <= 0 || h <= 0) return;
    gmb_cell_color_t f = fg ? *fg : GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t b = bg ? *bg : GMB_CELLCOLOR_DEFAULT;
    int weight = 0;
    if (style.up > weight) weight = style.up;
    if (style.right > weight) weight = style.right;
    if (style.left > weight) weight = style.left;
    if (style.down > weight) weight = style.down;
    gmb_box_char_t hline = gmb_box_char_horizontal(weight);
    gmb_box_char_t vline = gmb_box_char_vertical(weight);
    hline.style = style.style;
    vline.style = style.style;
    uint32_t hc = gmb_box_char_to_char(hline);
    uint32_t vc = gmb_box_char_to_char(vline);
    gmb_box_char_t tl = gmb_box_char_upper_left(weight); tl.style = style.style;
    gmb_box_char_t tr = gmb_box_char_upper_right(weight); tr.style = style.style;
    gmb_box_char_t bl = gmb_box_char_lower_left(weight); bl.style = style.style;
    gmb_box_char_t br = gmb_box_char_lower_right(weight); br.style = style.style;
    int lx = x + w - 1, by = y + h - 1;
    gmb_screen_setchar(buffer, x, y, gmb_box_char_to_char(tl), f, b);
    gmb_screen_setchar(buffer, lx, y, gmb_box_char_to_char(tr), f, b);
    gmb_screen_setchar(buffer, x, by, gmb_box_char_to_char(bl), f, b);
    gmb_screen_setchar(buffer, lx, by, gmb_box_char_to_char(br), f, b);
    for (int cx = x + 1; cx < lx; cx++)
    {
        gmb_screen_setchar(buffer, cx, y, hc, f, b);
        gmb_screen_setchar(buffer, cx, by, hc, f, b);
    }
    for (int cy = y + 1; cy < by; cy++)
    {
        gmb_screen_setchar(buffer, x, cy, vc, f, b);
        gmb_screen_setchar(buffer, lx, cy, vc, f, b);
    }
}

void gmb_box_widget_init(gmb_box_widget_t* b, int x, int y, int w, int h)
{
    memset(b, 0, sizeof(*b));
    memset(b, 0, sizeof(*b));
    b->x = x; b->y = y; b->w = w; b->h = h;
    b->style = gmb_box_char_thin();
}
gmb_rect_t gmb_box_widget_inner(gmb_box_widget_t* b)
{
    gmb_rect_t r = { b->x + 1, b->y + 1, b->w - 2, b->h - 2 };
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
void gmb_box_widget_draw(gmb_box_widget_t* b, gmb_screen_buffer_t* buffer)
{
    gmb_cell_color_t f = b->has_fg ? b->fg : GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t bg = b->has_bg ? b->bg : GMB_CELLCOLOR_DEFAULT;
    gmb_box_drawing_draw_box(buffer, b->x, b->y, b->w, b->h, b->style, &f, &bg);
    if (b->title[0] && b->w > 4)
    {
        char t[128];
        snprintf(t, sizeof(t), " %s ", b->title);
        gmb_screen_setstring(buffer, b->x + 1, b->y, t, f, bg);
    }
}
