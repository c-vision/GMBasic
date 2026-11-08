/* ============================================================================
 * gmb_tui_tab.c — Tab (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: le schede sono un gmb_widget_t (W_TABS); nomi in `items`,
 * indice attivo in `ival`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool tab_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    if (!c->enabled || t->nitems == 0) return false;
    int sel = (int)t->ival;
    switch (key)
    {
        case GMB_K_LEFT:
            if (sel > 0) t->ival = sel - 1;
            return true;
        case GMB_K_RIGHT:
            if (sel < t->nitems - 1) t->ival = sel + 1;
            return true;
        case GMB_K_HOME: t->ival = 0; return true;
        case GMB_K_END: t->ival = t->nitems - 1; return true;
    }
    return false;
}

static bool tab_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int cx = c->x;
    for (int i = 0; i < t->nitems; i++)
    {
        char tb[80];
        snprintf(tb, sizeof(tb), " %s ", t->items[i]);
        int tl = (int)strlen(tb);
        if (cx + tl > c->x + c->width) break;
        if (x >= cx && x < cx + tl) { t->ival = i; return true; }
        cx += tl;
    }
    return true;
}

void gmb_tab_init(gmb_tab_t* t, int x, int y, int w)
{
    gmb_widget_t* g = (gmb_widget_t*)t;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, 1, gmb_widget_draw_cells, tab_key);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_TAB;
    g->base.handle_click_fn = tab_click;
    g->base.user = g;
    g->kind = W_TABS;
}

void gmb_tab_set_tabs(gmb_tab_t* t, const char* const* tabs, int count)
{
    gmb_widget_t* g = (gmb_widget_t*)t;
    g->nitems = count < 32 ? count : 32;
    for (int i = 0; i < g->nitems && tabs; i++)
        snprintf(g->items[i], sizeof(g->items[i]), "%s", tabs[i] ? tabs[i] : "");
}

int gmb_tab_render_line(gmb_tab_t* t, char* out, int cap)
{
    /* C# RenderLine: concatena " name " finché non supera Width, poi PadRight(Width). */
    gmb_widget_t* g = (gmb_widget_t*)t;
    int o = 0;
    for (int i = 0; i < g->nitems; i++)
    {
        char tb[80];
        snprintf(tb, sizeof(tb), " %s ", g->items[i]);
        int tl = (int)strlen(tb);
        if (o + tl > g->base.width) break;
        if (o + tl > cap - 1) break;
        memcpy(out + o, tb, (size_t)tl);
        o += tl;
    }
    for (; o < g->base.width && o < cap - 1; o++) out[o] = ' ';
    out[o] = '\0';
    return o;
}
