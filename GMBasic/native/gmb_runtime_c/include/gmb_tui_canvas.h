/* ============================================================================
 * gmb_tui_canvas.h — Centaurea canvas/widget (specchio ClipperLib/Clipper
 * Centaurea.cs). Superficie di disegno su sub-rettangolo di ScreenBuffer.
 * ============================================================================ */
#ifndef GMB_TUI_CANVAS_H
#define GMB_TUI_CANVAS_H

#include "gmb_tui.h"
#include "gmb_tui_text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmb_canvas {
    gmb_screen_buffer_t* buffer;
    gmb_rect_t frame;
} gmb_canvas_t;

void gmb_canvas_new(gmb_canvas_t* c, gmb_screen_buffer_t* buffer, gmb_rect_t frame);
void gmb_canvas_full(gmb_canvas_t* c, gmb_screen_buffer_t* buffer);
void gmb_canvas_set_frame(gmb_canvas_t* c, gmb_rect_t frame);
void gmb_canvas_pixel(gmb_canvas_t* c, int x, int y, uint32_t ch,
                      const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_fill(gmb_canvas_t* c, uint32_t ch, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_rect_fill(gmb_canvas_t* c, gmb_rect_t rect, uint32_t ch,
                          const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_draw_string(gmb_canvas_t* c, int x, int y, const char* text,
                            const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_draw_string_n(gmb_canvas_t* c, int x, int y, const char* text, int max_width,
                              const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
int  gmb_canvas_draw_line(gmb_canvas_t* c, int x, int y, const gmb_line_t* line, int has_max, int max_width);
void gmb_canvas_draw_text(gmb_canvas_t* c, int x, int y, const gmb_text_t* text, int has_max, int max_width);
void gmb_canvas_hline(gmb_canvas_t* c, int x, int y, int len, uint32_t ch,
                      const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_vline(gmb_canvas_t* c, int x, int y, int len, uint32_t ch,
                      const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_segment(gmb_canvas_t* c, int x0, int y0, int x1, int y1, uint32_t ch,
                        const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_canvas_box(gmb_canvas_t* c, gmb_rect_t rect, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);

/* ---- CanvasLabel / CanvasPanel (specchio Centaurea.cs) ---- */
typedef struct gmb_canvas_label {
    gmb_text_t content;
    gmb_rect_t area;
} gmb_canvas_label_t;

void gmb_canvas_label_init(gmb_canvas_label_t* l, const char* text, gmb_rect_t area);
void gmb_canvas_label_draw(gmb_canvas_label_t* l, gmb_canvas_t* canvas);

typedef struct gmb_canvas_panel {
    char title[GMB_TEXT_MAX_CONTENT];
    bool has_title;
    bool has_child;
    gmb_canvas_label_t child;
    gmb_rect_t area;
    bool has_border_fg, has_border_bg;
    gmb_cell_color_t border_fg, border_bg;
} gmb_canvas_panel_t;

void gmb_canvas_panel_init(gmb_canvas_panel_t* p, gmb_rect_t area, const char* title, const gmb_canvas_label_t* child);
void gmb_canvas_panel_layout(gmb_canvas_panel_t* p, gmb_rect_t area);
gmb_rect_t gmb_canvas_panel_inner(gmb_canvas_panel_t* p);
void gmb_canvas_panel_draw(gmb_canvas_panel_t* p, gmb_canvas_t* canvas);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_CANVAS_H */
