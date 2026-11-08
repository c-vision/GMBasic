/* ============================================================================
 * gmb_tui_statusbar.c — StatusBar.
 *
 * MODELLO UNICO: la status bar è un gmb_widget_t (W_STATUSBAR); il testo vive in
 * `label`. Il renderer (pixel o celle) è scelto da gmb_control_draw in base alla
 * superficie: qui restano solo il costruttore e le funzioni del modello.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <string.h>

void gmb_status_bar_init(gmb_status_bar_t* s, int x, int y, int w)
{
    gmb_widget_t* g = (gmb_widget_t*)s;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, 1, gmb_widget_draw_cells, NULL);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_STATUSBAR;
    g->base.user = g;
    g->kind = W_STATUSBAR;
}

void gmb_status_bar_set_text(gmb_status_bar_t* s, const char* text)
{
    if (!s) return;
    snprintf(s->label, sizeof(s->label), "%s", text ? text : "");
}

int gmb_status_bar_render_line(gmb_status_bar_t* s, char* out, int cap)
{
    if (!s || !out || cap < 1) return 0;
    const char* t = s->label;
    int w = s->base.width > 0 ? s->base.width : 0;
    int o = snprintf(out, cap, "%s", t);
    for (int i = o; i < w && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
