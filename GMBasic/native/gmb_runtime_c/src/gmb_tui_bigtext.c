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

static const uint8_t bigfont_letters[26][7] = {
    {0x04,0x0A,0x0A,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x12,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}  /* Z */
};
static const uint8_t bigfont_digits[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}  /* 9 */
};

static const uint8_t* bigfont_row(int key, int* out_width)
{
    if (out_width) *out_width = 5;
    if (key >= 'a' && key <= 'z') key = key - 'a' + 'A';
    if (key >= 'A' && key <= 'Z') return bigfont_letters[key - 'A'];
    if (key >= '0' && key <= '9') return bigfont_digits[key - '0'];
    return NULL;
}

static void bigfont_row_string(uint32_t key, int row, char* out, int cap)
{
    const uint8_t* g = bigfont_row((int)key, NULL);
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

int gmb_bigtext_get(uint32_t key, char rows[7][8])
{
    const uint8_t* g = bigfont_row((int)key, NULL);
    if (!g) return 0;
    for (int r = 0; r < 7; r++) bigfont_row_string(key, r, rows[r], 8);
    return 1;
}
int gmb_bigtext_width(uint32_t key)
{
    return bigfont_row((int)key, NULL) ? 5 : 0;
}
int gmb_bigtext_to_block(const char* text, char out[7][512])
{
    for (int r = 0; r < 7; r++) out[r][0] = '\0';
    if (!text) return 0;
    for (const char* p = text; *p; p++)
    {
        uint32_t key = (unsigned char)*p;
        for (int r = 0; r < 7; r++)
        {
            char row[8];
            bigfont_row_string(key, r, row, 8);
            int cur = (int)strlen(out[r]);
            if (cur + 6 < 512)
            {
                if (p != text) out[r][cur++] = ' ';
                strncat(out[r], row, 6);
            }
        }
    }
    return 7;
}

static void bigtext_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_big_text_t* b = (gmb_big_text_t*)u;
    gmb_cell_color_t fg = b->has_color ? b->color : GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t bg = b->has_bg ? b->bg : GMB_CELLCOLOR_DEFAULT;
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        int x2 = c->x, y2 = c->y;
        for (const char* p = b->text; *p; p++)
        {
            const uint8_t* g = bigfont_row((unsigned char)*p, NULL);
            if (!g) continue;
            for (int r = 0; r < 7; r++)
                for (int cc = 0; cc < 5; cc++)
                    if (g[r] & (1 << (4 - cc)))
                        gmb_surface_rounded_rect(buf, x2 + cc, y2 + r, 1, 1, 0, fg, fg, 1);
            x2 += 6;
        }
        return;
    }
    int x = c->x, y = c->y;
    for (const char* p = b->text; *p; p++)
    {
        const uint8_t* g = bigfont_row((unsigned char)*p, NULL);
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int cc = 0; cc < 5; cc++)
                if (g[r] & (1 << (4 - cc)))
                    gmb_screen_setchar(buf, x + cc, y + r, 0x2588, fg, bg);
        x += 6;
    }
}
static void bigtext_draw_px(void* u, gmb_control_t* c)
{
    gmb_big_text_t* b = (gmb_big_text_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    const char* op = gmb_gfx_font_path(); char ops[160]; snprintf(ops, sizeof(ops), "%s", op ? op : "");
    int os = gmb_gfx_font_logical_size();   /* LOGICO: gmb_gfx_font riapplica la scala */
    int bs = c->height - 6; if (bs < 48) bs = 48;   /* BIGTEXT: dimensione leggibile anche con h di default */
    gmb_gfx_font(ops, bs);
    int col = (int)(0x40000000u | (st.text & 0xFFFFFFu));
    gmb_gfx_draw_text(b->text, c->x + 2, c->y + 3, 0, col);
    gmb_gfx_font(ops, os);
}
void gmb_big_text_init(gmb_big_text_t* b, int x, int y)
{
    memset(b, 0, sizeof(*b));
    memset(b, 0, sizeof(*b));
    gmb_control_init(&b->ctrl, "", x, y, 40, 7, bigtext_draw, NULL);
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
    for (const char* p = text; *p; p++)
    {
        const uint8_t* g = bigfont_row((unsigned char)*p, NULL);
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int cc = 0; cc < 5; cc++)
                if (g[r] & (1 << (4 - cc)))
                    gmb_screen_setchar(buf, cx + cc, y + r, 0x2588, f, b);
        cx += 6;
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
    for (const char* p = b->text; *p; p++)
    {
        if (bigfont_row((unsigned char)*p, NULL)) wsum += (p == b->text) ? 5 : 6;
    }
    if (w) *w = wsum;
    if (h) *h = 7;
}
