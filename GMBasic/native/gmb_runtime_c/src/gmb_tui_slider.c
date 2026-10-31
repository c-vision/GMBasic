/* ============================================================================
 * gmb_tui_slider.c — gmb_tui_slider. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "gmb_form.h"

/* ===================== Slider ===================== */

static void slider_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_slider_t* s = (gmb_slider_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    char line[256];
    gmb_slider_render_line(s, line, (int)sizeof(line));
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
}
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
static void slider_set_from_x(gmb_slider_t* s, gmb_control_t* c, int x)
{
    int inner = c->width - 2; if (inner < 1) inner = 1;
    int relX = x - c->x - 1;
    if (relX < 0) relX = 0;
    if (relX > inner) relX = inner;
    int r = slider_round_half_even((double)relX / inner * (s->max - s->min));
    int val = s->min + r;
    if (val < s->min) val = s->min;
    if (val > s->max) val = s->max;
    s->value = val;
}
static bool slider_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_slider_t* s = (gmb_slider_t*)u;
    switch (key)
    {
        case GMB_K_RIGHT: case GMB_K_UP: gmb_slider_set_value(s, s->value + s->step); return true;
        case GMB_K_LEFT: case GMB_K_DOWN: gmb_slider_set_value(s, s->value - s->step); return true;
        case GMB_K_HOME: gmb_slider_set_value(s, s->min); return true;
        case GMB_K_END: gmb_slider_set_value(s, s->max); return true;
    }
    return false;
}
/* C# Slider.HandleMouse: Click/Double/Drag aggiornano il valore; Release chiude il drag. */
static bool slider_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_slider_t* s = (gmb_slider_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK:
        case GMB_MA_DOUBLECLICK:
            if (a->y != c->y || a->x < c->x || a->x >= c->x + c->width) return false;
            s->dragging = true;
            slider_set_from_x(s, c, a->x);
            return true;
        case GMB_MA_DRAG:
            if (!s->dragging) return false;
            slider_set_from_x(s, c, a->x);
            return true;
        case GMB_MA_RELEASE:
            s->dragging = false;
            return true;
    }
    return false;
}
static void slider_draw_px(void* u, gmb_control_t* c)
{
    gmb_slider_t* s = (gmb_slider_t*)u;
    gmb_px_draw_ex(GMB_PX_TRACK, c->x, c->y, c->width, c->height, NULL, NULL, 0, s->value, s->min, s->max, NULL, 0, NULL, 0);
}
void gmb_slider_init(gmb_slider_t* s, int x, int y, int w)
{
    memset(s, 0, sizeof(*s));
    gmb_control_init(&s->ctrl, "", x, y, w, 1, slider_draw, slider_key);
    s->ctrl.draw_px_fn = slider_draw_px;
    s->ctrl.ctrl_kind = GMB_CK_SLIDER;
    s->ctrl.handle_mouse_fn = slider_mouse;
    s->ctrl.user = s;
    s->min = 0; s->max = 100; s->value = 0; s->step = 1;
}
void gmb_slider_set_range(gmb_slider_t* s, int min, int max)
{
    s->min = min; s->max = max;
    if (s->value < min) s->value = min;
    if (s->value > max) s->value = max;
}
void gmb_slider_set_value(gmb_slider_t* s, int value)
{
    if (value < s->min) value = s->min;
    if (value > s->max) value = s->max;
    s->value = value;
}
int gmb_slider_render_line(gmb_slider_t* s, char* out, int cap)
{
    /* C# RenderLine: "[" + '─'*pos + '█' + '─'*(inner-pos) + "]", pos=Round(Ratio*inner). */
    int width = s->ctrl.width;
    int inner = width - 2; if (inner < 0) inner = 0;
    double ratio = (s->max > s->min) ? (double)(s->value - s->min) / (s->max - s->min) : 0.0;
    int pos = slider_round_half_even(ratio * inner);
    if (pos < 0) pos = 0;
    if (pos > inner) pos = inner;
    int o = 0;
    if (o < cap - 1) out[o++] = '[';
    for (int i = 0; i < pos && o + 3 <= cap - 1; i++) { out[o++] = (char)0xE2; out[o++] = (char)0x94; out[o++] = (char)0x80; }
    if (o + 3 <= cap - 1) { out[o++] = (char)0xE2; out[o++] = (char)0x96; out[o++] = (char)0x88; }
    for (int i = 0; i < inner - pos && o + 3 <= cap - 1; i++) { out[o++] = (char)0xE2; out[o++] = (char)0x94; out[o++] = (char)0x80; }
    /* C#: lunghezza = inner+3 = Width+1 -> troncata a Width (perde ']'). */
    int chcount = 1 + pos + 1 + (inner - pos);
    if (chcount < width && o < cap - 1) out[o++] = ']';
    out[o] = '\0';
    return o;
}
