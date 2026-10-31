/* gmb_console_extra.h — parità CommonLib/Consoles: ScreenSnapshot.cs, TeeWriter.cs,
 * Headless.cs, SystemConsole.cs. Port completo, nessun bridge C↔C#.
 */
#ifndef GMB_CONSOLE_EXTRA_H
#define GMB_CONSOLE_EXTRA_H

#include "gmb_runtime.h"
#include "gmb_tui.h"

/* ---- ScreenSnapshot (ScreenSnapshot.cs:91) ---- */
typedef struct gmb_screen_snapshot {
    char label[64];
    int width, height;
    uint32_t* chars; /* w*h codepoints (copia) */
} gmb_screen_snapshot_t;

int gmb_screen_snapshot_capture(const char* label, gmb_screen_buffer_t* buffer, gmb_screen_snapshot_t* out);
void gmb_screen_snapshot_free(gmb_screen_snapshot_t* s);
int gmb_screen_snapshot_render_text(const gmb_screen_snapshot_t* s, char* out, int cap);      /* trim end */
int gmb_screen_snapshot_render_text_raw(const gmb_screen_snapshot_t* s, char* out, int cap); /* no trim */
int gmb_screen_snapshot_contains(const gmb_screen_snapshot_t* s, const char* text);          /* Ordinal */
int gmb_screen_snapshot_contains_error_indicators(const gmb_screen_snapshot_t* s);

/* ---- TeeWriter (TeeWriter.cs:95) — scrive a due destinazioni ---- */
typedef struct gmb_tee_writer {
    gmb_text_writer_t a, b;
} gmb_tee_writer_t;
void gmb_tee_writer_init(gmb_tee_writer_t* t, gmb_text_writer_t a, gmb_text_writer_t b);
int gmb_tee_writer_write(gmb_tee_writer_t* t, const char* s, int len);

/* ---- HeadlessConsole (Headless.cs) — output lineare, niente render ---- */
typedef struct gmb_headless_console {
    gmb_text_writer_t linear_out;
    char title[128];
    int fg, bg;
} gmb_headless_console_t;
void gmb_headless_console_init(gmb_headless_console_t* c, gmb_text_writer_t linear_out);
void gmb_headless_console_write(gmb_headless_console_t* c, const char* s);
void gmb_headless_console_write_line(gmb_headless_console_t* c, const char* s);

/* ---- SystemConsole (SystemConsole.cs) — wrappano Console (stdin/stdout/beep) ---- */
int gmb_system_console_key_available(void);   /* Console.KeyAvailable (headless: 0) */
int gmb_system_console_read(void);            /* Console.Read → getchar (EOF=-1) */
char* gmb_system_console_read_line(void);     /* Console.ReadLine (allocata; free()) */
void gmb_system_console_beep(void);           /* Console.Beep */

#endif
