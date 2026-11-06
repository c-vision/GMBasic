/* ============================================================================
 * gmb_tui_listbox.c — ListBox.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static void listbox_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_listbox_t* l = (gmb_listbox_t*)u;
    for (int i = 0; i < c->height; i++)
    {
        int idx = l->scroll_offset + i;
        if (idx >= l->items_count) break;
        bool sel = idx == l->selected;
        gmb_cell_color_t fg, bg;
        gmb_control_style_effective(&c->style, c->focused, false, sel, &fg, &bg);
        char line[128];
        gmb_listbox_render_line(l, idx, line, (int)sizeof(line));
        char cased[160];
        gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
        gmb_screen_setstring(buf, c->x, c->y + i, cased, fg, bg);
        /* TV TListViewer: barra verticale `│` sull'ultima colonna. */
        if (c->width > 1)
        {
            gmb_theme_t th = gmb_ui_theme();
            gmb_screen_setchar(buf, c->x + c->width - 1, c->y + i, 0x2502,
                               th.dim, th.window_bg);
        }
    }
}
static bool listbox_key(void* u, gmb_control_t* c, int key)
{
    gmb_listbox_t* l = (gmb_listbox_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (l->selected > 0) l->selected--;
            if (l->selected < l->scroll_offset) l->scroll_offset = l->selected;
            return true;
        case GMB_K_DOWN:
            if (l->selected < l->items_count - 1) l->selected++;
            if (l->selected >= l->scroll_offset + c->height) l->scroll_offset = l->selected - c->height + 1;
            return true;
        case GMB_K_PGUP:
            l->selected -= c->height;
            if (l->selected < 0) l->selected = 0;
            if (l->selected < l->scroll_offset) l->scroll_offset = l->selected;
            return true;
        case GMB_K_PGDN:
            l->selected += c->height;
            if (l->selected >= l->items_count) l->selected = l->items_count - 1;
            if (l->selected >= l->scroll_offset + c->height) l->scroll_offset = l->selected - c->height + 1;
            return true;
        case GMB_K_HOME: l->selected = 0; l->scroll_offset = 0; return true;
        case GMB_K_END: l->selected = l->items_count - 1; l->scroll_offset = l->selected - c->height + 1; if (l->scroll_offset < 0) l->scroll_offset = 0; return true;
    }
    return false;
}
static bool listbox_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_listbox_t* l = (gmb_listbox_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = l->scroll_offset + (y - c->y);
    if (idx >= 0 && idx < l->items_count)
    {
        l->selected = idx;
        /* C# HandleClick -> EnsureVisible() */
        if (l->selected < l->scroll_offset) l->scroll_offset = l->selected;
        if (l->selected >= l->scroll_offset + c->height) l->scroll_offset = l->selected - c->height + 1;
    }
    return true;
}
/* C# ListBox.HandleMouse: click/dbl -> HandleClick; rotella -> Selected/Top. */
static bool listbox_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_listbox_t* l = (gmb_listbox_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK:
        case GMB_MA_DOUBLECLICK:
            return listbox_click(u, c, a->x, a->y);
        case GMB_MA_SCROLLUP:
            if (l->selected > 0)
            {
                l->selected--;
                if (l->selected < l->scroll_offset) l->scroll_offset = l->selected;
            }
            return true;
        case GMB_MA_SCROLLDOWN:
            if (l->selected >= 0 && l->selected < l->items_count - 1)
            {
                l->selected++;
                if (l->selected >= l->scroll_offset + c->height) l->scroll_offset = l->selected - c->height + 1;
            }
            return true;
        default:
            return false;
    }
}
static void listbox_draw_px(void* u, gmb_control_t* c)
{
    gmb_listbox_t* l = (gmb_listbox_t*)u;
    const char* _it[32]; for (int i = 0; i < l->items_count && i < 32; i++) _it[i] = l->items[i];
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, _it, l->items_count, l->selected, 0, 0, NULL, l->scroll_offset, NULL, 0);
}
void gmb_listbox_init(gmb_listbox_t* l, int x, int y, int w, int h)
{
    memset(l, 0, sizeof(*l));
    gmb_control_init(&l->ctrl, "", x, y, w, h, listbox_draw, listbox_key);
    l->ctrl.draw_px_fn = listbox_draw_px;
    l->ctrl.ctrl_kind = GMB_CK_LISTBOX;
    l->ctrl.handle_click_fn = listbox_click;
    l->ctrl.handle_mouse_fn = listbox_mouse;
    l->ctrl.user = l;
}
void gmb_listbox_set_items(gmb_listbox_t* l, const char* const* items, int count)
{
    l->items_count = count < 32 ? count : 32;
    for (int i = 0; i < l->items_count && items; i++)
        snprintf(l->items[i], sizeof(l->items[i]), "%s", items[i] ? items[i] : "");
}
const char* gmb_listbox_selected_value(gmb_listbox_t* l)
{
    if (l->selected >= 0 && l->selected < l->items_count) return l->items[l->selected];
    return NULL;
}
int gmb_listbox_render_line(gmb_listbox_t* l, int idx, char* out, int cap)
{
    /* TV TListViewer: item a col 1, ultima colonna riservata alla barra `│`. */
    const char* v = idx < l->items_count ? l->items[idx] : "";
    int w = l->ctrl.width > 1 ? l->ctrl.width - 1 : l->ctrl.width;
    int o = 0;
    out[o++] = ' ';
    int n = (int)strlen(v);
    if (n > w - 1) n = w - 1 > 0 ? w - 1 : 0;
    for (int i = 0; i < n && o < cap - 1; i++) out[o++] = v[i];
    for (; o < w && o < cap - 1; o++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- ScrollBar ---- */
