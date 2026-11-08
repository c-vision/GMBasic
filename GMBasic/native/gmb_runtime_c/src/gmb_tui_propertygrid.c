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
    gmb_widget_t* p = (gmb_widget_t*)u;
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
            int idx = p->first + i;
            if (idx >= p->nitems) break;
            char line[256];
            gmb_property_grid_render_line(p, idx, line, (int)sizeof(line));
            gmb_cell_color_t tf = fg, tb = bg;
            int hl = 0;
            if (p->editing && idx == (int)p->ival) { tf = ef; tb = eb; hl = 1; }
            else if (idx == (int)p->ival) { tf = sf; tb = sb; hl = 1; }
            if (hl) gmb_surface_rounded_rect(buf, c->x, c->y + i, c->width, 1, 2, tb, tb, 1);
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(tf).r << 16) | ((unsigned)(tf).g << 8) | (tf).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = p->first + i;
        if (idx >= p->nitems) break;
        char line[256];
        gmb_property_grid_render_line(p, idx, line, (int)sizeof(line));
        if (p->editing && idx == (int)p->ival) gmb_screen_setstring(buf, c->x, c->y + i, line, ef, eb);
        else if (idx == (int)p->ival) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool propertygrid_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* p = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    if (p->editing)
    {
        if (key == GMB_K_ENTER)
        {
            if ((int)p->ival >= 0 && (int)p->ival < p->nitems)
                snprintf(p->pvalues[(int)p->ival], sizeof(p->pvalues[0]), "%s", p->edited_value);
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
            if ((int)p->ival > 0) p->ival = (int)p->ival - 1;
            if ((int)p->ival < p->first) p->first--;
            return true;
        case GMB_K_DOWN:
            if ((int)p->ival < p->nitems - 1) p->ival = (int)p->ival + 1;
            if ((int)p->ival >= p->first + c->height) p->first++;
            return true;
        case GMB_K_ENTER:
            p->editing = true;
            snprintf(p->edited_value, sizeof(p->edited_value), "%s", p->pvalues[(int)p->ival]);
            return true;
    }
    return false;
}
static bool propertygrid_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* p = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    /* In GRAFICA il rect e' in PIXEL (px_*) e le righe hanno il passo del tema;
     * in TEXT si ragiona a celle. La formula a celle in pixel selezionava righe
     * inesistenti (prima riga sempre, o niente). */
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
        int rowh = (st.row_h > 0 ? st.row_h : 24);
        int top = ry + (soft ? 4 : 0);
        idx = (y - top) / (rowh > 0 ? rowh : 1);
    }
    else idx = (y - c->y);
    idx += p->first;
    if (idx >= 0 && idx < p->nitems)
    {
        p->ival = idx;
        p->editing = true;
        snprintf(p->edited_value, sizeof(p->edited_value), "%s", p->pvalues[idx]);
    }
    return true;
}
static void propertygrid_draw_px(void* u, gmb_control_t* c)
{
    /* PropertyGrid pixel: card del tema, righe nome/valore, selezione accento. */
    gmb_widget_t* p = (gmb_widget_t*)u;
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
    int start = p->first; if (start < 0) start = 0;
    int half = (w - (soft ? 24 : 8)) / 2;
    for (int r = 0; r < vis && start + r < p->nitems; r++) {
        int i = start + r, ry = y + pad + r * rh;
        if (i == (int)p->ival) gmb_gfx_fill_rounded_rect(x + 4, ry + 1, w - 8, rh - 2, soft ? (rh - 2) / 2 : 0, (acc & 0xFFFFFFu) | (0x22u << 24));
        int ty = ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2;
        {   /* riga "k=v": nome a sinistra, valore a destra */
            char nm[64]; const char* eq = strchr(p->items[i], '=');
            const char* val = p->pvalues[i];
            if (eq && !val[0]) { size_t L = (size_t)(eq - p->items[i]); if (L > 63) L = 63; memcpy(nm, p->items[i], L); nm[L] = 0; val = eq + 1; }
            else snprintf(nm, sizeof(nm), "%s", p->items[i]);
            gmb_gfx_draw_text(nm, x + (soft ? 12 : 6), ty, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
            gmb_gfx_draw_text(val, x + (soft ? 12 : 6) + half, ty, 0, (int)(0x40000000u | (st.text_disabled & 0xFFFFFFu)));
        }
    }
}

void gmb_property_grid_init(gmb_property_grid_t* pg, int x, int y, int w, int h)
{
    gmb_widget_t* p = (gmb_widget_t*)pg;
    memset(p, 0, sizeof(*p));
    gmb_control_init(&p->base, "", x, y, w, h, propertygrid_draw, propertygrid_key);
    p->base.ctrl_kind = GMB_CK_PROPERTYGRID;
    p->base.draw_px_fn = propertygrid_draw_px;
    p->base.handle_click_fn = propertygrid_click;
    p->base.user = p;
    p->kind = W_GRID;
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

void gmb_property_grid_add(gmb_property_grid_t* pg, const char* name, const char* value)
{
    gmb_widget_t* p = (gmb_widget_t*)pg;
    if (p->nitems >= 32) return;
    snprintf(p->items[p->nitems], sizeof(p->items[0]), "%s", name ? name : "");
    snprintf(p->pvalues[p->nitems], sizeof(p->pvalues[0]), "%s", value ? value : "");
    p->nitems++;
}
int gmb_property_grid_render_line(gmb_property_grid_t* pg, int idx, char* out, int cap)
{
    gmb_widget_t* p = (gmb_widget_t*)pg;
    if (idx < 0 || idx >= p->nitems)
    {
        out[0] = '\0';
        return 0;
    }
    const char* marker = idx == (int)p->ival ? ">" : " ";
    const char* display = (p->editing && idx == (int)p->ival) ? p->edited_value : p->pvalues[idx];
    int o = snprintf(out, cap, "%s %-16s %s", marker, p->items[idx], display);
    if (o > p->base.width) o = p->base.width;
    else for (int i = o; i < p->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
