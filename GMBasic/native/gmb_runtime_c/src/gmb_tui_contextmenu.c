/* ============================================================================
 * gmb_tui_contextmenu.c — Menù contestuale (popup). Controllo built-in.
 * Voci/separatori/voci disabilitate, navigazione, voce scelta. CHIUSO di default.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cm_is_sep(const char* s)
{
    if (!s) return 0;
    return (strcmp(s, "-") == 0 || strcmp(s, "--") == 0 || strcmp(s, "---") == 0) ? 1 : 0;
}

static void cm_first_enabled(gmb_context_menu_t* m)
{
    m->sel = -1;
    for (int i = 0; i < m->n; i++) if (!m->sep[i] && m->en[i]) { m->sel = i; break; }
}

static void contextmenu_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_context_menu_t* m = (gmb_context_menu_t*)u;
    if (!m->open) return;
    int w = m->pw > 0 ? m->pw : (int)c->width;
    gmb_cell_color_t bfg = GMB_CELLCOLOR_BLACK, bbg = GMB_CELLCOLOR_WHITE;
    /* Grafica (way 3): sfondo come rettangolo + voci/testo. Stesso controllo. */
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        if (m->n > 0) gmb_surface_rounded_rect(buf, c->x + m->px, c->y + m->py, w, m->n, 2, bfg, bbg, 1);
        for (int i = 0; i < m->n; i++) {
            int ly = c->y + m->py + i;
            if (m->sep[i] || m->items[i][0] == 0) continue;
            gmb_cell_color_t tf = m->en[i] ? bfg : GMB_CELLCOLOR_WHITE;
            if (i == m->sel) gmb_surface_rounded_rect(buf, c->x + m->px + 1, ly, w - 2, 1, 1, bfg, GMB_CELLCOLOR_CYAN, 1);
            gmb_gfx_draw_text(m->items[i], sk->origin_x + (c->x + m->px + 1) * sk->cell_w,
                              sk->origin_y + ly * sk->cell_h + sk->cell_h / 4, 0,
                              (int)(0x40000000u | ((unsigned)tf.r << 16) | ((unsigned)tf.g << 8) | tf.b));
        }
        return;
    }
    gmb_screen_fillrect(buf, c->y + m->py, c->x + m->px, c->y + m->py + m->n - 1, c->x + m->px + w - 1, ' ', bfg, bbg);
    for (int i = 0; i < m->n; i++) {
        int ly = c->y + m->py + i;
        if (m->sep[i]) {
            for (int x = 0; x < w; x++) gmb_screen_setstring(buf, c->x + m->px + x, ly, "\xE2\x94\x80" /* ─ */, GMB_CELLCOLOR_WHITE, bbg);
        } else if (!m->en[i]) {
            gmb_screen_setstring(buf, c->x + m->px + 1, ly, m->items[i], GMB_CELLCOLOR_WHITE, bbg);
        } else if (i == m->sel) {
            gmb_screen_fillrect(buf, ly, c->x + m->px + 1, ly, c->x + m->px + w - 1, ' ', bfg, GMB_CELLCOLOR_CYAN);
            gmb_screen_setstring(buf, c->x + m->px + 1, ly, m->items[i], bfg, GMB_CELLCOLOR_CYAN);
        } else {
            gmb_screen_setstring(buf, c->x + m->px + 1, ly, m->items[i], bfg, bbg);
        }
    }
}

static void cm_nav(gmb_context_menu_t* m, int d)
{
    if (m->n == 0) return;
    int n = m->sel;
    for (int i = 0; i < m->n; i++) {
        n += d;
        if (n < 0) n = m->n - 1;
        if (n > m->n - 1) n = 0;
        if (!m->sep[n] && m->en[n]) { m->sel = n; return; }
    }
}

static bool contextmenu_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_context_menu_t* m = (gmb_context_menu_t*)u;
    if (!m->open) return false;
    if (key == GMB_K_UP)   { cm_nav(m, -1); return true; }
    if (key == GMB_K_DOWN) { cm_nav(m,  1); return true; }
    if (key == GMB_K_ENTER) {
        if (m->sel >= 0 && m->sel < m->n && !m->sep[m->sel] && m->en[m->sel]) m->chosen = m->sel;
        m->open = 0; return true;
    }
    if (key == 27 /* ESC */) { m->open = 0; return true; }
    return false;
}

static bool contextmenu_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_context_menu_t* m = (gmb_context_menu_t*)u;
    if (!c->enabled) return false;
    if (!m->open) {
        /* apertura: click nel controllo */
        if (!rect_contains(gmb_control_rect(c), x, y)) return false;
        m->px = 0; m->py = 0; m->open = 1; m->chosen = -1; cm_first_enabled(m);
        return true;
    }
    int r = (y - c->y) - m->py;
    if (r < 0 || r >= m->n) { m->open = 0; return true; }
    if (m->sep[r] || !m->en[r]) return true;
    m->sel = r; m->chosen = r; m->open = 0;
    return true;
}

static bool contextmenu_hit_extra(void* u, gmb_control_t* c, int x, int y)
{
    gmb_context_menu_t* m = (gmb_context_menu_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int rh = st.row_h > 0 ? st.row_h : 22;
    int rx = c->px_w > 0 ? c->px_x : c->x, ry = c->px_h > 0 ? c->px_y : c->y;
    int rw = c->px_w > 0 ? c->px_w : c->width;
    if (!m->open) { int bh = rh + (st.shape == 1 ? 10 : 6); return x >= rx && x < rx + rw && y >= ry && y < ry + bh; }
    int ph = m->n * rh + 12;
    return (x >= rx && x < rx + rw && y >= ry && y < ry + ph);
}

static void contextmenu_draw_px(void* u, gmb_control_t* c)
{
    gmb_context_menu_t* m = (gmb_context_menu_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    int rh = (st.row_h > 0 ? st.row_h : 22);
    int bw = w > 0 ? w : 200;
    if (!m->open)
    {
        int bh = rh + (soft ? 10 : 6);
        gmb_gfx_fill_rounded_rect(x, y, bw, bh, rr, fill);
        gmb_gfx_stroke_rounded_rect(x, y, bw, bh, rr, st.border);
        gmb_gfx_draw_text("Azioni  v", x + (soft ? 14 : 6), y + (bh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2,
                          0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
        return;
    }
    int ph = m->n * rh + 12;
    gmb_gfx_fill_rounded_rect(x, y, bw, ph, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, bw, ph, rr, st.border);
    for (int i = 0; i < m->n; i++)
    {
        int ry = y + 6 + i * rh;
        if (i == m->sel) gmb_gfx_fill_rounded_rect(x + 4, ry, bw - 8, rh - 1, soft ? rh / 2 : 0, (acc & 0xFFFFFFu) | (0x22u << 24));
        gmb_gfx_draw_text(m->items[i], x + (soft ? 14 : 8), ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2,
                          0, (int)(0x40000000u | ((m->en[i] ? st.text : st.text_disabled) & 0xFFFFFFu)));
    }
}
static bool contextmenu_click_px(void* u, gmb_control_t* c, int x, int y)
{
    gmb_context_menu_t* m = (gmb_context_menu_t*)u;
    if (!c->enabled) return false;
    int rx = c->px_w > 0 ? c->px_x : c->x;
    int ry0 = c->px_h > 0 ? c->px_y : c->y;
    int rw = c->px_w > 0 ? c->px_w : c->width;
    int rh = (gmb_widget_style_current().row_h); if (rh < 14) rh = 22;
    int soft = (gmb_widget_style_current().shape == 1);
    if (!m->open)
    {
        int bh = rh + (soft ? 10 : 6);
        if (x < rx || x >= rx + rw || y < ry0 || y >= ry0 + bh) return false;
        m->open = 1; m->chosen = -1; cm_first_enabled(m);
        return true;
    }
    int ph = m->n * rh + 12;
    if (y < ry0 || y >= ry0 + ph) { m->open = 0; return true; }
    int r = (y - ry0 - 6) / rh;
    if (r < 0 || r >= m->n) { m->open = 0; return true; }
    if (m->sep[r] || !m->en[r]) return true;
    m->sel = r; m->chosen = r; m->open = 0;
    return true;
}

void gmb_context_menu_init(gmb_context_menu_t* m, int x, int y, int w, int h)
{
    memset(m, 0, sizeof(*m));
    (void)h;
    gmb_control_init(&m->ctrl, "", x, y, w, h, contextmenu_draw, contextmenu_key);
    m->ctrl.user = m;
    m->ctrl.handle_click_fn = contextmenu_click_px;
    m->ctrl.ctrl_kind = GMB_CK_CONTEXTMENU;
    m->ctrl.draw_px_fn = contextmenu_draw_px;
    m->ctrl.hit_extra_fn = contextmenu_hit_extra;
    m->n = 0; m->sel = -1; m->open = 0; m->chosen = -1;
    m->px = 0; m->py = 0; m->pw = w;
    for (int i = 0; i < 16; i++) { m->items[i][0] = 0; m->sep[i] = 0; m->en[i] = 1; }
}

void gmb_context_menu_set_items(gmb_context_menu_t* m, const char** items, int n)
{
    if (!m) return;
    m->n = 0;
    for (int i = 0; i < n && i < 16; i++) {
        snprintf(m->items[m->n], sizeof(m->items[m->n]), "%s", items[i] ? items[i] : "");
        m->sep[m->n] = cm_is_sep(items[i]);
        m->en[m->n] = m->sep[m->n] ? 0 : 1;
        m->n++;
    }
}
