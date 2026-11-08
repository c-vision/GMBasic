/* ============================================================================
 * gmb_tui_combobox.c — ComboBox (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: una combobox è un gmb_widget_t (W_DROPDOWN); voci in `items`,
 * scelta in `ival`, stato della lista in open/highlight/drop_down_height.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static bool combobox_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* cb = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    if (cb->open)
    {
        switch (key)
        {
            case GMB_K_UP:
                if (cb->highlight > 0) cb->highlight--;
                return true;
            case GMB_K_DOWN:
                if (cb->highlight < cb->nitems - 1) cb->highlight++;
                return true;
            case GMB_K_ENTER:
                if (cb->highlight >= 0 && cb->highlight < cb->nitems)
                {
                    cb->ival = cb->highlight;
                    cb->open = 0;
                }
                return true;
            case GMB_K_ESC:
                cb->open = 0;
                return true;
        }
    }
    else
    {
        switch (key)
        {
            case GMB_K_ENTER:
            case GMB_K_DOWN:
                if (cb->nitems > 0)
                {
                    cb->open = 1;
                    cb->highlight = cb->ival >= 0 ? (int)cb->ival : 0;
                }
                return true;
            case GMB_K_ESC:
                return true;
        }
    }
    return false;
}

static bool combobox_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* cb = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    r.h = cb->open ? cb->drop_down_height + 1 : 1;
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    if (cb->open)
    {
        int itemIdx = y - (c->y + 1);
        int maxH = cb->drop_down_height < cb->nitems ? cb->drop_down_height : cb->nitems;
        if (itemIdx >= 0 && itemIdx < maxH)
        {
            cb->ival = itemIdx;
            cb->highlight = itemIdx;
            cb->open = 0;
        }
        else
        {
            cb->open = 0;
        }
    }
    else
    {
        cb->open = 1;
        cb->highlight = cb->ival >= 0 ? (int)cb->ival : 0;
    }
    return true;
}

void gmb_combobox_init(gmb_combobox_t* c, int x, int y, int w)
{
    gmb_widget_t* g = (gmb_widget_t*)c;
    memset(g, 0, sizeof(*g));
    gmb_control_init(&g->base, "", x, y, w, 1, gmb_widget_draw_cells, combobox_key);
    g->base.handle_click_fn = combobox_click;
    g->base.user = g;
    g->base.draw_px_fn = gmb_widget_draw_px;
    g->base.ctrl_kind = GMB_CK_COMBOBOX;
    g->kind = W_DROPDOWN;
    g->nitems = 0;
    g->ival = -1;
    g->open = 0;
    g->highlight = 0;
    g->drop_down_height = 5;
}

void gmb_combobox_set_items(gmb_combobox_t* c, const char* const* items, int count)
{
    gmb_widget_t* g = (gmb_widget_t*)c;
    g->nitems = count < 32 ? count : 32;
    for (int i = 0; i < g->nitems && items; i++)
        snprintf(g->items[i], sizeof(g->items[i]), "%s", items[i] ? items[i] : "");
    if (g->nitems > 0 && g->ival < 0) { g->ival = 0; g->highlight = 0; }
}

const char* gmb_combobox_selected_value(gmb_combobox_t* c)
{
    gmb_widget_t* g = (gmb_widget_t*)c;
    int sel = (int)g->ival;
    if (sel >= 0 && sel < g->nitems) return g->items[sel];
    return NULL;
}

int gmb_combobox_render_line(gmb_combobox_t* c, char* out, int cap)
{
    gmb_widget_t* g = (gmb_widget_t*)c;
    const char* val = gmb_combobox_selected_value(c);
    /* TV THistory: icona a 3 celle (`▐↓▌`), quindi il testo si ferma a Width-3. */
    int maxw = g->base.width - (g->base.width >= 4 ? 3 : 1);
    if (maxw < 0) maxw = 0;
    int o = snprintf(out, cap, "%.*s", maxw, val ? val : "");
    for (int i = o; i < g->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
