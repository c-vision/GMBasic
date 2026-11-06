/* ============================================================================
 * gmb_tui_tiling.c — gmb_tui_tiling. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Tiling ===================== */

int gmb_tiling_orientation_flip(int o) { return o == GMB_TILING_H ? GMB_TILING_V : GMB_TILING_H; }
int gmb_tiling_direction_opposite(int d)
{
    switch (d)
    {
        case GMB_DIR_LEFT: return GMB_DIR_RIGHT;
        case GMB_DIR_RIGHT: return GMB_DIR_LEFT;
        case GMB_DIR_ABOVE: return GMB_DIR_BELOW;
        case GMB_DIR_BELOW: return GMB_DIR_ABOVE;
        default: return d;
    }
}

void gmb_tiling_stack_init(gmb_tiling_stack_t* s)
{
    memset(s, 0, sizeof(*s));
    memset(s, 0, sizeof(*s));
    s->focused = -1;
}
int gmb_tiling_stack_add_window(gmb_tiling_stack_t* s, gmb_window_id_t w, bool focus)
{
    if (s->windows_count >= GMB_STACK_MAX) return -1;
    s->windows[s->windows_count].id = w.id;
    int idx = s->windows_count++;
    if (focus || s->focused < 0) s->focused = idx;
    return idx;
}
bool gmb_tiling_stack_remove_window(gmb_tiling_stack_t* s, gmb_window_id_t w)
{
    int pos = -1;
    for (int i = 0; i < s->windows_count; i++)
        if (s->windows[i].id == w.id) { pos = i; break; }
    if (pos < 0) return false;
    for (int i = pos; i < s->windows_count - 1; i++) s->windows[i] = s->windows[i + 1];
    s->windows_count--;
    if (s->windows_count == 0) { s->focused = -1; return true; }
    int new_focus = pos < s->windows_count - 1 ? pos : s->windows_count - 1;
    s->focused = new_focus;
    return true;
}
int gmb_tiling_stack_cycle_focus(gmb_tiling_stack_t* s, int dir)
{
    if (s->windows_count == 0) return -1;
    int cur = s->focused >= 0 ? s->focused : 0;
    int next = ((cur + dir) % s->windows_count + s->windows_count) % s->windows_count;
    s->focused = next;
    return next;
}
int gmb_tiling_stack_insert_after(gmb_tiling_stack_t* s, gmb_window_id_t w, gmb_window_id_t after, bool focus)
{
    if (s->windows_count >= GMB_STACK_MAX) return -1;
    int pos = -1;
    for (int i = 0; i < s->windows_count; i++)
        if (s->windows[i].id == after.id) { pos = i; break; }
    pos = pos < 0 ? 0 : pos + 1;
    for (int i = s->windows_count; i > pos; i--) s->windows[i] = s->windows[i - 1];
    s->windows[pos].id = w.id;
    s->windows_count++;
    if (focus) s->focused = pos;
    return pos;
}
bool gmb_tiling_stack_contains(gmb_tiling_stack_t* s, gmb_window_id_t w)
{
    for (int i = 0; i < s->windows_count; i++)
        if (s->windows[i].id == w.id) return true;
    return false;
}
int gmb_tiling_stack_focused_index(gmb_tiling_stack_t* s) { return s->focused; }
gmb_window_id_t gmb_tiling_stack_focused_window(gmb_tiling_stack_t* s)
{
    gmb_window_id_t w = { 0 };
    if (s->focused >= 0 && s->focused < s->windows_count) w.id = s->windows[s->focused].id;
    return w;
}

static float tiling_clamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void gmb_tiling_split_rect(gmb_rect_t rect, int orientation, float ratio, gmb_rect_t* first, gmb_rect_t* second)
{
    ratio = tiling_clamp(ratio, 0.05f, 0.95f);
    if (orientation == GMB_TILING_H)
    {
        int w0 = (int)(rect.w * ratio);
        if (w0 < 1) w0 = 1;
        if (w0 > rect.w - 1) w0 = rect.w - 1;
        first->x = rect.x; first->y = rect.y; first->w = w0; first->h = rect.h;
        second->x = rect.x + w0; second->y = rect.y; second->w = rect.w - w0; second->h = rect.h;
    }
    else
    {
        int h0 = (int)(rect.h * ratio);
        if (h0 < 1) h0 = 1;
        if (h0 > rect.h - 1) h0 = rect.h - 1;
        first->x = rect.x; first->y = rect.y; first->w = rect.w; first->h = h0;
        second->x = rect.x; second->y = rect.y + h0; second->w = rect.w; second->h = rect.h - h0;
    }
}

void gmb_tiler_init(gmb_tiler_t* t)
{
    memset(t, 0, sizeof(*t));
    memset(t, 0, sizeof(*t));
}
void gmb_tiler_set_work_area(gmb_tiler_t* t, gmb_rect_t r) { t->work_area = r; }
int gmb_tiler_count(gmb_tiler_t* t) { return t->windows_count; }
bool gmb_tiler_is_empty(gmb_tiler_t* t) { return t->windows_count == 0; }
bool gmb_tiler_contains(gmb_tiler_t* t, gmb_window_id_t w)
{
    for (int i = 0; i < t->windows_count; i++)
        if (t->windows[i].id == w.id) return true;
    return false;
}
bool gmb_tiler_add(gmb_tiler_t* t, gmb_window_id_t w)
{
    if (t->windows_count >= GMB_STACK_MAX) return false;
    if (gmb_tiler_contains(t, w)) return false;
    t->windows[t->windows_count++].id = w.id;
    return true;
}
bool gmb_tiler_remove(gmb_tiler_t* t, gmb_window_id_t w)
{
    int pos = -1;
    for (int i = 0; i < t->windows_count; i++)
        if (t->windows[i].id == w.id) { pos = i; break; }
    if (pos < 0) return false;
    for (int i = pos; i < t->windows_count - 1; i++) t->windows[i] = t->windows[i + 1];
    t->windows_count--;
    return true;
}
