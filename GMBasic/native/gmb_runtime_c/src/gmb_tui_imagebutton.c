/* ============================================================================
 * gmb_tui_imagebutton.c — gmb_tui_imagebutton. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ImageButton ===================== */

static void imagebutton_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_image_button_t* b = (gmb_image_button_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        char line[128];
        gmb_image_button_render_line(b, line, (int)sizeof(line));
        gmb_gfx_draw_text(line, sk->origin_x + (c->x + c->width / 2) * sk->cell_w,
                          sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 1, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        return;
    }
    char line[128];
    gmb_image_button_render_line(b, line, (int)sizeof(line));
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
}
static bool imagebutton_key(void* u, gmb_control_t* c, int key)
{
    gmb_image_button_t* b = (gmb_image_button_t*)u;
    if (!c->enabled) return false;
    if (key == GMB_K_ENTER || key == ' ') { if (b->on_click) b->on_click(b->click_ctx); return true; }
    return false;
}
static bool imagebutton_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_image_button_t* b = (gmb_image_button_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    if (b->on_click) b->on_click(b->click_ctx);
    return true;
}
void gmb_image_button_init(gmb_image_button_t* b, int x, int y, int w)
{
    memset(b, 0, sizeof(*b));
    gmb_control_init(&b->ctrl, "", x, y, w, 1, imagebutton_draw, imagebutton_key);
    b->ctrl.handle_click_fn = imagebutton_click;
    b->ctrl.user = b;
}
int gmb_image_button_render_line(gmb_image_button_t* b, char* out, int cap)
{
    char inner[96];
    snprintf(inner, sizeof(inner), " %s %s ", b->icon, b->label);
    int o = snprintf(out, cap, "[ %s ]", inner);
    if (o > b->ctrl.width) o = b->ctrl.width;
    else for (int i = o; i < b->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
