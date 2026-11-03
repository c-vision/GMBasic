/* ============================================================================
 * gmb_tui_io.h — I/O console (specchio ClipperLib/Clipper RawTerminal.cs,
 * TerminalReader.cs, RawInputReader.cs). I/O reale stdin//dev/tty.
 * ============================================================================ */
#ifndef GMB_TUI_IO_H
#define GMB_TUI_IO_H

#include "gmb_tui.h"
#include "gmb_tui_console.h"
#include "gmb_tui_mouse.h"
#include "gmb_tui_clipper.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- RawTerminal (specchio RawTerminal.cs) ---- */
bool gmb_raw_terminal_is_platform_supported(void);
int  gmb_raw_terminal_tty_fd(void);
void gmb_raw_terminal_enable_raw(void);
void gmb_raw_terminal_disable_raw(void);

/* ---- TerminalReader (specchio TerminalReader.cs) ---- */
bool gmb_terminal_reader_is_event_available(void);
bool gmb_terminal_reader_is_event_available_timeout(int timeout_ms);
int  gmb_terminal_reader_read_event_core(gmb_clipper_state_t* st, gmb_ui_event_t* out);
int  gmb_terminal_reader_decode_csi_key(const char* seq);
int  gmb_terminal_reader_decode_csi_key_len(const char* seq, int len);

/* ---- RawInputReader (specchio RawInputReader.cs, Unix termios) ---- */
bool gmb_raw_input_reader_has_byte(void);
bool gmb_raw_input_reader_is_event_available(void);
int  gmb_raw_input_reader_read_event_core(gmb_clipper_state_t* st, gmb_ui_event_t* out);

/* ── Imbuto input (gmb_input_feeder.c) ─────────────────────────────────────────────────
 * Il terminale è un PRODUTTORE della coda eventi globale (gmb_gfx_input), come la pump
 * SDL: gmb_input_pump() drena i tasti pronti e li immette nella coda; INKEY/WAITKEY/GET,
 * ON KEY e i controlli TUI consumano SOLO la coda (nessun lettore diretto del tty).
 * gmb_input_terminal_available(): 1 = c'è un tty reale (no grafica, no script iniettato). */
bool gmb_input_terminal_available(void);
int  gmb_input_pump(void);
void gmb_raw_input_reader_shutdown(void);
void gmb_raw_input_reader_ensure_raw(void);

/* Dimensione del terminale (colonne/righe) via ioctl(TIOCGWINSZ); fallback 80x25. */
void gmb_terminal_size(int* w, int* h);

/* Lettura eventi con priorità typeahead scriptata GMB_TUI_SCRIPT (loop TBROWSE in CI). */
int  gmb_browse_read_event(gmb_clipper_state_t* st, gmb_ui_event_t* out);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_IO_H */
