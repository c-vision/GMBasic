/* ============================================================================
 * gmb_tui_starrating.c — gmb_tui_starrating. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

/* ===================== StarRating ===================== */

static void starrating_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_star_rating_t* s = (gmb_star_rating_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 3, fg, bg, 1);
        char line[128];
        gmb_star_rating_render_line(s, line, (int)sizeof(line));
        GMB_GFX_DRAW_TEXT(line, sk->origin_x + c->x * sk->cell_w,
                          sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        return;
    }
    char line[128];
    gmb_star_rating_render_line(s, line, (int)sizeof(line));
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
}
static bool starrating_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_star_rating_t* s = (gmb_star_rating_t*)u;
    if (key == GMB_K_RIGHT) { if (s->value < s->max) s->value++; return true; }
    if (key == GMB_K_LEFT) { if (s->value > 0) s->value--; return true; }
    return false;
}
static int star_rgb(unsigned c) { return (int)(0x40000000u | (c & 0xFFFFFFu)); }
static bool starrating_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    /* Click su una stella imposta il valore (1..max). Coordinate in CELLE
     * (il dispatch px -> cella e' gia' avvenuto). */
    if (!a || !c || (a->kind != GMB_MA_CLICK && a->kind != GMB_MA_RELEASE)) return false;
    gmb_star_rating_t* s = (gmb_star_rating_t*)u;
    int n = s->max > 0 ? s->max : 5;
    if (c->width < n || n < 1) return false;
    int idx = (a->x - c->x) * n / c->width;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    int nv = idx + 1;
    if (nv == s->value) return true;
    s->value = nv;
    return true;
}
static void starrating_draw_px(void* u, gmb_control_t* c)
{
    /* Stelle come glifi del font (★/☆): nitide a ogni dimensione e colorate dal tema.
     * Si disegna prima la riga "vuota" (bordo) e poi il prefisso pieno (accent) sopra. */
    gmb_star_rating_t* s = (gmb_star_rating_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int n = s->max > 0 ? s->max : 5;
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (n < 1 || w < 8 || h < 6) return;
    unsigned on = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    unsigned off = st.border;
    char empty[256], filled[256];
    static const char STAR_ON[]  = "\xE2\x98\x85";
    static const char STAR_OFF[] = "\xE2\x98\x86";
    int o = 0;
    for (int i = 0; i < n && o + 4 < (int)sizeof(empty); i++) {
        empty[o++] = STAR_OFF[0]; empty[o++] = STAR_OFF[1]; empty[o++] = STAR_OFF[2];
    }
    empty[o] = 0;
    filled[0] = 0;
    int th = GMB_GFX_TEXT_H(); if (th <= 0) th = 16;
    int ty = y + (h - th) / 2;
    GMB_GFX_DRAW_TEXT(empty, x, ty, 0, star_rgb(off));
    if (s->value > 0) {
        int o2 = 0, lim = s->value < n ? s->value : n;
        for (int i = 0; i < lim && o2 + 4 < (int)sizeof(filled); i++) {
            filled[o2++] = STAR_ON[0]; filled[o2++] = STAR_ON[1]; filled[o2++] = STAR_ON[2];
        }
        filled[o2] = 0;
        GMB_GFX_DRAW_TEXT(filled, x, ty, 0, star_rgb(on));
    }
}
void gmb_star_rating_init(gmb_star_rating_t* s, int x, int y, int w)
{
    memset(s, 0, sizeof(*s));
    memset(s, 0, sizeof(*s));
    gmb_control_init(&s->ctrl, "", x, y, w, 1, starrating_draw, starrating_key);
    s->ctrl.draw_px_fn = starrating_draw_px;
    s->ctrl.handle_mouse_fn = starrating_mouse;
    s->ctrl.user = s;
    s->max = 5;
}
int gmb_star_rating_render_line(gmb_star_rating_t* s, char* out, int cap)
{
    int o = 0;
    for (int i = 0; i < s->max && o < cap - 3; i++)
    {
        const char* star = (i < s->value) ? "\xE2\x98\x85" : "\xE2\x98\x86";
        out[o++] = star[0]; out[o++] = star[1]; out[o++] = star[2];
    }
    out[o] = '\0';
    return o;
}
