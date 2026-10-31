/* ============================================================================
 * gmb_tui_numericupdown_ctrl.c — NumericUpDown.cs (specchio ClipperLib/Controls,
 * variante moderna gmb_numeric_up_down_*). Il vecchio porting usa
 * gmb_numeric_updown_* (gmb_tui_numericupdown.c).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void numeric_modern_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_numeric_up_down_t* n = (gmb_numeric_up_down_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    char line[128];
    gmb_numeric_up_down_render(n, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 3, fg, bg, 1);
        GMB_GFX_DRAW_TEXT(cased, sk->origin_x + c->x * sk->cell_w,
                          sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0,
                          (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
}

static bool numeric_modern_key(void* u, gmb_control_t* c, int key)
{
    gmb_numeric_up_down_t* n = (gmb_numeric_up_down_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP: case GMB_K_RIGHT: gmb_numeric_up_down_increment(n); return true;
        case GMB_K_DOWN: case GMB_K_LEFT: gmb_numeric_up_down_decrement(n); return true;
        default: return false;
    }
}

void gmb_numeric_up_down_init(gmb_numeric_up_down_t* n, int x, int y, int w)
{
    memset(n, 0, sizeof(*n));
    gmb_control_init(&n->ctrl, "", x, y, w, 1, numeric_modern_draw, numeric_modern_key);
    n->ctrl.user = n;
    n->value = 0;
    n->min = 0; n->max = 100; n->step = 1;
    n->decimals = 0;
}
void gmb_numeric_up_down_set_range(gmb_numeric_up_down_t* n, double min, double max, double step)
{
    n->min = min; n->max = max; n->step = step;
    if (n->value < min) n->value = min;
    if (n->value > max) n->value = max;
}
void gmb_numeric_up_down_set_value(gmb_numeric_up_down_t* n, double v)
{
    if (v < n->min) v = n->min;
    if (v > n->max) v = n->max;
    n->value = v;
}
double gmb_numeric_up_down_value(gmb_numeric_up_down_t* n) { return n->value; }
void gmb_numeric_up_down_increment(gmb_numeric_up_down_t* n)
{
    double v = n->value + n->step;
    if (v > n->max) v = n->max;
    n->value = v;
}
void gmb_numeric_up_down_decrement(gmb_numeric_up_down_t* n)
{
    double v = n->value - n->step;
    if (v < n->min) v = n->min;
    n->value = v;
}
int gmb_numeric_up_down_render(gmb_numeric_up_down_t* n, char* out, int cap)
{
    char num[48];
    if (n->decimals > 0) snprintf(num, sizeof(num), "%.*f", n->decimals, n->value);
    else snprintf(num, sizeof(num), "%.0f", n->value);
    return snprintf(out, (size_t)cap, "< %s >", num);
}
