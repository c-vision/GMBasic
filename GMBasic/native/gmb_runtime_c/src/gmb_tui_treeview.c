/* ============================================================================
 * gmb_tui_treeview.c — TreeView (porting del runtime C, specchio ClipperLib).
 *
 * MODELLO UNICO: tree view = gmb_widget_t. Nodi in `items`, profondita' in
 * `tdepth`, stato (figli/espanso) nei bit di `tnode_flags`, scelta in `ival`,
 * prima riga visibile in `first`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== TreeView ===================== */
#define TV_SEL(t)          ((int)((gmb_widget_t*)(t))->ival)
#define TV_SET_SEL(t, v)   (((gmb_widget_t*)(t))->ival = (double)(v))
#define TV_HAS_CHILD(t, i) (((gmb_widget_t*)(t))->tnode_flags[i] & 1)
#define TV_EXPANDED(t, i)  (((gmb_widget_t*)(t))->tnode_flags[i] & 2)
#define TV_SET_FLAG(t, i, on, bit) do { if (on) ((gmb_widget_t*)(t))->tnode_flags[i] |= (bit); \
                                          else ((gmb_widget_t*)(t))->tnode_flags[i] &= (unsigned char)~(bit); } while (0)

static void treeview_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    if (c->focused) { sf = bg; sb = fg; } else { sf = fg; sb = bg; }
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    for (int i = 0; i < c->height; i++)
    {
        int vi = t->first + i;
        if (vi >= vn) break;
        char line[256];
        gmb_tree_view_render_line(t, vi, line, (int)sizeof(line));
        if (vis[vi] == TV_SEL(t)) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool treeview_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    int sel = TV_SEL(t);
    switch (key)
    {
        case GMB_K_UP:
            if (sel > 0)
            {
                int prev_vis = -1;
                for (int i = 0; i < vn; i++) if (vis[i] == sel) { prev_vis = i - 1; break; }
                if (prev_vis >= 0) TV_SET_SEL(t, vis[prev_vis]);
                if (TV_SEL(t) < t->first) t->first--;
            }
            return true;
        case GMB_K_DOWN:
            if (sel < t->nitems - 1)
            {
                int next_vis = -1;
                for (int i = 0; i < vn; i++) if (vis[i] == sel) { next_vis = i + 1; break; }
                if (next_vis < vn) TV_SET_SEL(t, vis[next_vis]);
                if (TV_SEL(t) >= t->first + c->height) t->first++;
            }
            return true;
        case GMB_K_RIGHT:
            if (sel >= 0 && sel < t->nitems)
            {
                if (TV_HAS_CHILD(t, sel) && !TV_EXPANDED(t, sel)) TV_SET_FLAG(t, sel, 1, 2);
            }
            return true;
        case GMB_K_LEFT:
            if (sel >= 0 && sel < t->nitems)
            {
                if (TV_HAS_CHILD(t, sel) && TV_EXPANDED(t, sel)) TV_SET_FLAG(t, sel, 0, 2);
            }
            return true;
        case GMB_K_HOME:
            if (vn > 0) TV_SET_SEL(t, vis[0]);
            t->first = 0;
            return true;
        case GMB_K_END:
            if (vn > 0) TV_SET_SEL(t, vis[vn - 1]);
            t->first = vn - c->height > 0 ? vn - c->height : 0;
            return true;
    }
    return false;
}
static bool treeview_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int vi = (y - c->y) + t->first;
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    if (vi >= 0 && vi < vn)
    {
        int ni = vis[vi];
        TV_SET_SEL(t, ni);
        if (TV_HAS_CHILD(t, ni)) TV_SET_FLAG(t, ni, !TV_EXPANDED(t, ni), 2);
    }
    return true;
}
static void treeview_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    static char _buf[64][128]; static const char* _it[64]; int n = 0;
    for (int i = 0; i < t->nitems && n < 64; i++) { int ind = t->tdepth[i]; if (ind > 12) ind = 12; snprintf(_buf[n], 128, "%*s%s", ind * 2, "", t->items[i]); _it[n] = _buf[n]; n++; }
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, _it, n, TV_SEL(t), 0, 0, NULL, t->first, NULL, 0);
}
void gmb_tree_view_init(gmb_tree_view_t* tv, int x, int y, int w, int h)
{
    gmb_widget_t* t = (gmb_widget_t*)tv;
    memset(t, 0, sizeof(*t));
    gmb_control_init(&t->base, "", x, y, w, h, treeview_draw, treeview_key);
    t->base.draw_px_fn = treeview_draw_px;
    t->base.ctrl_kind = GMB_CK_TREE;
    t->base.handle_click_fn = treeview_click;
    t->base.user = t;
    t->kind = W_TREE;
}
int gmb_tree_view_add_node(gmb_tree_view_t* tv, const char* label, int depth, bool has_children)
{
    gmb_widget_t* t = (gmb_widget_t*)tv;
    if (t->nitems >= 64) return -1;
    int i = t->nitems++;
    snprintf(t->items[i], sizeof(t->items[i]), "%s", label ? label : "");
    t->tdepth[i] = depth;
    TV_SET_FLAG(t, i, has_children, 1);
    TV_SET_FLAG(t, i, 1, 2);        /* espanso di default */
    return i;
}
int gmb_tree_view_visible_indices(gmb_tree_view_t* tv, int* out, int cap)
{
    gmb_widget_t* t = (gmb_widget_t*)tv;
    int n = 0;
    for (int i = 0; i < t->nitems && n < cap; i++)
    {
        out[n++] = i;
        if (!TV_EXPANDED(t, i) && TV_HAS_CHILD(t, i))
        {
            int skip = 1;
            while (i + skip < t->nitems && t->tdepth[i + skip] > t->tdepth[i])
                skip++;
            i += skip - 1;
        }
    }
    return n;
}
int gmb_tree_view_render_line(gmb_tree_view_t* tv, int vi, char* out, int cap)
{
    gmb_widget_t* t = (gmb_widget_t*)tv;
    int vis[64];
    int vn = gmb_tree_view_visible_indices(t, vis, 64);
    if (vi >= vn)
    {
        for (int i = 0; i < t->base.width && i < cap - 1; i++) out[i] = ' ';
        out[t->base.width < cap - 1 ? t->base.width : cap - 1] = '\0';
        return t->base.width;
    }
    int ni = vis[vi];
    int o = 0;
    if (ni == TV_SEL(t)) out[o++] = '>';
    else out[o++] = ' ';
    for (int i = 0; i < t->tdepth[ni] * 2; i++) out[o++] = ' ';
    if (TV_HAS_CHILD(t, ni))
    {
        const char* e = TV_EXPANDED(t, ni) ? "\xE2\x88\x92 " : "+ ";
        for (const char* p = e; *p && o < cap - 1; p++) out[o++] = *p;
    }
    else
    {
        out[o++] = ' '; out[o++] = ' ';
    }
    for (const char* p = t->items[ni]; *p && o < cap - 1; p++) out[o++] = *p;
    for (int i = o; i < t->base.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
