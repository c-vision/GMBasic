/* gmb_iconsole.h — port C di CommonLib/Consoles/IConsole.cs:50 (+ ITerminalBuffer.cs:14)
 * Interfaccia astratta di console con dispatch polimorfico (vtable), speculare al C#:
 *   SystemConsole / StandardConsole / SdlConsole / HeadlessConsole.
 * La console si sceglie ALL'AVVIO (GMB_CONSOLE, GMBASIC_HEADLESS) tramite gmb_iconsole_boot().
 * Nessun bridge C<->C#.
 */
#ifndef GMB_ICONSOLE_H
#define GMB_ICONSOLE_H

#include "gmb_screen_config.h"
#include "gmb_tui.h"
#include "gmb_runtime.h"   /* gmb_text_reader_t / gmb_text_writer_t (IConsole SetIn/Out/Error) */
#include "gmb_keyboard_extra.h"   /* gmb_terminal_key_info_t (IConsole.ReadKey) */

/* ScreenRect (IConsole.cs:8-12). */
typedef struct gmb_screen_rect { int x, y, w, h; } gmb_screen_rect_t;

/* CursorStyle (IConsole.cs:17-25). Distinto da gmb_cursor_style_t (SdlConsole, 3 forme). */
typedef enum gmb_cursor {
    GMB_CURSOR_BLINKING_BLOCK = 0,
    GMB_CURSOR_STEADY_BLOCK,
    GMB_CURSOR_BLINKING_UNDERLINE,
    GMB_CURSOR_STEADY_UNDERLINE,
    GMB_CURSOR_BLINKING_BAR,
    GMB_CURSOR_STEADY_BAR
} gmb_cursor_t;

typedef struct gmb_iconsole gmb_iconsole_t;

/* Vtable + stato comune (IConsole.cs). I TextReader/TextWriter C# sono resi con
 * gmb_text_reader_t/gmb_text_writer_t (vedi gmb_runtime.h); l'evento CancelKeyPress
 * e' un callback (gmb_iconsole_set_cancel_handler). */
struct gmb_iconsole {
    gmb_screen_config_t configuration;   /* IConsole.Configuration (IConsole.cs:72) */
    int auto_flush;                      /* IConsole.AutoFlush (IConsole.cs:59) */
    void* impl;                          /* stato dell'implementazione concreta */

    /* --- ciclo di vita --- */
    void (*dispose)(gmb_iconsole_t*);

    /* --- stato e aspetto (IConsole.cs:111-122) --- */
    const char* (*get_title)(gmb_iconsole_t*);
    void        (*set_title)(gmb_iconsole_t*, const char*);
    int  (*get_bg)(gmb_iconsole_t*);  void (*set_bg)(gmb_iconsole_t*, int);
    int  (*get_fg)(gmb_iconsole_t*);  void (*set_fg)(gmb_iconsole_t*, int);
    int  (*get_buffer_width)(gmb_iconsole_t*);
    int  (*get_buffer_height)(gmb_iconsole_t*);
    void (*set_buffer_size)(gmb_iconsole_t*, int w, int h);
    int  (*get_window_width)(gmb_iconsole_t*);
    int  (*get_window_height)(gmb_iconsole_t*);
    void (*set_window_size)(gmb_iconsole_t*, int w, int h);
    int  (*get_window_left)(gmb_iconsole_t*);
    int  (*get_window_top)(gmb_iconsole_t*);
    void (*set_window_position)(gmb_iconsole_t*, int left, int top);
    int  (*get_cursor_left)(gmb_iconsole_t*);
    int  (*get_cursor_top)(gmb_iconsole_t*);
    void (*set_cursor_position)(gmb_iconsole_t*, int left, int top);
    int  (*get_cursor_visible)(gmb_iconsole_t*);
    void (*set_cursor_visible)(gmb_iconsole_t*, int visible);
    void (*set_cursor_style)(gmb_iconsole_t*, gmb_cursor_t style);

    /* --- output (IConsole.cs:126-137) --- */
    void (*beep)(gmb_iconsole_t*);
    void (*clear)(gmb_iconsole_t*);
    void (*reset_color)(gmb_iconsole_t*);
    void (*set_scroll_region)(gmb_iconsole_t*, int top, int bottom);

    /* IConsole.IsDirty (IConsole.cs:144) */
    int  (*is_dirty)(gmb_iconsole_t*);

    /* --- ciclo di vita non-standard (IConsole.cs:146-177) --- */
    int  (*process_events)(gmb_iconsole_t*);   /* true = chiusura richiesta */
    void (*show)(gmb_iconsole_t*);
    void (*present)(gmb_iconsole_t*);
    void (*set_visibility)(gmb_iconsole_t*, int visible);
    void (*set_font)(gmb_iconsole_t*, const char* font_path, int font_size);

    /* --- appunti (IConsole.cs:200-206) --- */
    void (*copy_buffer_to_clipboard)(gmb_iconsole_t*);
    void (*paste_from_clipboard)(gmb_iconsole_t*);

    /* --- palette (IConsole.cs:215-226) --- */
    void (*set_palette_color)(gmb_iconsole_t*, int index, int r, int g, int b);

    /* --- streams (IConsole.cs:74-107) --- */
    void (*write)(gmb_iconsole_t*, const char* text);   /* Out/Error */
    int  (*read_char)(gmb_iconsole_t*);                 /* In.Read() */

    /* --- buffer di celle (IConsole.cs:228-265) --- */
    void (*get_cell)(gmb_iconsole_t*, int x, int y, gmb_screen_cell_t* out);
    void (*put_cell)(gmb_iconsole_t*, int x, int y, gmb_screen_cell_t cell);
    void (*clear_rect)(gmb_iconsole_t*, gmb_screen_rect_t rect, const gmb_screen_cell_t* fill);

    /* --- stream (IConsole.cs:95-107) --- */
    void (*set_in)(gmb_iconsole_t*, gmb_text_reader_t);
    void (*set_out)(gmb_iconsole_t*, gmb_text_writer_t);
    void (*set_error)(gmb_iconsole_t*, gmb_text_writer_t);
    /* AutoFlush (IConsole.cs:59) */
    int  (*get_auto_flush)(gmb_iconsole_t*);
    void (*set_auto_flush)(gmb_iconsole_t*, int);
    /* palette attiva (IConsole.cs:220-226) */
    void (*set_active_palette)(gmb_iconsole_t*, const unsigned char* palette, int n, const char* name);
    const char* (*get_active_palette_name)(gmb_iconsole_t*);
    /* letto/scritto blocco rettangolare (IConsole.cs:241-263) */
    void (*get_rect)(gmb_iconsole_t*, gmb_screen_rect_t rect, gmb_screen_cell_t* out);
    void (*put_rect)(gmb_iconsole_t*, int x, int y, int w, int h, const gmb_screen_cell_t* cells);
    /* window left/top singoli (IConsole.cs:118-119) */
    void (*set_window_left)(gmb_iconsole_t*, int);
    void (*set_window_top)(gmb_iconsole_t*, int);
    /* lettura (IConsole: In) */
    int  (*read)(gmb_iconsole_t*);
    int  (*read_line)(gmb_iconsole_t*, char* out, int cap);
    int  (*read_key)(gmb_iconsole_t*, gmb_terminal_key_info_t* out, int intercept);
    /* CancelKeyPress (IConsole.cs:65) */
    void (*set_cancel_handler)(gmb_iconsole_t*, void* ctx, int (*handler)(void*, void*));
    /* Headless (Headless.cs:124,135) */
    char* (*get_all_screen_text)(gmb_iconsole_t*);
    /* StandardConsole (StandardConsole.cs:702) */
    void (*poll_for_resize)(gmb_iconsole_t*);
    /* SdlConsole (SdlConsole.Internals.cs:235) */
    void (*reset_to_initial_state)(gmb_iconsole_t*);
};

/* --- Costruttori (uno per implementazione C#) --- */
gmb_iconsole_t* gmb_iconsole_system_new(const gmb_screen_config_t* config);   /* SystemConsole.cs:20 */
gmb_iconsole_t* gmb_iconsole_standard_new(const gmb_screen_config_t* config); /* StandardConsole.cs:16 */
gmb_iconsole_t* gmb_iconsole_headless_new(const gmb_screen_config_t* config); /* Headless.cs:20 */
gmb_iconsole_t* gmb_iconsole_sdl_new(const gmb_screen_config_t* config);      /* SdlConsole.cs:22 */

/* --- Factory / selezione all'avvio ---
 * mode = GMB_CONSOLE (0=%SDL, 1=C64, 2=ANSI, 3=SYSTEM; vedi gmb_console_select).
 * Se GMBASIC_HEADLESS e' impostato, ritorna sempre una HeadlessConsole. */
gmb_iconsole_t* gmb_iconsole_create(int mode, const gmb_screen_config_t* config);
/* Legge GMB_CONSOLE/GMBASIC_HEADLESS, crea la console d'avvio e la rende attiva. */
gmb_iconsole_t* gmb_iconsole_boot(void);

/* Console attiva (per il routing di PRINT/present). */
gmb_iconsole_t* gmb_iconsole_active(void);
void            gmb_iconsole_set_active(gmb_iconsole_t* c);

/* Routing (parità main loop C#: activeConsole.Out / activeConsole.Present()).
 * Ritorna 1 se la console attiva GUIDA l'output (Standard/System espliciti, o SDL con
 * finestra reale); 0 altrimenti (default SDL headless → stdout come prima). */
int  gmb_iconsole_drives_output(void);
/* Scrive il testo sul canale Out della console attiva; 1 se gestito, 0 → fallback stdout. */
int  gmb_iconsole_try_write(const char* text);
/* Present() della console attiva se sporca; 1 se presentato. */
int  gmb_iconsole_present_if_active(void);
/* 1 se la console attiva e' la SDL con finestra aperta (sopprime il dump ANSI). */
int  gmb_iconsole_sdl_active(void);

/* Modello SdlConsole della console attiva (solo se e' SDL/SDL-C64), altrimenti NULL.
 * Serve alla REPL per pompare eventi/rendere/leggere tasti DENTRO la finestra. */
struct gmb_sdl_console* gmb_iconsole_sdl_model(gmb_iconsole_t* c);

/* TeeWriter + ScreenSnapshot sono in gmb_console_extra.h (gia' portati). */

#endif /* GMB_ICONSOLE_H */
