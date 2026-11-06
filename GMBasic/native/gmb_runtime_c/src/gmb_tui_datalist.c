/* ============================================================================
 * gmb_tui_datalist.c — gmb_tui_datalist. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* ===================== DataList ===================== */

static void datalist_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_data_list_t* d = (gmb_data_list_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sf, &sb);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = d->scroll_offset + i;
            if (idx >= d->items_count) break;
            int sel2 = (idx == d->selected_index) || (d->multi_select && d->selected_items[idx]);
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
        int idx = d->scroll_offset + i;
        if (idx >= d->items_count) break;
        char line[256];
        gmb_data_list_render_line(d, idx, line, (int)sizeof(line));
        if (idx == d->selected_index) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else if (d->multi_select && d->selected_items[idx]) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool datalist_key(void* u, gmb_control_t* c, int key)
{
    gmb_data_list_t* d = (gmb_data_list_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (d->selected_index > 0) { d->selected_index--; if (d->selected_index < d->scroll_offset) d->scroll_offset--; }
            return true;
        case GMB_K_DOWN:
            if (d->selected_index < d->items_count - 1) { d->selected_index++; if (d->selected_index >= d->scroll_offset + c->height) d->scroll_offset++; }
            return true;
        case GMB_K_PGUP:
            d->selected_index = d->selected_index - c->height > 0 ? d->selected_index - c->height : 0;
            d->scroll_offset = d->scroll_offset - c->height > 0 ? d->scroll_offset - c->height : 0;
            return true;
        case GMB_K_PGDN:
            d->selected_index = d->selected_index + c->height < d->items_count - 1 ? d->selected_index + c->height : d->items_count - 1;
            d->scroll_offset = d->scroll_offset + c->height < (d->items_count - c->height > 0 ? d->items_count - c->height : 0) ? d->scroll_offset + c->height : (d->items_count - c->height > 0 ? d->items_count - c->height : 0);
            return true;
        case GMB_K_HOME: d->selected_index = 0; d->scroll_offset = 0; return true;
        case GMB_K_END:
            d->selected_index = d->items_count - 1;
            d->scroll_offset = d->items_count - c->height > 0 ? d->items_count - c->height : 0;
            return true;
        case ' ':
            if (d->multi_select && d->selected_index >= 0 && d->selected_index < d->items_count)
                d->selected_items[d->selected_index] = !d->selected_items[d->selected_index];
            return true;
        default:
            if (d->searchable && key >= 32 && key <= 126)
            {
                int bl = (int)strlen(d->search_buffer);
                if (bl < 63) { d->search_buffer[bl] = (char)key; d->search_buffer[bl + 1] = '\0'; }
                for (int i = 0; i < d->items_count; i++)
                    if (strncasecmp(d->items[i], d->search_buffer, strlen(d->search_buffer)) == 0)
                    { d->selected_index = i; break; }
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
    gmb_data_list_t* d = (gmb_data_list_t*)u;
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
    idx += d->scroll_offset;
    if (idx >= 0 && idx < d->items_count)
    {
        d->selected_index = idx;
        if (d->multi_select) d->selected_items[idx] = !d->selected_items[idx];
    }
    return true;
}
static bool datalist_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_data_list_t* d = (gmb_data_list_t*)u;
    if (!c->enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK: case GMB_MA_DOUBLECLICK:
            return datalist_click(u, c, a->x, a->y);
        case GMB_MA_SCROLLUP:
            if (d->selected_index > 0) d->selected_index--;
            if (d->selected_index < d->scroll_offset) d->scroll_offset--;
            return true;
        case GMB_MA_SCROLLDOWN:
            if (d->selected_index < d->items_count - 1) d->selected_index++;
            if (d->selected_index >= d->scroll_offset + c->height) d->scroll_offset++;
            return true;
        default:
            return false;
    }
}
static void datalist_draw_px(void* u, gmb_control_t* c)
{
    gmb_data_list_t* d = (gmb_data_list_t*)u;
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
    int start = d->scroll_offset; if (start < 0) start = 0;
    for (int r = 0; r < vis && start + r < d->items_count; r++) {
        int i = start + r, ry = y + (soft ? 4 : 0) + r * rh;
        int sel = (i == d->selected_index);
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
void gmb_data_list_init(gmb_data_list_t* d, int x, int y, int w, int h)
{
    memset(d, 0, sizeof(*d));
    gmb_control_init(&d->ctrl, "", x, y, w, h, datalist_draw, datalist_key);
    d->ctrl.ctrl_kind = GMB_CK_DATALIST;
    d->ctrl.handle_click_fn = datalist_click;
    d->ctrl.draw_px_fn = datalist_draw_px;
    d->ctrl.handle_mouse_fn = datalist_mouse;
    d->ctrl.user = d;
}
void gmb_data_list_set_items(gmb_data_list_t* d, const char* const* items, int count)
{
    d->items_count = count < GMB_DL_MAX_ITEMS ? count : GMB_DL_MAX_ITEMS;
    for (int i = 0; i < d->items_count && items; i++)
        snprintf(d->items[i], sizeof(d->items[i]), "%s", items[i] ? items[i] : "");
}
int gmb_data_list_render_line(gmb_data_list_t* d, int idx, char* out, int cap)
{
    if (idx < 0 || idx >= d->items_count)
    {
        for (int i = 0; i < d->ctrl.width && i < cap - 1; i++) out[i] = ' ';
        out[d->ctrl.width < cap - 1 ? d->ctrl.width : cap - 1] = '\0';
        return d->ctrl.width;
    }
    const char* marker = idx == d->selected_index ? ">" : " ";
    const char* check = (d->multi_select && d->selected_items[idx]) ? "[X]" : "  ";
    int o = snprintf(out, cap, "%s%s %s", marker, check, d->items[idx]);
    if (o > d->ctrl.width) o = d->ctrl.width;
    else for (int i = o; i < d->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
