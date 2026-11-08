/* ============================================================================
 * gmb_tui_toggle.c — ToggleButton (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: un toggle è un gmb_widget_t (W_TOGGLE). On/off = `ival`,
 * etichetta = `caption`. Non esiste una struct parallela per la TUI.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool toggle_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* g = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    if (key == GMB_K_ENTER || key == ' ') { g->ival = g->ival != 0 ? 0.0 : 1.0; return true; }
    return false;
}
static bool toggle_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* g = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    g->ival = g->ival != 0 ? 0.0 : 1.0;
    return true;
}
void gmb_toggle_button_init(gmb_toggle_button_t* t, int x, int y, int w, const char* caption)
{
    gmb_widget_t* g = (gmb_widget_t*)t;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, 1, gmb_widget_draw_cells, toggle_key);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_TOGGLE;
    g->base.handle_click_fn = toggle_click;
    g->base.user = g;
    g->kind = W_TOGGLE;
    snprintf(g->caption, sizeof(g->caption), "%s", caption ? caption : "");
}

/* ---- Label ---- */
