/* ============================================================================
 * gmb_tui_radio.c — RadioGroup.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static void radio_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_radio_group_t* r = (gmb_radio_group_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    for (int i = 0; i < r->options_count && i < c->height; i++)
    {
        char line[128];
        gmb_radio_group_render_line(r, i, line, (int)sizeof(line));
        char cased[160];
        gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
        gmb_screen_setstring(buf, c->x, c->y + i, cased, fg, bg);
    }
}
static bool radio_key(void* u, gmb_control_t* c, int key)
{
    gmb_radio_group_t* r = (gmb_radio_group_t*)u;
    if (!c->enabled || r->options_count == 0) return false;
    switch (key)
    {
        case GMB_K_UP: case GMB_K_LEFT:
            if (r->selected > 0) r->selected--;
            return true;
        case GMB_K_DOWN: case GMB_K_RIGHT:
            if (r->selected < r->options_count - 1) r->selected++;
            return true;
    }
    return false;
}
static bool radio_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_radio_group_t* r = (gmb_radio_group_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t rect = gmb_control_rect(c);
    if (!rect_contains(rect, x, y)) return false;
    c->focused = true;
    int idx = y - c->y;
    if (idx >= 0 && idx < r->options_count) r->selected = idx;
    return true;
}
static void radio_draw_px(void* u, gmb_control_t* c)
{
    gmb_radio_group_t* r = (gmb_radio_group_t*)u;
    const char* _it[16]; for (int i = 0; i < r->options_count && i < 16; i++) _it[i] = r->options[i];
    gmb_px_draw_ex(GMB_PX_RADIO, c->x, c->y, c->width, c->height, NULL, _it, r->options_count, r->selected, 0, 0, NULL, 0, NULL, 0);
}
void gmb_radio_group_init(gmb_radio_group_t* r, int x, int y, int w, int h)
{
    gmb_control_init(&r->ctrl, "", x, y, w, h, radio_draw, radio_key);
    r->ctrl.draw_px_fn = radio_draw_px;
    r->ctrl.ctrl_kind = GMB_CK_RADIO;
    r->ctrl.handle_click_fn = radio_click;
    r->ctrl.user = r;
}
void gmb_radio_group_set_options(gmb_radio_group_t* r, const char* const* options, int count)
{
    r->options_count = count < 16 ? count : 16;
    for (int i = 0; i < r->options_count && options; i++)
        snprintf(r->options[i], sizeof(r->options[i]), "%s", options[i] ? options[i] : "");
}
const char* gmb_radio_group_selected_value(gmb_radio_group_t* r)
{
    if (r->selected >= 0 && r->selected < r->options_count) return r->options[r->selected];
    return NULL;
}
int gmb_radio_group_render_line(gmb_radio_group_t* r, int idx, char* out, int cap)
{
    const char* mark = idx == r->selected ? "(•)" : "( )";
    int o = snprintf(out, cap, "%s %s", mark, idx < r->options_count ? r->options[idx] : "");
    if (o > r->ctrl.width) o = r->ctrl.width;
    else for (int i = o; i < r->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- ComboBox ---- */
