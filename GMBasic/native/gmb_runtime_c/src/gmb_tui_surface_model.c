/* gmb_tui_surface_model.c — primitivo di disegno a CELLE (core), ramo pixel via iface. */
#include "gmb_tui.h"
#include "gmb_ui_gfx.h"

void gmb_surface_rounded_rect(gmb_screen_buffer_t* b, int cx, int cy, int cw, int ch,
                              int radius, gmb_cell_color_t fg, gmb_cell_color_t bg, int filled)
{
    gmb_screen_sink_t* sk = gmb_screen_get_sink(b);
    if (sk && sk->is_gfx)
    {
        int px = sk->origin_x + cx * sk->cell_w;
        int py = sk->origin_y + cy * sk->cell_h;
        int pw = cw * sk->cell_w, ph = ch * sk->cell_h;
        int rp = radius > 0 ? radius : 6;
        unsigned int bgu = 0xFF000000u | ((unsigned)bg.r << 16) | ((unsigned)bg.g << 8) | bg.b;
        unsigned int fgu = 0xFF000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b;
        if (filled) GMB_GFX_FILL_RRECT(px, py, pw, ph, rp, bgu);
        GMB_GFX_STROKE_RRECT(px, py, pw, ph, rp, fgu);
        return;
    }
    for (int y = cy; y < cy + ch; y++)
        for (int x = cx; x < cx + cw; x++)
            gmb_screen_setchar(b, x, y, ' ', fg, bg);
}
