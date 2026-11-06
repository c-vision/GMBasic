/* ============================================================================
 * gmb_tui_toggle.c — ToggleButton.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static void toggle_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_toggle_button_t* t = (gmb_toggle_button_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    char line[128];
    snprintf(line, sizeof(line), "%s %s", t->pressed ? "[X]" : "[ ]", t->caption);
    int l = (int)strlen(line);
    if (l > c->width) line[c->width] = '\0';
    else for (int i = l; i < c->width && i < 127; i++) line[i] = ' ';
    if (l < c->width) line[c->width < 127 ? c->width : 127] = '\0';
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
}
static bool toggle_key(void* u, gmb_control_t* c, int key)
{
    gmb_toggle_button_t* t = (gmb_toggle_button_t*)u;
    if (!c->enabled) return false;
    if (key == GMB_K_ENTER || key == ' ') { t->pressed = !t->pressed; return true; }
    return false;
}
static bool toggle_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_toggle_button_t* t = (gmb_toggle_button_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    t->pressed = !t->pressed;
    return true;
}
static void toggle_draw_px(void* u, gmb_control_t* c)
{
    gmb_toggle_button_t* t = (gmb_toggle_button_t*)u;
    gmb_px_draw_ex(GMB_PX_TOGGLE, c->x, c->y, c->width, c->height, t->caption, NULL, 0, t->pressed ? 1.0 : 0.0, 0, 0, NULL, 0, NULL, 0);
}
void gmb_toggle_button_init(gmb_toggle_button_t* t, int x, int y, int w, const char* caption)
{
    memset(t, 0, sizeof(*t));
    gmb_control_init(&t->ctrl, "", x, y, w, 1, toggle_draw, toggle_key);
    t->ctrl.draw_px_fn = toggle_draw_px;
    t->ctrl.ctrl_kind = GMB_CK_TOGGLE;
    t->ctrl.handle_click_fn = toggle_click;
    t->ctrl.user = t;
    snprintf(t->caption, sizeof(t->caption), "%s", caption ? caption : "");
}

/* ---- Label ---- */
