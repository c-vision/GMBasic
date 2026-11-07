/* ============================================================================
 * gmb_tui_panel.c — Panel.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void panel_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_panel_t* p = (gmb_panel_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    int w = c->width, h = c->height;
    if (w < 2 || h < 2) return;
    /* Surface gfx: chrome nativo (rounded) invece del bordo a celle. */
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, w, h, 6, fg, bg, 1);
        if (p->title[0])
        {
            char t[128], ct[160];
            snprintf(t, sizeof(t), "%s", p->title);
            gmb_control_apply_case(c, t, ct, (int)sizeof(ct));
            int px = sk->origin_x + (c->x + 1) * sk->cell_w;
            int py = sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4;
            gmb_gfx_draw_text(ct, px, py, 0, (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        }
        return;
    }
    gmb_screen_setchar(buf, c->x, c->y, 0x250C, fg, bg);
    gmb_screen_setchar(buf, c->x + w - 1, c->y, 0x2510, fg, bg);
    gmb_screen_setchar(buf, c->x, c->y + h - 1, 0x2514, fg, bg);
    gmb_screen_setchar(buf, c->x + w - 1, c->y + h - 1, 0x2518, fg, bg);
    for (int i = 1; i < w - 1; i++)
    {
        gmb_screen_setchar(buf, c->x + i, c->y, 0x2500, fg, bg);
        gmb_screen_setchar(buf, c->x + i, c->y + h - 1, 0x2500, fg, bg);
    }
    for (int i = 1; i < h - 1; i++)
    {
        gmb_screen_setchar(buf, c->x, c->y + i, 0x2502, fg, bg);
        gmb_screen_setchar(buf, c->x + w - 1, c->y + i, 0x2502, fg, bg);
    }
    if (p->title[0])
    {
        char t[128];
        snprintf(t, sizeof(t), " %s ", p->title);
        char ct[160];
        gmb_control_apply_case(c, t, ct, (int)sizeof(ct));
        gmb_screen_setstring(buf, c->x + 1, c->y, ct, fg, bg);
    }
}
static void panel_draw_px(void* u, gmb_control_t* c)
{
    gmb_panel_t* p = (gmb_panel_t*)u;
    gmb_px_draw_ex(GMB_PX_PANEL, c->px_x, c->px_y, c->px_w, c->px_h, p->title,
                   NULL, 0, 0.0, 0.0, 0.0, NULL, 0, NULL, 0);
}

void gmb_panel_init(gmb_panel_t* p, int x, int y, int w, int h, const char* title)
{
    memset(p, 0, sizeof(*p));
    gmb_control_init(&p->ctrl, "", x, y, w, h, panel_draw, NULL);
    p->ctrl.user = p;
    p->ctrl.draw_px_fn = panel_draw_px;   /* grafica: card nativa, no celle */
    snprintf(p->title, sizeof(p->title), "%s", title ? title : "");
    p->border = GMB_PB_SINGLE;
    p->draggable = true;
    p->is_dragging = false;
    p->drag_off_x = 0;
    p->drag_off_y = 0;
}

/* ---- 100% parity Panel.cs:202 — with_*, InnerArea, drag ---- */
void gmb_panel_with_title(gmb_panel_t* p, const char* title)
{
    if (!p) return;
    snprintf(p->title, sizeof(p->title), "%s", title ? title : "");
}
void gmb_panel_with_border(gmb_panel_t* p, int border)
{
    if (!p) return;
    p->border = border;
}
gmb_rect_t gmb_panel_inner_area(const gmb_panel_t* p)
{
    gmb_rect_t r = { p->ctrl.x, p->ctrl.y, p->ctrl.width, p->ctrl.height };
    int inset = (p->border == GMB_PB_NONE) ? 0 : 1;
    r.x += inset; r.y += inset; r.w -= 2 * inset; r.h -= 2 * inset;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
void gmb_panel_start_drag(gmb_panel_t* p, int x, int y)
{
    if (!p || !p->draggable) return;
    p->is_dragging = true;
    p->drag_off_x = x - p->ctrl.x;
    p->drag_off_y = y - p->ctrl.y;
}
bool gmb_panel_update_drag(gmb_panel_t* p, int x, int y)
{
    if (!p || !p->is_dragging) return false;
    p->ctrl.x = x - p->drag_off_x;
    p->ctrl.y = y - p->drag_off_y;
    return true;
}
void gmb_panel_end_drag(gmb_panel_t* p)
{
    if (p) p->is_dragging = false;
}
