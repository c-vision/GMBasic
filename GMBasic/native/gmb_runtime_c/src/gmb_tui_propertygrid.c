/* ============================================================================
 * gmb_tui_propertygrid.c — gmb_tui_propertygrid. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== PropertyGrid ===================== */

static void propertygrid_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_property_grid_t* p = (gmb_property_grid_t*)u;
    gmb_cell_color_t fg, bg, sf, sb, ef, eb;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sf, &sb);
    ef = GMB_CELLCOLOR_BLACK; eb = GMB_CELLCOLOR_CYAN;
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = p->scroll_offset + i;
            if (idx >= p->items_count) break;
            char line[256];
            gmb_property_grid_render_line(p, idx, line, (int)sizeof(line));
            gmb_cell_color_t tf = fg, tb = bg;
            int hl = 0;
            if (p->editing && idx == p->selected_index) { tf = ef; tb = eb; hl = 1; }
            else if (idx == p->selected_index) { tf = sf; tb = sb; hl = 1; }
            if (hl) gmb_surface_rounded_rect(buf, c->x, c->y + i, c->width, 1, 2, tb, tb, 1);
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(tf).r << 16) | ((unsigned)(tf).g << 8) | (tf).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = p->scroll_offset + i;
        if (idx >= p->items_count) break;
        char line[256];
        gmb_property_grid_render_line(p, idx, line, (int)sizeof(line));
        if (p->editing && idx == p->selected_index) gmb_screen_setstring(buf, c->x, c->y + i, line, ef, eb);
        else if (idx == p->selected_index) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool propertygrid_key(void* u, gmb_control_t* c, int key)
{
    gmb_property_grid_t* p = (gmb_property_grid_t*)u;
    if (!c->enabled) return false;
    if (p->editing)
    {
        if (key == GMB_K_ENTER)
        {
            if (p->selected_index >= 0 && p->selected_index < p->items_count)
                snprintf(p->values[p->selected_index], sizeof(p->values[0]), "%s", p->edited_value);
            p->editing = false;
            return true;
        }
        if (key == GMB_K_ESC) { p->editing = false; return true; }
        if (key == GMB_K_BS)
        {
            int bl = (int)strlen(p->edited_value);
            if (bl > 0) p->edited_value[bl - 1] = '\0';
            return true;
        }
        if (key >= 32 && key <= 126)
        {
            int bl = (int)strlen(p->edited_value);
            if (bl < 127) { p->edited_value[bl] = (char)key; p->edited_value[bl + 1] = '\0'; }
            return true;
        }
        return false;
    }
    switch (key)
    {
        case GMB_K_UP:
            if (p->selected_index > 0) p->selected_index--;
            if (p->selected_index < p->scroll_offset) p->scroll_offset--;
            return true;
        case GMB_K_DOWN:
            if (p->selected_index < p->items_count - 1) p->selected_index++;
            if (p->selected_index >= p->scroll_offset + c->height) p->scroll_offset++;
            return true;
        case GMB_K_ENTER:
            p->editing = true;
            snprintf(p->edited_value, sizeof(p->edited_value), "%s", p->values[p->selected_index]);
            return true;
    }
    return false;
}
static bool propertygrid_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_property_grid_t* p = (gmb_property_grid_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = (y - c->y) + p->scroll_offset;
    if (idx >= 0 && idx < p->items_count)
    {
        p->selected_index = idx;
        p->editing = true;
        snprintf(p->edited_value, sizeof(p->edited_value), "%s", p->values[idx]);
    }
    return true;
}
static void propertygrid_draw_px(void* u, gmb_control_t* c)
{
    /* PropertyGrid pixel: card del tema, righe nome/valore, selezione accento. */
    gmb_property_grid_t* p = (gmb_property_grid_t*)u;
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
    if (rh < 14) rh = 14;
    int pad = soft ? 4 : 0;
    int vis = rh > 0 ? (h - pad * 2) / rh : 0;
    int start = p->scroll_offset; if (start < 0) start = 0;
    int half = (w - (soft ? 24 : 8)) / 2;
    for (int r = 0; r < vis && start + r < p->items_count; r++) {
        int i = start + r, ry = y + pad + r * rh;
        if (i == p->selected_index) gmb_gfx_fill_rounded_rect(x + 4, ry + 1, w - 8, rh - 2, soft ? (rh - 2) / 2 : 0, (acc & 0xFFFFFFu) | (0x22u << 24));
        int ty = ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2;
        {   /* riga "k=v": nome a sinistra, valore a destra */
            char nm[64]; const char* eq = strchr(p->names[i], '=');
            const char* val = p->values[i];
            if (eq && !val[0]) { size_t L = (size_t)(eq - p->names[i]); if (L > 63) L = 63; memcpy(nm, p->names[i], L); nm[L] = 0; val = eq + 1; }
            else snprintf(nm, sizeof(nm), "%s", p->names[i]);
            gmb_gfx_draw_text(nm, x + (soft ? 12 : 6), ty, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
            gmb_gfx_draw_text(val, x + (soft ? 12 : 6) + half, ty, 0, (int)(0x40000000u | (st.text_disabled & 0xFFFFFFu)));
        }
    }
}

void gmb_property_grid_init(gmb_property_grid_t* p, int x, int y, int w, int h)
{
    memset(p, 0, sizeof(*p));
    gmb_control_init(&p->ctrl, "", x, y, w, h, propertygrid_draw, propertygrid_key);
    p->ctrl.ctrl_kind = GMB_CK_PROPERTYGRID;
    p->ctrl.draw_px_fn = propertygrid_draw_px;
    p->ctrl.handle_click_fn = propertygrid_click;
    p->ctrl.user = p;
}
/* Aggiunge una proprietà da "nome=valore" (o solo "nome"). */
void gmb_property_grid_add_kv(gmb_property_grid_t* p, const char* kv)
{
    if (!p || !kv) return;
    const char* eq = strchr(kv, '=');
    if (!eq) { gmb_property_grid_add(p, kv, ""); return; }
    char nm[64]; size_t L = (size_t)(eq - kv); if (L > sizeof(nm) - 1) L = sizeof(nm) - 1;
    memcpy(nm, kv, L); nm[L] = 0;
    gmb_property_grid_add(p, nm, eq + 1);
}

void gmb_property_grid_add(gmb_property_grid_t* p, const char* name, const char* value)
{
    if (p->items_count >= 32) return;
    snprintf(p->names[p->items_count], sizeof(p->names[0]), "%s", name ? name : "");
    snprintf(p->values[p->items_count], sizeof(p->values[0]), "%s", value ? value : "");
    p->items_count++;
}
int gmb_property_grid_render_line(gmb_property_grid_t* p, int idx, char* out, int cap)
{
    if (idx < 0 || idx >= p->items_count)
    {
        out[0] = '\0';
        return 0;
    }
    const char* marker = idx == p->selected_index ? ">" : " ";
    const char* display = (p->editing && idx == p->selected_index) ? p->edited_value : p->values[idx];
    int o = snprintf(out, cap, "%s %-16s %s", marker, p->names[idx], display);
    if (o > p->ctrl.width) o = p->ctrl.width;
    else for (int i = o; i < p->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
