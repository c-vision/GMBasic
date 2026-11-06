/* gmb_sdl_console.h — parità CommonGraphics/Consoles/SdlConsole*.cs (console config-accepting)
 * Emulatore di terminale: buffer celle (ITerminalBuffer), cursore, viewport, scroll region,
 * palette, processore escape (EscapeSequences.cs), streams. Accetta una gmb_screen_config_t.
 * Port completo in C: nessun bridge C↔C#.
 */
#ifndef GMB_SDL_CONSOLE_H
#define GMB_SDL_CONSOLE_H

#include "gmb_screen_config.h"
#include "gmb_tui.h"

typedef enum gmb_cursor_style { GMB_CS_BLOCK = 0, GMB_CS_UNDERLINE, GMB_CS_BAR } gmb_cursor_style_t;

typedef struct gmb_sdl_console {
    gmb_screen_config_t config;
    gmb_screen_buffer_t* buffer;          /* ITerminalBuffer: griglia celle */
    /* stato cursore/vista */
    int cursor_row, cursor_col;           /* 0-based */
    int cursor_visible;
    gmb_cursor_style_t cursor_style;
    /* lampeggio del cursore (SdlConsole: BLINK_MS = 400) */
    int blink_on;
    long long blink_at;
    int view_top, view_left, view_width, view_height;
    int scroll_top, scroll_bottom;        /* -1 = fine */
    /* attributi correnti (ITerminalBuffer) */
    int current_fg, current_bg;           /* indici palette */
    int attributes;
    unsigned char palette[GMB_SCFG_MAX_PALETTE][3];
    int palette_size;
    /* processore escape */
    int esc_state;                        /* 0=none 1='[' 2=']' 3=param 4='(' ')' */
    char esc_param[32];
    int esc_param_len;
    int dirty;
    /* scroll mode (StandardConsole.cs:84-87,409-432) */
    int native_scroll_mode;               /* 1=scrivi al terminale reale, 0=al buffer */
    /* rendering proprio (SdlConsole.cs/Internals.cs) */
    void* context;                        /* gmb_sdl_context_t* (heap) */
    void* font_atlas;                     /* gmb_font_atlas_t* */
    void* texture;                        /* SDL_Texture* (screen target) */
    int char_width, char_height;          /* metriche cella in pixel nativi */
    int render_scale;                     /* output px / window points */
    int opened, visible;
    char font_path[1024];
    int font_size;
} gmb_sdl_console_t;

/* Crea una console a partire dalla config (SdlConsole(ScreenConfig) — SdlConsole.cs:93).
 * width/height (celle) da config.char_grid oppure logical; 0 = default. */
gmb_sdl_console_t* gmb_sdl_console_new(const gmb_screen_config_t* config, int width, int height);
void gmb_sdl_console_free(gmb_sdl_console_t* c);

/* --- ITerminalBuffer (ITerminalBuffer.cs:14-74) --- */
int gmb_sdl_console_current_fg(const gmb_sdl_console_t* c);
int gmb_sdl_console_current_bg(const gmb_sdl_console_t* c);
void gmb_sdl_console_set_current_fg(gmb_sdl_console_t* c, int fg);
void gmb_sdl_console_set_current_bg(gmb_sdl_console_t* c, int bg);
int gmb_sdl_console_cursor_row(const gmb_sdl_console_t* c);
int gmb_sdl_console_cursor_col(const gmb_sdl_console_t* c);
int gmb_sdl_console_cursor_visible(const gmb_sdl_console_t* c);
void gmb_sdl_console_set_cursor_visible(gmb_sdl_console_t* c, int vis);
gmb_cursor_style_t gmb_sdl_console_cursor_style(const gmb_sdl_console_t* c);
void gmb_sdl_console_set_cursor_style(gmb_sdl_console_t* c, gmb_cursor_style_t st);
void gmb_sdl_console_print(gmb_sdl_console_t* c, char character);
void gmb_sdl_console_clear_screen(gmb_sdl_console_t* c);
void gmb_sdl_console_set_cursor_position(gmb_sdl_console_t* c, int row, int col); /* 1-based */
void gmb_sdl_console_erase_to_eol(gmb_sdl_console_t* c);

/* --- EscapeSequences (EscapeSequences.cs:15-47) --- */
void gmb_sdl_console_process_char(gmb_sdl_console_t* c, char ch);

/* --- scrittura testo (Writer) --- */
void gmb_sdl_console_write(gmb_sdl_console_t* c, const char* s);
void gmb_sdl_console_write_line(gmb_sdl_console_t* c, const char* s);

/* celle (per render): buffer condiviso */
gmb_screen_buffer_t* gmb_sdl_console_screen(const gmb_sdl_console_t* c);

/* --- StandardConsole scroll mode (StandardConsole.cs:84-87,409-432) --- */
int gmb_sdl_console_is_native_scroll_mode(const gmb_sdl_console_t* c);
void gmb_sdl_console_set_scroll_mode(gmb_sdl_console_t* c, int native);
void gmb_console_clear_scrollback(void); /* ANSI/xterm ESC[3J (StandardConsole.cs:432) */

/* --- rendering proprio = SdlConsole (SdlConsole.cs ctor + Internals UpdateFontAndRecalibrate/RenderBuffer) ---
 * SdlConsole possiede il proprio contesto SDL + font + texture; le primitive grafiche
 * (gmb_sdl_context / gmb_font_atlas / gmb_sdl_render) sono usate SOLO per disegnare. */
int  gmb_sdl_console_open(gmb_sdl_console_t* c);          /* crea contesto + font + texture (idempotente) */
void gmb_sdl_console_update_font(gmb_sdl_console_t* c);   /* ricalibra font/griglia/finestra (SetFont) */
void gmb_sdl_console_render(gmb_sdl_console_t* c);
/* Risolve gli indici di palette delle celle in RGB (serve al render ANSI:
 * gmb_render_screen usa solo i campi RGB, la print() scrive solo gli indici). */
void gmb_sdl_console_resolve_colors(gmb_sdl_console_t* c);        /* RenderBuffer: clear + celle + present */
void gmb_sdl_console_show(gmb_sdl_console_t* c);          /* Show(): apre e mostra la finestra */
void gmb_sdl_console_set_visibility(gmb_sdl_console_t* c, int visible);
void gmb_sdl_console_set_font(gmb_sdl_console_t* c, const char* path, int size);

/* Pump eventi SDL della finestra console: tastiera -> FIFO input TUI + modello console;
 * mouse -> handler celle (finestre/controlli rispondono). Ritorna 1 se quit richiesto.
 * Ridisegna le finestre TUI nel buffer della console. */
int  gmb_sdl_console_pump(gmb_sdl_console_t* c);
/* Screenshot PNG della finestra console (render corrente letto dal renderer SDL).
 * Ritorna 0 se scritto, -1 se non disponibile (build senza SDL / finestra non aperta). */
int  gmb_sdl_console_save_png(gmb_sdl_console_t* c, const char* path);
/* Status bar riusabile (SDL e console ANSI): disegna testo status + chip/raggruppamento
 * delle finestre minimizzate; hit-test del chip. */
void gmb_console_statusbar_draw(gmb_sdl_console_t* c);
int  gmb_console_statusbar_click(gmb_sdl_console_t* c, int cx, int cy);

#endif
