/* ============================================================================
 * gmb_tui_render.c — Colorize + DoubleBuffer (porting Colorize.cs/DoubleBuffer.cs).
 * ============================================================================ */
#include "gmb_tui_render.h"
#include <stdio.h>
#include <string.h>

#define RGB_START 16
#define RGB_MAX_FIELD 5
#define RGB_FIELD_RANGE 6
#define RGB_END 231
#define GREY_START 232
#define GREY_MAX_LEVEL 23

static uint32_t color_sqdist(gmb_cell_color_t a, gmb_cell_color_t b)
{
    int r = (int)a.r - (int)b.r;
    int g = (int)a.g - (int)b.g;
    int bl = (int)a.b - (int)b.b;
    return (uint32_t)(r * r + g * g + bl * bl);
}

int gmb_colorize_rgb_to_ansi256(gmb_cell_color_t c)
{
    int pr = (int)c.r * RGB_MAX_FIELD / 255;
    int pg = (int)c.g * RGB_MAX_FIELD / 255;
    int pb = (int)c.b * RGB_MAX_FIELD / 255;
    int palette = RGB_START + RGB_FIELD_RANGE * RGB_FIELD_RANGE * pr + RGB_FIELD_RANGE * pg + pb;
    int grey = GREY_START + (int)((c.r + c.g + c.b) / (GREY_MAX_LEVEL * 3));
    gmb_cell_color_t pal = gmb_colorize_ansi256_to_rgb(palette);
    gmb_cell_color_t gr = gmb_colorize_ansi256_to_rgb(grey);
    return color_sqdist(c, pal) <= color_sqdist(c, gr) ? palette : grey;
}

gmb_cell_color_t gmb_colorize_ansi256_to_rgb(int idx)
{
    gmb_cell_color_t out;
    out.is_default = false;
    if (idx >= GREY_START)
    {
        int level = idx - GREY_START;
        uint8_t v = (uint8_t)((level * 255 + GREY_MAX_LEVEL / 2) / GREY_MAX_LEVEL);
        out.r = v; out.g = v; out.b = v;
        return out;
    }
    int n = idx - RGB_START;
    int step = 255 / RGB_MAX_FIELD;
    int r = n / (RGB_FIELD_RANGE * RGB_FIELD_RANGE);
    int g = n / RGB_FIELD_RANGE % RGB_FIELD_RANGE;
    int b = n % RGB_FIELD_RANGE;
    out.r = (uint8_t)(r * step);
    out.g = (uint8_t)(g * step);
    out.b = (uint8_t)(b * step);
    return out;
}

int gmb_colorize_sgr(bool bold, bool dim, bool italic, bool underline,
                     bool has_fg, gmb_cell_color_t fg, bool has_bg, gmb_cell_color_t bg,
                     char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    char codes[8][24];
    int n = 0;
    if (bold) snprintf(codes[n++], 24, "1");
    if (dim) snprintf(codes[n++], 24, "2");
    if (italic) snprintf(codes[n++], 24, "3");
    if (underline) snprintf(codes[n++], 24, "4");
    if (has_fg && !fg.is_default) snprintf(codes[n++], 24, "38;2;%u;%u;%u", fg.r, fg.g, fg.b);
    if (has_bg && !bg.is_default) snprintf(codes[n++], 24, "48;2;%u;%u;%u", bg.r, bg.g, bg.b);
    if (n == 0)
    {
        snprintf(out, cap, "%s", GMB_ANSI_RESET);
        return (int)strlen(out);
    }
    int o = snprintf(out, cap, "%s", GMB_ANSI_CSI);
    for (int i = 0; i < n; i++)
        o += snprintf(out + o, cap - o, "%s%s", i > 0 ? ";" : "", codes[i]);
    if (o < cap - 1) out[o++] = 'm';
    out[o] = '\0';
    return o;
}

/* ===================== DoubleBuffer ===================== */

void gmb_double_buffer_new(gmb_double_buffer_t* db)
{
    memset(db, 0, sizeof(*db));
}

void gmb_double_buffer_free(gmb_double_buffer_t* db)
{
    if (!db) return;
    gmb_screen_free(db->previous);
    db->previous = NULL;
}

static bool cell_equal(gmb_screen_cell_t a, gmb_screen_cell_t b)
{
    return a.character == b.character
        && gmb_cellcolor_eq(a.fg_rgb, b.fg_rgb)
        && gmb_cellcolor_eq(a.bg_rgb, b.bg_rgb);
}

int gmb_double_buffer_compute_diff(gmb_double_buffer_t* db, gmb_screen_buffer_t* current,
                                   gmb_cell_change_t* out, int max)
{
    if (!db || !current || !out || max <= 0) return 0;
    gmb_screen_buffer_t* prev = db->previous;
    int h = gmb_screen_height(current);
    int w = gmb_screen_width(current);
    gmb_screen_cell_t* cur = gmb_screen_cells(current);
    gmb_screen_cell_t* prv = prev ? gmb_screen_cells(prev) : NULL;
    int pw = prev ? gmb_screen_width(prev) : 0;
    int ph = prev ? gmb_screen_height(prev) : 0;
    int count = 0;
    for (int y = 0; y < h && count < max; y++)
    {
        for (int x = 0; x < w && count < max; x++)
        {
            gmb_screen_cell_t c = cur[(size_t)y * (size_t)w + (size_t)x];
            bool changed;
            if (!prv || y >= ph || x >= pw)
                changed = true;
            else
            {
                gmb_screen_cell_t p = prv[(size_t)y * (size_t)pw + (size_t)x];
                changed = !cell_equal(c, p);
            }
            if (changed)
            {
                gmb_cell_change_t* ch = &out[count++];
                ch->x = x;
                ch->y = y;
                ch->ch = c.character;
                ch->fg = c.fg_rgb;
                ch->bg = c.bg_rgb;
            }
        }
    }
    return count;
}

static int db_append(char* out, int cap, int o, const char* s)
{
    for (const char* p = s; *p && o < cap - 1; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

static int db_append_fg(char* out, int cap, int o, gmb_cell_color_t fg)
{
    if (fg.is_default)
        return db_append(out, cap, o, GMB_ANSI_RESET);
    char tmp[64];
    gmb_ansi_fgrgb(fg.r, fg.g, fg.b, tmp, sizeof(tmp));
    return db_append(out, cap, o, tmp);
}

static int db_append_bg(char* out, int cap, int o, gmb_cell_color_t bg)
{
    if (bg.is_default)
        return db_append(out, cap, o, GMB_ANSI_RESET);
    char tmp[64];
    gmb_ansi_bgrgb(bg.r, bg.g, bg.b, tmp, sizeof(tmp));
    return db_append(out, cap, o, tmp);
}

int gmb_double_buffer_present(gmb_double_buffer_t* db, gmb_screen_buffer_t* current, char* out, int cap)
{
    if (!db || !current || !out || cap <= 0) return 0;
    gmb_screen_buffer_t* prev = db->previous;
    int h = gmb_screen_height(current);
    int w = gmb_screen_width(current);
    gmb_screen_cell_t* cur = gmb_screen_cells(current);
    gmb_screen_cell_t* prv = prev ? gmb_screen_cells(prev) : NULL;
    int pw = prev ? gmb_screen_width(prev) : 0;
    int ph = prev ? gmb_screen_height(prev) : 0;
    int o = 0;
    int cur_x = -1, cur_y = -1;
    gmb_cell_color_t cur_fg = GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t cur_bg = GMB_CELLCOLOR_DEFAULT;
    bool style_set = false;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            gmb_screen_cell_t c = cur[(size_t)y * (size_t)w + (size_t)x];
            bool changed;
            if (!prv || y >= ph || x >= pw)
                changed = true;
            else
            {
                gmb_screen_cell_t p = prv[(size_t)y * (size_t)pw + (size_t)x];
                changed = !cell_equal(c, p);
            }
            if (!changed) { cur_x = x + 1; cur_y = y; continue; }

            if (cur_x != x || cur_y != y)
            {
                char tmp[32];
                gmb_ansi_moveto(x + 1, y + 1, tmp, sizeof(tmp));
                o = db_append(out, cap, o, tmp);
            }
            cur_x = x + 1;
            cur_y = y;

            gmb_cell_color_t fg = c.fg_rgb;
            gmb_cell_color_t bg = c.bg_rgb;
            if (!style_set || !gmb_cellcolor_eq(fg, cur_fg))
            {
                o = db_append_fg(out, cap, o, fg);
                cur_fg = fg;
            }
            if (!style_set || !gmb_cellcolor_eq(bg, cur_bg))
            {
                o = db_append_bg(out, cap, o, bg);
                cur_bg = bg;
            }
            style_set = true;

            if (o < cap - 1) out[o++] = (char)c.character;
        }
    }
    out[o] = '\0';

    if (db->previous) gmb_screen_free(db->previous);
    db->previous = gmb_screen_clone(current);
    return o;
}

void gmb_double_buffer_invalidate(gmb_double_buffer_t* db)
{
    if (!db) return;
    gmb_screen_free(db->previous);
    db->previous = NULL;
}
