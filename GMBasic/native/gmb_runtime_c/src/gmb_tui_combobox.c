/* ============================================================================
 * gmb_tui_combobox.c — ComboBox.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static void combobox_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_combobox_t* cb = (gmb_combobox_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    char line[128];
    gmb_combobox_render_line(cb, line, (int)sizeof(line));
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
    /* C# Draw: SetChar(X + Width - 1, Y, '▼') */
    gmb_screen_setchar(buf, c->x + c->width - 1, c->y, 0x25BC, fg, bg);

    if (cb->open && cb->items_count > 0)
    {
        gmb_cell_color_t popFg, popBg, selFg, selBg;
        gmb_control_style_effective(&c->style, false, false, false, &popFg, &popBg);
        gmb_control_style_effective(&c->style, false, false, true, &selFg, &selBg);
        int maxH = cb->drop_down_height < cb->items_count ? cb->drop_down_height : cb->items_count;
        for (int i = 0; i < maxH; i++)
        {
            const char* it = cb->items[i];
            int il = (int)strlen(it);
            char tmp[128], item[160];
            if (il > c->width) snprintf(tmp, sizeof(tmp), "%.*s", c->width, it);
            else snprintf(tmp, sizeof(tmp), "%-*s", c->width, it);
            gmb_control_apply_case(c, tmp, item, (int)sizeof(item));
            gmb_cell_color_t f = (i == cb->highlight) ? selFg : popFg;
            gmb_cell_color_t b = (i == cb->highlight) ? selBg : popBg;
            gmb_screen_setstring(buf, c->x, c->y + 1 + i, item, f, b);
        }
    }
}

static bool combobox_key(void* u, gmb_control_t* c, int key)
{
    gmb_combobox_t* cb = (gmb_combobox_t*)u;
    if (!c->enabled) return false;
    if (cb->open)
    {
        switch (key)
        {
            case GMB_K_UP:
                if (cb->highlight > 0) cb->highlight--;
                return true;
            case GMB_K_DOWN:
                if (cb->highlight < cb->items_count - 1) cb->highlight++;
                return true;
            case GMB_K_ENTER:
                if (cb->highlight >= 0 && cb->highlight < cb->items_count)
                {
                    cb->selected = cb->highlight;
                    cb->open = false;
                }
                return true;
            case GMB_K_ESC:
                cb->open = false;
                return true;
        }
    }
    else
    {
        switch (key)
        {
            case GMB_K_ENTER:
            case GMB_K_DOWN:
                if (cb->items_count > 0)
                {
                    cb->open = true;
                    cb->highlight = cb->selected >= 0 ? cb->selected : 0;
                }
                return true;
            case GMB_K_ESC:
                return true;
        }
    }
    return false;
}

static bool combobox_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_combobox_t* cb = (gmb_combobox_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    r.h = cb->open ? cb->drop_down_height + 1 : 1;
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    if (cb->open)
    {
        int itemIdx = y - (c->y + 1);
        int maxH = cb->drop_down_height < cb->items_count ? cb->drop_down_height : cb->items_count;
        if (itemIdx >= 0 && itemIdx < maxH)
        {
            cb->selected = cb->highlight = itemIdx;
            cb->open = false;
        }
        else
        {
            cb->open = false;
        }
    }
    else
    {
        cb->open = true;
        cb->highlight = cb->selected >= 0 ? cb->selected : 0;
    }
    return true;
}

static void combobox_draw_px(void* u, gmb_control_t* c)
{
    gmb_combobox_t* cb = (gmb_combobox_t*)u;
    const char* _it[16]; for (int i = 0; i < cb->items_count && i < 16; i++) _it[i] = cb->items[i];
    gmb_px_draw_ex(GMB_PX_DROPDOWN, c->x, c->y, c->width, c->height, NULL, _it, cb->items_count, cb->selected, 0, 0, NULL, 0, NULL, 0);
}
void gmb_combobox_init(gmb_combobox_t* c, int x, int y, int w)
{
    gmb_control_init(&c->ctrl, "", x, y, w, 1, combobox_draw, combobox_key);
    c->ctrl.handle_click_fn = combobox_click;
    c->ctrl.user = c;
    c->ctrl.draw_px_fn = combobox_draw_px;
    c->ctrl.ctrl_kind = GMB_CK_COMBOBOX;
    c->items_count = 0;
    c->selected = -1;
    c->open = false;
    c->highlight = 0;
    c->drop_down_height = 5;
}

void gmb_combobox_set_items(gmb_combobox_t* c, const char* const* items, int count)
{
    c->items_count = count < 16 ? count : 16;
    for (int i = 0; i < c->items_count && items; i++)
        snprintf(c->items[i], sizeof(c->items[i]), "%s", items[i] ? items[i] : "");
    if (c->items_count > 0 && c->selected < 0) { c->selected = 0; c->highlight = 0; }
}

const char* gmb_combobox_selected_value(gmb_combobox_t* c)
{
    if (c->selected >= 0 && c->selected < c->items_count) return c->items[c->selected];
    return NULL;
}

int gmb_combobox_render_line(gmb_combobox_t* c, char* out, int cap)
{
    const char* val = gmb_combobox_selected_value(c);
    /* C# RenderLine: tronca a Width-1 (spazio per ▼), poi PadRight(Width). */
    int maxw = c->ctrl.width - 1;
    if (maxw < 0) maxw = 0;
    int o = snprintf(out, cap, "%.*s", maxw, val ? val : "");
    for (int i = o; i < c->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
