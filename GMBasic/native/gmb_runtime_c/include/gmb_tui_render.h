/* ============================================================================
 * gmb_tui_render.h — Colorize + DoubleBuffer (specchio ClipperLib/Clipper
 * Colorize.cs, DoubleBuffer.cs). Puro, TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_RENDER_H
#define GMB_TUI_RENDER_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Colorize (specchio Colorize.cs) ---- */
int gmb_colorize_sgr(bool bold, bool dim, bool italic, bool underline,
                     bool has_fg, gmb_cell_color_t fg, bool has_bg, gmb_cell_color_t bg,
                     char* out, int cap);
int gmb_colorize_rgb_to_ansi256(gmb_cell_color_t c);
gmb_cell_color_t gmb_colorize_ansi256_to_rgb(int idx);

/* ---- DoubleBuffer (specchio DoubleBuffer.cs) ---- */
#define GMB_DIFF_MAX 4096

typedef struct gmb_cell_change {
    int x, y;
    uint32_t ch;
    gmb_cell_color_t fg, bg;
} gmb_cell_change_t;

typedef struct gmb_double_buffer {
    gmb_screen_buffer_t* previous;
} gmb_double_buffer_t;

void gmb_double_buffer_new(gmb_double_buffer_t* db);
void gmb_double_buffer_free(gmb_double_buffer_t* db);
/* Ritorna il numero di celle cambiate (≤ max). Non aggiorna il precedente. */
int gmb_double_buffer_compute_diff(gmb_double_buffer_t* db, gmb_screen_buffer_t* current,
                                   gmb_cell_change_t* out, int max);
/* Present: scrive ANSI minimale in out; aggiorna il buffer precedente. Ritorna len. */
int gmb_double_buffer_present(gmb_double_buffer_t* db, gmb_screen_buffer_t* current, char* out, int cap);
void gmb_double_buffer_invalidate(gmb_double_buffer_t* db);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_RENDER_H */
