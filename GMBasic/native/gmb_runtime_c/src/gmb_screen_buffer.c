/* gmb_screen_buffer.c — buffer di schermo a celle (era in gmb_tui.c).
 *
 * ESTRATTO in file/target separato: la console testuale (C# ScreenBuffer/CClipperBuffer) usa
 * SOLO queste funzioni. Tenendole in gmb_tui.c, per avere un buffer di schermo il processo
 * doveva caricare `libgmb_gfx` e con essa SDL2/SDL3/SDL2_ttf/freetype/harfbuzz/MoltenVK
 * (~8 MB) anche per un `PRINT`. Ora: `libgmb_screen.dylib` (~30 KB, zero terze parti).
 */
#include "gmb_tui.h"
#include "gmb_screen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool gmb_cellcolor_eq(gmb_cell_color_t a, gmb_cell_color_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.is_default == b.is_default;
}

size_t gmb_cellcolor_tostr(gmb_cell_color_t c, char* out, size_t out_cap)
{
    if (c.is_default)
    {
        static const char def[] = "default";
        size_t n = sizeof(def) - 1;
        if (out_cap < n + 1) return 0;
        memcpy(out, def, n + 1);
        return n;
    }
    if (out_cap < 8) return 0;   /* "#RRGGBB\0" = 8 */
    snprintf(out, out_cap, "#%02X%02X%02X", c.r, c.g, c.b);
    return 7;
}

bool gmb_cell_eq(gmb_screen_cell_t a, gmb_screen_cell_t b)
{
    if (a.character != b.character) return false;
    if (a.fg_palette != b.fg_palette || a.bg_palette != b.bg_palette) return false;
    if (a.attrs != b.attrs) return false;
    if (!gmb_cellcolor_eq(a.fg_rgb, b.fg_rgb)) return false;
    if (!gmb_cellcolor_eq(a.bg_rgb, b.bg_rgb)) return false;
    return true;
}

size_t gmb_ansi_fgrgb(uint8_t r, uint8_t g, uint8_t b, char* out, size_t out_cap)
{
    int n = snprintf(out, out_cap, "\x1b[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return n < 0 ? 0 : (size_t)n;
}

size_t gmb_ansi_bgrgb(uint8_t r, uint8_t g, uint8_t b, char* out, size_t out_cap)
{
    int n = snprintf(out, out_cap, "\x1b[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return n < 0 ? 0 : (size_t)n;
}

size_t gmb_ansi_fg256(int idx, char* out, size_t out_cap)
{
    int n = snprintf(out, out_cap, "\x1b[38;5;%dm", idx);
    return n < 0 ? 0 : (size_t)n;
}

size_t gmb_ansi_bg256(int idx, char* out, size_t out_cap)
{
    int n = snprintf(out, out_cap, "\x1b[48;5;%dm", idx);
    return n < 0 ? 0 : (size_t)n;
}

size_t gmb_ansi_moveto(int col, int row, char* out, size_t out_cap)
{
    int n = snprintf(out, out_cap, "\x1b[%d;%dH", row, col);
    return n < 0 ? 0 : (size_t)n;
}

/* ============================================================================
 * ScreenBuffer
 * ============================================================================ */

gmb_screen_buffer_t* gmb_screen_new(int width, int height)
{
    if (width <= 0 || height <= 0) return NULL;
    gmb_screen_buffer_t* b = (gmb_screen_buffer_t*)calloc(1, sizeof(gmb_screen_buffer_t));
    if (!b) return NULL;
    b->width = width;
    b->height = height;
    b->cells = (gmb_screen_cell_t*)calloc((size_t)width * height, sizeof(gmb_screen_cell_t));
    if (!b->cells) { free(b); return NULL; }
    gmb_screen_clear(b, GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK);
    return b;
}

void gmb_screen_free(gmb_screen_buffer_t* b)
{
    if (!b) return;
    free(b->cells);
    free(b);
}

int gmb_screen_width(const gmb_screen_buffer_t* b) { return b ? b->width : 0; }
int gmb_screen_height(const gmb_screen_buffer_t* b) { return b ? b->height : 0; }
gmb_screen_cell_t* gmb_screen_cells(gmb_screen_buffer_t* b) { return b ? b->cells : NULL; }

void gmb_screen_set_sink(gmb_screen_buffer_t* b, gmb_screen_sink_t* sink)
{
    if (!b) return;
    if (b->sink && b->sink != sink && b->sink->destroy) b->sink->destroy(b->sink->user);
    b->sink = sink;
}
gmb_screen_sink_t* gmb_screen_get_sink(gmb_screen_buffer_t* b) { return b ? b->sink : NULL; }
bool gmb_screen_has_sink(const gmb_screen_buffer_t* b) { return b && b->sink != NULL; }
void gmb_screen_detach_sink(gmb_screen_buffer_t* b)
{
    if (!b || !b->sink) return;
    if (b->sink->destroy) b->sink->destroy(b->sink->user);
    b->sink = NULL;
}

void gmb_screen_resize(gmb_screen_buffer_t* b, int width, int height)
{
    if (!b || width <= 0 || height <= 0) return;
    gmb_screen_cell_t* old = b->cells;
    int ow = b->width, oh = b->height;
    gmb_screen_cell_t* nc = (gmb_screen_cell_t*)calloc((size_t)width * height, sizeof(gmb_screen_cell_t));
    if (!nc) return;
    for (int y = 0; y < height && y < oh; y++)
        for (int x = 0; x < width && x < ow; x++)
            nc[(size_t)y * width + x] = old[(size_t)y * ow + x];
    /* celle nuove restano a zero (attributi/color default, char 0) */
    free(old);
    b->cells = nc;
    b->width = width;
    b->height = height;
}

void gmb_screen_clear(gmb_screen_buffer_t* b, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    if (!b) return;
    if (b->sink) { b->sink->fill(b->sink->user, 0, 0, b->width, b->height, bg); return; }
    if (!b->cells) return;
    gmb_screen_cell_t empty;
    empty.character = ' ';
    empty.fg_palette = -1; empty.bg_palette = -1;
    empty.attrs = 0;
    empty.fg_rgb = fg;
    empty.bg_rgb = bg;
    for (int i = 0; i < b->width * b->height; i++)
        b->cells[i] = empty;
}

void gmb_screen_setchar(gmb_screen_buffer_t* b, int x, int y, uint32_t ch,
                        gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    if (!b) return;
    if (x < 0 || x >= b->width || y < 0 || y >= b->height) return;
    if (b->sink) { b->sink->put_cell(b->sink->user, x, y, ch, fg, bg); return; }
    if (!b->cells) return;
    gmb_screen_cell_t* c = &b->cells[(size_t)y * b->width + x];
    c->character = ch;
    c->fg_palette = -1; c->bg_palette = -1;
    c->fg_rgb = fg;
    c->bg_rgb = bg;
}

/* Converte una stringa UTF-8 in codepoint (il primo carattere). */
static uint32_t utf8_first(const char* s)
{
    const unsigned char* u = (const unsigned char*)s;
    if (u[0] < 0x80) return u[0];
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80)
        return ((uint32_t)(u[0] & 0x1F) << 6) | (u[1] & 0x3F);
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80)
        return ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80)
        return ((uint32_t)(u[0] & 0x07) << 18) | ((uint32_t)(u[1] & 0x3F) << 12)
             | ((uint32_t)(u[2] & 0x3F) << 6) | (u[3] & 0x3F);
    return 0xFFFD; /* replacement char */
}

/* Larghezza in byte del primo carattere UTF-8. */
static int utf8_len(const char* s)
{
    const unsigned char* u = (const unsigned char*)s;
    if (u[0] < 0x80) return 1;
    if ((u[0] & 0xE0) == 0xC0) return 2;
    if ((u[0] & 0xF0) == 0xE0) return 3;
    if ((u[0] & 0xF8) == 0xF0) return 4;
    return 1;
}

int gmb_screen_setstring(gmb_screen_buffer_t* b, int x, int y, const char* s,
                         gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    if (!b || !s) return x;
    int cx = x;
    const char* p = s;
    while (*p)
    {
        gmb_screen_setchar(b, cx, y, utf8_first(p), fg, bg);
        p += utf8_len(p);
        cx++;
    }
    return cx;
}

int gmb_screen_setstring_n(gmb_screen_buffer_t* b, int x, int y, const char* s,
                           int max_chars, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    if (!b || !s) return x;
    int cx = x;
    const char* p = s;
    int n = 0;
    while (*p && n < max_chars)
    {
        gmb_screen_setchar(b, cx, y, utf8_first(p), fg, bg);
        p += utf8_len(p);
        cx++;
        n++;
    }
    return cx;
}

void gmb_screen_fillrect(gmb_screen_buffer_t* b, int top, int left, int bottom, int right,
                         uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    if (!b) return;
    for (int y = top; y <= bottom && y < b->height; y++)
        for (int x = left; x <= right && x < b->width; x++)
            gmb_screen_setchar(b, x, y, ch, fg, bg);
}

gmb_screen_buffer_t* gmb_screen_clone(const gmb_screen_buffer_t* b)
{
    if (!b) return NULL;
    gmb_screen_buffer_t* n = gmb_screen_new(b->width, b->height);
    if (!n) return NULL;
    memcpy(n->cells, b->cells, (size_t)b->width * b->height * sizeof(gmb_screen_cell_t));
    return n;
}

void gmb_screen_get_cell(const gmb_screen_buffer_t* b, int x, int y, int* out)
{
    if (!out) return;
    for (int i = 0; i < 10; i++) out[i] = 0;
    if (!b || x < 0 || y < 0 || x >= b->width || y >= b->height) return;
    const gmb_screen_cell_t* c = &b->cells[(size_t)y * b->width + x];
    out[0] = (int)c->character;
    out[1] = c->fg_rgb.r; out[2] = c->fg_rgb.g; out[3] = c->fg_rgb.b;
    out[4] = c->fg_rgb.is_default ? 1 : 0;
    out[5] = c->bg_rgb.r; out[6] = c->bg_rgb.g; out[7] = c->bg_rgb.b;
    out[8] = c->bg_rgb.is_default ? 1 : 0;
    out[9] = (int)c->attrs;
}

void gmb_screen_set_cell(gmb_screen_buffer_t* b, int x, int y, int ch, int attrs,
                         int fg_r, int fg_g, int fg_b, int fg_def,
                         int bg_r, int bg_g, int bg_b, int bg_def)
{
    if (!b || x < 0 || y < 0 || x >= b->width || y >= b->height) return;
    gmb_screen_cell_t* c = &b->cells[(size_t)y * b->width + x];
    c->character = (uint32_t)ch;
    c->attrs = (uint8_t)attrs;
    c->fg_rgb.r = (uint8_t)fg_r; c->fg_rgb.g = (uint8_t)fg_g; c->fg_rgb.b = (uint8_t)fg_b;
    c->fg_rgb.is_default = fg_def != 0;
    c->bg_rgb.r = (uint8_t)bg_r; c->bg_rgb.g = (uint8_t)bg_g; c->bg_rgb.b = (uint8_t)bg_b;
    c->bg_rgb.is_default = bg_def != 0;
    c->fg_palette = -1; c->bg_palette = -1;
}

/* ============================================================================
 * GridLayout — suddivisione rettangolare (specchio GridLayout.cs, puro).
 * ============================================================================ */
