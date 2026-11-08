/* ============================================================================
 * gmb_tui_listbox.c — ListBox (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: una listbox è un gmb_widget_t (W_LISTBOX); voci in `items`,
 * indice scelto in `ival`, prima riga visibile in `first`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool listbox_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* l = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int sel = (int)l->ival;
    switch (key)
    {
        case GMB_K_UP:
            if (sel > 0) sel--;
            if (sel < l->first) l->first = sel;
            l->ival = sel;
            return true;
        case GMB_K_DOWN:
            if (sel < l->nitems - 1) sel++;
            if (sel >= l->first + c->height) l->first = sel - c->height + 1;
            l->ival = sel;
            return true;
        case GMB_K_PGUP:
            sel -= c->height;
            if (sel < 0) sel = 0;
            if (sel < l->first) l->first = sel;
            l->ival = sel;
            return true;
        case GMB_K_PGDN:
            sel += c->height;
            if (sel >= l->nitems) sel = l->nitems - 1;
            if (sel >= l->first + c->height) l->first = sel - c->height + 1;
            l->ival = sel;
            return true;
        case GMB_K_HOME: l->ival = 0; l->first = 0; return true;
        case GMB_K_END: l->ival = l->nitems - 1; l->first = (int)l->ival - c->height + 1; if (l->first < 0) l->first = 0; return true;
    }
    return false;
}
static bool listbox_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* l = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = l->first + (y - c->y);
    if (idx >= 0 && idx < l->nitems)
    {
        l->ival = idx;
        /* C# HandleClick -> EnsureVisible() */
        if ((int)l->ival < l->first) l->first = (int)l->ival;
        if ((int)l->ival >= l->first + c->height) l->first = (int)l->ival - c->height + 1;
    }
    return true;
}
/* C# ListBox.HandleMouse: click/dbl -> HandleClick; rotella -> Selected/Top. */
static bool listbox_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_widget_t* l = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK:
        case GMB_MA_DOUBLECLICK:
            return listbox_click(u, c, a->x, a->y);
        case GMB_MA_SCROLLUP:
            if ((int)l->ival > 0)
            {
                l->ival = (int)l->ival - 1;
                if ((int)l->ival < l->first) l->first = (int)l->ival;
            }
            return true;
        case GMB_MA_SCROLLDOWN:
            if (l->ival >= 0 && (int)l->ival < l->nitems - 1)
            {
                l->ival = (int)l->ival + 1;
                if ((int)l->ival >= l->first + c->height) l->first = (int)l->ival - c->height + 1;
            }
            return true;
        default:
            return false;
    }
}
void gmb_listbox_init(gmb_listbox_t* l, int x, int y, int w, int h)
{
    gmb_widget_t* g = (gmb_widget_t*)l;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, h, gmb_widget_draw_cells, listbox_key);
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_LISTBOX;
    g->base.handle_click_fn = listbox_click;
    g->base.handle_mouse_fn = listbox_mouse;
    g->base.user = g;
    g->kind = W_LISTBOX;
}
void gmb_listbox_set_items(gmb_listbox_t* l, const char* const* items, int count)
{
    gmb_widget_t* g = (gmb_widget_t*)l;
    g->nitems = count < 32 ? count : 32;
    for (int i = 0; i < g->nitems && items; i++)
        snprintf(g->items[i], sizeof(g->items[i]), "%s", items[i] ? items[i] : "");
}
const char* gmb_listbox_selected_value(gmb_listbox_t* l)
{
    gmb_widget_t* g = (gmb_widget_t*)l;
    int sel = (int)g->ival;
    if (sel >= 0 && sel < g->nitems) return g->items[sel];
    return NULL;
}
int gmb_listbox_render_line(gmb_listbox_t* l, int idx, char* out, int cap)
{
    /* TV TListViewer: item a col 1, ultima colonna riservata alla barra `│`. */
    gmb_widget_t* g = (gmb_widget_t*)l;
    const char* v = idx < g->nitems ? g->items[idx] : "";
    int w = g->base.width > 1 ? g->base.width - 1 : g->base.width;
    int o = 0;
    out[o++] = ' ';
    int n = (int)strlen(v);
    if (n > w - 1) n = w - 1 > 0 ? w - 1 : 0;
    for (int i = 0; i < n && o < cap - 1; i++) out[o++] = v[i];
    /* Padding a TUTTE le celle rimanenti (un solo incremento per carattere). */
    for (; o < w && o < cap - 1; o++) out[o] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- ScrollBar ---- */
