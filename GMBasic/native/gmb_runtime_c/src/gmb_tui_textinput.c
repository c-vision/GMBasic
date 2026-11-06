/* ============================================================================
 * gmb_tui_textinput.c — gmb_tui_textinput. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== TextInput ===================== */

static void textinput_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_text_input_t* t = (gmb_text_input_t*)u;
    bool error = c->has_error || !gmb_text_input_is_valid(t);
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, error, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 3, fg, bg, 1);
        char shown[512];
        gmb_text_input_render_line(t, shown, (int)sizeof(shown));
        if (shown[0])
            gmb_gfx_draw_text(shown, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        return;
    }
    char fill[512];
    for (int i = 0; i < c->width && i < 511; i++) fill[i] = c->style.fill_char;
    fill[c->width < 511 ? c->width : 511] = '\0';
    gmb_screen_setstring(buf, c->x, c->y, fill, fg, bg);
    char shown[512];
    gmb_text_input_render_line(t, shown, (int)sizeof(shown));
    /* TV TInputLine: il testo inizia a col 1 (una cella di margine). */
    for (int i = 0; i < c->width - 1 && i < (int)strlen(shown); i++)
        gmb_screen_setchar(buf, c->x + 1 + i, c->y, (unsigned char)shown[i], fg, bg);
    if (c->focused && t->cursor < c->width - 1 && t->blink_on)
    {
        int sl = (int)strlen(shown);
        uint32_t ch = t->cursor < sl ? (unsigned char)shown[t->cursor] : (unsigned char)c->style.fill_char;
        gmb_screen_setchar(buf, c->x + 1 + t->cursor, c->y, ch, bg, fg);
    }
}
static bool textinput_key(void* u, gmb_control_t* c, int key)
{
    gmb_text_input_t* t = (gmb_text_input_t*)u;
    if (!c->enabled || t->read_only) return false;
    if (key >= 32 && key <= 126)
    {
        int tl = (int)strlen(t->text);
        if (tl < t->max_length && tl < 511)
        {
            memmove(t->text + t->cursor + 1, t->text + t->cursor, (size_t)(tl - t->cursor) + 1);
            t->text[t->cursor] = (char)key;
            t->cursor++;
        }
        return true;
    }
    switch (key)
    {
        case GMB_K_BS:
            if (t->cursor > 0)
            {
                memmove(t->text + t->cursor - 1, t->text + t->cursor, strlen(t->text + t->cursor) + 1);
                t->cursor--;
            }
            return true;
        case GMB_K_DEL:
            if (t->cursor < (int)strlen(t->text))
                memmove(t->text + t->cursor, t->text + t->cursor + 1, strlen(t->text + t->cursor + 1) + 1);
            return true;
        case GMB_K_LEFT:
            if (t->cursor > 0) t->cursor--;
            return true;
        case GMB_K_RIGHT:
            if (t->cursor < (int)strlen(t->text)) t->cursor++;
            return true;
        case GMB_K_HOME: t->cursor = 0; return true;
        case GMB_K_END: t->cursor = (int)strlen(t->text); return true;
    }
    return false;
}
static bool textinput_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_text_input_t* t = (gmb_text_input_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int click_col = x - c->x;
    if (click_col >= 0)
    {
        int tl = (int)strlen(t->text);
        t->cursor = click_col < tl ? click_col : tl;
    }
    return true;
}

/* Render PIXEL del campo (schermo grafico): card + testo + caret. Usato da
 * gmb_tui_draw_px_all quando i controlli sono disegnati sulla pagina. */
static void textinput_draw_px(void* u, gmb_control_t* c)
{
    extern void gmb_gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba);
    extern void gmb_gfx_draw_text(const char* text, int x, int y, int align, int color);
    extern int  gmb_gfx_text_width(const char* text);
    extern long long gmb_c99_now_ms(void);
    gmb_text_input_t* t = (gmb_text_input_t*)u;
    int x = c->px_x, y = c->px_y, w = c->px_w, h = c->px_h;
    if (w <= 0 || h <= 0) return;
    /* Focus: anello accent visibile (2px) attorno alla card. */
    if (c->focused) gmb_gfx_fill_rounded_rect(x - 2, y - 2, w + 4, h + 4, 9, 0x406C5CE7u);
    gmb_gfx_fill_rounded_rect(x, y, w, h, 8, 0x402A2A3Au);
    int ty = y + (h - 16) / 2;
    if (t->text[0]) gmb_gfx_draw_text(t->text, x + 10, ty, 0, 0x40FFFFFF);
    if (c->focused) {
        /* Caret lampeggiante (~500 ms) alla posizione del cursore di testo. */
        long long ms = gmb_c99_now_ms();
        if (((ms / 500) % 2) == 0) {
            char pref[512]; int cur = t->cursor;
            if (cur < 0) cur = 0;
            if (cur > (int)sizeof(pref) - 1) cur = (int)sizeof(pref) - 1;
            memcpy(pref, t->text, (size_t)cur); pref[cur] = 0;
            /* Posizione REALE di fine testo: stesso renderer (niente misure sfasate). */
            extern int gmb_gfx_text_end_x(const char*, int, int);
            int cx = pref[0] ? gmb_gfx_text_end_x(pref, x + 10, ty) : (x + 10);
            /* Caret a LINEA sottile: con font proporzionale il blocco non
             * rispecchia la larghezza del carattere. */
            /* Altezza del caret = altezza dei glifi MA dentro la card. */
            int th = 16; extern int gmb_gfx_text_height(void); th = gmb_gfx_text_height();
            if (th > h - 8) th = h - 8;
            if (th < 8) th = 8;
            int cy = y + (h - th) / 2;
            if (cx < x + 8) cx = x + 8;
            if (cx > x + w - 6) cx = x + w - 6;
            gmb_gfx_fill_rounded_rect(cx, cy, 2, th, 1, 0x40FFC048u);
        }
    }
}

void gmb_text_input_init(gmb_text_input_t* t, int x, int y, int w)
{
    memset(t, 0, sizeof(*t));
    gmb_control_init(&t->ctrl, "", x, y, w, 1, textinput_draw, textinput_key);
    t->ctrl.handle_click_fn = textinput_click;
    t->ctrl.draw_px_fn = textinput_draw_px;
    t->ctrl.user = t;
    t->max_length = 50;
    t->blink_on = true;
}
void gmb_text_input_set_text(gmb_text_input_t* t, const char* text)
{
    snprintf(t->text, sizeof(t->text), "%s", text ? text : "");
    t->cursor = (int)strlen(t->text);
}
bool gmb_text_input_is_valid(gmb_text_input_t* t)
{
    return t->validator ? t->validator(t->validator_ctx, t->text) : true;
}
int gmb_text_input_render_line(gmb_text_input_t* t, char* out, int cap)
{
    int o = 0;
    int tl = (int)strlen(t->text);
    if (t->password_mode)
    {
        for (int i = 0; i < tl && o < t->ctrl.width && o < cap - 1; i++) out[o++] = '*';
    }
    else
    {
        for (int i = 0; i < tl && o < t->ctrl.width && o < cap - 1; i++)
        {
            char ch = t->text[i];
            if (t->ctrl.uppercase && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
            out[o++] = ch;
        }
    }
    for (int i = o; i < t->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
