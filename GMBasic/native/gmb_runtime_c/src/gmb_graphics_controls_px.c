/* gmb_graphics_controls_px.c — renderer PIXEL dei controlli (target UI).
 * Estratti dai file TUI: lì resta il renderer `draw` (celle, core). */
#include <string.h>
#include "gmb_px_renderer.h"
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
static void px_spinner(void* u, gmb_control_t* c);

static void px_alertbox(void* u, gmb_control_t* c)
{
    gmb_alert_box_t* a = (gmb_alert_box_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int tl = (int)gmb_gfx_text_height(); if (tl <= 0) tl = 14;
    gmb_gfx_fill_rounded_rect(x, y, w, h, 6, st.fill);
    int th = tl + 8;
    gmb_gfx_fill_rounded_rect(x, y, w, th, 6, st.title_bg);
    gmb_gfx_draw_text(a->title[0] ? a->title : "Avviso", x + 8, y + 4, 0, (int)st.title_fg);
    gmb_gfx_draw_text(a->message, x + 10, y + th + 8, 0, (int)st.text);
    int bh = tl + 8, bw = gmb_gfx_text_width(a->ok_label[0] ? a->ok_label : "OK") + 24; if (bw < 60) bw = 60;
    int bx = x + w - bw - 10, by = y + h - bh - 8;
    gmb_gfx_fill_rounded_rect(bx, by, bw, bh, 4, st.fill_hover);
    gmb_gfx_stroke_rounded_rect(bx, by, bw, bh, 4, st.border);
    gmb_gfx_draw_text(a->ok_label[0] ? a->ok_label : "OK", bx + bw / 2, by + (bh - tl) / 2, 1, (int)st.text);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, 6, st.border);
}

void gmb_px_renderers_install(void)
{
    gmb_px_renderer_set(GMB_CK_ALERTBOX, px_alertbox);
    gmb_px_renderer_set(GMB_CK_SPINNER, px_spinner);
}

#include "gmb_tui.h"
#include "gmb_tui_app.h"
#include "gmb_tui_tbrowse.h"

static void px_spinner(void* u, gmb_control_t* c)
{
    gmb_spinner_t* s = (gmb_spinner_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    if (!s || s->frames_count <= 0) return;
    int idx = s->index % s->frames_count; if (idx < 0) idx = 0;
    const char* fr = s->frames[idx];
    if (!fr || !*fr) return;
    int th = gmb_gfx_text_height();
    unsigned col = (st.accent & 0xFFFFFFu) ? st.accent : st.text;
    gmb_gfx_draw_text(fr, c->x + c->width / 2, c->y + (c->height - th) / 2, 1,
                      (int)(0x40000000u | (col & 0xFFFFFFu)));
}

/* Tbrowse: ramo PIXEL del draw (era inline in gmb_tui_tbrowse.c). */
void gmb_px_browse_draw(void* bv, void* buf)
{
    gmb_browse_t* b = (gmb_browse_t*)bv;
    gmb_screen_buffer_t* buffer = (gmb_screen_buffer_t*)buf;
    int ax = b->area.x, ay = b->area.y, w = b->area.w;
    gmb_cell_color_t border_fg = b->header_colors.foreground;
    gmb_cell_color_t border_bg = b->header_colors.background;
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buffer);
    if (!sk) return;
    gmb_surface_rounded_rect(buffer, ax, ay, w, b->area.h, 4, border_fg, border_bg, 1);
    int oy = ay;
    if (b->show_header)
    {
        char inner[2048];
        gmb_browse_render_header(b, inner, (int)sizeof(inner));
        gmb_gfx_draw_text(inner, sk->origin_x + (ax + 1) * sk->cell_w,
                          sk->origin_y + oy * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(b->header_colors.foreground).r << 16) | ((unsigned)(b->header_colors.foreground).g << 8) | (b->header_colors.foreground).b));
        oy++;
    }
    char rows[8192];
    gmb_browse_render_rows(b, rows, (int)sizeof(rows));
    char* p = rows;
    int disp = 0;
    while (*p && oy < ay + b->area.h)
    {
        char* nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        int data_row = b->top_row + disp;
        int hl = b->highlight_row && (data_row == b->current_row);
        if (hl) gmb_surface_rounded_rect(buffer, ax, oy, w, 1, 2, b->highlight_colors.background, b->highlight_colors.background, 1);
        gmb_gfx_draw_text(p, sk->origin_x + (ax + 1) * sk->cell_w,
                          sk->origin_y + oy * sk->cell_h + sk->cell_h / 4, 0,
                          hl ? (int)(0x40000000u | ((unsigned)(b->highlight_colors.foreground).r << 16) | ((unsigned)(b->highlight_colors.foreground).g << 8) | (b->highlight_colors.foreground).b) : (int)(0x40000000u | ((unsigned)(b->row_colors.foreground).r << 16) | ((unsigned)(b->row_colors.foreground).g << 8) | (b->row_colors.foreground).b));
        oy++; disp++;
        if (!nl) break;
        p = nl + 1;
    }
}
