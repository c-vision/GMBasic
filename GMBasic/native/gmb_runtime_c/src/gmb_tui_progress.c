/* ============================================================================
 * gmb_tui_progress.c — ProgressBar.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void progress_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_progress_bar_t* p = (gmb_progress_bar_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 3, fg, bg, 1);
        char line[256];
        gmb_progress_bar_render_line(p, line, (int)sizeof(line));
        if (line[0])
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
        return;
    }
    char line[256];
    gmb_progress_bar_render_line(p, line, (int)sizeof(line));
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
}
static int prog_lum(unsigned c)
{
    int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
    return (r * 30 + g * 59 + b * 11) / 100;
}
static int prog_close(unsigned a, unsigned b)
{
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);
    return dr * dr + dg * dg + db * db < 2500;   /* ~50 per canale */
}
static void progress_draw_px(void* u, gmb_control_t* c)
{
    /* Renderer PIXEL guidato dal tema: pill (shape/radius), traccia fill, riempimento
     * con accent (fallback head_bg/title_bg se coinciderebbe con la traccia, es. Turbo),
     * bordo del tema e percentuale centrata con colore a contrasto. */
    gmb_progress_bar_t* p = (gmb_progress_bar_t*)u;
    gmb_widget_style_t s = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (w < 8 || h < 4) return;
    int rad = (s.shape == 1) ? h / 2 : 0;
    unsigned fill = s.accent;
    if (!(fill & 0xFFFFFFu) || prog_close(fill, s.fill)) fill = s.head_bg;
    if (!(fill & 0xFFFFFFu) || prog_close(fill, s.fill)) fill = s.title_bg;
    if (!(fill & 0xFFFFFFu) || prog_close(fill, s.fill)) fill = s.sel_bg;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rad, s.fill);
    int range = p->max - p->min;
    double ratio = range > 0 ? (double)(p->value - p->min) / (double)range : 0.0;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int fw = (int)(w * ratio + 0.5);
    if (fw > 0) gmb_gfx_fill_rounded_rect(x, y, fw, h, rad, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rad, s.border);
    if (p->show_percent)
    {
        char pb[16];
        snprintf(pb, sizeof(pb), "%d%%", (int)(ratio * 100.0 + 0.5));
        int th = gmb_gfx_text_height();
        if (th <= 0) th = 16;
        unsigned lbl = s.text;
        if (w / 2 < fw) lbl = prog_lum(fill) < 128 ? 0xFFFFFFu : 0x000000u;
        gmb_gfx_draw_text(pb, x + w / 2, y + (h - th) / 2, 1,
                          (int)(0x40000000u | (lbl & 0xFFFFFFu)));
    }
}
void gmb_progress_bar_init(gmb_progress_bar_t* p, int x, int y, int w)
{
    gmb_control_init(&p->ctrl, "", x, y, w, 1, progress_draw, NULL);
    p->ctrl.draw_px_fn = progress_draw_px;
    p->ctrl.user = p;
    p->min = 0;
    p->max = 100;
    p->show_percent = true;
}
void gmb_progress_bar_set(gmb_progress_bar_t* p, int value)
{
    p->value = value < p->min ? p->min : (value > p->max ? p->max : value);
}
void gmb_progress_bar_set_percentage(gmb_progress_bar_t* p, int pct)
{
    int c = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    p->value = p->min + (p->max - p->min) * c / 100;
}
int gmb_progress_bar_render_line(gmb_progress_bar_t* p, char* out, int cap)
{
    int range = p->max - p->min;
    int pct = range > 0 ? (p->value - p->min) * 100 / range : 0;
    int inner = p->ctrl.width - 2;
    if (inner < 0) inner = 0;
    int filled = range > 0 ? (p->value - p->min) * inner / range : 0;
    if (filled < 0) filled = 0;
    if (filled > inner) filled = inner;
    int o = 0;
    out[o++] = '[';
    for (int i = 0; i < filled; i++) out[o++] = (char)0x2588;
    for (int i = filled; i < inner; i++) out[o++] = ' ';
    out[o++] = ']';
    if (p->show_percent && inner >= 3)
    {
        char per[16];
        int pn = snprintf(per, sizeof(per), " %d%%", pct);
        for (int i = 0; i < pn && o < cap - 1; i++) out[o++] = per[i];
    }
    if (o > p->ctrl.width) o = p->ctrl.width;
    else for (int i = o; i < p->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}

/* ---- NumericUpDown ---- */
