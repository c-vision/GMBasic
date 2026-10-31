/* ============================================================================
 * gmb_tui_splitter.c — Splitter (due pannelli + barra divisoria trascinabile).
 * Controllo built-in (namespace CONTROLS). Mirror del modello Control (Clutter/VB6).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Posizione (colonna/riga) della barra, clampata ai minimi. */
static void sp_clamp(gmb_splitter_t* s)
{
    if (s->pos < s->min_a) s->pos = s->min_a;
    int maxp = s->ctrl.width - s->thick - s->min_b;
    if (s->orient) maxp = s->ctrl.height - s->thick - s->min_b;
    if (maxp < s->min_a) maxp = s->min_a;
    if (s->pos > maxp) s->pos = maxp;
}

/* Asse lungo (verticale=colonna, orizzontale=riga). */
static int sp_axis(const gmb_splitter_t* s)   { return s->orient ? s->ctrl.height : s->ctrl.width; }
static int sp_cross(const gmb_splitter_t* s)  { return s->orient ? s->ctrl.width  : s->ctrl.height; }

static void splitter_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_splitter_t* s = (gmb_splitter_t*)u;
    int cross = sp_cross(s);
    gmb_cell_color_t bgA = GMB_CELLCOLOR_BLACK, bgB = GMB_CELLCOLOR_BLACK, fg = GMB_CELLCOLOR_WHITE;
    /* Grafica (way 3): pannelli come rettangoli + barra divisoria. Stesso controllo. */
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_cell_color_t dbfg = (s->dragging || c->focused) ? GMB_CELLCOLOR_YELLOW : GMB_CELLCOLOR_WHITE;
        if (s->orient == 0) {
            gmb_surface_rounded_rect(buf, c->x, c->y, s->pos, cross, 2, fg, bgA, 1);
            gmb_surface_rounded_rect(buf, c->x + s->pos + s->thick, c->y, c->width - s->pos - s->thick, cross, 2, fg, bgB, 1);
            gmb_surface_rounded_rect(buf, c->x + s->pos, c->y, s->thick, cross, 1, dbfg, GMB_CELLCOLOR_BLUE, 1);
        } else {
            gmb_surface_rounded_rect(buf, c->x, c->y, cross, s->pos, 2, fg, bgA, 1);
            gmb_surface_rounded_rect(buf, c->x, c->y + s->pos + s->thick, cross, c->height - s->pos - s->thick, 2, fg, bgB, 1);
            gmb_surface_rounded_rect(buf, c->x, c->y + s->pos, cross, s->thick, 1, dbfg, GMB_CELLCOLOR_BLUE, 1);
        }
        return;
    }
    if (s->orient == 0) {
        gmb_screen_fillrect(buf, c->y, c->x, c->y + cross - 1, c->x + s->pos - 1, ' ', fg, bgA);
        gmb_screen_fillrect(buf, c->y, c->x + s->pos + s->thick, c->y + cross - 1, c->x + c->width - 1, ' ', fg, bgB);
    } else {
        gmb_screen_fillrect(buf, c->y, c->x, c->y + s->pos - 1, c->x + cross - 1, ' ', fg, bgA);
        gmb_screen_fillrect(buf, c->y + s->pos + s->thick, c->x, c->y + c->height - 1, c->x + cross - 1, ' ', fg, bgB);
    }
    gmb_cell_color_t dbfg = (s->dragging || c->focused) ? GMB_CELLCOLOR_YELLOW : GMB_CELLCOLOR_WHITE;
    if (s->orient == 0) gmb_screen_fillrect(buf, c->y, c->x + s->pos, c->y + cross - 1, c->x + s->pos + s->thick - 1, ' ', dbfg, GMB_CELLCOLOR_BLUE);
    else                gmb_screen_fillrect(buf, c->y + s->pos, c->x, c->y + s->pos + s->thick - 1, c->x + cross - 1, ' ', dbfg, GMB_CELLCOLOR_BLUE);
}

static bool splitter_key(void* u, gmb_control_t* c, int key)
{
    gmb_splitter_t* s = (gmb_splitter_t*)u;
    if (!c->enabled) return false;
    if (key == GMB_K_LEFT  && s->orient == 0) { s->pos--; sp_clamp(s); return true; }
    if (key == GMB_K_RIGHT && s->orient == 0) { s->pos++; sp_clamp(s); return true; }
    if (key == GMB_K_UP    && s->orient == 1) { s->pos--; sp_clamp(s); return true; }
    if (key == GMB_K_DOWN  && s->orient == 1) { s->pos++; sp_clamp(s); return true; }
    return false;
}

static bool splitter_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_splitter_t* s = (gmb_splitter_t*)u;
    if (!c->enabled) return false;
    if (!rect_contains(gmb_control_rect(c), x, y)) return false;
    c->focused = true;
    int a = s->orient == 0 ? (x - c->x) : (y - c->y);
    if (a >= s->pos && a < s->pos + s->thick) { s->dragging = 1; return true; }
    /* click su un pannello: sposta la barra lì (comodo) */
    s->pos = a;
    sp_clamp(s);
    return true;
}

static bool splitter_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_splitter_t* s = (gmb_splitter_t*)u;
    if (!a) return false;
    if (a->kind == GMB_MA_RELEASE) { s->dragging = 0; return true; }
    if ((a->kind == GMB_MA_DRAG || a->kind == GMB_MA_MOVE) && s->dragging) {
        int p = (s->orient == 0 ? (a->x - c->x) : (a->y - c->y)) - s->thick / 2;
        s->pos = p; sp_clamp(s); return true;
    }
    return false;
}

static void splitter_draw_px(void* u, gmb_control_t* c)
{
    gmb_splitter_t* s = (gmb_splitter_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    int on = s->dragging || c->focused;
    if (s->orient == 0) {   /* verticale: barra verticale a pos */
        int px = x + s->pos; if (px < x) px = x; if (px > x + w - 6) px = x + w - 6;
        gmb_gfx_fill_rounded_rect(px, y, 6, h, 3, on ? acc : st.border);
    } else {
        int py = y + s->pos; if (py < y) py = y; if (py > y + h - 6) py = y + h - 6;
        gmb_gfx_fill_rounded_rect(x, py, w, 6, 3, on ? acc : st.border);
    }
}
static bool splitter_drag_px(gmb_splitter_t* s, gmb_control_t* c, int x, int y, int kind)
{
    if (!s || !c) return false;
    int rel = (s->orient == 0) ? (x - c->x) : (y - c->y);
    if (kind == 0) { s->dragging = 1; }           /* click: prende la maniglia */
    if (kind == 2) { if (!s->dragging) return false; }
    if (kind == 1) { s->dragging = 0; return true; }  /* release */
    if (rel < 0) rel = 0;
    if (s->orient == 0) { if (rel > c->width - 6) rel = c->width - 6; } else { if (rel > c->height - 6) rel = c->height - 6; }
    s->pos = rel;
    return true;
}
static bool splitter_click_px(void* u, gmb_control_t* c, int x, int y) { return splitter_drag_px((gmb_splitter_t*)u, c, x, y, 0); }
static bool splitter_mouse_px(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    if (!a) return false;
    if (a->kind == GMB_MA_DRAG || a->kind == GMB_MA_MOVE) return splitter_drag_px((gmb_splitter_t*)u, c, a->x, a->y, 2);
    if (a->kind == GMB_MA_RELEASE) return splitter_drag_px((gmb_splitter_t*)u, c, a->x, a->y, 1);
    return false;
}

void gmb_splitter_init(gmb_splitter_t* s, int x, int y, int w, int h, int orient)
{
    (void)w; (void)h;
    gmb_control_init(&s->ctrl, "", x, y, w, h, splitter_draw, splitter_key);
    s->ctrl.user = s;
    s->ctrl.handle_click_fn = splitter_click;
    s->ctrl.handle_mouse_fn = splitter_mouse;
    s->ctrl.ctrl_kind = GMB_CK_SPLITTER;
    s->ctrl.draw_px_fn = splitter_draw_px;
    s->ctrl.handle_click_fn = splitter_click_px;
    s->ctrl.handle_mouse_fn = splitter_mouse_px;
    s->orient = orient ? 1 : 0;
    s->thick = 1;
    s->min_a = 3; s->min_b = 3;
    s->dragging = 0;
    s->pos = (sp_axis(s) - s->thick) / 2;
    sp_clamp(s);
}
