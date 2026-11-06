/* ============================================================================
 * gmb_tui_control.c — gmb_tui_control. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Window (radice) ===================== */
/* Radice di default: il backend attivo decide il kind. Transitorio: derivato da
 * gmb_gfx_screen_on(); in futuro la console/screen attiva setta il kind. */
static gmb_base_window_t g_root_window = { GMB_WIN_STANDARD, 1, NULL };  /* handle 1 = finestra principale */
gmb_base_window_t* gmb_base_window_root(void) { return &g_root_window; }
void gmb_base_window_set_kind(gmb_window_kind_t kind) { g_root_window.kind = kind; }
void gmb_base_window_set_handle(int handle) { g_root_window.handle = handle; }
gmb_window_kind_t gmb_base_window_kind(void)
{
    /* Kind deciso dal backend attivo (che chiama gmb_base_window_set_kind), non da un flag. */
    return g_root_window.kind;
}

/* ===================== Control ===================== */

void gmb_control_init(gmb_control_t* c, const char* id, int x, int y, int w, int h,
                      void (*draw)(void*, gmb_control_t*, gmb_screen_buffer_t*),
                      bool (*key)(void*, gmb_control_t*, int))
{
    memset(c, 0, sizeof(*c));
    memset(c, 0, sizeof(*c));
    if (id) snprintf(c->id, sizeof(c->id), "%s", id);
    c->x = x; c->y = y; c->width = w; c->height = h;
    c->visible = true;
    c->enabled = true;
    c->tab_stop = true;
    c->style = gmb_control_style_global_default();
    c->window = gmb_base_window_root();   /* parent fino alla radice */
    c->window_handle = g_root_window.handle;   /* [nascosta] cachata alla creazione */
    c->window_kind = gmb_base_window_kind();   /* [nascosta] cachata alla creazione */
    c->draw_fn = draw;
    c->handle_key_fn = key;
}

gmb_rect_t gmb_control_rect(const gmb_control_t* c)
{
    gmb_rect_t r = { c->x, c->y, c->width, c->height > 1 ? c->height : 1 };
    return r;
}

void gmb_control_draw(gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    /* Il render e' deciso dal kind cachato alla creazione (nessuna risalita del parent). */
    if (c->draw_px_fn && c->window_kind == GMB_WIN_GRAPHICS) { c->draw_px_fn(c->user, c); return; }
    GMB_GFX_SURFACE_AUTOATTACH(buf);
    if (c->draw_fn) c->draw_fn(c->user, c, buf);
}

bool gmb_control_handle_key(gmb_control_t* c, int key)
{
    return c->handle_key_fn ? c->handle_key_fn(c->user, c, key) : false;
}

bool gmb_control_handle_mouse(gmb_control_t* c, const gmb_mouse_action_t* a)
{
    /* handle_mouse_fn gestisce il mouse "ricco" (hover/drag); se DECLINA (false)
     * si prova comunque handle_click_fn per CLICK/DBLCLICK — altrimenti un handler
     * di solo hover inghiottirebbe i click. */
    if (c->handle_mouse_fn && c->handle_mouse_fn(c->user, c, a)) return true;
    if (c->handle_click_fn && (a->kind == GMB_MA_CLICK || a->kind == GMB_MA_DOUBLECLICK))
        return c->handle_click_fn(c->user, c, a->x, a->y);
    return false;
}

bool gmb_control_handle_input(gmb_control_t* c, int key)
{
    if (key < 0)
    {
        /* mouse inkey */
        gmb_mouse_action_t ma;
        if (gmb_mouse_action_from_inkey_stub(key, &ma))
            return gmb_control_handle_mouse(c, &ma);
        return false;
    }
    return gmb_control_handle_key(c, key);
}

int gmb_control_apply_case(gmb_control_t* c, const char* s, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int n = 0;
    for (const char* p = s; *p && n < cap - 1; p++)
    {
        char ch = *p;
        if (c->uppercase && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        out[n++] = ch;
    }
    out[n] = '\0';
    return n;
}

/* ===================== Damage / Delta ===================== */

gmb_delta_t gmb_delta_unchanged(void)
{
    gmb_delta_t d = { 0, 0 };
    return d;
}
bool gmb_delta_is_empty(const gmb_delta_t* d) { return d->start >= d->end; }
void gmb_delta_add(gmb_delta_t* d, int i)
{
    if (gmb_delta_is_empty(d)) { d->start = i; d->end = i + 1; }
    else
    {
        if (i < d->start) d->start = i;
        if (i + 1 > d->end) d->end = i + 1;
    }
}
void gmb_delta_add_range(gmb_delta_t* d, int start, int end)
{
    if (start >= end) return;
    if (gmb_delta_is_empty(d)) { d->start = start; d->end = end; }
    else
    {
        if (start < d->start) d->start = start;
        if (end > d->end) d->end = end;
    }
}
bool gmb_delta_contains(const gmb_delta_t* d, int i)
{
    return !gmb_delta_is_empty(d) && d->start <= i && i < d->end;
}
void gmb_delta_to_range(const gmb_delta_t* d, int* start, int* end)
{
    if (gmb_delta_is_empty(d)) { *start = 0; *end = 0; }
    else { *start = d->start; *end = d->end; }
}
void gmb_delta_clear(gmb_delta_t* d) { d->start = 0; d->end = 0; }
void gmb_delta_set_all(gmb_delta_t* d, int len) { d->start = 0; d->end = len; }

void gmb_damage_tracker_init(gmb_damage_tracker_t* t, int height)
{
    memset(t, 0, sizeof(*t));
    t->height = height;
    t->start = 0;
    t->end = 0;
}
void gmb_damage_tracker_resize(gmb_damage_tracker_t* t, int height)
{
    t->height = height;
    t->start = 0;
    t->end = 0;
}
void gmb_damage_tracker_mark(gmb_damage_tracker_t* t, int row)
{
    if (row < 0 || row >= t->height) return;
    if (t->start >= t->end) { t->start = row; t->end = row + 1; }
    else
    {
        if (row < t->start) t->start = row;
        if (row + 1 > t->end) t->end = row + 1;
    }
}
void gmb_damage_tracker_mark_range(gmb_damage_tracker_t* t, int start, int end)
{
    int e = end < t->height ? end : t->height;
    if (start >= e) return;
    if (t->start >= t->end) { t->start = start; t->end = e; }
    else
    {
        if (start < t->start) t->start = start;
        if (e > t->end) t->end = e;
    }
}
void gmb_damage_tracker_mark_all(gmb_damage_tracker_t* t) { t->start = 0; t->end = t->height; }
void gmb_damage_tracker_clear(gmb_damage_tracker_t* t) { t->start = 0; t->end = 0; }
bool gmb_damage_tracker_is_dirty(const gmb_damage_tracker_t* t) { return t->start < t->end; }
void gmb_damage_tracker_dirty_rows(const gmb_damage_tracker_t* t, int* start, int* end)
{
    *start = t->start;
    *end = t->end;
}
