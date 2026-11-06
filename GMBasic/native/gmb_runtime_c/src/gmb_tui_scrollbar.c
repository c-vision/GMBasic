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
    gmb_scrollbar_t* s = (gmb_scrollbar_t*)u;
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
    gmb_scrollbar_t* s = (gmb_scrollbar_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int horizontal = (s->orientation == 0);
    int len = horizontal ? w : h;
    GMB_GFX_FILL_RRECT(x, y, w, h, 2, st.fill);
    double span = (double)(s->max - s->min); if (span <= 0) span = 1;
    double t = ((double)s->value - s->min) / span; if (t < 0) t = 0; if (t > 1) t = 1;
    int thumb = len / 4; if (thumb < 12) thumb = 12; if (thumb > len) thumb = len;
    int pos = (int)(t * (len - thumb));
    if (horizontal) GMB_GFX_FILL_RRECT(x + pos, y, thumb, h, 2, st.accent);
    else GMB_GFX_FILL_RRECT(x, y + pos, w, thumb, 2, st.accent);
    GMB_GFX_STROKE_RRECT(x, y, w, h, 2, st.border);
}
static bool scrollbar_click(void* u, gmb_control_t* c, int x, int y)
{
    /* Click nella barra: salta al valore proporzionale alla posizione. */
    gmb_scrollbar_t* s = (gmb_scrollbar_t*)u;
    if (!s) return false;
    int len = s->orientation ? c->height : c->width;
    int pos = s->orientation ? (y - c->y) : (x - c->x);
    if (len <= 1 || s->max <= s->min) return false;
    if (pos < 0) pos = 0;
    if (pos >= len) pos = len - 1;
    s->value = s->min + (int)((double)pos * (s->max - s->min) / (len - 1) + 0.5);
    return true;
}
void gmb_scrollbar_init(gmb_scrollbar_t* s, int x, int y, int len, int orientation)
{
    memset(s, 0, sizeof(*s));
    if (orientation == 0) gmb_control_init(&s->ctrl, "", x, y, len, 1, scrollbar_draw, NULL);
    else gmb_control_init(&s->ctrl, "", x, y, 1, len, scrollbar_draw, NULL);
    s->ctrl.draw_px_fn = scrollbar_draw_px;
    s->ctrl.handle_click_fn = scrollbar_click;
    s->ctrl.user = s;
    s->orientation = orientation;
}
void gmb_scrollbar_set(gmb_scrollbar_t* s, int value, int min, int max)
{
    s->value = value;
    s->min = min;
    s->max = max;
}
int gmb_scrollbar_render_line(gmb_scrollbar_t* s, int index, char* out, int cap)
{
    /* TV TScrollBar: vChars = {▲ 0x1E, ▼ 0x1F, ▒ 0xB1, ■ 0xFE, ▓ 0xB2}. */
    int range = s->max - s->min;
    int len = s->orientation == 0 ? s->ctrl.width : s->ctrl.height;
    uint32_t ch;
    if (range <= 0) ch = 0x2593;                                  /* ▓ "no range" */
    else {
        int thumb = len > 4 ? len / 4 : 1;
        int pos = (s->value - s->min) * (len - thumb) / range;
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
