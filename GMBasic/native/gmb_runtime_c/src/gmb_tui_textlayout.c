/* ============================================================================
 * gmb_tui_textlayout.c — TextLayout.cs (specchio ClipperLib/Controls).
 * Item: box / glue / penalty e layout Knuth-Plass (naturale).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gmb_layout_item_t gmb_layout_box(float width)
{
    gmb_layout_item_t it;
    memset(&it, 0, sizeof(it));
    it.type = GMB_ITEM_BOX;
    it.width = width;
    return it;
}
gmb_layout_item_t gmb_layout_glue(float width, float stretch, float shrink)
{
    gmb_layout_item_t it;
    memset(&it, 0, sizeof(it));
    it.type = GMB_ITEM_GLUE;
    it.width = width;
    it.stretch = stretch;
    it.shrink = shrink;
    return it;
}
gmb_layout_item_t gmb_layout_penalty(float width, float cost, bool flagged)
{
    gmb_layout_item_t it;
    memset(&it, 0, sizeof(it));
    it.type = GMB_ITEM_PENALTY;
    it.width = width;
    it.cost = cost;
    it.flagged = flagged;
    return it;
}
gmb_layout_item_t gmb_layout_forced_break(void)
{
    gmb_layout_item_t it;
    memset(&it, 0, sizeof(it));
    it.type = GMB_ITEM_PENALTY;
    it.cost = -1e30f;
    it.flagged = true;
    return it;
}
float gmb_layout_natural_width(const gmb_layout_item_t* items, int count)
{
    float w = 0.0f;
    for (int i = 0; i < count; i++)
    {
        if (items[i].type == GMB_ITEM_PENALTY)
        {
            if (items[i].cost < 0 && items[i].flagged) return w;
        }
        w += items[i].width;
    }
    return w;
}
