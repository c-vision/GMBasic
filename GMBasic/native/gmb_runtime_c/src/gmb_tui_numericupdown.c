/* ============================================================================
 * gmb_tui_numericupdown.c — NumericUpDown.cs (vecchio porting) (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void numeric_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_numeric_updown_t* n = (gmb_numeric_updown_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 3, fg, bg, 1);
        char line[128], cased[160];
        gmb_numeric_updown_render_line(n, line, (int)sizeof(line));
        gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
        GMB_GFX_DRAW_TEXT(cased, sk->origin_x + c->x * sk->cell_w,
                          sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    char line[128];
    gmb_numeric_updown_render_line(n, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
}
static bool numeric_key(void* u, gmb_control_t* c, int key)
{
    gmb_numeric_updown_t* n = (gmb_numeric_updown_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP: case GMB_K_RIGHT:
            if (n->value < n->max) n->value = (n->value + n->step < n->max ? n->value + n->step : n->max);
            return true;
        case GMB_K_DOWN: case GMB_K_LEFT:
            if (n->value > n->min) n->value = (n->value - n->step > n->min ? n->value - n->step : n->min);
            return true;
        case GMB_K_PGUP:
            n->value = (n->value + n->step * 10 < n->max ? n->value + n->step * 10 : n->max);
            return true;
        case GMB_K_PGDN:
            n->value = (n->value - n->step * 10 > n->min ? n->value - n->step * 10 : n->min);
            return true;
        default:
            if (key >= '0' && key <= '9')
            {
                int digit = key - '0';
                long nv = (long)n->value * 10 + digit;
                if (nv <= n->max) n->value = (int)nv;
                return true;
            }
            break;
    }
    return false;
}
static bool numeric_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_numeric_updown_t* n = (gmb_numeric_updown_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int local_x = x - c->x;
    if (local_x >= c->width - 2)
    {
        if (local_x == c->width - 2) { if (n->value < n->max) n->value = (n->value + n->step < n->max ? n->value + n->step : n->max); }
        else { if (n->value > n->min) n->value = (n->value - n->step > n->min ? n->value - n->step : n->min); }
        return true;
    }
    if (n->value > n->min) n->value = (n->value - n->step > n->min ? n->value - n->step : n->min);
    return true;
}
static void numericupdown_draw_px(void* u, gmb_control_t* c)
{
    gmb_numeric_updown_t* n = (gmb_numeric_updown_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int tl = (int)GMB_GFX_TEXT_H(); if (tl <= 0) tl = 14;
    if (h < tl + 6) h = tl + 6;
    int bw = h > 24 ? 24 : h;
    GMB_GFX_FILL_RRECT(x, y, w, h, 4, st.fill);
    GMB_GFX_STROKE_RRECT(x, y, w, h, 4, st.border);
    char vb[32]; snprintf(vb, sizeof(vb), "%d", n->value);
    GMB_GFX_DRAW_TEXT(vb, x + 8, y + (h - tl) / 2, 0, (int)st.text);
    GMB_GFX_FILL_RRECT(x + w - bw, y, bw, h / 2, 0, st.fill_hover);
    GMB_GFX_DRAW_TEXT("+", x + w - bw / 2, y + (h / 4 - tl / 2), 1, (int)st.text);
    GMB_GFX_FILL_RRECT(x + w - bw, y + h / 2, bw, h - h / 2, 0, st.fill);
    GMB_GFX_DRAW_TEXT("-", x + w - bw / 2, y + h / 2 + (h / 4 - tl / 2), 1, (int)st.text);
    GMB_GFX_LINE(x + w - bw, y, x + w - bw, y + h, (int)st.border, "");
}
void gmb_numeric_updown_init(gmb_numeric_updown_t* n, int x, int y, int w)
{
    memset(n, 0, sizeof(*n));
    gmb_control_init(&n->ctrl, "", x, y, w, 1, numeric_draw, numeric_key);
    n->ctrl.handle_click_fn = numeric_click;
    n->ctrl.draw_px_fn = numericupdown_draw_px;
    n->ctrl.user = n;
    n->min = -2147483647 - 1;
    n->max = 2147483647;
    n->step = 1;
}
void gmb_numeric_updown_set_range(gmb_numeric_updown_t* n, int min, int max)
{
    n->min = min;
    n->max = max;
}
void gmb_numeric_updown_set_value(gmb_numeric_updown_t* n, int value)
{
    n->value = value < n->min ? n->min : (value > n->max ? n->max : value);
}
int gmb_numeric_updown_render_line(gmb_numeric_updown_t* n, char* out, int cap)
{
    char s[32];
    snprintf(s, sizeof(s), "%d", n->value);
    int numlen = (int)strlen(s);
    /* Celle: 1 spazio + padding + cifre + 2 frecce = Width. Si costruisce a CARATTERI,
     * non a byte: prima il troncamento a Width tagliava a metà `▴`/`▼` (byte UTF-8). */
    int pad = n->ctrl.width - 1 - numlen - 2;
    if (pad < 0) pad = 0;
    int o = 0;
    if (o < cap - 1) out[o++] = ' ';
    for (int i = 0; i < pad && o < cap - 1; i++) out[o++] = ' ';
    for (int i = 0; i < numlen && o < cap - 1; i++) out[o++] = s[i];
    { const char* arrows = "\xE2\x96\xB4\xE2\x96\xBC";   /* ▴▼ */
      while (*arrows && o + 2 < cap - 1) out[o++] = *arrows++; }
    while (o < n->ctrl.width && o < cap - 1) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- RadioGroup ---- */
