/* ============================================================================
 * gmb_tui_canvas.c — Centaurea canvas/widget (porting ClipperLib/Clipper
 * Centaurea.cs). Superficie di disegno su ScreenBuffer.
 * ============================================================================ */
#include "gmb_tui_canvas.h"
#include <stdio.h>
#include <string.h>

static void canvas_to_absolute(gmb_canvas_t* c, int x, int y, int* ax, int* ay)
{
    *ax = c->frame.x + x;
    *ay = c->frame.y + y;
}

static bool canvas_inside(gmb_canvas_t* c, int x, int y)
{
    return x >= 0 && y >= 0 && x < c->frame.w && y < c->frame.h;
}

static gmb_cell_color_t color_or(const gmb_cell_color_t* p, gmb_cell_color_t def)
{
    return p ? *p : def;
}

void gmb_canvas_new(gmb_canvas_t* c, gmb_screen_buffer_t* buffer, gmb_rect_t frame)
{
    c->buffer = buffer;
    c->frame = frame;
}

void gmb_canvas_full(gmb_canvas_t* c, gmb_screen_buffer_t* buffer)
{
    c->buffer = buffer;
    c->frame.x = 0;
    c->frame.y = 0;
    c->frame.w = gmb_screen_width(buffer);
    c->frame.h = gmb_screen_height(buffer);
}

void gmb_canvas_set_frame(gmb_canvas_t* c, gmb_rect_t frame)
{
    c->frame = frame;
}

void gmb_canvas_pixel(gmb_canvas_t* c, int x, int y, uint32_t ch,
                      const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (!canvas_inside(c, x, y)) return;
    int ax, ay;
    canvas_to_absolute(c, x, y, &ax, &ay);
    gmb_cell_color_t f = color_or(fg, GMB_CELLCOLOR_DEFAULT);
    gmb_cell_color_t b = color_or(bg, GMB_CELLCOLOR_DEFAULT);
    gmb_screen_setchar(c->buffer, ax, ay, ch, f, b);
}

void gmb_canvas_fill(gmb_canvas_t* c, uint32_t ch, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    gmb_rect_t r = { 0, 0, c->frame.w, c->frame.h };
    gmb_canvas_rect_fill(c, r, ch, fg, bg);
}

void gmb_canvas_rect_fill(gmb_canvas_t* c, gmb_rect_t rect, uint32_t ch,
                          const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    for (int y = 0; y < rect.h; y++)
        for (int x = 0; x < rect.w; x++)
            gmb_canvas_pixel(c, rect.x + x, rect.y + y, ch, fg, bg);
}

void gmb_canvas_draw_string(gmb_canvas_t* c, int x, int y, const char* text,
                            const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (!text) return;
    int cx = x, cy = y;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++)
    {
        gmb_canvas_pixel(c, cx, cy, *p, fg, bg);
        cx++;
        if (cx >= c->frame.w) { cx = 0; cy++; }
    }
}

void gmb_canvas_draw_string_n(gmb_canvas_t* c, int x, int y, const char* text, int max_width,
                              const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (!text) return;
    int n = (int)strlen(text);
    if (max_width < 0) max_width = 0;
    if (n > max_width) n = max_width;
    for (int i = 0; i < n; i++)
        gmb_canvas_pixel(c, x + i, y, (unsigned char)text[i], fg, bg);
}

int gmb_canvas_draw_line(gmb_canvas_t* c, int x, int y, const gmb_line_t* line, int has_max, int max_width)
{
    int cx = x;
    int limit = has_max ? x + max_width : c->frame.w;
    for (int i = 0; i < line->span_count; i++)
    {
        gmb_style_t style = gmb_style_patch(line->style, line->spans[i].style);
        const char* content = line->spans[i].content;
        for (const unsigned char* p = (const unsigned char*)content; *p; p++)
        {
            if (cx >= limit) return cx;
            const gmb_cell_color_t* fg = style.has_fg ? &style.fg : NULL;
            const gmb_cell_color_t* bg = style.has_bg ? &style.bg : NULL;
            gmb_canvas_pixel(c, cx, y, *p, fg, bg);
            cx++;
        }
    }
    return cx;
}

void gmb_canvas_draw_text(gmb_canvas_t* c, int x, int y, const gmb_text_t* text, int has_max, int max_width)
{
    for (int i = 0; i < text->line_count; i++)
        gmb_canvas_draw_line(c, x, y + i, &text->lines[i], has_max, max_width);
}

void gmb_canvas_hline(gmb_canvas_t* c, int x, int y, int len, uint32_t ch,
                      const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    for (int i = 0; i < len; i++)
        gmb_canvas_pixel(c, x + i, y, ch, fg, bg);
}

void gmb_canvas_vline(gmb_canvas_t* c, int x, int y, int len, uint32_t ch,
                      const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    for (int i = 0; i < len; i++)
        gmb_canvas_pixel(c, x, y + i, ch, fg, bg);
}

void gmb_canvas_segment(gmb_canvas_t* c, int x0, int y0, int x1, int y1, uint32_t ch,
                        const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    int dx = x1 - x0 > 0 ? x1 - x0 : -(x1 - x0);
    int dy = -(y1 - y0 > 0 ? y1 - y0 : -(y1 - y0));
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int x = x0, y = y0;
    while (true)
    {
        gmb_canvas_pixel(c, x, y, ch, fg, bg);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

void gmb_canvas_box(gmb_canvas_t* c, gmb_rect_t rect, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (rect.w < 2 || rect.h < 2) return;
    int right = rect.w - 1;
    int bottom = rect.h - 1;
    gmb_canvas_pixel(c, rect.x, rect.y, 0x250C, fg, bg);          /* ┌ */
    gmb_canvas_pixel(c, rect.x + right, rect.y, 0x2510, fg, bg);  /* ┐ */
    gmb_canvas_pixel(c, rect.x, rect.y + bottom, 0x2514, fg, bg); /* └ */
    gmb_canvas_pixel(c, rect.x + right, rect.y + bottom, 0x2518, fg, bg); /* ┘ */
    gmb_canvas_hline(c, rect.x + 1, rect.y, right - 1, 0x2500, fg, bg);
    gmb_canvas_hline(c, rect.x + 1, rect.y + bottom, right - 1, 0x2500, fg, bg);
    gmb_canvas_vline(c, rect.x, rect.y + 1, bottom - 1, 0x2502, fg, bg);
    gmb_canvas_vline(c, rect.x + right, rect.y + 1, bottom - 1, 0x2502, fg, bg);
}

/* ===================== CanvasLabel / CanvasPanel ===================== */

void gmb_canvas_label_init(gmb_canvas_label_t* l, const char* text, gmb_rect_t area)
{
    memset(l, 0, sizeof(*l));
    gmb_text_raw(&l->content, text ? text : "");
    l->area = area;
}

void gmb_canvas_label_draw(gmb_canvas_label_t* l, gmb_canvas_t* canvas)
{
    gmb_canvas_t inner;
    gmb_canvas_new(&inner, canvas->buffer, l->area);
    gmb_canvas_draw_text(&inner, 0, 0, &l->content, 1, l->area.w);
}

void gmb_canvas_panel_init(gmb_canvas_panel_t* p, gmb_rect_t area, const char* title, const gmb_canvas_label_t* child)
{
    memset(p, 0, sizeof(*p));
    memset(p, 0, sizeof(*p));
    p->area = area;
    if (title && title[0])
    {
        snprintf(p->title, sizeof(p->title), "%s", title);
        p->has_title = true;
    }
    if (child)
    {
        p->child = *child;
        p->has_child = true;
    }
}

void gmb_canvas_panel_layout(gmb_canvas_panel_t* p, gmb_rect_t area)
{
    p->area = area;
    if (p->has_child)
        p->child.area = gmb_canvas_panel_inner(p);
}

gmb_rect_t gmb_canvas_panel_inner(gmb_canvas_panel_t* p)
{
    gmb_rect_t r = { p->area.x, p->area.y, 0, 0 };
    if (p->area.w < 2 || p->area.h < 2) return r;
    r.x = p->area.x + 1;
    r.y = p->area.y + 1;
    r.w = p->area.w - 2;
    r.h = p->area.h - 2;
    return r;
}

void gmb_canvas_panel_draw(gmb_canvas_panel_t* p, gmb_canvas_t* canvas)
{
    gmb_canvas_t frame_canvas;
    gmb_canvas_new(&frame_canvas, canvas->buffer, p->area);
    gmb_rect_t r = { 0, 0, p->area.w, p->area.h };
    const gmb_cell_color_t* fg = p->has_border_fg ? &p->border_fg : NULL;
    const gmb_cell_color_t* bg = p->has_border_bg ? &p->border_bg : NULL;
    gmb_canvas_box(&frame_canvas, r, fg, bg);
    if (p->has_title && p->area.w > 4)
    {
        char t[GMB_TEXT_MAX_CONTENT];
        snprintf(t, sizeof(t), "%s", p->title);
        int max = p->area.w - 4;
        int tl = (int)strlen(t);
        if (tl > max) t[max] = '\0';
        char disp[GMB_TEXT_MAX_CONTENT + 2];
        snprintf(disp, sizeof(disp), " %s ", t);
        gmb_canvas_draw_string_n(&frame_canvas, 2, 0, disp, p->area.w - 2, fg, bg);
    }
    if (p->has_child)
        gmb_canvas_label_draw(&p->child, canvas);
}
