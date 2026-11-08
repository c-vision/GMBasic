/* ============================================================================
 * gmb_tui_menu.c — gmb_tui_menu. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"
#include "gmb_tui_app.h"

/* ===================== Menu ===================== */

static long long g_menu_activated = -1;   /* ultima voce attivata: top*100+sub, -1 se nessuna */

void gmb_tui_menu_set(int idx, const char* spec)
{
    gmb_control_t* c = gmb_tui_control_at(idx);
    if (!c || c->ctrl_kind != GMB_CK_MENU || !spec) return;
    gmb_menu_t* m = (gmb_menu_t*)c;
    m->items_count = 0; m->selected_index = 0; m->sub_selected_index = 0; m->is_open = false;
    /* spec = "Top:sub|sub;Top2:sub|sub;..." */
    char buf[1024]; snprintf(buf, sizeof(buf), "%s", spec);
    char* save1 = NULL;
    for (char* top = strtok_r(buf, ";", &save1); top && m->items_count < 16; top = strtok_r(NULL, ";", &save1))
    {
        char* colon = strchr(top, ':');
        char* subs = NULL;
        if (colon) { *colon = 0; subs = colon + 1; }
        gmb_menu_item_t* it = gmb_menu_add_item(m, top);
        if (!it || !subs) continue;
        char* save2 = NULL;
        for (char* sub = strtok_r(subs, "|", &save2); sub && it->sub_count < 16; sub = strtok_r(NULL, "|", &save2))
            gmb_menu_item_add_sub(it, sub);
    }
}

long long gmb_tui_menu_poll(void) { long long r = g_menu_activated; g_menu_activated = -1; return r; }

/* 1 se un menu registrato e' APERTO (dropdown visibile): il cursore va nascosto finche' dura. */
int gmb_tui_menu_any_open(void)
{
    for (int i = 0; i < gmb_tui_control_count(); i++)
    {
        gmb_control_t* c = gmb_tui_control_at(i);
        if (c && c->ctrl_kind == GMB_CK_MENU && ((gmb_menu_t*)c)->is_open) return 1;
    }
    return 0;
}

/* Chiude tutti i menu aperti (es. click fuori dal menu: comportamento TV). */
void gmb_tui_menu_close_all(void)
{
    for (int i = 0; i < gmb_tui_control_count(); i++)
    {
        gmb_control_t* c = gmb_tui_control_at(i);
        if (c && c->ctrl_kind == GMB_CK_MENU) ((gmb_menu_t*)c)->is_open = false;
    }
}

/* 1 se un menu APERTO (la sua dropdown) copre la cella (x,y): il cursore va spento. */
int gmb_tui_menu_covers(int x, int y)
{
    for (int i = 0; i < gmb_tui_control_count(); i++)
    {
        gmb_control_t* c = gmb_tui_control_at(i);
        if (!c || c->ctrl_kind != GMB_CK_MENU) continue;
        gmb_menu_t* m = (gmb_menu_t*)c;
        if (!m->is_open || m->selected_index < 0 || m->selected_index >= m->items_count) continue;
        gmb_menu_item_t* it = &m->items[m->selected_index];
        if (it->sub_count == 0) continue;
        int px = c->x;
        for (int k = 0; k < m->selected_index; k++) px += (int)strlen(m->items[k].label) + 2;
        int wmax = 0;
        for (int k = 0; k < it->sub_count; k++) { int l = (int)strlen(it->sub[k].label); if (l > wmax) wmax = l; }
        int innerW = wmax + 3;
        int sy = c->y + 1;                 /* primo bordo */
        int by = sy + 1 + it->sub_count;   /* ultimo bordo */
        if (y >= sy && y <= by && x >= px && x <= px + innerW + 1) return 1;
    }
    return 0;
}

/* Mouse: click sulla barra apre la voce sotto il cursore; click su una sottovoce la attiva. */
static bool menu_click(void* u, gmb_control_t* c, int x, int y);
static bool menu_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_menu_t* m = (gmb_menu_t*)u;
    if (!c || !c->enabled || !a || m->items_count == 0) return false;
    /* GRAFICA: hit-test pixel (chip e dropdown), stessa geometria del renderer. */
    if (GMB_GFX_SCREEN_ON() && a->kind == GMB_MA_CLICK) return menu_click(u, c, a->x, a->y);
    /* Hover sulla barra (menu aperto): cambia il menu aperto SENZA clic (TV). */
    if (a->kind == GMB_MA_MOVE)
    {
        if (!m->is_open || a->y != c->y) return false;
        int x = c->x;
        for (int i = 0; i < m->items_count; i++)
        {
            int w = (int)strlen(m->items[i].label) + 2;
            if (a->x >= x && a->x < x + w) { if (m->selected_index != i) { m->selected_index = i; m->sub_selected_index = 0; } return true; }
            x += w;
        }
        return false;
    }
    if (a->kind != GMB_MA_CLICK) return false;
    if (a->y == c->y)
    {
        int x = c->x;
        for (int i = 0; i < m->items_count; i++)
        {
            int w = (int)strlen(m->items[i].label) + 2;
            if (a->x >= x && a->x < x + w) { m->selected_index = i; m->sub_selected_index = 0; m->is_open = true; return true; }
            x += w;
        }
        return false;
    }
    if (m->is_open && a->y > c->y)
    {
        /* Le voci della dropdown stanno a y = c->y + 2 + i (bordo a c->y+1). */
        int si = a->y - c->y - 2;
        gmb_menu_item_t* it = &m->items[m->selected_index];
        if (si >= 0 && si < it->sub_count) { m->sub_selected_index = si; g_menu_activated = m->selected_index * 100 + si; m->is_open = false; return true; }
    }
    return false;
}

static void menu_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_menu_t* m = (gmb_menu_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    { gmb_theme_t th_ = gmb_ui_theme(); gmb_cell_color_t tf, tb; gmb_theme_kind(&th_, 1, &tf, &tb); if (!c->style.has_fg || gmb_cellcolor_eq(c->style.fg, th_.text)) fg = tf; if (!c->style.has_bg || gmb_cellcolor_eq(c->style.bg, th_.window_bg)) bg = tb; }
    /* Barra: riempi TUTTA la riga col bg (Menu.cs: SetString(0, Y, ' ' * Width)). */
    { int bw = gmb_screen_width(buf);
      for (int xx = 0; xx < bw; xx++) gmb_screen_setchar(buf, xx, c->y, ' ', fg, bg); }
    gmb_cell_color_t red = { 170, 0, 0, 0 };   /* HotkeyRed (Menu.cs) */
    int x = c->x;
    for (int i = 0; i < m->items_count; i++)
    {
        const char* it = m->items[i].label;
        int sel = (m->is_open && i == m->selected_index);
        gmb_cell_color_t ef = sel ? bg : fg, eb = sel ? fg : bg;
        char lbl[160];
        if (it[0]) snprintf(lbl, sizeof(lbl), " ~%c~%s ", it[0], it + 1);
        else snprintf(lbl, sizeof(lbl), " ");
        char clbl[160];
        gmb_control_apply_case(c, lbl, clbl, (int)sizeof(clbl));
        gmb_draw_string_keys(buf, x, c->y, clbl, (int)strlen(clbl), ef, eb, sel ? ef : red);
        x += (int)strlen(it) + 2;
    }
    if (!m->is_open || m->selected_index < 0 || m->selected_index >= m->items_count) return;
    gmb_menu_item_t* item = &m->items[m->selected_index];
    if (item->sub_count == 0) return;

    /* px = X + somma(label+2) delle voci precedenti (specchio Menu.cs). */
    int px = c->x;
    for (int i = 0; i < m->selected_index; i++) px += (int)strlen(m->items[i].label) + 2;
    int wmax = 0;
    for (int i = 0; i < item->sub_count; i++) { int l = (int)strlen(item->sub[i].label); if (l > wmax) wmax = l; }
    int innerW = wmax + 3;
    int sy = c->y + 1;
    /* Costanti Menu.cs: BorderFg = nero, BorderBg = grigio 192. */
    gmb_cell_color_t BF = { 0, 0, 0, 0 };
    gmb_cell_color_t BB = { 192, 192, 192, 0 };

    /* bordo superiore: ┌ ─… ┐ */
    gmb_screen_setchar(buf, px, sy, 0x250Cu, BF, BB);
    for (int i = 1; i <= innerW; i++) gmb_screen_setchar(buf, px + i, sy, 0x2500u, BF, BB);
    gmb_screen_setchar(buf, px + innerW + 1, sy, 0x2510u, BF, BB);

    for (int i = 0; i < item->sub_count; i++)
    {
        int iy = sy + 1 + i;
        int isSel = (i == m->sub_selected_index);
        gmb_cell_color_t sf = isSel ? BB : BF;
        gmb_cell_color_t sb = isSel ? BF : BB;
        gmb_screen_setchar(buf, px, iy, 0x2502u, BF, BB);
        /* Riempi TUTTA la riga interna col fondo (altrimenti si vedono i caratteri sotto). */
        for (int k = 1; k <= innerW; k++) gmb_screen_setchar(buf, px + k, iy, ' ', sb, sb);
        const char* sl = item->sub[i].label;
        char lbl[160];
        if (sl[0]) snprintf(lbl, sizeof(lbl), " ~%c~%s", sl[0], sl + 1);
        else snprintf(lbl, sizeof(lbl), " ");
        while ((int)strlen(lbl) < innerW) strncat(lbl, " ", sizeof(lbl) - strlen(lbl) - 1);
        char clbl[160];
        gmb_control_apply_case(c, lbl, clbl, (int)sizeof(clbl));
        gmb_draw_string_keys(buf, px + 1, iy, clbl, innerW, sf, sb, isSel ? sf : red);
        gmb_screen_setchar(buf, px + innerW + 1, iy, 0x2502u, BF, BB);
    }

    /* bordo inferiore: └ ─… ┘ */
    int by = sy + 1 + item->sub_count;
    gmb_screen_setchar(buf, px, by, 0x2514u, BF, BB);
    for (int i = 1; i <= innerW; i++) gmb_screen_setchar(buf, px + i, by, 0x2500u, BF, BB);
    gmb_screen_setchar(buf, px + innerW + 1, by, 0x2518u, BF, BB);
}

static bool menu_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_menu_t* m = (gmb_menu_t*)u;
    if (!m->items_count) return false;
    switch (key)
    {
        case GMB_K_ENTER:
            if (m->is_open && m->items_count > 0)
            {
                /* Attiva la sottovoce corrente: salva (top, sub) per l'host e chiudi. */
                int ti = m->selected_index;
                int si = m->sub_selected_index;
                if (ti < 0) ti = 0; if (ti >= m->items_count) ti = m->items_count - 1;
                g_menu_activated = ti * 100 + si;
                m->is_open = false;
                return true;
            }
            m->is_open = !m->is_open;
            m->sub_selected_index = 0;
            return true;
        case GMB_K_DOWN:
            /* Giù: apre il menu chiuso (TV); se aperto scorre le voci. */
            if (!m->is_open) { m->is_open = true; m->sub_selected_index = 0; return true; }
            {
                int n = m->items[m->selected_index].sub_count;
                if (n > 0)
                {
                    m->sub_selected_index++;
                    if (m->sub_selected_index >= n) m->sub_selected_index = 0;
                }
                return true;
            }
        case GMB_K_UP:
            /* Su: chiude il menu aperto (TV). */
            if (m->is_open) { m->is_open = false; return true; }
            return false;
        case GMB_K_RIGHT:
            if (m->is_open)
            {
                m->selected_index++;
                if (m->selected_index >= m->items_count) m->selected_index = 0;
                m->sub_selected_index = 0;
                return true;
            }
            m->selected_index++;
            if (m->selected_index >= m->items_count) m->selected_index = 0;
            return true;
        case GMB_K_LEFT:
            m->selected_index--;
            if (m->selected_index < 0) m->selected_index = m->items_count - 1;
            m->sub_selected_index = 0;
            return true;
        case GMB_K_ESC:
            m->is_open = false;
            return true;
    }
    return false;
}

/* ── Geometria PIXEL del menubar (UNICA fonte per render e hit-test) ────── */
typedef struct {
    int x, y, w, h;                 /* barra */
    int th, pad, gap;
    int n;
    int cx[16], cw[16];             /* chip */
    int dd_x, dd_y, dd_w, dd_h;     /* dropdown (se aperto e con sotto-voci) */
} menu_px_t;

static void menu_px_geom(gmb_menu_t* m, gmb_control_t* c, menu_px_t* g)
{
    gmb_widget_style_t st = gmb_widget_style_current();
    memset(g, 0, sizeof(*g));
    g->x = (c->px_w > 0) ? c->px_x : c->x;
    g->y = (c->px_h > 0) ? c->px_y : c->y;
    g->w = (c->px_w > 0) ? c->px_w : c->width;
    g->th = gmb_gfx_text_height_logical(); if (g->th <= 0) g->th = 16;
    g->h = st.row_h > 0 ? st.row_h : g->th + 10;
    g->pad = 10; g->gap = 2;
    g->n = m->items_count > 16 ? 16 : m->items_count;
    int x = g->x;
    for (int i = 0; i < g->n; i++)
    {
        int tw = gmb_gfx_text_width_logical(m->items[i].label) + 2 * g->pad;
        g->cx[i] = x; g->cw[i] = tw;
        x += tw + g->gap;
    }
    /* dropdown del menù selezionato, se ha sotto-voci */
    g->dd_x = g->dd_y = g->dd_w = g->dd_h = 0;
    if (m->is_open && m->selected_index >= 0 && m->selected_index < g->n)
    {
        gmb_menu_item_t* it = &m->items[m->selected_index];
        if (it->sub_count > 0)
        {
            int rh = st.row_h > 0 ? st.row_h : g->th + 10;
            g->dd_x = g->cx[m->selected_index];
            g->dd_y = g->y + g->h + 2;
            g->dd_w = 180;
            g->dd_h = it->sub_count * rh + 8;
        }
    }
}

static void menu_draw_px(void* u, gmb_control_t* c)
{
    gmb_menu_t* m = (gmb_menu_t*)u;
    menu_px_t g; menu_px_geom(m, c, &g);
    gmb_widget_style_t st = gmb_widget_style_current();
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;

    for (int i = 0; i < g.n; i++)
    {
        int sel = (i == m->selected_index);
        if (sel)
            gmb_gfx_fill_rounded_rect(g.cx[i], g.y, g.cw[i], g.h, (st.shape == 1) ? g.h / 2 : 4,
                                      (acc & 0xFFFFFFu) | 0x55000000u);
        gmb_gfx_draw_text(m->items[i].label, g.cx[i] + g.pad, g.y + (g.h - g.th) / 2, 0,
                          (int)(0x40000000u | ((sel ? 0xFFFFFFu : (st.text & 0xFFFFFFu)))));
    }
    if (g.dd_h <= 0) return;

    /* dropdown OPACO sopra la pagina (il popup e' disegnato per ultimo dal livello TUI) */
    unsigned base = 0xFF000000u | (st.fill & 0xFFFFFFu);
    gmb_gfx_fill_rounded_rect(g.dd_x, g.dd_y, g.dd_w, g.dd_h, 8, base);
    gmb_gfx_stroke_rounded_rect(g.dd_x, g.dd_y, g.dd_w, g.dd_h, 8, st.border);
    gmb_menu_item_t* it = &m->items[m->selected_index];
    int rh = (g.dd_h - 8) / (it->sub_count > 0 ? it->sub_count : 1);
    for (int i = 0; i < it->sub_count; i++)
    {
        int iy = g.dd_y + 4 + i * rh;
        if (i == m->sub_selected_index)
            gmb_gfx_fill_rounded_rect(g.dd_x + 2, iy, g.dd_w - 4, rh - 1, 5,
                                      (acc & 0xFFFFFFu) | 0x44000000u);
        gmb_gfx_draw_text(it->sub[i].label, g.dd_x + 12, iy + (rh - g.th) / 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    }
}

static bool menu_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_menu_t* m = (gmb_menu_t*)u;
    if (!c->enabled) return false;
    menu_px_t g; menu_px_geom(m, c, &g);

    /* dropdown aperto: scelta voce */
    if (g.dd_h > 0 && x >= g.dd_x && x < g.dd_x + g.dd_w && y >= g.dd_y && y < g.dd_y + g.dd_h)
    {
        gmb_menu_item_t* it = &m->items[m->selected_index];
        int rh = (g.dd_h - 8) / (it->sub_count > 0 ? it->sub_count : 1);
        int r = (y - g.dd_y - 4) / (rh > 0 ? rh : 1);
        if (r >= 0 && r < it->sub_count)
        {
            m->sub_selected_index = r;
            m->is_open = false;
            if (it->sub[r].action) it->sub[r].action(it->sub[r].action_ctx);
            { extern void gmb_tui_fire(int, int); gmb_tui_fire(c->handle, GMB_EV_CLICK); }
        }
        else m->is_open = false;
        return true;
    }
    /* barra: seleziona la voce (e apre il dropdown se ha sotto-voci) */
    if (y >= g.y && y < g.y + g.h)
    {
        for (int i = 0; i < g.n; i++)
        {
            if (x >= g.cx[i] && x < g.cx[i] + g.cw[i])
            {
                m->selected_index = i;
                m->sub_selected_index = 0;
                m->is_open = (m->items[i].sub_count > 0);
                { extern void gmb_tui_fire(int, int); gmb_tui_fire(c->handle, GMB_EV_CLICK); }
                return true;
            }
        }
    }
    if (m->is_open) { m->is_open = false; return true; }
    return false;
}
void gmb_menu_init(gmb_menu_t* m, int x, int y, int w)
{
    memset(m, 0, sizeof(*m));
    memset(m, 0, sizeof(*m));
    gmb_control_init(&m->ctrl, "", x, y, w, 1, menu_draw, menu_key);
    m->ctrl.handle_mouse_fn = menu_mouse;
    m->ctrl.draw_px_fn = menu_draw_px;
    m->ctrl.user = m;
    m->is_open = false;
}

gmb_menu_item_t* gmb_menu_add_item(gmb_menu_t* m, const char* label)
{
    if (m->items_count >= 16) return NULL;
    gmb_menu_item_t* it = &m->items[m->items_count++];
    memset(it, 0, sizeof(*it));
    snprintf(it->label, sizeof(it->label), "%s", label ? label : "");
    return it;
}

gmb_menu_sub_item_t* gmb_menu_item_add_sub(gmb_menu_item_t* item, const char* label)
{
    if (item->sub_count >= 8) return NULL;
    gmb_menu_sub_item_t* s = &item->sub[item->sub_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->label, sizeof(s->label), "%s", label ? label : "");
    return s;
}

void gmb_menu_item_set_action(gmb_menu_item_t* item, void (*fn)(void* ctx), void* ctx)
{
    item->action = fn;
    item->action_ctx = ctx;
}
void gmb_menu_sub_item_set_action(gmb_menu_sub_item_t* item, void (*fn)(void* ctx), void* ctx)
{
    item->action = fn;
    item->action_ctx = ctx;
}

int gmb_menu_compute_width(gmb_menu_t* m)
{
    int w = 0;
    for (int i = 0; i < m->items_count; i++)
        w += (int)strlen(m->items[i].label) + 2;
    return w;
}
int gmb_menu_submenu_width(gmb_menu_t* m)
{
    int w = 0;
    if (m->selected_index >= 0 && m->selected_index < m->items_count)
        for (int i = 0; i < m->items[m->selected_index].sub_count; i++)
        {
            int l = (int)strlen(m->items[m->selected_index].sub[i].label) + 2;
            if (l > w) w = l;
        }
    return w;
}
const char* gmb_menu_get_selected_label(gmb_menu_t* m)
{
    if (m->selected_index < 0 || m->selected_index >= m->items_count) return "";
    gmb_menu_item_t* it = &m->items[m->selected_index];
    if (it->sub_count == 0) return it->label;
    if (m->sub_selected_index >= 0 && m->sub_selected_index < it->sub_count)
        return it->sub[m->sub_selected_index].label;
    return it->label;
}
