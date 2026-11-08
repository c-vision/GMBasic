/* ============================================================================
 * gmb_tui_reorderlist.c — gmb_tui_reorderlist. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ReorderListBox ===================== */

static void reorderlistbox_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    if (c->focused) { sf = bg; sb = fg; } else { sf = fg; sb = bg; }
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = r->first + i;
            if (idx >= r->nitems) break;
            int sel2 = (r->has_selected && idx == (int)r->ival);
            char line[256];
            gmb_reorder_listbox_render_line(r, idx, line, (int)sizeof(line));
            if (sel2) gmb_surface_rounded_rect(buf, c->x, c->y + i, c->width, 1, 2, sb, sb, 1);
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0,
                              sel2 ? (int)(0x40000000u | ((unsigned)(sf).r << 16) | ((unsigned)(sf).g << 8) | (sf).b) : (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = r->first + i;
        if (idx >= r->nitems) break;
        char line[256];
        gmb_reorder_listbox_render_line(r, idx, line, (int)sizeof(line));
        if (r->has_selected && idx == (int)r->ival) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool reorderlistbox_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (r->ctrl_held && r->has_selected && (int)r->ival > 0)
            {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", r->items[(int)r->ival]);
                snprintf(r->items[(int)r->ival], 256, "%s", r->items[(int)r->ival - 1]);
                snprintf(r->items[(int)r->ival - 1], 256, "%s", tmp);
                bool tc = r->selected_items[(int)r->ival];
                r->selected_items[(int)r->ival] = r->selected_items[(int)r->ival - 1];
                r->selected_items[(int)r->ival - 1] = tc;
                r->ival = (int)r->ival - 1;
                if ((int)r->ival < r->first) r->first--;
            }
            else if (r->has_selected && (int)r->ival > 0)
            {
                r->ival = (int)r->ival - 1;
                if ((int)r->ival < r->first) r->first--;
            }
            return true;
        case GMB_K_DOWN:
            if (r->ctrl_held && r->has_selected && (int)r->ival < r->nitems - 1)
            {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", r->items[(int)r->ival]);
                snprintf(r->items[(int)r->ival], 256, "%s", r->items[(int)r->ival + 1]);
                snprintf(r->items[(int)r->ival + 1], 256, "%s", tmp);
                bool tc = r->selected_items[(int)r->ival];
                r->selected_items[(int)r->ival] = r->selected_items[(int)r->ival + 1];
                r->selected_items[(int)r->ival + 1] = tc;
                r->ival = (int)r->ival + 1;
                if ((int)r->ival >= r->first + c->height) r->first++;
            }
            else if (r->has_selected && (int)r->ival < r->nitems - 1)
            {
                r->ival = (int)r->ival + 1;
                if ((int)r->ival >= r->first + c->height) r->first++;
            }
            else if (!r->has_selected && r->nitems > 0)
            {
                r->has_selected = true;
                r->ival = 0;
            }
            return true;
        case ' ':
            if (r->multi_select && r->has_selected && (int)r->ival < r->nitems)
                r->selected_items[(int)r->ival] = !r->selected_items[(int)r->ival];
            return true;
    }
    return false;
}
static bool reorderlistbox_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t rr = gmb_control_rect(c);
    if (!rect_contains(rr, x, y)) return false;
    c->focused = true;
    int clicked = (y - c->y) + r->first;
    if (clicked >= 0 && clicked < r->nitems)
    {
        r->has_selected = true;
        r->ival = clicked;
        if (r->multi_select && clicked < r->nitems)
            r->selected_items[clicked] = !r->selected_items[clicked];
    }
    return true;
}
static bool reorderlistbox_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK: case GMB_MA_DOUBLECLICK:
            return reorderlistbox_click(u, c, a->x, a->y);
        case GMB_MA_SCROLLUP:
            if (r->has_selected && (int)r->ival > 0)
            {
                r->ival = (int)r->ival - 1;
                if ((int)r->ival < r->first) r->first--;
            }
            return true;
        case GMB_MA_SCROLLDOWN:
            if (r->has_selected && (int)r->ival < r->nitems - 1)
            {
                r->ival = (int)r->ival + 1;
                if ((int)r->ival >= r->first + c->height) r->first++;
            }
            else if (!r->has_selected && r->nitems > 0)
            {
                r->has_selected = true;
                r->ival = 0;
            }
            return true;
    }
    return false;
}
static void reorderlistbox_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* r = (gmb_widget_t*)u;
    const char* its[32];
    int n = r->nitems; if (n > 32) n = 32;
    for (int i = 0; i < n; i++) its[i] = r->items[i];
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, its, n, (int)r->ival, 0, n, NULL, r->first, NULL, 0);
}
void gmb_reorder_listbox_init(gmb_reorder_listbox_t* rl, int x, int y, int w, int h)
{
    gmb_widget_t* r = (gmb_widget_t*)rl;
    memset(r, 0, sizeof(*r));
    gmb_control_init(&r->base, "", x, y, w, h, reorderlistbox_draw, reorderlistbox_key);
    r->base.draw_px_fn = reorderlistbox_draw_px;
    r->base.handle_click_fn = reorderlistbox_click;
    r->base.handle_mouse_fn = reorderlistbox_mouse;
    r->base.user = r;
    r->kind = W_LISTBOX;
}
void gmb_reorder_listbox_set_items(gmb_reorder_listbox_t* rl, const char* const* items, int count)
{
    gmb_widget_t* r = (gmb_widget_t*)rl;
    r->nitems = count < 32 ? count : 32;
    for (int i = 0; i < r->nitems && items; i++)
    {
        snprintf(r->items[i], sizeof(r->items[i]), "%s", items[i] ? items[i] : "");
        r->selected_items[i] = false;
    }
    if (r->nitems > 0 && !r->has_selected) { r->has_selected = true; r->ival = 0; }
}
int gmb_reorder_listbox_render_line(gmb_reorder_listbox_t* rl, int idx, char* out, int cap)
{
    gmb_widget_t* r = (gmb_widget_t*)rl;
    const char* item = idx < r->nitems ? r->items[idx] : "";
    const char* marker = (r->has_selected && idx == (int)r->ival) ? ">" : " ";
    const char* check = "";
    char chk[8] = "";
    if (r->multi_select)
    {
        snprintf(chk, sizeof(chk), "%s", (idx < r->nitems && r->selected_items[idx]) ? "[X] " : "[ ] ");
        check = chk;
    }
    int o = snprintf(out, cap, "%s%s%s", marker, check, item);
    if (o > r->base.width) o = r->base.width;
    else for (int i = o; i < r->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
