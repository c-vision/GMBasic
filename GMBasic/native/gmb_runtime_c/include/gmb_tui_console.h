/* ============================================================================
 * gmb_tui_console.h — costanti terminale + eventi UI + adattatori INKEY
 * (specchio ClipperLib/Clipper Consts.cs, UiEvents.cs, InkeyAdapter.cs, TermUtils.cs).
 * ============================================================================ */
#ifndef GMB_TUI_CONSOLE_H
#define GMB_TUI_CONSOLE_H

#include "gmb_tui.h"
#include "gmb_tui_mouse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Consts (specchio Consts.cs) ---- */
#define GMB_C_SEQ_ESC   "\x1b"
#define GMB_C_SEQ_CSI   "\x1b["
#define GMB_C_SEQ_DCS   "\x1bP"
#define GMB_C_SEQ_OSC   "\x1b]"

#define GMB_C_BELL      "\x07"
#define GMB_C_BS        "\x08"
#define GMB_C_HT        "\x09"
#define GMB_C_LF        "\x0A"
#define GMB_C_CR        "\x0D"
#define GMB_C_ESC       "\x1b"
#define GMB_C_DEL       "\x7F"

#define GMB_C_RESET          "\x1b[0m"
#define GMB_C_BOLD_ON        "\x1b[1m"
#define GMB_C_BOLD_OFF       "\x1b[22m"
#define GMB_C_FAINT_ON       "\x1b[2m"
#define GMB_C_ITALIC_ON      "\x1b[3m"
#define GMB_C_ITALIC_OFF     "\x1b[23m"
#define GMB_C_UNDERLINE_ON   "\x1b[4m"
#define GMB_C_UNDERLINE_OFF  "\x1b[24m"
#define GMB_C_BLINK_ON       "\x1b[5m"
#define GMB_C_BLINK_OFF      "\x1b[25m"
#define GMB_C_INVERSE_ON     "\x1b[7m"
#define GMB_C_INVERSE_OFF    "\x1b[27m"
#define GMB_C_HIDDEN_ON      "\x1b[8m"
#define GMB_C_HIDDEN_OFF     "\x1b[28m"
#define GMB_C_STRIKE_ON      "\x1b[9m"
#define GMB_C_STRIKE_OFF     "\x1b[29m"

#define GMB_C_FG_BLACK    "\x1b[30m"
#define GMB_C_FG_RED      "\x1b[31m"
#define GMB_C_FG_GREEN    "\x1b[32m"
#define GMB_C_FG_YELLOW   "\x1b[33m"
#define GMB_C_FG_BLUE     "\x1b[34m"
#define GMB_C_FG_MAGENTA  "\x1b[35m"
#define GMB_C_FG_CYAN     "\x1b[36m"
#define GMB_C_FG_WHITE    "\x1b[37m"
#define GMB_C_FG_DEFAULT  "\x1b[39m"
#define GMB_C_FG_BRIGHT_BLACK   "\x1b[90m"
#define GMB_C_FG_BRIGHT_RED     "\x1b[91m"
#define GMB_C_FG_BRIGHT_GREEN   "\x1b[92m"
#define GMB_C_FG_BRIGHT_YELLOW  "\x1b[93m"
#define GMB_C_FG_BRIGHT_BLUE    "\x1b[94m"
#define GMB_C_FG_BRIGHT_MAGENTA "\x1b[95m"
#define GMB_C_FG_BRIGHT_CYAN    "\x1b[96m"
#define GMB_C_FG_BRIGHT_WHITE   "\x1b[97m"

#define GMB_C_BG_BLACK    "\x1b[40m"
#define GMB_C_BG_RED      "\x1b[41m"
#define GMB_C_BG_GREEN    "\x1b[42m"
#define GMB_C_BG_YELLOW   "\x1b[43m"
#define GMB_C_BG_BLUE     "\x1b[44m"
#define GMB_C_BG_MAGENTA  "\x1b[45m"
#define GMB_C_BG_CYAN     "\x1b[46m"
#define GMB_C_BG_WHITE    "\x1b[47m"
#define GMB_C_BG_DEFAULT  "\x1b[49m"
#define GMB_C_BG_BRIGHT_BLACK   "\x1b[100m"
#define GMB_C_BG_BRIGHT_RED     "\x1b[101m"
#define GMB_C_BG_BRIGHT_GREEN   "\x1b[102m"
#define GMB_C_BG_BRIGHT_YELLOW  "\x1b[103m"
#define GMB_C_BG_BRIGHT_BLUE    "\x1b[104m"
#define GMB_C_BG_BRIGHT_MAGENTA "\x1b[105m"
#define GMB_C_BG_BRIGHT_CYAN    "\x1b[106m"
#define GMB_C_BG_BRIGHT_WHITE   "\x1b[107m"

#define GMB_C_CURSOR_HOME        "\x1b[H"
#define GMB_C_CURSOR_SCROLL_UP   "\x1bM"
#define GMB_C_CURSOR_POSITION    "\x1b[6n"
#define GMB_C_CURSOR_SAVE_DEC    "\x1b7"
#define GMB_C_CURSOR_RESTORE_DEC "\x1b8"
#define GMB_C_CURSOR_SAVE_SCO    "\x1b[s"
#define GMB_C_CURSOR_RESTORE_SCO "\x1b[u"

#define GMB_C_ERASE_TILL_END_OF_SCREEN    "\x1b[0J"
#define GMB_C_ERASE_FROM_BEGIN_OF_SCREEN  "\x1b[1J"
#define GMB_C_ERASE_SCREEN                "\x1b[2J"
#define GMB_C_ERASE_SAVED_LINES           "\x1b[3J"
#define GMB_C_ERASE_TILL_END_OF_LINE      "\x1b[0K"
#define GMB_C_ERASE_FROM_BEGIN_OF_LINE    "\x1b[1K"
#define GMB_C_ERASE_LINE                  "\x1b[2K"

#define GMB_C_CURSOR_INVISIBLE "\x1b[?25l"
#define GMB_C_CURSOR_VISIBLE  "\x1b[?25h"
#define GMB_C_RESTORE_SCREEN "\x1b[?47l"
#define GMB_C_SAVE_SCREEN    "\x1b[?47h"
#define GMB_C_ALT_BUFFER_ON  "\x1b[?1049h"
#define GMB_C_ALT_BUFFER_OFF "\x1b[?1049l"

#define GMB_C_CURSOR_BLINK_BLOCK   "\x1b[1q"
#define GMB_C_CURSOR_STEADY_BLOCK  "\x1b[2q"
#define GMB_C_CURSOR_BLINK_UNDERSCORE "\x1b[3q"
#define GMB_C_CURSOR_STEADY_UNDERSCORE "\x1b[4q"
#define GMB_C_CURSOR_BLINK_BAR "\x1b[5q"
#define GMB_C_CURSOR_STEADY_BAR "\x1b[6q"

/* Builder Consts: fg256/bg256/rgb/goto/cursor movimenti — funzioni qui sotto. */

/* ---- UiEvent (specchio UiEvents.cs) ---- */
enum { GMB_UIEVENT_NONE = 0, GMB_UIEVENT_KEY, GMB_UIEVENT_MOUSE, GMB_UIEVENT_RESIZE, GMB_UIEVENT_PASTE };

typedef struct gmb_ui_event {
    int type;
    int key;                    /* GMB_UIEVENT_KEY */
    gmb_key_modifiers_t mods;
    gmb_mouse_action_t mouse;   /* GMB_UIEVENT_MOUSE */
    int w, h;                   /* GMB_UIEVENT_RESIZE */
    char paste[512];            /* GMB_UIEVENT_PASTE */
} gmb_ui_event_t;

/* ---- InkeyAdapter (specchio InkeyAdapter.cs) ---- */
int gmb_inkey_to_event(const gmb_ui_event_t* ev);   /* UiEvent → INKEY */
void gmb_inkey_to_uievent(int inkey, gmb_clipper_state_t* st, gmb_ui_event_t* out);

/* ---- TermUtils (specchio TermUtils.cs) ---- */
int gmb_termutils_show_cursor(bool on, char* out, int cap);
int gmb_termutils_alternate_screen(bool on, char* out, int cap);
int gmb_termutils_set_title(const char* t, char* out, int cap);
int gmb_termutils_enable_mouse_capture(char* out, int cap);
int gmb_termutils_disable_mouse_capture(char* out, int cap);
int gmb_termutils_enable_bracketed_paste(char* out, int cap);
int gmb_termutils_disable_bracketed_paste(char* out, int cap);
int gmb_termutils_move_to(int x, int y, char* out, int cap);
int gmb_termutils_home(char* out, int cap);
int gmb_termutils_clear_screen(char* out, int cap);
int gmb_termutils_clear_line(char* out, int cap);
void gmb_termutils_query_size(int* w, int* h);

/* Builder Consts */
int gmb_consts_fg256(uint8_t color, char* out, int cap);
int gmb_consts_bg256(uint8_t color, char* out, int cap);
int gmb_consts_fg_rgb(uint8_t r, uint8_t g, uint8_t b, char* out, int cap);
int gmb_consts_bg_rgb(uint8_t r, uint8_t g, uint8_t b, char* out, int cap);
int gmb_consts_cursor_goto(int line, int column, char* out, int cap);
int gmb_consts_cursor_up(int rows, char* out, int cap);
int gmb_consts_cursor_down(int rows, char* out, int cap);
int gmb_consts_cursor_right(int cols, char* out, int cap);
int gmb_consts_cursor_left(int cols, char* out, int cap);
int gmb_consts_screen_change(uint8_t value, char* out, int cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_CONSOLE_H */
int gmb_termutils_enable_mouse_capture_motion(char* out, int cap);
