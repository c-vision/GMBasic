/* ============================================================================
 * gmb_tui_emu.h — emulatore VT (specchio ClipperLib/Clipper EmuCell.cs,
 * EmuGrid.cs, EmuRender.cs, Emulator.cs). OS-agnostico, TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_EMU_H
#define GMB_TUI_EMU_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_EMU_MAX_ROWS 64
#define GMB_EMU_MAX_COLS 240

typedef struct gmb_emu_style {
    bool has_fg;
    gmb_cell_color_t fg;
    bool has_bg;
    gmb_cell_color_t bg;
    bool bold, dim, italic, underline, reverse, blink;
} gmb_emu_style_t;

typedef struct gmb_emu_cell {
    uint32_t ch;
    gmb_emu_style_t style;
    bool is_wide;
    bool is_continuation;
} gmb_emu_cell_t;

typedef struct gmb_emu_grid {
    int cols, rows;
    gmb_emu_cell_t cells[GMB_EMU_MAX_ROWS][GMB_EMU_MAX_COLS];
    int cursor_x, cursor_y;
    int saved_x, saved_y;
    gmb_emu_style_t saved_style;
    gmb_emu_style_t default_style;
    bool auto_wrap;
    bool origin_mode;
    bool cursor_visible;
    int scroll_top, scroll_bottom;
    bool alt_active;
    gmb_emu_cell_t alt_cells[GMB_EMU_MAX_ROWS][GMB_EMU_MAX_COLS];
} gmb_emu_grid_t;

void gmb_emu_grid_init(gmb_emu_grid_t* g, int cols, int rows);
gmb_emu_cell_t* gmb_emu_grid_cell(gmb_emu_grid_t* g, int x, int y);
void gmb_emu_grid_set_cell(gmb_emu_grid_t* g, int x, int y, uint32_t ch, gmb_emu_style_t style,
                           bool wide, bool cont);
void gmb_emu_grid_clear(gmb_emu_grid_t* g);
void gmb_emu_grid_clear_rect(gmb_emu_grid_t* g, int x0, int y0, int x1, int y1);
void gmb_emu_grid_resize(gmb_emu_grid_t* g, int cols, int rows);
void gmb_emu_grid_scroll_up(gmb_emu_grid_t* g, int top, int bottom, int n);
void gmb_emu_grid_scroll_down(gmb_emu_grid_t* g, int top, int bottom, int n);
void gmb_emu_grid_insert_chars(gmb_emu_grid_t* g, int y, int at, int n);
void gmb_emu_grid_delete_chars(gmb_emu_grid_t* g, int y, int at, int n);
void gmb_emu_grid_new_line(gmb_emu_grid_t* g);
void gmb_emu_grid_carriage_return(gmb_emu_grid_t* g);
void gmb_emu_grid_set_scroll_region(gmb_emu_grid_t* g, int top1, int bottom1);
void gmb_emu_grid_save_cursor(gmb_emu_grid_t* g);
void gmb_emu_grid_restore_cursor(gmb_emu_grid_t* g);
void gmb_emu_grid_swap_alt_screen(gmb_emu_grid_t* g, bool on);

/* ---- Emulator (parser semplificato: output-only) ---- */
typedef struct gmb_emulator {
    gmb_emu_grid_t grid;
    gmb_emu_style_t cur_style;
    gmb_emu_style_t saved_style;
} gmb_emulator_t;

void gmb_emulator_init(gmb_emulator_t* e, int cols, int rows);
int  gmb_emulator_cols(const gmb_emulator_t* e);
int  gmb_emulator_rows(const gmb_emulator_t* e);
void gmb_emulator_cursor(const gmb_emulator_t* e, int* x, int* y);
void gmb_emulator_feed(gmb_emulator_t* e, const char* utf8);

/* ---- EmuRender ---- */
void gmb_emu_render_to_buffer(gmb_emulator_t* e, gmb_screen_buffer_t* buf, bool draw_cursor,
                              int off_x, int off_y);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_EMU_H */
