/* ============================================================================
 * gmb_tui_scrollbar.c — ScrollBar.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void scrollbar_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    char line[256];
    int len = s->orientation == 0 ? c->width : c->height;
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        if (s->orientation == 0) gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 2, fg, bg, 1);
        else gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 2, fg, bg, 1);
        for (int i = 0; i < len; i++)
        {
            gmb_scrollbar_render_line(s, i, line, (int)sizeof(line));
            if (line[0] == ' ') continue;
            if (s->orientation == 0) gmb_surface_rounded_rect(buf, c->x + i, c->y, 1, c->height, 0, fg, fg, 1);
            else gmb_surface_rounded_rect(buf, c->x, c->y + i, c->width, 1, 0, fg, fg, 1);
        }
        return;
    }
    for (int i = 0; i < len; i++)
    {
        gmb_scrollbar_render_line(s, i, line, (int)sizeof(line));
        if (s->orientation == 0)
            gmb_screen_setstring_n(buf, c->x + i, c->y, line, 1, fg, bg);
        else
            gmb_screen_setstring_n(buf, c->x, c->y + i, line, 1, fg, bg);
    }
}
static void scrollbar_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = (c->px_w > 0) ? c->px_x : c->x;
    int y = (c->px_h > 0) ? c->px_y : c->y;
    int w = (c->px_w > 0) ? c->px_w : c->width;
    int h = (c->px_h > 0) ? c->px_h : c->height;
    int horizontal = (s->orientation == 0);
    /* Binario SOTTILE centrato (6 px) come una scrollbar di sistema; il thumb ha il
     * colore accent del tema. Prima track e thumb erano quasi invisibili. */
    int th = 6;
    unsigned accent = (st.accent & 0xFFFFFFu) ? st.accent : st.text;
    if (horizontal && h < th) h = th;
    if (!horizontal && w < th) w = th;
    if (horizontal)
    {
        int ty = y + (h - th) / 2;
        gmb_gfx_fill_rounded_rect(x, ty, w, th, 3, (st.border & 0xFFFFFFu) | 0x33000000u);
        double span = (double)((int)s->vmax - (int)s->vmin); if (span <= 0) span = 1;
        double t = ((double)(int)s->ival - (int)s->vmin) / span; if (t < 0) t = 0; if (t > 1) t = 1;
        int tw = w / 4; if (tw < 24) tw = 24; if (tw > w) tw = w;
        int pos = (int)(t * (w - tw));
        gmb_gfx_fill_rounded_rect(x + pos, ty, tw, th, 3, accent);
    }
    else
    {
        int tx = x + (w - th) / 2;
        gmb_gfx_fill_rounded_rect(tx, y, th, h, 3, (st.border & 0xFFFFFFu) | 0x33000000u);
        double span = (double)((int)s->vmax - (int)s->vmin); if (span <= 0) span = 1;
        double t = ((double)(int)s->ival - (int)s->vmin) / span; if (t < 0) t = 0; if (t > 1) t = 1;
        int tw = h / 4; if (tw < 24) tw = 24; if (tw > h) tw = h;
        int pos = (int)(t * (h - tw));
        gmb_gfx_fill_rounded_rect(tx, y + pos, th, tw, 3, accent);
    }
}
static bool scrollbar_click(void* u, gmb_control_t* c, int x, int y)
{
    /* Click nella barra: salta al valore proporzionale alla posizione. */
    gmb_widget_t* s = (gmb_widget_t*)u;
    if (!s) return false;
    int len = s->orientation ? c->height : c->width;
    int pos = s->orientation ? (y - c->y) : (x - c->x);
    if (len <= 1 || (int)s->vmax <= (int)s->vmin) return false;
    if (pos < 0) pos = 0;
    if (pos >= len) pos = len - 1;
    s->ival = (int)s->vmin + (int)((double)pos * ((int)s->vmax - (int)s->vmin) / (len - 1) + 0.5);
    return true;
}
void gmb_scrollbar_init(gmb_scrollbar_t* sb, int x, int y, int len, int orientation)
{
    gmb_widget_t* s = (gmb_widget_t*)sb;
    memset(s, 0, sizeof(*s));
    if (orientation == 0) gmb_control_init(&s->base, "", x, y, len, 1, scrollbar_draw, NULL);
    else gmb_control_init(&s->base, "", x, y, 1, len, scrollbar_draw, NULL);
    s->base.draw_px_fn = scrollbar_draw_px;
    s->base.handle_click_fn = scrollbar_click;
    s->base.user = s;
    s->kind = W_SCROLLBAR;
    s->orientation = orientation;
}
void gmb_scrollbar_set(gmb_scrollbar_t* sb, int value, int min, int max)
{
    gmb_widget_t* s = (gmb_widget_t*)sb;
    s->ival = value;
    s->vmin = min;
    s->vmax = max;
}
int gmb_scrollbar_render_line(gmb_scrollbar_t* s, int index, char* out, int cap)
{
    /* TV TScrollBar: vChars = {▲ 0x1E, ▼ 0x1F, ▒ 0xB1, ■ 0xFE, ▓ 0xB2}. */
    int vmin = (int)s->vmin, vmax = (int)s->vmax, value = (int)s->ival;
    int range = vmax - vmin;
    int len = s->orientation == 0 ? s->base.width : s->base.height;
    uint32_t ch;
    if (range <= 0) ch = 0x2593;                                  /* ▓ "no range" */
    else {
        int thumb = len > 4 ? len / 4 : 1;
        int pos = (value - vmin) * (len - thumb) / range;
        if (index >= pos && index < pos + thumb) ch = 0x25A0;     /* ■ thumb */
        else if (index == 0 && s->show_arrows) ch = 0x25B2;       /* ▲ */
        else if (index == len - 1 && s->show_arrows) ch = 0x25BC; /* ▼ */
        else ch = 0x2592;                                         /* ▒ track */
    }
    int o = 0;
    if (ch < 0x80) out[o++] = (char)ch;
    else if (ch < 0x800) { out[o++] = (char)(0xC0 | (ch >> 6)); out[o++] = (char)(0x80 | (ch & 0x3F)); }
    else { out[o++] = (char)(0xE0 | (ch >> 12)); out[o++] = (char)(0x80 | ((ch >> 6) & 0x3F)); out[o++] = (char)(0x80 | (ch & 0x3F)); }
    out[o < cap ? o : cap - 1] = '\0';
    return o;
}

/* ---- Tab ---- */
