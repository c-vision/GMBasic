/* ============================================================================
 * gmb_tui_slider.c — Slider (porting del runtime C, specchio ClipperLib).
 *
 * MODELLO UNICO: uno slider è un gmb_widget_t (W_TRACK); range in vmin/vmax,
 * valore in `ival`, passo in vstep.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "gmb_form.h"

/* C# Math.Round(double) default = MidpointRounding.ToEven. */
static int slider_round_half_even(double v)
{
    double f = floor(v);
    double d = v - f;
    if (d < 0.5) return (int)f;
    if (d > 0.5) return (int)f + 1;
    int fi = (int)f;
    return (fi % 2 == 0) ? fi : fi + 1;
}
static void slider_set_from_x(gmb_widget_t* s, gmb_control_t* c, int x)
{
    int vmin = (int)s->vmin, vmax = (int)s->vmax;
    int inner = c->width - 2; if (inner < 1) inner = 1;
    int relX = x - c->x - 1;
    if (relX < 0) relX = 0;
    if (relX > inner) relX = inner;
    int r = slider_round_half_even((double)relX / inner * (vmax - vmin));
    int val = vmin + r;
    if (val < vmin) val = vmin;
    if (val > vmax) val = vmax;
    s->ival = val;
}
static bool slider_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_widget_t* s = (gmb_widget_t*)u;
    switch (key)
    {
        case GMB_K_RIGHT: case GMB_K_UP: gmb_slider_set_value((gmb_slider_t*)s, (int)s->ival + (int)s->vstep); return true;
        case GMB_K_LEFT: case GMB_K_DOWN: gmb_slider_set_value((gmb_slider_t*)s, (int)s->ival - (int)s->vstep); return true;
        case GMB_K_HOME: gmb_slider_set_value((gmb_slider_t*)s, (int)s->vmin); return true;
        case GMB_K_END: gmb_slider_set_value((gmb_slider_t*)s, (int)s->vmax); return true;
    }
    return false;
}
/* C# Slider.HandleMouse: Click/Double/Drag aggiornano il valore; Release chiude il drag. */
static bool slider_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK:
        case GMB_MA_DOUBLECLICK:
            if (a->y != c->y || a->x < c->x || a->x >= c->x + c->width) return false;
            s->dragging = 1;
            slider_set_from_x(s, c, a->x);
            return true;
        case GMB_MA_DRAG:
            if (!s->dragging) return false;
            slider_set_from_x(s, c, a->x);
            return true;
        case GMB_MA_RELEASE:
            s->dragging = 0;
            return true;
    }
    return false;
}
void gmb_slider_init(gmb_slider_t* s, int x, int y, int w)
{
    gmb_widget_t* g = (gmb_widget_t*)s;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, 1, gmb_widget_draw_cells, slider_key);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_SLIDER;
    g->base.handle_mouse_fn = slider_mouse;
    g->base.user = g;
    g->kind = W_TRACK;
    g->vmin = 0; g->vmax = 100; g->ival = 0; g->vstep = 1;
}
void gmb_slider_set_range(gmb_slider_t* s, int min, int max)
{
    gmb_widget_t* g = (gmb_widget_t*)s;
    g->vmin = min; g->vmax = max;
    if (g->ival < min) g->ival = min;
    if (g->ival > max) g->ival = max;
}
void gmb_slider_set_value(gmb_slider_t* s, int value)
{
    gmb_widget_t* g = (gmb_widget_t*)s;
    if (value < g->vmin) value = (int)g->vmin;
    if (value > g->vmax) value = (int)g->vmax;
    g->ival = value;
}
int gmb_slider_render_line(gmb_slider_t* s, char* out, int cap)
{
    /* C# RenderLine: "[" + '─'*pos + '█' + '─'*(inner-pos) + "]", pos=Round(Ratio*inner). */
    gmb_widget_t* g = (gmb_widget_t*)s;
    int width = g->base.width;
    int inner = width - 2; if (inner < 0) inner = 0;
    double ratio = (g->vmax > g->vmin) ? (g->ival - g->vmin) / (g->vmax - g->vmin) : 0.0;
    int pos = slider_round_half_even(ratio * inner);
    if (pos < 0) pos = 0;
    if (pos > inner) pos = inner;
    int o = 0;
    if (o < cap - 1) out[o++] = '[';
    for (int i = 0; i < pos && o + 3 <= cap - 1; i++) { out[o++] = (char)0xE2; out[o++] = (char)0x94; out[o++] = (char)0x80; }
    if (o + 3 <= cap - 1) { out[o++] = (char)0xE2; out[o++] = (char)0x96; out[o++] = (char)0x88; }
    /* Traccia destra: inner-pos-1 segmenti, così `[`+pos+`█`+resto+`]` = Width esatto. */
    for (int i = 0; i < inner - pos - 1 && o + 3 <= cap - 1; i++) { out[o++] = (char)0xE2; out[o++] = (char)0x94; out[o++] = (char)0x80; }
    if (o < cap - 1) out[o++] = ']';
    out[o] = '\0';
    return o;
}
