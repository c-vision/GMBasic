/* ============================================================================
 * gmb_tui_statusbar.c — StatusBar.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== StatusBar ===================== */

static void statusbar_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_status_bar_t* s = (gmb_status_bar_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    { gmb_theme_t th_ = gmb_ui_theme(); gmb_cell_color_t tf, tb; gmb_theme_kind(&th_, 3, &tf, &tb); if (!c->style.has_fg || gmb_cellcolor_eq(c->style.fg, th_.text)) fg = tf; if (!c->style.has_bg || gmb_cellcolor_eq(c->style.bg, th_.window_bg)) bg = tb; }
    char line[256];
    gmb_status_bar_render_line(s, line, (int)sizeof(line));
    char cased[160];
    gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
    /* TV TStatusLine: i separatori degli item sono `│` (0xB3), non `|`. */
    for (char* p = cased; *p; p++) if (*p == '|') *p = ' ';
    gmb_cell_color_t red = { 170, 0, 0, 0 };
    gmb_draw_string_keys(buf, c->x, c->y, cased, c->width, fg, bg, red);
    for (int i = 0; i < (int)strlen(line); i++)
        if (line[i] == '|' && i < c->width)
            gmb_screen_setchar(buf, c->x + i, c->y, 0x2502, fg, bg);
}
static void statusbar_draw_px(void* u, gmb_control_t* c)
{
    gmb_status_bar_t* s = (gmb_status_bar_t*)u;
    int x = (c->px_w > 0) ? c->px_x : c->x, y = (c->px_h > 0) ? c->px_y : c->y;
    int w = (c->px_w > 0) ? c->px_w : c->width, h = (c->px_h > 0) ? c->px_h : c->height;
    gmb_form_paint_widget_px("statusbar", x, y, w, h, NULL, 0, 0, 0, 0, 0, s->text, NULL);
}
void gmb_status_bar_init(gmb_status_bar_t* s, int x, int y, int w)
{
    memset(s, 0, sizeof(*s));
    gmb_control_init(&s->ctrl, "", x, y, w, 1, statusbar_draw, NULL);
    s->ctrl.draw_px_fn = statusbar_draw_px;
    s->ctrl.ctrl_kind = GMB_CK_STATUSBAR;
    s->ctrl.user = s;
}
void gmb_status_bar_set_text(gmb_status_bar_t* s, const char* text)
{
    snprintf(s->text, sizeof(s->text), "%s", text ? text : "");
}
int gmb_status_bar_render_line(gmb_status_bar_t* s, char* out, int cap)
{
    int o = snprintf(out, cap, "%.*s", s->ctrl.width, s->text);
    if (o > s->ctrl.width) o = s->ctrl.width;
    else for (int i = o; i < s->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
