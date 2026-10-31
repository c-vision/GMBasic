/* ============================================================================
 * gmb_tui_flex.c — motore Flexbox (porting ClipperLib/Clipper FlexLayout.cs).
 * ============================================================================ */
#include "gmb_tui_flex.h"
#include <stdlib.h>
#include <string.h>

#define GMB_FLEX_MAX_AXES 64
#define GMB_FLEX_MAX_ITEMS 128

void gmb_flex_item_init(gmb_flex_item_t* it)
{
    memset(it, 0, sizeof(*it));
    it->shrink = 1.0f;
    it->max = 2147483647;
}

void gmb_flex_item_set_size(gmb_flex_item_t* it, int w, int h, float grow)
{
    it->size_w = w;
    it->size_h = h;
    it->grow = grow;
}

static int flex_main_basis(gmb_flex_item_t* it, int dir)
{
    if (it->has_basis) return it->basis;
    return dir == GMB_FLEX_ROW ? it->size_w : it->size_h;
}

static int flex_cross_basis(gmb_flex_item_t* it, int dir)
{
    return dir == GMB_FLEX_ROW ? it->size_h : it->size_w;
}

int gmb_flex_generate(gmb_rect_t container, gmb_flex_item_t* items, int items_count,
                      int dir, int wrap, int justify, int alignItems, int alignContent)
{
    if (items_count <= 0 || !items) return 0;
    int main_size = dir == GMB_FLEX_ROW ? container.w : container.h;
    int container_cross = dir == GMB_FLEX_ROW ? container.h : container.w;

    /* split in assi */
    int axis_of[GMB_FLEX_MAX_ITEMS];
    int axes[GMB_FLEX_MAX_AXES][GMB_FLEX_MAX_ITEMS];
    int axis_len[GMB_FLEX_MAX_AXES];
    int axis_count = 0;

    if (wrap == GMB_FLEX_NOWRAP)
    {
        axis_count = 1;
        axis_len[0] = items_count;
        for (int i = 0; i < items_count; i++) axes[0][i] = i;
    }
    else
    {
        int cur[GMB_FLEX_MAX_ITEMS];
        int cur_len = 0;
        int used = 0;
        for (int i = 0; i < items_count; i++)
        {
            int need = flex_main_basis(&items[i], dir) + (cur_len == 0 ? 0 : 1);
            if (cur_len > 0 && used + need > main_size)
            {
                for (int j = 0; j < cur_len; j++) axes[axis_count][j] = cur[j];
                axis_len[axis_count] = cur_len;
                axis_count++;
                cur_len = 0;
                used = 0;
            }
            used += need;
            cur[cur_len++] = i;
        }
        if (cur_len > 0)
        {
            for (int j = 0; j < cur_len; j++) axes[axis_count][j] = cur[j];
            axis_len[axis_count] = cur_len;
            axis_count++;
        }
    }

    /* layout locale per asse */
    int local_x[GMB_FLEX_MAX_ITEMS], local_y[GMB_FLEX_MAX_ITEMS];
    int local_w[GMB_FLEX_MAX_ITEMS], local_h[GMB_FLEX_MAX_ITEMS];
    int item_axis[GMB_FLEX_MAX_ITEMS];
    int axis_cross[GMB_FLEX_MAX_AXES];

    for (int a = 0; a < axis_count; a++)
    {
        int len = axis_len[a];
        float combined_grow = 0;
        for (int k = 0; k < len; k++) combined_grow += items[axes[a][k]].grow;
        int cross_size = 0;
        for (int k = 0; k < len; k++)
        {
            int cb = flex_cross_basis(&items[axes[a][k]], dir);
            if (cb > cross_size) cross_size = cb;
        }

        int offset = 0;
        int sum_basis = 0;
        for (int k = 0; k < len; k++) sum_basis += flex_main_basis(&items[axes[a][k]], dir);
        int assignable = main_size - sum_basis;
        float remaining_grow = combined_grow;

        for (int ai = 0; ai < len; ai++)
        {
            int i = axes[a][ai];
            gmb_flex_item_t* it = &items[i];
            int basis = flex_main_basis(it, dir);
            int main_start = offset;
            int main_len = basis;

            if (combined_grow > 0)
            {
                int assigned = (it->grow > 0 && remaining_grow > 0)
                    ? (int)(it->grow * assignable / remaining_grow)
                    : 0;
                main_len = basis + assigned;
                assignable -= assigned;
                remaining_grow -= it->grow;
            }
            else
            {
                switch (justify)
                {
                    case GMB_JUSTIFY_END:
                        if (assignable > 0) { offset = assignable; assignable = 0; }
                        main_start = offset;
                        break;
                    case GMB_JUSTIFY_CENTER:
                        if (assignable > 0) { offset = assignable / 2; assignable = 0; }
                        main_start = offset;
                        break;
                    case GMB_JUSTIFY_SPACE_BETWEEN:
                        if (assignable > 0 && ai + 1 < len)
                        {
                            int extra = assignable / (len - ai - 1);
                            assignable -= extra;
                            offset += extra;
                        }
                        main_start = offset;
                        break;
                    case GMB_JUSTIFY_SPACE_AROUND:
                    {
                        int denom = 2 * len - ai * 2;
                        if (assignable > 0 && denom > 0)
                        {
                            int extra = assignable / denom;
                            assignable -= extra;
                            offset += extra;
                        }
                        main_start = offset;
                        int denom2 = 2 * len - (ai * 2 + 1);
                        if (assignable > 0 && denom2 > 0)
                        {
                            int extra2 = assignable / denom2;
                            assignable -= extra2;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            if (main_len < it->min) main_len = it->min;
            if (main_len > it->max) main_len = it->max;

            int cross_start, cross_len;
            int cb = flex_cross_basis(it, dir);
            switch (alignItems)
            {
                case GMB_ALIGN_ITEMS_STRETCH:
                    cross_start = 0; cross_len = cross_size; break;
                case GMB_ALIGN_ITEMS_START:
                    cross_start = 0; cross_len = cb; break;
                case GMB_ALIGN_ITEMS_END:
                    cross_start = cross_size - cb; cross_len = cb; break;
                default: /* center */
                    cross_start = (cross_size - cb) / 2; cross_len = cb; break;
            }

            int rx = dir == GMB_FLEX_ROW ? main_start : cross_start;
            int ry = dir == GMB_FLEX_ROW ? cross_start : main_start;
            int rw = dir == GMB_FLEX_ROW ? (main_len > 0 ? main_len : 0) : (cross_len > 0 ? cross_len : 0);
            int rh = dir == GMB_FLEX_ROW ? (cross_len > 0 ? cross_len : 0) : (main_len > 0 ? main_len : 0);
            local_x[i] = rx;
            local_y[i] = ry;
            local_w[i] = rw;
            local_h[i] = rh;
            item_axis[i] = a;

            offset = main_start + main_len + 1;
        }
        axis_cross[a] = cross_size;
    }

    /* offset cross degli assi da align-content */
    int total_cross = 0;
    for (int k = 0; k < axis_count; k++) total_cross += axis_cross[k];
    total_cross += axis_count - 1;
    int align_assignable = container_cross - total_cross;

    int axis_offsets[GMB_FLEX_MAX_AXES];
    int cursor = 0;
    for (int idx = 0; idx < axis_count; idx++)
    {
        int off = cursor;
        switch (alignContent)
        {
            case GMB_ALIGN_CONTENT_END:
                if (align_assignable > 0) { off += align_assignable; align_assignable = 0; }
                break;
            case GMB_ALIGN_CONTENT_CENTER:
                if (align_assignable > 0) { off += align_assignable / 2; align_assignable = 0; }
                break;
            case GMB_ALIGN_CONTENT_SPACE_AROUND:
            {
                int denom = 2 * axis_count - idx * 2;
                if (align_assignable > 0 && denom > 0)
                {
                    off += align_assignable / denom;
                    align_assignable -= align_assignable / denom;
                }
                break;
            }
            default:
                break;
        }
        axis_offsets[idx] = off;
        switch (alignContent)
        {
            case GMB_ALIGN_CONTENT_SPACE_BETWEEN:
                if (align_assignable > 0 && idx + 1 < axis_count)
                {
                    int extra = align_assignable / (axis_count - idx - 1);
                    align_assignable -= extra;
                    axis_offsets[idx] += extra;
                }
                break;
            case GMB_ALIGN_CONTENT_STRETCH:
            {
                int extra = axis_count - idx > 0 ? align_assignable / (axis_count - idx) : 0;
                if (align_assignable > 0) { axis_offsets[idx] += extra; align_assignable -= extra; }
                break;
            }
            case GMB_ALIGN_CONTENT_SPACE_AROUND:
            {
                int denom = 2 * axis_count - (idx * 2 + 1);
                if (align_assignable > 0 && denom > 0)
                {
                    axis_offsets[idx] += align_assignable / denom;
                    align_assignable -= align_assignable / denom;
                }
                break;
            }
            default:
                break;
        }
        cursor = axis_offsets[idx] + axis_cross[idx] + 1;
    }

    /* composizione rect finali assoluti */
    for (int idx = 0; idx < axis_count; idx++)
    {
        int cross_offset = axis_offsets[idx];
        for (int k = 0; k < axis_len[idx]; k++)
        {
            int i = axes[idx][k];
            int x, y;
            if (dir == GMB_FLEX_ROW)
            {
                x = container.x + local_x[i];
                y = container.y + cross_offset + local_y[i];
            }
            else
            {
                x = container.x + cross_offset + local_x[i];
                y = container.y + local_y[i];
            }
            items[i].rect.x = x;
            items[i].rect.y = y;
            items[i].rect.w = local_w[i];
            items[i].rect.h = local_h[i];
        }
    }
    (void)axis_of;
    return items_count;
}
