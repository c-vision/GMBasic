/* ============================================================================
 * gmb_tui_radio.c — RadioGroup (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: un radio group è un gmb_widget_t (W_RADIO); voci in `items`,
 * indice scelto in `ival`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool radio_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    if (!c->enabled || r->nitems == 0) return false;
    int sel = (int)r->ival;
    switch (key)
    {
        case GMB_K_UP: case GMB_K_LEFT:
            if (sel > 0) r->ival = sel - 1;
            return true;
        case GMB_K_DOWN: case GMB_K_RIGHT:
            if (sel < r->nitems - 1) r->ival = sel + 1;
            return true;
    }
    return false;
}
static bool radio_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t rect = gmb_control_rect(c);
    if (!rect_contains(rect, x, y)) return false;
    c->focused = true;
    int idx = y - c->y;
    if (idx >= 0 && idx < r->nitems) r->ival = idx;
    return true;
}
void gmb_radio_group_init(gmb_radio_group_t* r, int x, int y, int w, int h)
{
    gmb_widget_t* g = (gmb_widget_t*)r;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, h, gmb_widget_draw_cells, radio_key);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_RADIO;
    g->base.handle_click_fn = radio_click;
    g->base.user = g;
    g->kind = W_RADIO;
}
void gmb_radio_group_set_options(gmb_radio_group_t* r, const char* const* options, int count)
{
    gmb_widget_t* g = (gmb_widget_t*)r;
    g->nitems = count < 32 ? count : 32;
    for (int i = 0; i < g->nitems && options; i++)
        snprintf(g->items[i], sizeof(g->items[i]), "%s", options[i] ? options[i] : "");
}
const char* gmb_radio_group_selected_value(gmb_radio_group_t* r)
{
    gmb_widget_t* g = (gmb_widget_t*)r;
    int sel = (int)g->ival;
    if (sel >= 0 && sel < g->nitems) return g->items[sel];
    return NULL;
}
int gmb_radio_group_render_line(gmb_radio_group_t* r, int idx, char* out, int cap)
{
    gmb_widget_t* g = (gmb_widget_t*)r;
    int sel = (int)g->ival;
    const char* mark = idx == sel ? "(\xE2\x80\xA2)" : "( )";
    /* TV TRadioButtons: icona " ( ) " — spazio iniziale compreso. */
    int o = snprintf(out, cap, " %s %s", mark, idx < g->nitems ? g->items[idx] : "");
    if (o > g->base.width) o = g->base.width;
    else for (int i = o; i < g->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- ComboBox ---- */
