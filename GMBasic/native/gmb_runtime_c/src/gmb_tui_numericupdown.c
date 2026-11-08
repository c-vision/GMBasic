/* ============================================================================
 * gmb_tui_numericupdown.c — NumericUpDown (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: numeric up/down = gmb_widget_t (range vmin/vmax, valore `ival`).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Valori del modello, nella forma intera usata dal controllo. */
#define NU_VALUE(n) ((int)((gmb_widget_t*)(n))->ival)
#define NU_MIN(n)   ((int)((gmb_widget_t*)(n))->vmin)
#define NU_MAX(n)   ((int)((gmb_widget_t*)(n))->vmax)
#define NU_STEP(n)  ((int)((gmb_widget_t*)(n))->vstep)
#define NU_SET(n, v) (((gmb_widget_t*)(n))->ival = (double)(v))

static void numeric_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* n = (gmb_widget_t*)u;
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
    gmb_widget_t* n = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int value = NU_VALUE(n), vmin = NU_MIN(n), vmax = NU_MAX(n), step = NU_STEP(n);
    switch (key)
    {
        case GMB_K_UP: case GMB_K_RIGHT:
            if (value < vmax) NU_SET(n, (value + step < vmax ? value + step : vmax));
            return true;
        case GMB_K_DOWN: case GMB_K_LEFT:
            if (value > vmin) NU_SET(n, (value - step > vmin ? value - step : vmin));
            return true;
        case GMB_K_PGUP:
            NU_SET(n, (value + step * 10 < vmax ? value + step * 10 : vmax));
            return true;
        case GMB_K_PGDN:
            NU_SET(n, (value - step * 10 > vmin ? value - step * 10 : vmin));
            return true;
        default:
            if (key >= '0' && key <= '9')
            {
                int digit = key - '0';
                long nv = (long)value * 10 + digit;
                if (nv <= vmax) NU_SET(n, (int)nv);
                return true;
            }
            break;
    }
    return false;
}
static bool numeric_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* n = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    /* Rect PIXEL (px_*): in grafica i due pulsanti sono larghi ~26px, mentre le
     * coordinate del controllo sono in CELLE (width in celle, height 1). Usando
     * c->width/c->height il click funzionava solo su una striscia di 2px. */
    int bx = (c->px_w > 0) ? c->px_x : c->x;
    int by = (c->px_h > 0) ? c->px_y : c->y;
    int bw = (c->px_w > 0) ? c->px_w : c->width;
    int bh = (c->px_h > 0) ? c->px_h : c->height;
    if (x < bx || x >= bx + bw || y < by || y >= by + bh) return false;
    c->focused = true;
    int bwd = 26; if (bw < 60) bwd = bw / 3; if (bwd < 8) bwd = 8;
    int value = NU_VALUE(n), vmin = NU_MIN(n), vmax = NU_MAX(n), step = NU_STEP(n);
    if (x >= bx + bw - bwd) {            /* '+' */
        if (value < vmax) NU_SET(n, (value + step < vmax ? value + step : vmax));
        return true;
    }
    if (x >= bx + bw - 2 * bwd) {        /* '-' */
        if (value > vmin) NU_SET(n, (value - step > vmin ? value - step : vmin));
        return true;
    }
    return true;                          /* click sul campo: solo focus */
}
static void numericupdown_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* n = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int tl = (int)GMB_GFX_TEXT_H(); if (tl <= 0) tl = 14;
    if (h < tl + 6) h = tl + 6;
    int bw = h > 24 ? 24 : h;
    GMB_GFX_FILL_RRECT(x, y, w, h, 4, st.fill);
    GMB_GFX_STROKE_RRECT(x, y, w, h, 4, st.border);
    char vb[32]; snprintf(vb, sizeof(vb), "%d", NU_VALUE(n));
    GMB_GFX_DRAW_TEXT(vb, x + 8, y + (h - tl) / 2, 0, (int)st.text);
    GMB_GFX_FILL_RRECT(x + w - bw, y, bw, h / 2, 0, st.fill_hover);
    GMB_GFX_DRAW_TEXT("+", x + w - bw / 2, y + (h / 4 - tl / 2), 1, (int)st.text);
    GMB_GFX_FILL_RRECT(x + w - bw, y + h / 2, bw, h - h / 2, 0, st.fill);
    GMB_GFX_DRAW_TEXT("-", x + w - bw / 2, y + h / 2 + (h / 4 - tl / 2), 1, (int)st.text);
    GMB_GFX_LINE(x + w - bw, y, x + w - bw, y + h, (int)st.border, "");
}
void gmb_numeric_updown_init(gmb_numeric_updown_t* n, int x, int y, int w)
{
    gmb_widget_t* g = (gmb_widget_t*)n;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, 1, numeric_draw, numeric_key);
    g->base.handle_click_fn = numeric_click;
    g->base.draw_px_fn = numericupdown_draw_px;
    g->base.user = g;
    g->kind = W_NUMERIC;
    g->vmin = -2147483647.0 - 1.0;
    g->vmax = 2147483647.0;
    g->vstep = 1;
}
void gmb_numeric_updown_set_range(gmb_numeric_updown_t* n, int min, int max)
{
    gmb_widget_t* g = (gmb_widget_t*)n;
    g->vmin = min;
    g->vmax = max;
}
void gmb_numeric_updown_set_value(gmb_numeric_updown_t* n, int value)
{
    gmb_widget_t* g = (gmb_widget_t*)n;
    int vmin = (int)g->vmin, vmax = (int)g->vmax;
    g->ival = value < vmin ? vmin : (value > vmax ? vmax : value);
}
int gmb_numeric_updown_render_line(gmb_numeric_updown_t* n, char* out, int cap)
{
    gmb_widget_t* g = (gmb_widget_t*)n;
    char s[32];
    snprintf(s, sizeof(s), "%d", NU_VALUE(n));
    int numlen = (int)strlen(s);
    /* Celle: 1 spazio + padding + cifre + 2 frecce = Width. Si costruisce a CARATTERI,
     * non a byte: prima il troncamento a Width tagliava a metà `▴`/`▼` (byte UTF-8). */
    int pad = g->base.width - 1 - numlen - 2;
    if (pad < 0) pad = 0;
    int o = 0;
    if (o < cap - 1) out[o++] = ' ';
    for (int i = 0; i < pad && o < cap - 1; i++) out[o++] = ' ';
    for (int i = 0; i < numlen && o < cap - 1; i++) out[o++] = s[i];
    { const char* arrows = "\xE2\x96\xB4\xE2\x96\xBC";   /* ▴▼ */
      while (*arrows && o + 2 < cap - 1) out[o++] = *arrows++; }
    while (o < g->base.width && o < cap - 1) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- RadioGroup ---- */
