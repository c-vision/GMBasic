/* ============================================================================
 * gmb_tui_label.c — Label.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void label_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_label_t* l = (gmb_label_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        char line[256], cased[160];
        gmb_label_render_line(l, line, (int)sizeof(line));
        gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
        gmb_gfx_draw_text(cased, sk->origin_x + c->x * sk->cell_w,
                          sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0,
                          (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    char line[256];
    gmb_label_render_line(l, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
}
static void label_draw_px(void* u, gmb_control_t* c)
{
    /* Un LABEL è testo, non una chip: prima passava da GMB_PX_STATUSBAR e disegnava un
     * riquadro (bordo del tema) sopra il testo → le didascalie sembravano barrate. */
    gmb_label_t* l = (gmb_label_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int bx = (c->px_w > 0) ? c->px_x : c->x;
    int by = (c->px_h > 0) ? c->px_y : c->y;
    int bh = (c->px_h > 2) ? c->px_h : (st.row_h > 0 ? st.row_h : 20);
    int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;
    gmb_gfx_draw_text(l->text, bx, by + (bh - th) / 2, 0,
                      (int)(0x40000000u | (st.text & 0xFFFFFFu)));
}
void gmb_label_init(gmb_label_t* l, int x, int y, int w, const char* text)
{
    memset(l, 0, sizeof(*l));
    gmb_control_init(&l->ctrl, "", x, y, w, 1, label_draw, NULL);
    l->ctrl.draw_px_fn = label_draw_px;
    l->ctrl.user = l;
    snprintf(l->text, sizeof(l->text), "%s", text ? text : "");
}
void gmb_label_set_text(gmb_label_t* l, const char* text)
{
    snprintf(l->text, sizeof(l->text), "%s", text ? text : "");
}
int gmb_label_render_line(gmb_label_t* l, char* out, int cap)
{
    /* TV TLabel: testo a col 1 (una cella di margine), marker a col 0 quando attivi. */
    int w = l->ctrl.width;
    int len = (int)strlen(l->text);
    if (len > w - 1) len = w - 1 > 0 ? w - 1 : 0;
    int o = 0;
    out[o++] = ' ';
    int pad = w - 1 - len;
    if (pad < 0) pad = 0;
    if (l->align == 2)
    {
        for (int i = 0; i < pad; i++) out[o++] = ' ';
        for (int i = 0; i < len; i++) out[o++] = l->text[i];
    }
    else if (l->align == 1)
    {
        int half = pad / 2;
        for (int i = 0; i < half; i++) out[o++] = ' ';
        for (int i = 0; i < len; i++) out[o++] = l->text[i];
        for (int i = half; i < pad; i++) out[o++] = ' ';
    }
    else
    {
        for (int i = 0; i < len; i++) out[o++] = l->text[i];
        for (int i = 0; i < pad; i++) out[o++] = ' ';
    }
    out[o] = '\0';
    return o;
}

/* ---- HLine / VLine ---- */
