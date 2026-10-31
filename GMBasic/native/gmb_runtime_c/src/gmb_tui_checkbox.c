/* ============================================================================
 * gmb_tui_checkbox.c — CheckBox.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static void checkbox_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_checkbox_t* cb = (gmb_checkbox_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    char line[128];
    gmb_checkbox_render_line(cb, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
}
static bool checkbox_key(void* u, gmb_control_t* c, int key)
{
    gmb_checkbox_t* cb = (gmb_checkbox_t*)u;
    if (!c->enabled) return false;
    if (key == ' ' || key == GMB_K_ENTER) { cb->checked = !cb->checked; return true; }
    return false;
}
static bool checkbox_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_checkbox_t* cb = (gmb_checkbox_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    cb->checked = !cb->checked;
    return true;
}
static void checkbox_draw_px(void* u, gmb_control_t* c)
{
    gmb_checkbox_t* cb = (gmb_checkbox_t*)u;
    gmb_px_draw_ex(GMB_PX_CHECK, c->x, c->y, c->width, c->height, cb->label, NULL, 0, cb->checked ? 1.0 : 0.0, 0, 0, NULL, 0, NULL, 0);
}
void gmb_checkbox_init(gmb_checkbox_t* c, int x, int y, int w, const char* label)
{
    gmb_control_init(&c->ctrl, "", x, y, w, 1, checkbox_draw, checkbox_key);
    c->ctrl.handle_click_fn = checkbox_click;
    c->ctrl.user = c;
    c->ctrl.draw_px_fn = checkbox_draw_px;
    c->ctrl.ctrl_kind = GMB_CK_CHECKBOX;
    snprintf(c->label, sizeof(c->label), "%s", label ? label : "");
}
void gmb_checkbox_set_checked(gmb_checkbox_t* c, bool v) { c->checked = v; }
int gmb_checkbox_render_line(gmb_checkbox_t* c, char* out, int cap)
{
    int o = snprintf(out, cap, "%s %s", c->checked ? "[X]" : "[ ]", c->label);
    if (o > c->ctrl.width) o = c->ctrl.width;
    else for (int i = o; i < c->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- ToggleButton ---- */
