/* ============================================================================
 * gmb_tui_treeview.c — gmb_tui_treeview. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== TreeView ===================== */

static void treeview_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_tree_view_t* t = (gmb_tree_view_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    if (c->focused) { sf = bg; sb = fg; } else { sf = fg; sb = bg; }
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    for (int i = 0; i < c->height; i++)
    {
        int vi = t->scroll_offset + i;
        if (vi >= vn) break;
        char line[256];
        gmb_tree_view_render_line(t, vi, line, (int)sizeof(line));
        if (vis[vi] == t->selected_index) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool treeview_key(void* u, gmb_control_t* c, int key)
{
    gmb_tree_view_t* t = (gmb_tree_view_t*)u;
    if (!c->enabled) return false;
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    switch (key)
    {
        case GMB_K_UP:
            if (t->selected_index > 0)
            {
                int prev_vis = -1;
                for (int i = 0; i < vn; i++) if (vis[i] == t->selected_index) { prev_vis = i - 1; break; }
                if (prev_vis >= 0) t->selected_index = vis[prev_vis];
                if (t->selected_index < t->scroll_offset) t->scroll_offset--;
            }
            return true;
        case GMB_K_DOWN:
            if (t->selected_index < t->nodes_count - 1)
            {
                int next_vis = -1;
                for (int i = 0; i < vn; i++) if (vis[i] == t->selected_index) { next_vis = i + 1; break; }
                if (next_vis < vn) t->selected_index = vis[next_vis];
                if (t->selected_index >= t->scroll_offset + c->height) t->scroll_offset++;
            }
            return true;
        case GMB_K_RIGHT:
            if (t->selected_index >= 0 && t->selected_index < t->nodes_count)
            {
                gmb_tree_node_t* n = &t->nodes[t->selected_index];
                if (n->has_children && !n->expanded) n->expanded = true;
            }
            return true;
        case GMB_K_LEFT:
            if (t->selected_index >= 0 && t->selected_index < t->nodes_count)
            {
                gmb_tree_node_t* n = &t->nodes[t->selected_index];
                if (n->has_children && n->expanded) n->expanded = false;
            }
            return true;
        case GMB_K_HOME:
            if (vn > 0) t->selected_index = vis[0];
            t->scroll_offset = 0;
            return true;
        case GMB_K_END:
            if (vn > 0) t->selected_index = vis[vn - 1];
            t->scroll_offset = vn - c->height > 0 ? vn - c->height : 0;
            return true;
    }
    return false;
}
static bool treeview_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_tree_view_t* t = (gmb_tree_view_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int vi = (y - c->y) + t->scroll_offset;
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    if (vi >= 0 && vi < vn)
    {
        int ni = vis[vi];
        t->selected_index = ni;
        gmb_tree_node_t* n = &t->nodes[ni];
        if (n->has_children) n->expanded = !n->expanded;
    }
    return true;
}
static void treeview_draw_px(void* u, gmb_control_t* c)
{
    gmb_tree_view_t* t = (gmb_tree_view_t*)u;
    static char _buf[32][128]; static const char* _it[32]; int n = 0;
    for (int i = 0; i < t->nodes_count && n < 32; i++) { int ind = t->nodes[i].depth; if (ind > 12) ind = 12; snprintf(_buf[n], 128, "%*s%s", ind * 2, "", t->nodes[i].label); _it[n] = _buf[n]; n++; }
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, _it, n, t->selected_index, 0, 0, NULL, t->scroll_offset, NULL, 0);
}
void gmb_tree_view_init(gmb_tree_view_t* t, int x, int y, int w, int h)
{
    memset(t, 0, sizeof(*t));
    gmb_control_init(&t->ctrl, "", x, y, w, h, treeview_draw, treeview_key);
    t->ctrl.draw_px_fn = treeview_draw_px;
    t->ctrl.ctrl_kind = GMB_CK_TREE;
    t->ctrl.handle_click_fn = treeview_click;
    t->ctrl.user = t;
}
gmb_tree_node_t* gmb_tree_view_add_node(gmb_tree_view_t* t, const char* label, int depth, bool has_children)
{
    if (t->nodes_count >= 64) return NULL;
    gmb_tree_node_t* n = &t->nodes[t->nodes_count++];
    snprintf(n->label, sizeof(n->label), "%s", label ? label : "");
    n->depth = depth;
    n->expanded = true;
    n->has_children = has_children;
    return n;
}
int gmb_tree_view_visible_indices(gmb_tree_view_t* t, int* out, int cap)
{
    int n = 0;
    for (int i = 0; i < t->nodes_count && n < cap; i++)
    {
        out[n++] = i;
        if (!t->nodes[i].expanded && t->nodes[i].has_children)
        {
            int skip = 1;
            while (i + skip < t->nodes_count && t->nodes[i + skip].depth > t->nodes[i].depth)
                skip++;
            i += skip - 1;
        }
    }
    return n;
}
int gmb_tree_view_render_line(gmb_tree_view_t* t, int vi, char* out, int cap)
{
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    if (vi >= vn)
    {
        for (int i = 0; i < t->ctrl.width && i < cap - 1; i++) out[i] = ' ';
        out[t->ctrl.width < cap - 1 ? t->ctrl.width : cap - 1] = '\0';
        return t->ctrl.width;
    }
    int ni = vis[vi];
    gmb_tree_node_t* n = &t->nodes[ni];
    int o = 0;
    if (ni == t->selected_index) out[o++] = '>';
    else out[o++] = ' ';
    for (int i = 0; i < n->depth * 2; i++) out[o++] = ' ';
    if (n->has_children)
    {
        const char* e = n->expanded ? "\xE2\x88\x92 " : "+ ";
        for (const char* p = e; *p && o < cap - 1; p++) out[o++] = *p;
    }
    else
    {
        out[o++] = ' '; out[o++] = ' ';
    }
    for (const char* p = n->label; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = o; i < t->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
