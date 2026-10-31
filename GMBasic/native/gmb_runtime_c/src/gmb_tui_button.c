/* ============================================================================
 * gmb_tui_button.c — Button.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void button_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_button_t* b = (gmb_button_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    /* Surface gfx: pulsante nativo (rounded + etichetta centrata). */
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        char cased[160];
        gmb_control_apply_case(c, b->label, cased, (int)sizeof(cased));
        int px = sk->origin_x + (c->x + c->width / 2) * sk->cell_w;
        int py = sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4;
        gmb_gfx_draw_text(cased, px, py, 1,
                          (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    char line[128];
    gmb_button_render_line(b, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
}
static bool button_key(void* u, gmb_control_t* c, int key)
{
    gmb_button_t* b = (gmb_button_t*)u;
    if (!c->enabled) return false;
    if (key == GMB_K_ENTER || key == ' ')
    {
        if (b->on_click) b->on_click(b->click_ctx);
        return true;
    }
    return false;
}
static bool button_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_button_t* b = (gmb_button_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    if (b->on_click) b->on_click(b->click_ctx);
    return true;
}
void gmb_button_init(gmb_button_t* b, int x, int y, int w, const char* label)
{
    gmb_control_init(&b->ctrl, "", x, y, w, 1, button_draw, button_key);
    b->ctrl.handle_click_fn = button_click;
    b->ctrl.user = b;
    snprintf(b->label, sizeof(b->label), "%s", label ? label : "OK");
}
void gmb_button_set_onclick(gmb_button_t* b, void (*fn)(void* ctx), void* ctx)
{
    b->on_click = fn;
    b->click_ctx = ctx;
}
int gmb_button_render_line(gmb_button_t* b, char* out, int cap)
{
    int inner = b->ctrl.width - 2;
    if (inner < 0) inner = 0;
    int llen = (int)strlen(b->label);
    char text[64];
    snprintf(text, sizeof(text), "%.*s", inner, b->label);
    int tl = (int)strlen(text);
    int pad = inner - tl;
    if (pad < 0) pad = 0;
    int left = pad / 2;
    int right = pad - left;
    int o = 0;
    out[o++] = '[';
    for (int i = 0; i < left; i++) out[o++] = ' ';
    for (int i = 0; i < tl; i++) out[o++] = text[i];
    for (int i = 0; i < right; i++) out[o++] = ' ';
    out[o++] = ']';
    out[o] = '\0';
    (void)llen;
    return o;
}

/* ---- CheckBox ---- */
