/* ============================================================================
 * gmb_tui_tilingmodel.c — gmb_tui_tilingmodel. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== TilingModel ===================== */

void gmb_tiling_model_init(gmb_tiling_model_t* m)
{
    memset(m, 0, sizeof(*m));
    memset(m, 0, sizeof(*m));
    m->node_count = 1;
    m->root = 0;
    m->nodes[0].id = 0;
    m->nodes[0].split = -1;
}
int gmb_tiling_model_split_leaf(gmb_tiling_model_t* m, int id, int direction, double ratio)
{
    if (ratio < 0.05) ratio = 0.05;
    if (ratio > 0.95) ratio = 0.95;
    gmb_tiling_node_t* node = NULL;
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == id) { node = &m->nodes[i]; break; }
    if (!node || node->removed || node->split >= 0) return -1;
    gmb_rect_t old_rect = node->rect;
    bool old_view = node->has_view;
    int old_view_val = node->view;
    node->split = direction;
    node->ratio = ratio;
    node->offset = 0;
    node->has_view = false;
    if (m->node_count + 2 > GMB_TILING_MAX_NODES) return -1;
    int c0 = m->node_count++;
    m->nodes[c0].id = c0;
    m->nodes[c0].rect = old_rect;
    m->nodes[c0].split = -1;
    m->nodes[c0].has_view = old_view;
    m->nodes[c0].view = old_view_val;
    int c1 = m->node_count++;
    m->nodes[c1].id = c1;
    m->nodes[c1].rect = old_rect;
    m->nodes[c1].split = -1;
    node->child0 = c0;
    node->child1 = c1;
    node->has_child0 = true;
    node->has_child1 = true;
    return c1;
}
static int tiling_parent_of(gmb_tiling_model_t* m, int id)
{
    for (int i = 0; i < m->node_count; i++)
        if (!m->nodes[i].removed && ((m->nodes[i].has_child0 && m->nodes[i].child0 == id) || (m->nodes[i].has_child1 && m->nodes[i].child1 == id)))
            return m->nodes[i].id;
    return -1;
}
bool gmb_tiling_model_remove(gmb_tiling_model_t* m, int id)
{
    int parent = tiling_parent_of(m, id);
    if (parent < 0) return false;
    gmb_tiling_node_t* n = NULL;
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == parent) { n = &m->nodes[i]; break; }
    if (!n) return false;
    int sib = (n->has_child0 && n->child0 == id) ? n->child1 : n->child0;
    int gp = tiling_parent_of(m, parent);
    if (gp >= 0)
    {
        gmb_tiling_node_t* gpn = NULL;
        for (int i = 0; i < m->node_count; i++)
            if (m->nodes[i].id == gp) { gpn = &m->nodes[i]; break; }
        if (gpn)
        {
            if (gpn->has_child0 && gpn->child0 == parent) gpn->child0 = sib;
            else if (gpn->has_child1 && gpn->child1 == parent) gpn->child1 = sib;
        }
    }
    else
        m->root = sib;
    n->removed = true;
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == id) { m->nodes[i].removed = true; break; }
    return true;
}
void gmb_tiling_model_set_view(gmb_tiling_model_t* m, int id, int view)
{
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == id) { m->nodes[i].has_view = true; m->nodes[i].view = view; break; }
}
int gmb_tiling_model_view_of(gmb_tiling_model_t* m, int id)
{
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == id) return m->nodes[i].has_view ? m->nodes[i].view : -1;
    return -1;
}
double gmb_tiling_model_ratio_of(gmb_tiling_model_t* m, int id)
{
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == id) return m->nodes[i].ratio;
    return 0.5;
}
void gmb_tiling_model_set_ratio(gmb_tiling_model_t* m, int id, double ratio)
{
    if (ratio < 0.05) ratio = 0.05;
    if (ratio > 0.95) ratio = 0.95;
    int p = tiling_parent_of(m, id);
    if (p >= 0)
        for (int i = 0; i < m->node_count; i++)
            if (m->nodes[i].id == p) { m->nodes[i].ratio = ratio; break; }
}
void gmb_tiling_model_move_offset(gmb_tiling_model_t* m, int id, int delta)
{
    int p = tiling_parent_of(m, id);
    if (p < 0) return;
    gmb_tiling_node_t* node = NULL;
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == p) { node = &m->nodes[i]; break; }
    if (!node || node->split < 0) return;
    int max_off = (node->split == GMB_SPLIT_HORIZONTAL)
        ? (int)(node->rect.w * (1.0 - node->ratio)) - 1
        : (int)(node->rect.h * (1.0 - node->ratio)) - 1;
    node->offset += delta;
    if (node->offset < -max_off) node->offset = -max_off;
    if (node->offset > max_off) node->offset = max_off;
}
static void tiling_layout_rec(gmb_tiling_model_t* m, int id, gmb_rect_t area, int* ids, gmb_rect_t* rects, int cap, int* n)
{
    if (*n >= cap) return;
    gmb_tiling_node_t* node = NULL;
    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].id == id) { node = &m->nodes[i]; break; }
    if (!node || node->removed) return;
    if (node->split < 0)
    {
        ids[*n] = id;
        rects[*n] = area;
        (*n)++;
        return;
    }
    if (!node->has_child0 || !node->has_child1) return;
    int c0 = node->child0, c1 = node->child1;
    int off = node->offset;
    if (node->split == GMB_SPLIT_HORIZONTAL)
    {
        int lw = (int)(area.w * node->ratio) + off;
        if (lw < 1) lw = 1;
        if (lw > area.w - 1) lw = area.w - 1;
        int rw = area.w - lw;
        if (rw < 1) rw = 1;
        gmb_rect_t r0 = { area.x, area.y, lw, area.h };
        gmb_rect_t r1 = { area.x + lw, area.y, rw, area.h };
        tiling_layout_rec(m, c0, r0, ids, rects, cap, n);
        tiling_layout_rec(m, c1, r1, ids, rects, cap, n);
    }
    else
    {
        int th = (int)(area.h * node->ratio) + off;
        if (th < 1) th = 1;
        if (th > area.h - 1) th = area.h - 1;
        int bh = area.h - th;
        if (bh < 1) bh = 1;
        gmb_rect_t r0 = { area.x, area.y, area.w, th };
        gmb_rect_t r1 = { area.x, area.y + th, area.w, bh };
        tiling_layout_rec(m, c0, r0, ids, rects, cap, n);
        tiling_layout_rec(m, c1, r1, ids, rects, cap, n);
    }
}
int gmb_tiling_model_layout(gmb_tiling_model_t* m, gmb_rect_t area, int* out_ids, gmb_rect_t* out_rects, int cap)
{
    int n = 0;
    tiling_layout_rec(m, m->root, area, out_ids, out_rects, cap, &n);
    return n;
}
