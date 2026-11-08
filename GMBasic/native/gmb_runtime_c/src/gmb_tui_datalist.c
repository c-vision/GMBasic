/* ============================================================================
 * gmb_tui_datalist.c — DataList (specchio ClipperLib/Controls).
 *
 * MODELLO UNICO: data list = gmb_widget_t. Voci in `items`/`nitems`, scelta in
 * `ival`, prima riga in `first`, multi-selezione in `selected_items[]`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== DataList ===================== */

#define DL_SEL(d)     ((int)((gmb_widget_t*)(d))->ival)
#define DL_SET_SEL(d, v) (((gmb_widget_t*)(d))->ival = (double)(v))

static void datalist_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sf, &sb);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = d->first + i;
            if (idx >= d->nitems) break;
            int sel2 = (idx == DL_SEL(d)) || (d->multi_select && d->selected_items[idx]);
            char line[256];
            gmb_data_list_render_line(d, idx, line, (int)sizeof(line));
            if (sel2) gmb_surface_rounded_rect(buf, c->x, c->y + i, c->width, 1, 2, sb, sb, 1);
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0,
                              sel2 ? (int)(0x40000000u | ((unsigned)(sf).r << 16) | ((unsigned)(sf).g << 8) | (sf).b) : (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = d->first + i;
        if (idx >= d->nitems) break;
        char line[256];
        gmb_data_list_render_line(d, idx, line, (int)sizeof(line));
        if (idx == DL_SEL(d)) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else if (d->multi_select && d->selected_items[idx]) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool datalist_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int sel = DL_SEL(d);
    switch (key)
    {
        case GMB_K_UP:
            if (sel > 0) { sel--; if (sel < d->first) d->first--; DL_SET_SEL(d, sel); }
            return true;
        case GMB_K_DOWN:
            if (sel < d->nitems - 1) { sel++; if (sel >= d->first + c->height) d->first++; DL_SET_SEL(d, sel); }
            return true;
        case GMB_K_PGUP:
            sel = sel - c->height > 0 ? sel - c->height : 0;
            d->first = d->first - c->height > 0 ? d->first - c->height : 0;
            DL_SET_SEL(d, sel);
            return true;
        case GMB_K_PGDN:
            sel = sel + c->height < d->nitems - 1 ? sel + c->height : d->nitems - 1;
            d->first = d->first + c->height < (d->nitems - c->height > 0 ? d->nitems - c->height : 0) ? d->first + c->height : (d->nitems - c->height > 0 ? d->nitems - c->height : 0);
            DL_SET_SEL(d, sel);
            return true;
        case GMB_K_HOME: DL_SET_SEL(d, 0); d->first = 0; return true;
        case GMB_K_END:
            DL_SET_SEL(d, d->nitems - 1);
            d->first = d->nitems - c->height > 0 ? d->nitems - c->height : 0;
            return true;
        case ' ':
            if (d->multi_select && sel >= 0 && sel < d->nitems)
                d->selected_items[sel] = !d->selected_items[sel];
            return true;
        default:
            if (d->searchable && key >= 32 && key <= 126)
            {
                int bl = (int)strlen(d->search_buffer);
                if (bl < 63) { d->search_buffer[bl] = (char)key; d->search_buffer[bl + 1] = '\0'; }
                for (int i = 0; i < d->nitems; i++)
                    if (strncasecmp(d->items[i], d->search_buffer, strlen(d->search_buffer)) == 0)
                    { DL_SET_SEL(d, i); break; }
                return true;
            }
            if (key == GMB_K_BS && d->searchable && d->search_buffer[0])
            {
                int bl = (int)strlen(d->search_buffer);
                if (bl > 0) d->search_buffer[bl - 1] = '\0';
                return true;
            }
            break;
    }
    return false;
}
static bool datalist_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    /* In GRAPHICS il rect e' in PIXEL (px_*); in TEXT in celle. */
    int rx = c->px_w > 0 ? c->px_x : c->x;
    int ry = c->px_h > 0 ? c->px_y : c->y;
    int rw = c->px_w > 0 ? c->px_w : c->width;
    int rh = c->px_h > 0 ? c->px_h : c->height;
    if (x < rx || x >= rx + rw || y < ry || y >= ry + rh) return false;
    c->focused = true;
    int idx;
    if (c->px_h > 0)
    {
        gmb_widget_style_t st = gmb_widget_style_current();
        int soft = (st.shape == 1 && st.radius >= 12);
        int rowh = (st.row_h > 0 ? st.row_h : 22);
        int top = ry + (soft ? 4 : 0);
        idx = (y - top) / (rowh > 0 ? rowh : 1);
    }
    else idx = (y - c->y);
    idx += d->first;
    if (idx >= 0 && idx < d->nitems)
    {
        DL_SET_SEL(d, idx);
        if (d->multi_select) d->selected_items[idx] = !d->selected_items[idx];
    }
    return true;
}
static bool datalist_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK: case GMB_MA_DOUBLECLICK:
            return datalist_click(u, c, a->x, a->y);
        case GMB_MA_SCROLLUP:
            if (DL_SEL(d) > 0) DL_SET_SEL(d, DL_SEL(d) - 1);
            if (DL_SEL(d) < d->first) d->first--;
            return true;
        case GMB_MA_SCROLLDOWN:
            if (DL_SEL(d) < d->nitems - 1) DL_SET_SEL(d, DL_SEL(d) + 1);
            if (DL_SEL(d) >= d->first + c->height) d->first++;
            return true;
        default:
            return false;
    }
}
static void datalist_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    if (w < 8 || h < 8) return;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    int rh = (st.row_h > 0 ? st.row_h : 22);
    int vis = rh > 0 ? (h - (soft ? 8 : 0)) / rh : 0;
    int start = d->first; if (start < 0) start = 0;
    for (int r = 0; r < vis && start + r < d->nitems; r++) {
        int i = start + r, ry = y + (soft ? 4 : 0) + r * rh;
        int sel = (i == DL_SEL(d));
        if (sel && soft) gmb_gfx_fill_rounded_rect(x + 4, ry + 1, w - 8, rh - 2, (rh - 2) / 2, (acc & 0xFFFFFFu) | (0x2Au << 24));
        else if (sel) gmb_gfx_fill_rounded_rect(x + 1, ry, w - 2, rh - 1, 0, st.fill_hover);
        int tx = x + (soft ? 14 : 6);
        if (d->multi_select) {
            int bs = rh - 10; if (bs < 8) bs = 8;
            int by = ry + (rh - bs) / 2;
            gmb_gfx_stroke_rounded_rect(x + 6, by, bs, bs, 3, st.border);
            if (d->selected_items[i]) gmb_gfx_fill_rounded_rect(x + 9, by + 3, bs - 6, bs - 6, 2, acc);
            tx = x + 6 + bs + 8;
        }
        gmb_gfx_draw_text(d->items[i], tx, ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                          (int)(0x40000000u | ((sel ? acc : st.text) & 0xFFFFFFu)));
    }
}
void gmb_data_list_init(gmb_data_list_t* dl, int x, int y, int w, int h)
{
    gmb_widget_t* d = (gmb_widget_t*)dl;
    memset(d, 0, sizeof(*d));
    gmb_control_init(&d->base, "", x, y, w, h, datalist_draw, datalist_key);
    d->base.ctrl_kind = GMB_CK_DATALIST;
    d->base.handle_click_fn = datalist_click;
    d->base.draw_px_fn = datalist_draw_px;
    d->base.handle_mouse_fn = datalist_mouse;
    d->base.user = d;
    d->kind = W_LISTBOX;
}
void gmb_data_list_set_items(gmb_data_list_t* dl, const char* const* items, int count)
{
    gmb_widget_t* d = (gmb_widget_t*)dl;
    d->nitems = count < GMB_DL_MAX_ITEMS ? count : GMB_DL_MAX_ITEMS;
    for (int i = 0; i < d->nitems && items; i++)
        snprintf(d->items[i], sizeof(d->items[i]), "%s", items[i] ? items[i] : "");
}
int gmb_data_list_render_line(gmb_data_list_t* dl, int idx, char* out, int cap)
{
    gmb_widget_t* d = (gmb_widget_t*)dl;
    if (idx < 0 || idx >= d->nitems)
    {
        for (int i = 0; i < d->base.width && i < cap - 1; i++) out[i] = ' ';
        out[d->base.width < cap - 1 ? d->base.width : cap - 1] = '\0';
        return d->base.width;
    }
    const char* marker = idx == DL_SEL(d) ? ">" : " ";
    const char* check = (d->multi_select && d->selected_items[idx]) ? "[X]" : "  ";
    int o = snprintf(out, cap, "%s%s %s", marker, check, d->items[idx]);
    if (o > d->base.width) o = d->base.width;
    else for (int i = o; i < d->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
