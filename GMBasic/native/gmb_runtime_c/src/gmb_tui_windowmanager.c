/* ============================================================================
 * gmb_tui_windowmanager.c — WindowManager.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_runtime.h"   /* gmb_window_fire: eventi MOVE/RESIZE per-handle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void gmb_tile_grid_init(gmb_tile_grid_t* g, int rows, int cols)
{
    g->rows = rows < 1 ? 1 : rows;
    g->cols = cols < 1 ? 1 : cols;
}
int gmb_tile_grid_cells(const gmb_tile_grid_t* g) { return g->rows * g->cols; }
void gmb_tile_grid_rowcol(const gmb_tile_grid_t* g, int index, int* row, int* col)
{
    *row = index / g->cols;
    *col = index % g->cols;
}

gmb_rect_t gmb_managed_window_content_rect(const gmb_managed_window_t* w)
{
    gmb_rect_t r = { w->rect.x + 1, w->rect.y + 1, w->rect.w - 2, w->rect.h - 2 };
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

void gmb_window_manager_init(gmb_window_manager_t* m, gmb_rect_t work)
{
    memset(m, 0, sizeof(*m));
    m->work_area = work;
    m->focus = -1;
    m->next_id = 1;
    m->next_z = 1;
    m->current_desktop = 1; /* WindowManager.cs: default Desktop=1 */
}

int gmb_window_manager_add(gmb_window_manager_t* m, const char* title, gmb_rect_t rect)
{
    if (m->count >= GMB_WM_MAX_WINDOWS) return -1;
    int id = m->next_id++;
    int z = m->next_z++;
    gmb_managed_window_t* w = &m->windows[m->count++];
    memset(w, 0, sizeof(*w));
    w->id = id;
    snprintf(w->title, sizeof(w->title), "%s", title);
    w->rect = rect;
    w->z = z;
    w->state = GMB_WSTATE_FLOATING;
    w->restore_rect = rect;
    w->border = GMB_WM_BORDER_SINGLE;
    w->closable = true;
    w->draggable = true;
    w->desktop = 1; /* WindowManager.cs:67 ManagedWindow.Desktop default 1 */
    m->focus = id;
    return id;
}

gmb_managed_window_t* gmb_window_manager_get(gmb_window_manager_t* m, int id)
{
    for (int i = 0; i < m->count; i++)
        if (m->windows[i].id == id) return &m->windows[i];
    return NULL;
}
int gmb_window_manager_focused(gmb_window_manager_t* m) { return m->focus; }

int gmb_window_manager_topmost_at(gmb_window_manager_t* m, int x, int y)
{
    int best = -1, best_z = -1;
    for (int i = 0; i < m->count; i++)
    {
        gmb_managed_window_t* w = &m->windows[i];
        if (w->minimized) continue;
        if (x >= w->rect.x && x < w->rect.x + w->rect.w &&
            y >= w->rect.y && y < w->rect.y + w->rect.h &&
            w->z >= best_z)
        {
            best = w->id;
            best_z = w->z;
        }
    }
    return best;
}

void gmb_window_manager_raise(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return;
    w->z = m->next_z++;
    w->focused = true;
    m->focus = id;
}

void gmb_window_manager_deactivate(gmb_window_manager_t* m, int id)
{
    if (m->focus == id)
    {
        gmb_managed_window_t* w = gmb_window_manager_get(m, id);
        if (w) w->focused = false;
        m->focus = -1;
    }
}

static bool wm_title_iequals(const char* a, const char* b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

int gmb_window_manager_find_by_title(gmb_window_manager_t* m, const char* title)
{
    if (!title || title[0] == 0) return -1;
    for (int i = 0; i < m->count; i++)
        if (wm_title_iequals(m->windows[i].title, title)) return m->windows[i].id;
    return -1;
}

/* Nome BASIC della finestra (`WINDOW DEFINE nome ...`): distinto dal titolo. Serve a
 * riconoscere un re-DEFINE dello stesso nome → si aggiorna la finestra, non se ne crea
 * una seconda (parità con gmb_winstmt_define). */
int gmb_window_manager_find_by_name(gmb_window_manager_t* m, const char* name)
{
    if (!name || name[0] == 0) return -1;
    for (int i = 0; i < m->count; i++)
        if (m->windows[i].name[0] && wm_title_iequals(m->windows[i].name, name))
            return m->windows[i].id;
    return -1;
}

void gmb_window_manager_move_by(gmb_window_manager_t* m, int id, int dx, int dy)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (w) { w->rect.x += dx; w->rect.y += dy; gmb_window_fire(id, "MOVE"); }
}
void gmb_window_manager_move_to(gmb_window_manager_t* m, int id, int x, int y)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (w) { w->rect.x = x; w->rect.y = y; gmb_window_fire(id, "MOVE"); }
}
void gmb_window_manager_resize_by(gmb_window_manager_t* m, int id, int dw, int dh)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (w)
    {
        w->rect.w += dw;
        w->rect.h += dh;
        if (w->rect.w < 1) w->rect.w = 1;
        if (w->rect.h < 1) w->rect.h = 1;
        gmb_window_fire(id, "RESIZE");
    }
}

void gmb_window_manager_minimize(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return;
    w->restore_rect = w->rect;
    w->state = GMB_WSTATE_MINIMIZED;
    w->minimized = true;
    if (m->focus == id) { w->focused = false; m->focus = -1; }
}
void gmb_window_manager_restore(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return;
    w->state = GMB_WSTATE_FLOATING;
    w->minimized = false;
    w->rect = w->restore_rect;
}
void gmb_window_manager_maximize(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return;
    w->restore_rect = w->rect;
    w->rect = m->work_area;
    w->state = GMB_WSTATE_MAXIMIZED;
    w->minimized = false;
}

void gmb_window_manager_tile(gmb_window_manager_t* m, int rows, int cols)
{
    gmb_tile_grid_t g;
    gmb_tile_grid_init(&g, rows, cols);
    int cw = m->work_area.w / g.cols;
    int ch = m->work_area.h / g.rows;
    for (int i = 0; i < m->count && i < gmb_tile_grid_cells(&g); i++)
    {
        int r, c;
        gmb_tile_grid_rowcol(&g, i, &r, &c);
        gmb_managed_window_t* w = &m->windows[i];
        w->rect.x = m->work_area.x + c * cw;
        w->rect.y = m->work_area.y + r * ch;
        w->rect.w = cw;
        w->rect.h = ch;
        w->minimized = false;
        if (w->state == GMB_WSTATE_MINIMIZED) w->state = GMB_WSTATE_FLOATING;
    }
}

/* Snap a zona (parità WindowManager.cs Snap(id, zone)): 0=left half, 1=right half. */
void gmb_window_manager_snap_zone(gmb_window_manager_t* m, int id, int zone)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return;
    int half = m->work_area.w / 2;
    if (zone == 0) { gmb_rect_t r = { m->work_area.x, m->work_area.y, half, m->work_area.h }; w->rect = r; w->state = GMB_WSTATE_SNAPPED_LEFT; }
    else if (zone == 1) { gmb_rect_t r = { m->work_area.x + half, m->work_area.y, m->work_area.w - half, m->work_area.h }; w->rect = r; w->state = GMB_WSTATE_SNAPPED_RIGHT; }
}

void gmb_window_manager_cascade(gmb_window_manager_t* m, int offset_x, int offset_y)
{
    if (offset_x <= 0) offset_x = 2;
    if (offset_y <= 0) offset_y = 1;
    int x = m->work_area.x, y = m->work_area.y;
    for (int i = 0; i < m->count; i++) {
        gmb_managed_window_t* w = &m->windows[i];
        w->rect.x = x + (i * offset_x) % (m->work_area.w / 2);
        w->rect.y = y + (i * offset_y) % (m->work_area.h / 2);
        w->rect.w = m->work_area.w / 2;
        w->rect.h = m->work_area.h / 2;
        w->minimized = false;
        if (w->state == GMB_WSTATE_MINIMIZED) w->state = GMB_WSTATE_FLOATING;
    }
}

void gmb_window_manager_arrange_tiled(gmb_window_manager_t* m) { gmb_window_manager_tile(m, 2, 2); }
void gmb_window_manager_arrange_cascaded(gmb_window_manager_t* m) { gmb_window_manager_cascade(m, 2, 1); }

void gmb_window_manager_remove(gmb_window_manager_t* m, int id)
{
    int pos = -1;
    for (int i = 0; i < m->count; i++)
        if (m->windows[i].id == id) { pos = i; break; }
    if (pos < 0) return;
    if (m->focus == id) m->focus = -1;
    for (int i = pos; i < m->count - 1; i++) m->windows[i] = m->windows[i + 1];
    m->count--;
}

gmb_window_manager_t* gmb_window_manager_static(void)
{
    static gmb_window_manager_t wm;
    static bool inited = false;
    if (!inited)
    {
        gmb_window_manager_init(&wm, gmb_rect_new(1, 1, 80, 24));
        inited = true;
    }
    return &wm;
}

/* ── WINDOWS.X/Y/THEME/DESKTOP — stato del window manager GLOBALE ─────────────────
 * Parità IlRuntime: X/Y = posizione della finestra attiva (headless: -1), THEME =
 * tema corrente ("Turbo" default), DESKTOP = desktop corrente (1 default). Condivisi
 * da VM/C, C99Native (§0bis). */
double gmb_window_x(void)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    int id = gmb_window_manager_focused(m);
    gmb_managed_window_t* w = id >= 0 ? gmb_window_manager_get(m, id) : NULL;
    return w ? (double)w->rect.x : -1.0;
}
double gmb_window_y(void)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    int id = gmb_window_manager_focused(m);
    gmb_managed_window_t* w = id >= 0 ? gmb_window_manager_get(m, id) : NULL;
    return w ? (double)w->rect.y : -1.0;
}
double gmb_window_desktop(void)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    return (double)(m->current_desktop > 0 ? m->current_desktop : 1);
}
/* Tema corrente: UNICA fonte = registro tema del runtime (gmb_ui_theme_name, default
 * "turbo"). Prima era la costante "Turbo": su VM/C e C99Native il lettore
 * WINDOWS.THEME() ignorava il tema impostato (divergenza da IL). */
const char* gmb_window_theme(void) { return gmb_ui_theme_name(); }

/* Dimensioni della finestra attiva (celle, come X/Y): -1 se non c'e' finestra.
 * Prima erano stub (-1.0) su VM/C e C99Native. */
double gmb_window_width(void)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    int id = gmb_window_manager_focused(m);
    gmb_managed_window_t* w = id >= 0 ? gmb_window_manager_get(m, id) : NULL;
    return w ? (double)w->rect.w : -1.0;
}
double gmb_window_height(void)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    int id = gmb_window_manager_focused(m);
    gmb_managed_window_t* w = id >= 0 ? gmb_window_manager_get(m, id) : NULL;
    return w ? (double)w->rect.h : -1.0;
}

/* ============================================================================
 * 100% parity WindowManager.cs:1321 — desktop/status/drag/resize/select/tile/ctrl
 * ============================================================================ */

void gmb_window_manager_set_statusbar(gmb_window_manager_t* m, const char* text)
{
    if (!m) return;
    if (!text) { m->status_text[0] = '\0'; return; }
    snprintf(m->status_text, sizeof(m->status_text), "%s", text);
}
bool gmb_window_manager_has_status(const gmb_window_manager_t* m)
{ return m && m->status_text[0] != '\0'; }

void gmb_window_manager_sync_workarea(gmb_window_manager_t* m, int width, int height)
{
    if (!m) return;
    m->work_area.w = width;
    m->work_area.h = height;
}

void gmb_window_manager_switch_desktop(gmb_window_manager_t* m, int desktop)
{
    if (!m || desktop < 1) return;
    m->current_desktop = desktop;
    m->focus = -1;
    for (int i = 0; i < m->count; i++) m->windows[i].focused = false;
}

void gmb_window_manager_move_to_desktop(gmb_window_manager_t* m, int id, int desktop)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w || desktop < 1) return;
    w->desktop = desktop;
    if (desktop != m->current_desktop) {
        w->focused = false;
        if (m->focus == id) m->focus = -1;
    }
}

int gmb_window_manager_select(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return -1;
    if (w->desktop != m->current_desktop) return -1;
    gmb_window_manager_raise(m, id);
    return id;
}

int gmb_window_manager_select_title(gmb_window_manager_t* m, const char* title)
{
    if (!m || !title) return -1;
    int id = gmb_window_manager_find_by_title(m, title);
    if (id < 0) return -1;
    return gmb_window_manager_select(m, id);
}

void gmb_window_manager_send_to_cell(gmb_window_manager_t* m, int id, int rows, int cols, int row, int col, int gap)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w || rows < 1 || cols < 1) return;
    if (row < 0) row = 0; if (col < 0) col = 0;
    if (row >= rows) row = rows - 1; if (col >= cols) col = cols - 1;
    int cw = (m->work_area.w - (cols - 1) * gap) / cols;
    int ch = (m->work_area.h - (rows - 1) * gap) / rows;
    if (cw < 1) cw = 1; if (ch < 1) ch = 1;
    w->rect.x = m->work_area.x + col * (cw + gap);
    w->rect.y = m->work_area.y + row * (ch + gap);
    w->rect.w = cw;
    w->rect.h = ch;
}

void gmb_window_manager_tile_all(gmb_window_manager_t* m, int rows, int cols, int gap)
{
    if (!m || rows < 1 || cols < 1) return;
    int idx = 0;
    for (int i = 0; i < m->count; i++) {
        gmb_managed_window_t* w = &m->windows[i];
        if (w->minimized || w->desktop != m->current_desktop) continue;
        int r = idx / cols, c = idx % cols;
        gmb_window_manager_send_to_cell(m, w->id, rows, cols, r, c, gap);
        idx++;
    }
}

void gmb_window_manager_maximize_toggle(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return;
    if (w->rect.x == m->work_area.x && w->rect.y == m->work_area.y &&
        w->rect.w == m->work_area.w && w->rect.h == m->work_area.h) {
        gmb_window_manager_restore(m, id);
    } else {
        gmb_window_manager_maximize(m, id);
    }
}

void gmb_window_manager_unminimize(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w || !w->minimized) return;
    w->minimized = false;
    gmb_window_manager_raise(m, id);
}
bool gmb_window_manager_is_minimized(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    return w && w->minimized;
}

void gmb_window_manager_start_drag(gmb_window_manager_t* m, int id, int x, int y)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) { m->drag_id = -1; return; }
    m->drag_id = id;
    m->drag_ox = x - w->rect.x;
    m->drag_oy = y - w->rect.y;
}
bool gmb_window_manager_update_drag(gmb_window_manager_t* m, int x, int y)
{
    if (m->drag_id < 0) return false;
    gmb_window_manager_move_to(m, m->drag_id, x - m->drag_ox, y - m->drag_oy);
    return true;
}
int gmb_window_manager_end_drag(gmb_window_manager_t* m)
{
    int id = m->drag_id;
    m->drag_id = -1;
    return id;
}

int gmb_window_manager_window_at_titlebar(gmb_window_manager_t* m, int x, int y)
{
    if (!m) return -1;
    int best = -1, best_z = -1;
    for (int i = 0; i < m->count; i++) {
        gmb_managed_window_t* w = &m->windows[i];
        if (w->minimized || w->desktop != m->current_desktop) continue;
        if (x >= w->rect.x && x < w->rect.x + w->rect.w &&
            y >= w->rect.y && y < w->rect.y + 1 && w->z >= best_z) {
            best = w->id; best_z = w->z;
        }
    }
    return best;
}

bool gmb_window_manager_start_resize(gmb_window_manager_t* m, int id, int x, int y, const char* edges)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) return false;
    int mask = 0;
    if (edges) for (const char* p = edges; *p; p++) {
        switch (*p) { case 'N': case 'n': mask |= 1; break; case 'S': case 's': mask |= 2; break;
                      case 'E': case 'e': mask |= 4; break; case 'W': case 'w': mask |= 8; break; }
    }
    if (mask == 0) return false;
    m->resize_id = id;
    m->resize_edges = mask;
    m->drag_ox = x; m->drag_oy = y;
    m->resize_ow = w->rect.w; m->resize_oh = w->rect.h;
    return true;
}
bool gmb_window_manager_update_resize(gmb_window_manager_t* m, int x, int y)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, m->resize_id);
    if (!w || m->resize_edges == 0) return false;
    /* Semantica WindowManager.cs: minW=12 minH=4, corner (2 edge), bound al work-area. */
    const int minW = 12, minH = 4;
    int dx = x - m->drag_ox, dy = y - m->drag_oy;
    int rx = w->rect.x, ry = w->rect.y;
    int rw = m->resize_ow, rh = m->resize_oh;
    if (m->resize_edges & 4) { int n = m->resize_ow + dx; rw = n > minW ? n : minW; }               /* E */
    if (m->resize_edges & 8) { int n = m->resize_ow - dx; if (n < minW) n = minW; rx = w->rect.x + (rw - n); rw = n; } /* W */
    if (m->resize_edges & 2) { int n = m->resize_oh + dy; rh = n > minH ? n : minH; }               /* S */
    if (m->resize_edges & 1) { int n = m->resize_oh - dy; if (n < minH) n = minH; ry = w->rect.y + (rh - n); rh = n; } /* N */
    int wx = m->work_area.x, wy = m->work_area.y, wr = wx + m->work_area.w, wb = wy + m->work_area.h;
    if (rx < wx) rx = wx;
    if (ry < wy) ry = wy;
    if (rx + rw > wr) rw = wr - rx;
    if (ry + rh > wb) rh = wb - ry;
    w->rect.x = rx; w->rect.y = ry; w->rect.w = rw; w->rect.h = rh;
    gmb_window_fire(w->id, "RESIZE");
    return true;
}
void gmb_window_manager_end_resize(gmb_window_manager_t* m)
{
    m->resize_id = -1;
    m->resize_edges = 0;
}

void gmb_window_manager_control_action(gmb_window_manager_t* m, int id, int ctrl)
{
    if (!m) return;
    switch (ctrl) {
        case GMB_WM_CTRL_CLOSE: gmb_window_manager_remove(m, id); break;
        case GMB_WM_CTRL_MIN:   gmb_window_manager_minimize(m, id); break;
        case GMB_WM_CTRL_MAX:   gmb_window_manager_maximize_toggle(m, id); break;
        default: break;
    }
}

int gmb_window_manager_focus_next(gmb_window_manager_t* m, int forward)
{
    if (!m || m->count == 0) return -1;
    /* ordina per z */
    int ids[GMB_WM_MAX_WINDOWS];
    int zs[GMB_WM_MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < m->count; i++)
        if (m->windows[i].desktop == m->current_desktop && !m->windows[i].minimized)
            { ids[n] = m->windows[i].id; zs[n] = m->windows[i].z; n++; }
    if (n == 0) return -1;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++)
            if (zs[b] < zs[a]) { int t = zs[a]; zs[a] = zs[b]; zs[b] = t; t = ids[a]; ids[a] = ids[b]; ids[b] = t; }
    int cur = -1;
    for (int i = 0; i < n; i++) if (ids[i] == m->focus) { cur = i; break; }
    int nxt = forward ? (cur + 1) % n : (cur - 1 + n) % n;
    return gmb_window_manager_select(m, ids[nxt]);
}

int gmb_window_manager_taskbar_chip_at(gmb_window_manager_t* m, int x, int y)
{
    if (!m) return -1;
    /* Stesso layout del render (gmb_window_manager_render): chip delle finestre MINIMIZZATE
     * del desktop corrente, accodati da DESTRA, larghezza = lunghezza della label. */
    int W = m->work_area.w;
    if (W <= 0) return -1;
    if (y != m->work_area.y + m->work_area.h - 1) return -1;
    int x2 = W;
    for (int pass = 0; pass < m->count; pass++) {
        /* z-order crescente, come il render */
        gmb_managed_window_t* w = NULL;
        for (int i = 0; i < m->count; i++) {
            gmb_managed_window_t* c = &m->windows[i];
            if (c->desktop != m->current_desktop || !c->minimized) continue;
            int already = 0;
            for (int j = 0; j < i; j++) {
                gmb_managed_window_t* d = &m->windows[j];
                if (d->desktop != m->current_desktop || !d->minimized) continue;
                if (d->z <= c->z) already++;   /* conta i precedenti con z minore */
            }
            if (already == pass) { w = c; break; }
        }
        if (!w) break;
        char label[32];
        const char* t = w->title;
        int tl = (int)strlen(t); if (tl > 14) tl = 14;
        snprintf(label, sizeof(label), " %.*s ", tl, t);
        int ll = (int)strlen(label);
        int x0 = x2 - ll;
        if (x0 < 1) break;
        if (x >= x0 && x < x2) return w->id;
        x2 = x0 - 1;
    }
    return -1;
}

/* ---- Query per il client C# (P/Invoke). Lo stato vive SOLO in C. ---- */
int gmb_window_manager_window_count(gmb_window_manager_t* m, int desktop)
{
    if (!m) return 0;
    int n = 0;
    for (int i = 0; i < m->count; i++)
        if (desktop <= 0 || m->windows[i].desktop == desktop) n++;
    return n;
}
int gmb_window_manager_window_id_at(gmb_window_manager_t* m, int index)
{
    if (!m || index < 0 || index >= m->count) return -1;
    return m->windows[index].id;
}
int gmb_window_manager_current_desktop(gmb_window_manager_t* m) { return m ? m->current_desktop : 1; }
const char* gmb_window_manager_status_text(gmb_window_manager_t* m) { return m ? m->status_text : ""; }
void gmb_window_manager_set_title(gmb_window_manager_t* m, int id, const char* title)
{
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (w && title) snprintf(w->title, sizeof(w->title), "%s", title);
}
int gmb_window_manager_is_dragging(gmb_window_manager_t* m) { return m && m->drag_id >= 0; }
int gmb_window_manager_is_resizing(gmb_window_manager_t* m) { return m && m->resize_id >= 0; }
int gmb_window_manager_drag_id(gmb_window_manager_t* m) { return m ? m->drag_id : -1; }
int gmb_window_manager_resize_id(gmb_window_manager_t* m) { return m ? m->resize_id : -1; }
void gmb_window_manager_clear_focus(gmb_window_manager_t* m)
{
    if (!m) return;
    m->focus = -1;
    for (int i = 0; i < m->count; i++) m->windows[i].focused = false;
}
void gmb_window_manager_work_area4(gmb_window_manager_t* m, int* out4)
{
    if (!out4) return;
    if (!m) { out4[0] = 0; out4[1] = 0; out4[2] = 80; out4[3] = 24; return; }
    out4[0] = m->work_area.x; out4[1] = m->work_area.y;
    out4[2] = m->work_area.w; out4[3] = m->work_area.h;
}

/* ============================================================================
 * Helper per il client C# (render): alloca/azzera/carica le finestre dal lato C#.
 * Il C# resta autoritativo sulla logica; questi servono a popolare la struct C
 * per `gmb_window_manager_render`.
 * ============================================================================ */
gmb_window_manager_t* gmb_window_manager_new(int x, int y, int w, int h)
{
    gmb_window_manager_t* m = (gmb_window_manager_t*)calloc(1, sizeof(gmb_window_manager_t));
    if (m) { gmb_rect_t work = { x, y, w, h }; gmb_window_manager_init(m, work); }
    return m;
}
void gmb_window_manager_free(gmb_window_manager_t* m) { free(m); }
void gmb_window_manager_clear_windows(gmb_window_manager_t* m) { if (m) { m->count = 0; m->focus = -1; } }
void gmb_window_manager_set_current_desktop(gmb_window_manager_t* m, int d) { if (m) m->current_desktop = d; }
int gmb_window_manager_push(gmb_window_manager_t* m, const char* title, int id,
                            int x, int y, int w, int h, int z, int state,
                            int minimized, int border, int focused, int desktop, int closable)
{
    if (!m || m->count >= GMB_WM_MAX_WINDOWS) return -1;
    gmb_managed_window_t* win = &m->windows[m->count];
    memset(win, 0, sizeof(*win));
    win->id = id > 0 ? id : m->next_id++;
    if (win->id >= m->next_id) m->next_id = win->id + 1;
    if (title) strncpy(win->title, title, sizeof(win->title) - 1);
    win->rect.x = x; win->rect.y = y; win->rect.w = w; win->rect.h = h;
    win->z = z; win->state = state; win->minimized = minimized != 0; win->border = border;
    win->focused = focused != 0; win->desktop = desktop; win->closable = closable != 0; win->draggable = 1;
    m->count++;
    return win->id;
}

/* Read-back per il client C#: trova la finestra per id e ritorna rect (out4=[x,y,w,h])/state. */
static gmb_managed_window_t* wm_find(gmb_window_manager_t* m, int id)
{
    if (!m) return NULL;
    for (int i = 0; i < m->count; i++) if (m->windows[i].id == id) return &m->windows[i];
    return NULL;
}
void gmb_window_manager_window_rect(gmb_window_manager_t* m, int id, int* out4)
{
    if (!out4) return;
    for (int i = 0; i < 4; i++) out4[i] = 0;
    gmb_managed_window_t* w = wm_find(m, id);
    if (!w) return;
    out4[0] = w->rect.x; out4[1] = w->rect.y; out4[2] = w->rect.w; out4[3] = w->rect.h;
}
int gmb_window_manager_window_state(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = wm_find(m, id);
    return w ? w->state : 0;
}
int gmb_window_manager_window_focused(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = wm_find(m, id);
    return (w && w->focused) ? 1 : 0;
}
int gmb_window_manager_window_desktop(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = wm_find(m, id);
    return w ? w->desktop : 1;
}
int gmb_window_manager_window_z(gmb_window_manager_t* m, int id)
{
    gmb_managed_window_t* w = wm_find(m, id);
    return w ? w->z : 0;
}

/* ============================================================================
 * Render (port di WindowManager.cs: RenderDesktop/RenderWindow/RenderStatusBar).
 * Disegna sfondo + cornici + titlebar + statusbar. Il CONTENUTO delle finestre
 * è disegnato dal chiamante (split). Charset: codepoint (utf-32) come il buffer C.
 * ============================================================================ */
static void wmsc(gmb_screen_buffer_t* b, int x, int y, uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    gmb_screen_setchar(b, x, y, ch, fg, bg);
}
static void wm_fill(gmb_screen_buffer_t* b, int x, int y, int w, int h, uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) wmsc(b, x + xx, y + yy, ch, fg, bg);
}
/* primo codepoint di una stringa UTF-8 (per DesktopPattern). */
static uint32_t wm_first_cp(const char* s)
{
    if (!s || !*s) return ' ';
    const unsigned char* p = (const unsigned char*)s;
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xE0) == 0xC0) return ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    if ((p[0] & 0xF0) == 0xE0) return ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    return ' ';
}

static void wm_frame(gmb_screen_buffer_t* b, gmb_rect_t r, int style, gmb_cell_color_t frame, gmb_cell_color_t tbg)
{
    uint32_t tl, tr, bl, br, hz, vt;
    if (style == GMB_WM_BORDER_SINGLE)     { tl = 0x250C; tr = 0x2510; bl = 0x2514; br = 0x2518; hz = 0x2500; vt = 0x2502; }
    else if (style == GMB_WM_BORDER_ROUNDED) { tl = 0x256D; tr = 0x256E; bl = 0x2570; br = 0x256F; hz = 0x2500; vt = 0x2502; }
    else                                    { tl = 0x2554; tr = 0x2557; bl = 0x255A; br = 0x255D; hz = 0x2550; vt = 0x2551; }
    if (r.w < 2 || r.h < 2) { wm_fill(b, r.x, r.y, r.w, r.h, ' ', frame, tbg); return; }
    for (int x = r.x + 1; x < r.x + r.w - 1; x++) { wmsc(b, x, r.y, hz, frame, tbg); wmsc(b, x, r.y + r.h - 1, hz, frame, tbg); }
    for (int y = r.y + 1; y < r.y + r.h - 1; y++) { wmsc(b, r.x, y, vt, frame, tbg); wmsc(b, r.x + r.w - 1, y, vt, frame, tbg); }
    wmsc(b, r.x, r.y, tl, frame, tbg);
    wmsc(b, r.x + r.w - 1, r.y, tr, frame, tbg);
    wmsc(b, r.x, r.y + r.h - 1, bl, frame, tbg);
    wmsc(b, r.x + r.w - 1, r.y + r.h - 1, br, frame, tbg);
}

static void wm_titlebar(gmb_screen_buffer_t* b, gmb_rect_t r, gmb_cell_color_t frame, gmb_cell_color_t closeFg,
                        gmb_cell_color_t tbg, gmb_cell_color_t dim, const char* title, int focused, const gmb_theme_t* th)
{
    if (r.w < 6) return;
    /* [■] chiudi */
    wmsc(b, r.x + 2, r.y, '[', frame, tbg);
    wmsc(b, r.x + 3, r.y, 0x25A0, focused ? closeFg : dim, tbg);
    wmsc(b, r.x + 4, r.y, ']', frame, tbg);
    /* [▼][▲] riduci/ingrandisci */
    if (r.w >= 12) {
        int mnx = r.x + r.w - 8;
        wmsc(b, mnx, r.y, '[', frame, tbg); wmsc(b, mnx + 1, r.y, 0x25BC, focused ? th->title_fg : dim, tbg); wmsc(b, mnx + 2, r.y, ']', frame, tbg);
        int mxx = r.x + r.w - 5;
        wmsc(b, mxx, r.y, '[', frame, tbg); wmsc(b, mxx + 1, r.y, 0x25B2, focused ? th->title_fg : dim, tbg); wmsc(b, mxx + 2, r.y, ']', frame, tbg);
    }
    int left = r.x + 6, right = (r.w >= 12) ? r.x + r.w - 9 : r.x + r.w - 2;
    int avail = right - left; if (avail < 0) avail = 0;
    if (title && *title && avail >= 4) {
        int tl = (int)strlen(title);
        int maxlen = avail - 4; if (maxlen < 0) maxlen = 0;
        if (tl > maxlen) tl = maxlen;
        /* "╡ Titolo ╞" (focused) / "[ Titolo ]" : larghezza = tl + 4 */
        int fullw = tl + 4;
        int start = left + (avail - fullw) / 2; if (start < left) start = left;
        uint32_t open = focused ? 0x2561 : '[';   /* ╡ */
        uint32_t close = focused ? 0x255E : ']';  /* ╞ */
        gmb_cell_color_t decorFg = frame;
        gmb_cell_color_t titleFg = focused ? th->accent : dim;
        gmb_cell_color_t titleBg = focused ? th->title_focus : tbg;
        wmsc(b, start, r.y, open, decorFg, titleBg);
        wmsc(b, start + 1, r.y, ' ', decorFg, titleBg);
        for (int i = 0; i < tl && start + 2 + i < r.x + r.w - 1; i++)
            wmsc(b, start + 2 + i, r.y, (uint32_t)(unsigned char)title[i], titleFg, titleBg);
        wmsc(b, start + 2 + tl, r.y, ' ', decorFg, titleBg);
        wmsc(b, start + 3 + tl, r.y, close, decorFg, titleBg);
    }
}

static void wm_render_window(gmb_screen_buffer_t* b, gmb_managed_window_t* w, const gmb_theme_t* th, int desktop)
{
    if (w->minimized || w->desktop != desktop) return;
    if (w->pixel) return;   /* finestra PIXEL: la disegna il compositor gfx, non il renderer a celle */
    gmb_rect_t r = w->rect;
    int focused = w->focused;
    gmb_cell_color_t frame = focused ? th->border : th->dim;
    gmb_cell_color_t tbg = th->window_bg;

    /* ombra 3D */
    if (r.w >= 6 && r.h >= 3) {
        for (int y = r.y + 1; y <= r.y + r.h; y++)
            for (int sx = r.x + r.w; sx < r.x + r.w + 2; sx++) wmsc(b, sx, y, ' ', th->shadow, th->shadow);
        int by = r.y + r.h;
        for (int sx = r.x + 2; sx < r.x + r.w + 2; sx++) wmsc(b, sx, by, ' ', th->shadow, th->shadow);
    }
    /* corpo */
    wm_fill(b, r.x, r.y, r.w, r.h, ' ', th->text, th->window_bg);
    /* cornice */
    int style = w->border;
    if (style == GMB_WM_BORDER_ROUNDED) wm_frame(b, r, GMB_WM_BORDER_ROUNDED, frame, tbg);
    else if (style == GMB_WM_BORDER_SINGLE || (!focused && style != GMB_WM_BORDER_DOUBLE)) wm_frame(b, r, GMB_WM_BORDER_SINGLE, frame, tbg);
    else wm_frame(b, r, GMB_WM_BORDER_DOUBLE, frame, tbg);
    /* titlebar */
    wm_titlebar(b, r, frame, th->close_fg, tbg, th->dim, w->title, focused, th);
}

void gmb_window_manager_render(gmb_window_manager_t* m, gmb_screen_buffer_t* buf, const gmb_theme_t* th, const char* header)
{
    if (!m || !buf || !th) return;
    int W = gmb_screen_width(buf), H = gmb_screen_height(buf);
    /* desktop */
    uint32_t pat = wm_first_cp(th->desktop_pattern);
    for (int y = 0; y < H - 1; y++)
        for (int x = 0; x < W; x++) wmsc(buf, x, y, pat, th->dim, th->desktop_bg);
    if (header && *header) gmb_screen_setstring(buf, 2, 0, header, th->accent, th->desktop_bg);
    /* finestre in ordine z (dietro→davanti) */
    int order[GMB_WM_MAX_WINDOWS];
    for (int i = 0; i < m->count; i++) order[i] = i;
    for (int i = 0; i < m->count; i++)
        for (int j = i + 1; j < m->count; j++)
            if (m->windows[order[j]].z < m->windows[order[i]].z) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int i = 0; i < m->count; i++)
        wm_render_window(buf, &m->windows[order[i]], th, m->current_desktop);
    /* statusbar */
    int y = H - 1;
    for (int x = 0; x < W; x++) wmsc(buf, x, y, ' ', th->text, th->desktop_bg);
    int x2 = W;
    for (int i = 0; i < m->count; i++) {
        gmb_managed_window_t* w = &m->windows[order[i]];
        if (w->desktop != m->current_desktop || !w->minimized) continue;
        char label[32];
        const char* t = w->title;
        int tl = (int)strlen(t); if (tl > 14) tl = 14;
        snprintf(label, sizeof(label), " %.*s ", tl, t);
        int ll = (int)strlen(label);
        int x0 = x2 - ll;
        if (x0 < 1) break;
        gmb_screen_setstring(buf, x0, y, label, th->title_fg, th->border);
        x2 = x0 - 1;
    }
    int maxd = m->current_desktop;
    for (int i = 0; i < m->count; i++) if (m->windows[i].desktop > maxd) maxd = m->windows[i].desktop;
    char status[256]; status[0] = '\0';
    if (maxd > 1) {
        for (int d = 1; d <= maxd; d++) { char p[16]; snprintf(p, sizeof(p), d == m->current_desktop ? "[D%d*]" : "[D%d]", d); strncat(status, p, sizeof(status) - strlen(status) - 1); }
        strncat(status, " ", sizeof(status) - strlen(status) - 1);
    }
    strncat(status, m->status_text, sizeof(status) - strlen(status) - 1);
    if (status[0]) gmb_screen_setstring(buf, 1, y, status, th->text, th->desktop_bg);
}

void gmb_window_manager_render_named(gmb_window_manager_t* m, gmb_screen_buffer_t* buf, const char* theme_name, const char* header)
{
    gmb_theme_t th = gmb_theme_named((theme_name && *theme_name) ? theme_name : "turbo");
    gmb_window_manager_render(m, buf, &th, header);
}

/* ============================================================================
 * Modello di CONTENUTO della finestra (parità ManagedWindow.cs): Lines/ScrollOffset/
 * Cursor + SayAt/BoxAt/AppendLine/SetLines/ScrollTo/ScrollBy/Snapshot/RestoreFrom.
 * Geometria gia' presente sopra. Il testo vive NELLA finestra (non su stdout).
 * ============================================================================ */
gmb_rect_t gmb_managed_window_titlebar_rect(const gmb_managed_window_t* w)
{
    gmb_rect_t r; r.x = w ? w->rect.x : 0; r.y = w ? w->rect.y : 0; r.w = w ? w->rect.w : 0; r.h = 1; return r;
}
void gmb_managed_window_append_line(gmb_managed_window_t* w, const char* line)
{
    if (!w) return;
    int ch = gmb_managed_window_content_rect(w).h; if (ch < 1) ch = 1;
    bool at_bottom = (w->scroll_offset + ch >= w->lines_count) || w->lines_count == 0;
    if (w->lines_count < 256) snprintf(w->lines[w->lines_count], 256, "%s", line ? line : "");
    if (w->lines_count < 256) w->lines_count++;
    if (at_bottom && w->lines_count > ch) w->scroll_offset = w->lines_count - ch;
}
void gmb_managed_window_set_lines(gmb_managed_window_t* w, const char* const* lines, int n)
{
    if (!w) return;
    w->lines_count = 0;
    for (int i = 0; i < n && i < 256; i++) { snprintf(w->lines[i], 256, "%s", lines && lines[i] ? lines[i] : ""); w->lines_count++; }
    w->scroll_offset = 0; w->cursor_line = 0; w->cursor_col = 0;
}
void gmb_managed_window_scroll_to(gmb_managed_window_t* w, int offset)
{
    if (!w) return;
    int ch = gmb_managed_window_content_rect(w).h; if (ch < 1) ch = 1;
    int max = w->lines_count - ch; if (max < 0) max = 0;
    if (offset < 0) offset = 0;
    if (offset > max) offset = max;
    w->scroll_offset = offset;
}
void gmb_managed_window_scroll_by(gmb_managed_window_t* w, int delta) { if (w) gmb_managed_window_scroll_to(w, w->scroll_offset + delta); }
const char* gmb_managed_window_say_at(gmb_managed_window_t* w, int row, int col, const char* text)
{
    if (!w || row < 0 || col < 0 || col >= 255) return "";
    int abs = w->scroll_offset + row;
    if (abs < 0 || abs >= 256) return "";
    while (w->lines_count <= abs && w->lines_count < 256) { w->lines[w->lines_count][0] = 0; w->lines_count++; }
    char* line = w->lines[abs];
    int ll = (int)strlen(line);
    while (ll < col && ll < 255) line[ll++] = ' ';
    line[ll] = 0;
    const char* t = text ? text : "";
    int tl = (int)strlen(t);
    int remov = tl; if (remov > ll - col) remov = ll - col; if (remov < 0) remov = 0;
    int tail = ll - col - remov; if (tail < 0) tail = 0;
    int newlen = col + tl + tail;
    if (newlen > 255) { tl = 255 - col - tail; if (tl < 0) tl = 0; newlen = col + tl + tail; }
    memmove(line + col + tl, line + col + remov, (size_t)tail + 1);
    memcpy(line + col, t, (size_t)tl);
    return line;
}
void gmb_managed_window_box_at(gmb_managed_window_t* w, int top, int left, int bottom, int right, const char* border)
{
    if (!w) return;
    /* "single"/"double"/"none" (parità ManagedWindow.BoxAt). Multi-byte UTF-8: si usa SayAt
     * (best-effort: le colonne si riferiscono ai byte del carattere singolo). */
    const char* tl = "\xE2\x94\x8C", *th = "\xE2\x94\x80", *tr = "\xE2\x94\x90";
    const char* vl = "\xE2\x94\x82", *vr = "\xE2\x94\x82", *bl = "\xE2\x94\x94", *bh = "\xE2\x94\x80", *br = "\xE2\x94\x98";
    if (border && strcmp(border, "double") == 0)
    { tl = "\xE2\x95\x94"; th = "\xE2\x95\x90"; tr = "\xE2\x95\x97"; vl = "\xE2\x95\x91"; vr = "\xE2\x95\x91"; bl = "\xE2\x95\x9A"; bh = "\xE2\x95\x90"; br = "\xE2\x95\x9D"; }
    else if (border && strcmp(border, "none") == 0) return;
    for (int x = left; x <= right; x++) { gmb_managed_window_say_at(w, top, x, th); gmb_managed_window_say_at(w, bottom, x, bh); }
    for (int y = top; y <= bottom; y++) { gmb_managed_window_say_at(w, y, left, vl); gmb_managed_window_say_at(w, y, right, vr); }
    gmb_managed_window_say_at(w, top, left, tl);
    gmb_managed_window_say_at(w, top, right, tr);
    gmb_managed_window_say_at(w, bottom, left, bl);
    gmb_managed_window_say_at(w, bottom, right, br);
}
void gmb_managed_window_snapshot(const gmb_managed_window_t* w, gmb_managed_window_t* out)
{ if (w && out) memcpy(out, w, sizeof(*out)); }
void gmb_managed_window_restore_from(gmb_managed_window_t* w, const gmb_managed_window_t* snap)
{ if (w && snap) memcpy(w, snap, sizeof(*w)); }

/* ---- Hit-test della titlebar (parità ManagedWindow.ControlAt/ResizeEdgeAt) ---- */
int gmb_managed_window_control_at(const gmb_managed_window_t* w, int x, int y)
{
    if (!w || y != w->rect.y) return 0;
    int lx = x - w->rect.x, ww = w->rect.w;
    if (w->closable && ww >= 7 && lx >= 2 && lx <= 4) return 1;         /* Close */
    if (ww >= 12 && lx >= ww - 8 && lx <= ww - 6) return 2;             /* Minimize */
    if (ww >= 10 && lx >= ww - 5 && lx <= ww - 3) return 3;             /* Maximize */
    return 0;
}
int gmb_managed_window_resize_edge_at(const gmb_managed_window_t* w, int x, int y)
{
    if (!w) return 0;
    const int edge = 1;
    if (x < w->rect.x - edge || x >= w->rect.x + w->rect.w + edge) return 0;
    if (y < w->rect.y - edge || y >= w->rect.y + w->rect.h + edge) return 0;
    int onN = (y == w->rect.y), onS = (y == w->rect.y + w->rect.h - 1);
    int onE = (x == w->rect.x + w->rect.w - 1), onW = (x == w->rect.x);
    int m = (onN ? 1 : 0) | (onS ? 2 : 0) | (onE ? 4 : 0) | (onW ? 8 : 0);
    if (m == 1) return 0;   /* solo bordo nord (titlebar) → drag, non resize (parità C#) */
    return m;
}

/* Factory "ricca" (parità WindowManager.AddWindow + proprieta' ManagedWindow): nome logico,
 * border (PanelBorder), FrameStyle, shadow, desktop. Ritorna l'id (o -1). */
int gmb_window_manager_add_ex(gmb_window_manager_t* m, const char* name, const char* title,
                              gmb_rect_t rect, int border, int frame_style, int shadow, int desktop)
{
    int id = gmb_window_manager_add(m, title ? title : "", rect);
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (w)
    {
        snprintf(w->name, sizeof(w->name), "%s", name ? name : "");
        w->border = border;
        w->frame_style = frame_style;
        w->shadow = shadow ? 1 : 0;
        if (desktop > 0) w->desktop = desktop;
    }
    return id;
}

/* Client rect in pixel logici di una finestra PIXEL (title 24, padding 6). */
void gmb_managed_window_client_rect_px(const gmb_managed_window_t* w, int* out4)
{
    if (!out4) return;
    out4[0] = out4[1] = out4[2] = out4[3] = 0;
    if (!w) return;
    int titleh = 24; if (titleh > w->rect.h / 2) titleh = w->rect.h / 2;
    out4[0] = w->rect.x + 6;
    out4[1] = w->rect.y + titleh;
    out4[2] = w->rect.w - 12;
    out4[3] = w->rect.h - titleh - 6;
}

/* ---- Accessor generici per il binding C# (evita decine di P/Invoke per-campo) ---- */
enum {
    GMB_MWF_X=0, GMB_MWF_Y, GMB_MWF_W, GMB_MWF_H, GMB_MWF_Z, GMB_MWF_STATE,
    GMB_MWF_BORDER, GMB_MWF_FRAME_STYLE, GMB_MWF_SHADOW, GMB_MWF_FOCUSED,
    GMB_MWF_MINIMIZED, GMB_MWF_DESKTOP, GMB_MWF_CURSOR_LINE, GMB_MWF_CURSOR_COL,
    GMB_MWF_SCROLL, GMB_MWF_LINES_COUNT, GMB_MWF_CLOSABLE, GMB_MWF_DRAGGABLE, GMB_MWF_ID,
    GMB_MWF_RX, GMB_MWF_RY, GMB_MWF_RW, GMB_MWF_RH, GMB_MWF_EDITABLE,
    GMB_MWF_BODY_COLOR, GMB_MWF_FG_COLOR, GMB_MWF_TITLE_COLOR, GMB_MWF_PIXEL, GMB_MWF_RADIUS,
    GMB_MWF_RESIZABLE
};
int gmb_managed_window_get_int(const gmb_managed_window_t* w, int field)
{
    if (!w) return 0;
    switch (field) {
        case GMB_MWF_X: return w->rect.x;
        case GMB_MWF_Y: return w->rect.y;
        case GMB_MWF_W: return w->rect.w;
        case GMB_MWF_H: return w->rect.h;
        case GMB_MWF_Z: return w->z;
        case GMB_MWF_STATE: return w->state;
        case GMB_MWF_BORDER: return w->border;
        case GMB_MWF_FRAME_STYLE: return w->frame_style;
        case GMB_MWF_SHADOW: return w->shadow;
        case GMB_MWF_FOCUSED: return w->focused ? 1 : 0;
        case GMB_MWF_MINIMIZED: return w->minimized ? 1 : 0;
        case GMB_MWF_DESKTOP: return w->desktop;
        case GMB_MWF_CURSOR_LINE: return w->cursor_line;
        case GMB_MWF_CURSOR_COL: return w->cursor_col;
        case GMB_MWF_SCROLL: return w->scroll_offset;
        case GMB_MWF_LINES_COUNT: return w->lines_count;
        case GMB_MWF_CLOSABLE: return w->closable ? 1 : 0;
        case GMB_MWF_DRAGGABLE: return w->draggable ? 1 : 0;
        case GMB_MWF_ID: return w->id;
        case GMB_MWF_RX: return w->restore_rect.x;
        case GMB_MWF_RY: return w->restore_rect.y;
        case GMB_MWF_RW: return w->restore_rect.w;
        case GMB_MWF_RH: return w->restore_rect.h;
        case GMB_MWF_EDITABLE: return w->editable ? 1 : 0;
        case GMB_MWF_BODY_COLOR: return (int)w->body_color;
        case GMB_MWF_FG_COLOR: return (int)w->fg_color;
        case GMB_MWF_TITLE_COLOR: return (int)w->title_color;
        case GMB_MWF_PIXEL: return w->pixel;
        case GMB_MWF_RADIUS: return w->radius;
        case GMB_MWF_RESIZABLE: return w->resizable;
    }
    return 0;
}
void gmb_managed_window_set_int(gmb_managed_window_t* w, int field, int v)
{
    if (!w) return;
    switch (field) {
        case GMB_MWF_X: w->rect.x = v; break;
        case GMB_MWF_Y: w->rect.y = v; break;
        case GMB_MWF_W: w->rect.w = v; break;
        case GMB_MWF_H: w->rect.h = v; break;
        case GMB_MWF_Z: w->z = v; break;
        case GMB_MWF_STATE: w->state = v; break;
        case GMB_MWF_BORDER: w->border = v; break;
        case GMB_MWF_FRAME_STYLE: w->frame_style = v; break;
        case GMB_MWF_SHADOW: w->shadow = v; break;
        case GMB_MWF_FOCUSED: w->focused = v != 0; break;
        case GMB_MWF_MINIMIZED: w->minimized = v != 0; break;
        case GMB_MWF_DESKTOP: w->desktop = v; break;
        case GMB_MWF_CURSOR_LINE: w->cursor_line = v; break;
        case GMB_MWF_CURSOR_COL: w->cursor_col = v; break;
        case GMB_MWF_SCROLL: w->scroll_offset = v; break;
        case GMB_MWF_CLOSABLE: w->closable = v != 0; break;
        case GMB_MWF_DRAGGABLE: w->draggable = v != 0; break;
        case GMB_MWF_RX: w->restore_rect.x = v; break;
        case GMB_MWF_RY: w->restore_rect.y = v; break;
        case GMB_MWF_RW: w->restore_rect.w = v; break;
        case GMB_MWF_RH: w->restore_rect.h = v; break;
        case GMB_MWF_EDITABLE: w->editable = v != 0; break;
        case GMB_MWF_BODY_COLOR: w->body_color = (unsigned int)v; break;
        case GMB_MWF_FG_COLOR: w->fg_color = (unsigned int)v; break;
        case GMB_MWF_TITLE_COLOR: w->title_color = (unsigned int)v; break;
        case GMB_MWF_PIXEL: w->pixel = v != 0; break;
        case GMB_MWF_RADIUS: w->radius = v; break;
        case GMB_MWF_RESIZABLE: w->resizable = v != 0; break;
    }
}
/* field 0 = title, 1 = name */
const char* gmb_managed_window_get_str(const gmb_managed_window_t* w, int field)
{ if (!w) return ""; return field == 1 ? w->name : w->title; }
void gmb_managed_window_set_str(gmb_managed_window_t* w, int field, const char* v)
{
    if (!w) return;
    if (field == 1) snprintf(w->name, sizeof(w->name), "%s", v ? v : "");
    else snprintf(w->title, sizeof(w->title), "%s", v ? v : "");
}
int gmb_managed_window_line_at(const gmb_managed_window_t* w, int index)
{ (void)w; (void)index; return 0; }
const char* gmb_managed_window_get_line(const gmb_managed_window_t* w, int index)
{ if (!w || index < 0 || index >= w->lines_count) return ""; return w->lines[index]; }

/* ---- Editing righe del ManagedWindow (parità List<string>.Lines usata dal backend) ---- */
int gmb_managed_window_lines_count(const gmb_managed_window_t* w) { return w ? w->lines_count : 0; }
const char* gmb_managed_window_line_get(const gmb_managed_window_t* w, int i)
{ if (!w || i < 0 || i >= w->lines_count) return ""; return w->lines[i]; }
void gmb_managed_window_line_set(gmb_managed_window_t* w, int i, const char* s)
{ if (!w || i < 0 || i >= w->lines_count) return; snprintf(w->lines[i], 256, "%s", s ? s : ""); }
void gmb_managed_window_line_insert(gmb_managed_window_t* w, int i, const char* s)
{
    if (!w || w->lines_count >= 256) return;
    if (i < 0) i = 0; if (i > w->lines_count) i = w->lines_count;
    for (int k = w->lines_count; k > i; k--) memcpy(w->lines[k], w->lines[k - 1], 256);
    snprintf(w->lines[i], 256, "%s", s ? s : "");
    w->lines_count++;
}
void gmb_managed_window_line_remove(gmb_managed_window_t* w, int i)
{
    if (!w || i < 0 || i >= w->lines_count) return;
    for (int k = i; k < w->lines_count - 1; k++) memcpy(w->lines[k], w->lines[k + 1], 256);
    w->lines_count--;
}
void gmb_managed_window_lines_clear(gmb_managed_window_t* w)
{ if (w) { w->lines_count = 0; w->scroll_offset = 0; w->cursor_line = 0; w->cursor_col = 0; } }
