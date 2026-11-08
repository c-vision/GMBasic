/* ============================================================================
 * gmb_tui_scrollview.c — ScrollView.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ScrollView ===================== */

static void scrollview_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = s->first + i;
            if (idx >= s->nitems) break;
            char line[256];
            gmb_scroll_view_render_line(s, idx, line, (int)sizeof(line));
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = s->first + i;
        if (idx >= s->nitems) break;
        char line[256];
        gmb_scroll_view_render_line(s, idx, line, (int)sizeof(line));
        gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool scrollview_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int max_offset = s->nitems - c->height;
    if (max_offset < 0) max_offset = 0;
    switch (key)
    {
        case GMB_K_UP:
            if (s->first > 0) s->first--;
            return true;
        case GMB_K_DOWN:
            if (s->first < max_offset) s->first++;
            return true;
        case GMB_K_PGUP:
            s->first -= c->height;
            if (s->first < 0) s->first = 0;
            return true;
        case GMB_K_PGDN:
            s->first += c->height;
            if (s->first > max_offset) s->first = max_offset;
            return true;
        case GMB_K_HOME: s->first = 0; return true;
        case GMB_K_END: s->first = max_offset; return true;
    }
    return false;
}
static bool scrollview_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int max_offset = s->nitems - c->height;
    if (max_offset < 0) max_offset = 0;
    if (a->kind == GMB_MA_SCROLLUP) { if (s->first > 0) s->first--; return true; }
    if (a->kind == GMB_MA_SCROLLDOWN) { if (s->first < max_offset) s->first++; return true; }
    return false;
}
/* Renderer PIXEL della SCROLLVIEW — riscritto per la grafica (niente righe a celle):
 * card del tema, righe col passo del tema, zebra, troncamento al pixel e barra di
 * scorrimento proporzionale. Prima delegava a gmb_px_draw_ex (GMB_PX_LISTBOX), che
 * è il percorso a celle adattato alla grafica. */
static void scrollview_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = (c->px_w > 0) ? c->px_x : c->x;
    int y = (c->px_h > 0) ? c->px_y : c->y;
    int w = (c->px_w > 0) ? c->px_w : c->width;
    int h = (c->px_h > 0) ? c->px_h : c->height;
    if (w < 40 || h < 24) return;

    int rr = st.radius > 0 ? st.radius : 6;
    int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;
    int rh = st.row_h > 0 ? st.row_h : 24; if (rh < th + 6) rh = th + 6;
    int pad = 12;

    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, st.fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);

    int top = y + 6;
    int rows = (h - 12) / rh; if (rows < 1) rows = 1;
    int n = s->nitems > 64 ? 64 : s->nitems;
    int rail = (n > rows) ? 8 : 0;

    char line[512];
    for (int r = 0; r < rows; r++)
    {
        int idx = s->first + r;
        if (idx >= n) break;
        int ry = top + r * rh;
        if (r & 1) gmb_gfx_fill_rounded_rect(x + 2, ry, w - 4, rh, 0, (st.border & 0xFFFFFFu) | 0x14000000u);
        snprintf(line, sizeof(line), "%s", s->items[idx] ? s->items[idx] : "");
        int avail = w - 2 * pad - rail;
        int len = (int)strlen(line);
        while (len > 0 && gmb_gfx_text_width_logical(line) > avail) line[--len] = 0;
        gmb_gfx_draw_text(line, x + pad, ry + (rh - th) / 2, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    }
    if (rail)
    {
        int rx = x + w - 8, ry2 = top, rh2 = h - 12;
        gmb_gfx_fill_rounded_rect(rx, ry2, 4, rh2, 2, (st.border & 0xFFFFFFu) | 0x33000000u);
        int th2 = rh2 * rows / n; if (th2 < 16) th2 = 16;
        int maxo = n - rows; if (maxo < 1) maxo = 1;
        int off = s->first > maxo ? maxo : s->first;
        int ty2 = ry2 + (rh2 - th2) * off / maxo;
        gmb_gfx_fill_rounded_rect(rx, ty2, 4, th2, 2, (st.accent & 0xFFFFFFu) ? st.accent : st.fill_hover);
    }
    if (c->focused) gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.accent);
}
void gmb_scroll_view_init(gmb_scroll_view_t* sv, int x, int y, int w, int h)
{
    gmb_widget_t* s = (gmb_widget_t*)sv;
    memset(s, 0, sizeof(*s));
    gmb_control_init(&s->base, "", x, y, w, h, scrollview_draw, scrollview_key);
    s->base.draw_px_fn = scrollview_draw_px;
    s->base.handle_mouse_fn = scrollview_mouse;
    s->base.user = s;
    s->kind = W_LISTBOX;
}
void gmb_scroll_view_set_content(gmb_scroll_view_t* sv, const char* const* lines, int count)
{
    gmb_widget_t* s = (gmb_widget_t*)sv;
    s->nitems = count < 64 ? count : 64;
    for (int i = 0; i < s->nitems && lines; i++)
        snprintf(s->items[i], sizeof(s->items[i]), "%s", lines[i] ? lines[i] : "");
}
void gmb_scroll_view_push(gmb_scroll_view_t* sv, const char* line)
{
    gmb_widget_t* s = (gmb_widget_t*)sv;
    if (s->nitems >= 64) return;
    snprintf(s->items[s->nitems++], sizeof(s->items[0]), "%s", line ? line : "");
    int max_off = s->nitems - s->base.height;
    if (max_off < 0) max_off = 0;
    if (s->first > max_off) s->first = max_off;
}
int gmb_scroll_view_render_line(gmb_scroll_view_t* s, int idx, char* out, int cap)
{
    if (idx < 0 || idx >= s->nitems)
    {
        for (int i = 0; i < s->base.width && i < cap - 1; i++) out[i] = ' ';
        out[s->base.width < cap - 1 ? s->base.width : cap - 1] = '\0';
        return s->base.width;
    }
    int o = snprintf(out, cap, "%.*s", s->base.width, s->items[idx]);
    if (o > s->base.width) o = s->base.width;
    else for (int i = o; i < s->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
double gmb_scroll_view_scroll_fraction(gmb_scroll_view_t* sv)
{
    gmb_widget_t* s = (gmb_widget_t*)sv;
    int denom = s->nitems - s->base.height;
    return denom > 0 ? (double)s->first / denom : 0.0;
}

/* ===================== Form widget ===================== */
