/* ============================================================================
 * gmb_tui_panel.c — Panel (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: un pannello è un gmb_widget_t (W_PANEL); titolo in `title`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void gmb_panel_init(gmb_panel_t* p, int x, int y, int w, int h, const char* title)
{
    gmb_widget_t* g = (gmb_widget_t*)p;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, h, gmb_widget_draw_cells, NULL);
    g->base.user = g;
    g->base.draw_px_fn = gmb_widget_draw_px;   /* grafica: card nativa, no celle */
    g->kind = W_PANEL;
    snprintf(g->title, sizeof(g->title), "%s", title ? title : "");
    g->border = GMB_PB_SINGLE;
    g->draggable = 1;
    g->is_dragging = 0;
    g->drag_off_x = 0;
    g->drag_off_y = 0;
}

/* ---- 100% parity Panel.cs:202 — with_*, InnerArea, drag ---- */
void gmb_panel_with_title(gmb_panel_t* p, const char* title)
{
    if (!p) return;
    snprintf(p->title, sizeof(p->title), "%s", title ? title : "");
}
void gmb_panel_with_border(gmb_panel_t* p, int border)
{
    if (p) p->border = border;
}
gmb_rect_t gmb_panel_inner_area(const gmb_panel_t* p)
{
    gmb_rect_t r = { p->base.x, p->base.y, p->base.width, p->base.height };
    int inset = (p->border == GMB_PB_NONE) ? 0 : 1;
    r.x += inset; r.y += inset; r.w -= 2 * inset; r.h -= 2 * inset;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
void gmb_panel_start_drag(gmb_panel_t* p, int x, int y)
{
    if (!p || !p->draggable) return;
    p->is_dragging = 1;
    p->drag_off_x = x - p->base.x;
    p->drag_off_y = y - p->base.y;
}
bool gmb_panel_update_drag(gmb_panel_t* p, int x, int y)
{
    if (!p || !p->is_dragging) return false;
    p->base.x = x - p->drag_off_x;
    p->base.y = y - p->drag_off_y;
    return true;
}
void gmb_panel_end_drag(gmb_panel_t* p)
{
    if (p) p->is_dragging = 0;
}
