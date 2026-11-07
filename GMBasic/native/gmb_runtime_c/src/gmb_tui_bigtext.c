/* ============================================================================
 * gmb_tui_bigtext.c — gmb_tui_bigtext. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== BigText ===================== */

/* Tabella dei glifi nel modulo dedicato (gmb_tui_bigfont.c): unica fonte. */
static const uint8_t* bigfont_row(uint32_t key, int* out_width)
{
    return gmb_bigfont_row(key, out_width);
}

/* Decodifica UN codepoint UTF-8; ritorna i byte consumati (>=1). */
static int bt_utf8(const char* s, uint32_t* cp)
{
    const unsigned char* p = (const unsigned char*)s;
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
    { *cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F); return 2; }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
    { *cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F); return 3; }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80)
    { *cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12)
           | ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F); return 4; }
    *cp = p[0];
    return 1;
}

static void bigfont_row_string(uint32_t key, int row, char* out, int cap)
{
    const uint8_t* g = bigfont_row(key, NULL);
    int o = 0;
    for (int c = 0; c < 5 && o < cap - 1; c++)
    {
        if (g && (g[row] & (1 << (4 - c))))
        {
            if (o + 3 < cap) { out[o++] = (char)0xE2; out[o++] = (char)0x96; out[o++] = (char)0x88; }
        }
        else out[o++] = ' ';
    }
    out[o] = '\0';
}

int gmb_bigtext_get(uint32_t key, char rows[7][32])
{
    const uint8_t* g = bigfont_row(key, NULL);
    if (!g) return 0;
    for (int r = 0; r < 7; r++) bigfont_row_string(key, r, rows[r], 32);
    return 1;
}
int gmb_bigtext_width(uint32_t key)
{
    return bigfont_row(key, NULL) ? 5 : 0;
}
int gmb_bigtext_to_block(const char* text, char out[7][512])
{
    for (int r = 0; r < 7; r++) out[r][0] = '\0';
    if (!text) return 0;
    const char* p = text;
    int first = 1;
    while (*p)
    {
        uint32_t key; int adv = bt_utf8(p, &key);
        for (int r = 0; r < 7; r++)
        {
            char row[32];
            bigfont_row_string(key, r, row, (int)sizeof(row));
            int cur = (int)strlen(out[r]);
            int len = (int)strlen(row);
            int need = len + (first ? 0 : 1);
            if (cur + need < 512)
            {
                if (!first) out[r][cur++] = ' ';
                memcpy(out[r] + cur, row, (size_t)len + 1);
            }
        }
        first = 0;
        p += adv;
    }
    return 7;
}

/* Misura del blocco: larghezza in CELLE (glifo 5 + 1 di spazio) e altezza 7.
 * Ritorna il numero di caratteri disegnabili (0 = stringa vuota). */
int gmb_bigtext_measure(const char* text, int* out_w, int* out_h)
{
    int w = 0, n = 0;
    if (text)
    {
        const char* p = text;
        while (*p) { uint32_t cp; int adv = bt_utf8(p, &cp); w += (n == 0) ? 5 : 6; n++; p += adv; }
    }
    if (out_w) *out_w = w;
    if (out_h) *out_h = 7;
    return n;
}

static void bigtext_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_big_text_t* b = (gmb_big_text_t*)u;
    gmb_theme_t th = gmb_ui_theme();
    gmb_cell_color_t fg = b->has_color ? b->color : th.text;
    gmb_cell_color_t bg = b->has_bg ? b->bg : th.window_bg;
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        int x2 = c->x, y2 = c->y;
        for (const char* p = b->text; *p; )
        {
            uint32_t cp; int adv = bt_utf8(p, &cp);
            const uint8_t* g = bigfont_row(cp, NULL);
            if (g)
                for (int r = 0; r < 7; r++)
                    for (int cc = 0; cc < 5; cc++)
                        if (g[r] & (1 << (4 - cc)))
                            gmb_surface_rounded_rect(buf, x2 + cc, y2 + r, 1, 1, 0, fg, fg, 1);
            x2 += 6;
            p += adv;
        }
        return;
    }
    int x = c->x, y = c->y;
    for (const char* p = b->text; *p; )
    {
        uint32_t cp; int adv = bt_utf8(p, &cp);
        const uint8_t* g = bigfont_row(cp, NULL);
        if (g)
            for (int r = 0; r < 7; r++)
                for (int cc = 0; cc < 5; cc++)
                    if (g[r] & (1 << (4 - cc)))
                        gmb_screen_setchar(buf, x + cc, y + r, 0x2588, fg, bg);
        x += 6;
        p += adv;
    }
}
static void bigtext_draw_px(void* u, gmb_control_t* c)
{
    gmb_big_text_t* b = (gmb_big_text_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    const char* op = gmb_gfx_font_path(); char ops[160]; snprintf(ops, sizeof(ops), "%s", op ? op : "");
    int os = gmb_gfx_font_logical_size();   /* LOGICO: gmb_gfx_font riapplica la scala */
    /* Rect del render pixel (px_*) quando disponibile: è lì che il renderer ha già
     * applicato lo spazio coordinate dichiarato (USE CELLS / metric). */
    int bx = (c->px_w > 0) ? c->px_x : c->x;
    int by = (c->px_h > 0) ? c->px_y : c->y;
    int bh = (c->px_h > 0) ? c->px_h : c->height;
    /* Dimensione: esplicita con STYLE "font=NN"; altrimenti derivata dall'altezza.
     * Prima era FORZATA a >= 48px: testo sempre enorme e non regolabile. */
    int bs = c->font_size > 0 ? c->font_size : (bh - 6);
    if (bs < 14) bs = 14;
    gmb_gfx_font(ops, bs);
    int col = (int)(0x40000000u | (st.text & 0xFFFFFFu));
    gmb_gfx_draw_text(b->text, bx + 2, by + 3, 0, col);
    gmb_gfx_font(ops, os);
}
void gmb_big_text_init(gmb_big_text_t* b, int x, int y)
{
    memset(b, 0, sizeof(*b));
    gmb_control_init(&b->ctrl, "", x, y, 40, 7, bigtext_draw, NULL);
    /* Dimensioni DERIVATE dal testo (il programma dichiara solo x,y): la conversione
     * celle→pixel non deve scalarle. */
    b->ctrl.coord_auto_w = 1; b->ctrl.coord_auto_h = 1;
    b->ctrl.draw_px_fn = bigtext_draw_px;
    b->ctrl.user = b;
}
void gmb_big_text_set_text(gmb_big_text_t* b, const char* text)
{
    snprintf(b->text, sizeof(b->text), "%s", text ? text : "");
}
void gmb_bigtext_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, const char* text,
                                 const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (!text) return;
    gmb_cell_color_t f = fg ? *fg : GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t b = bg ? *bg : GMB_CELLCOLOR_DEFAULT;
    int cx = x;
    for (const char* p = text; *p; )
    {
        uint32_t cp; int adv = bt_utf8(p, &cp);
        const uint8_t* g = bigfont_row(cp, NULL);
        if (g)
            for (int r = 0; r < 7; r++)
                for (int cc = 0; cc < 5; cc++)
                    if (g[r] & (1 << (4 - cc)))
                        gmb_screen_setchar(buf, cx + cc, y + r, 0x2588, f, b);
        cx += 6;
        p += adv;
    }
}

void gmb_big_text_render(gmb_big_text_t* b, gmb_screen_buffer_t* buffer, int x, int y)
{
    gmb_big_text_t tmp = *b;
    tmp.ctrl.x = x; tmp.ctrl.y = y;
    bigtext_draw(&tmp, &tmp.ctrl, buffer);
}
void gmb_big_text_dimensions(gmb_big_text_t* b, int* w, int* h)
{
    int wsum = 0;
    gmb_bigtext_measure(b ? b->text : NULL, &wsum, NULL);
    if (w) *w = wsum;
    if (h) *h = 7;
}
