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
    gmb_widget_t* s = (gmb_widget_t*)u;
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
    gmb_widget_t* s = (gmb_widget_t*)u;
    if (key == GMB_K_RIGHT) { if ((int)s->ival < (int)s->vmax) s->ival = (int)s->ival + 1; return true; }
    if (key == GMB_K_LEFT) { if ((int)s->ival > 0) s->ival = (int)s->ival - 1; return true; }
    return false;
}
static int star_rgb(unsigned c) { return (int)(0x40000000u | (c & 0xFFFFFFu)); }
static bool starrating_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    /* Click su una stella imposta il valore (1..max). Coordinate in CELLE
     * (il dispatch px -> cella e' gia' avvenuto). */
    if (!a || !c || (a->kind != GMB_MA_CLICK && a->kind != GMB_MA_RELEASE)) return false;
    gmb_widget_t* s = (gmb_widget_t*)u;
    int n = (int)s->vmax > 0 ? (int)s->vmax : 5;
    if (c->width < n || n < 1) return false;
    int idx = (a->x - c->x) * n / c->width;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    int nv = idx + 1;
    if (nv == (int)s->ival) return true;
    s->ival = nv;
    return true;
}
static void starrating_draw_px(void* u, gmb_control_t* c)
{
    /* Valutazione disegnata in PRIMITIVE (pip tondi): i glifi ★/☆ non esistono nel font
     * della UI e il controllo restava invisibile. Pieni = accent, vuoti = bordo. */
    gmb_widget_t* s = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int n = (int)s->vmax > 0 ? (int)s->vmax : 5;
    int x = (c->px_w > 0) ? c->px_x : c->x;
    int y = (c->px_h > 0) ? c->px_y : c->y;
    int h = (c->px_h > 2) ? c->px_h : (st.row_h > 0 ? st.row_h : 22);
    if (n < 1) return;
    int d = h - 4; if (d > 20) d = 20; if (d < 6) d = 6;
    int gap = 7;
    unsigned on  = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    unsigned off = st.border;
    int cy = y + (h - d) / 2;
    for (int i = 0; i < n; i++) {
        int cx = x + i * (d + gap);
        if (i < (int)s->ival) gmb_gfx_fill_rounded_rect(cx, cy, d, d, d / 2, on | 0xFF000000u);
        else              gmb_gfx_stroke_rounded_rect(cx, cy, d, d, d / 2, off | 0xFF000000u);
    }
}
void gmb_star_rating_init(gmb_star_rating_t* sr, int x, int y, int w)
{
    gmb_widget_t* s = (gmb_widget_t*)sr;
    memset(s, 0, sizeof(*s));
    gmb_control_init(&s->base, "", x, y, w, 1, starrating_draw, starrating_key);
    s->base.draw_px_fn = starrating_draw_px;
    s->base.handle_mouse_fn = starrating_mouse;
    s->base.user = s;
    s->kind = W_STAR;
    s->vmax = 5;
}
int gmb_star_rating_render_line(gmb_star_rating_t* s, char* out, int cap)
{
    int o = 0;
    for (int i = 0; i < (int)s->vmax && o < cap - 3; i++)
    {
        const char* star = (i < (int)s->ival) ? "\xE2\x98\x85" : "\xE2\x98\x86";
        out[o++] = star[0]; out[o++] = star[1]; out[o++] = star[2];
    }
    out[o] = '\0';
    return o;
}
