/* ============================================================================
 * gmb_tui_button.c — Button.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Turbo Vision 2.0 — TButton::drawState: il bottone è alto 2 righe; la riga 0 ha il
 * titolo centrato con `▄` sull'ultima cella, la riga 1 è l'ombra `▀` (shadows[2]).
 * I marker `[ ]` esistono solo con showMarkers (TV default: false). */
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
        int py = sk->origin_y + (c->y + c->height / 2) * sk->cell_h;
        gmb_gfx_draw_text(cased, px, py, 1,
                          (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    gmb_theme_t th = gmb_ui_theme();
    gmb_cell_color_t sh = th.shadow;
    int h = c->height > 1 ? c->height : 1;
    int title_row = c->y + (h > 1 ? h / 2 - 1 : 0);
    /* riga titolo (+ eventuali righe di riempimento) */
    char line[128];
    gmb_button_render_line(b, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    for (int yy = c->y; yy < c->y + h; yy++)
    {
        if (yy == title_row) gmb_screen_setstring(buf, c->x, yy, cased, fg, bg);
        else {
            char fill[160];
            int n = c->width < (int)sizeof(fill) ? c->width : (int)sizeof(fill) - 1;
            for (int i = 0; i < n; i++) fill[i] = ' ';
            fill[n] = '\0';
            gmb_screen_setstring(buf, c->x, yy, fill, fg, bg);
        }
    }
    if (h == 1) return;
    /* bordo destro `▄` e riga di ombra `▀` (TV shadows = "\xDC\xDB\xDF") */
    gmb_screen_setchar(buf, c->x + c->width - 1, title_row, 0x2584, sh, bg);
    for (int i = 0; i < c->width - 2; i++)
        gmb_screen_setchar(buf, c->x + 2 + i, c->y + h - 1, 0x2580, sh, bg);
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
    memset(b, 0, sizeof(*b));
    gmb_control_init(&b->ctrl, "", x, y, w, 2, button_draw, button_key);   /* TV: alto 2 (titolo + ombra) */
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
    /* TV: titolo centrato tra col 1 e width-2 (l'ultima cella è `▄`). */
    int w = b->ctrl.width;
    int inner = w - 1;
    if (inner < 0) inner = 0;
    int llen = (int)strlen(b->label);
    if (llen > inner - 2) llen = inner - 2 > 0 ? inner - 2 : 0;
    int left = (inner - llen - 1) / 2;
    if (left < 1) left = 1;
    int o = 0;
    for (int i = 0; i < inner && o < cap - 1; i++) out[o++] = ' ';
    int start = left;
    if (gmb_tui_show_markers && inner >= 2) {
        out[1] = '[';
        out[inner - 1] = ']';
        start = 1 + (inner - 2 - llen) / 2;
        if (start < 2) start = 2;
    }
    for (int i = 0; i < llen && start + i < inner; i++) out[start + i] = b->label[i];
    out[o] = '\0';
    return o;
}

/* ---- CheckBox ---- */
