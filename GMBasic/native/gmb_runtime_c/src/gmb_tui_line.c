/* ============================================================================
 * gmb_tui_line.c — Line.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void hline_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_hline_t* h = (gmb_hline_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        int y = sk->origin_y + c->y * sk->cell_h + sk->cell_h / 2;
        int x1 = sk->origin_x + c->x * sk->cell_w;
        int x2 = x1 + c->width * sk->cell_w - 1;
        gmb_gfx_line(x1, y, x2, y, (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b), 0);
        if (h->label[0])
            gmb_gfx_draw_text(h->label, x1, sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0,
                              (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    char line[256];
    if (h->label[0] == '\0')
    {
        for (int i = 0; i < c->width && i < 255; i++) line[i] = 0x2500;
        line[c->width < 255 ? c->width : 255] = '\0';
    }
    else
    {
        int max = c->width - 4;
        if (max < 1) max = 1;
        char lbl[128];
        snprintf(lbl, sizeof(lbl), "%.*s", max, h->label);
        char inner[192];
        snprintf(inner, sizeof(inner), "─ %s ", lbl);
        int il = (int)strlen(inner);
        int left = (c->width - il) / 2;
        if (left < 0) left = 0;
        int right = c->width - left - il;
        if (right < 0) right = 0;
        int o = 0;
        for (int i = 0; i < left; i++) line[o++] = 0x2500;
        for (int i = 0; i < il; i++) line[o++] = inner[i];
        for (int i = 0; i < right; i++) line[o++] = 0x2500;
        line[o] = '\0';
    }
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
}
void gmb_hline_init(gmb_hline_t* h, int x, int y, int w, const char* label)
{
    gmb_control_init(&h->ctrl, "", x, y, w, 1, hline_draw, NULL);
    h->ctrl.user = h;
    snprintf(h->label, sizeof(h->label), "%s", label ? label : "");
}

static void vline_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_vline_t* v = (gmb_vline_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    for (int i = 0; i < c->height; i++)
        gmb_screen_setchar(buf, c->x, c->y + i, 0x2502, fg, bg);
}
void gmb_vline_init(gmb_vline_t* v, int x, int y, int h)
{
    gmb_control_init(&v->ctrl, "", x, y, 1, h, vline_draw, NULL);
    v->ctrl.user = v;
}

/* ---- ProgressBar ---- */
