/* ============================================================================
 * gmb_tui_concodes.h — ConCodes (specchio ClipperLib/Clipper ConCodes.cs).
 * Encoder ANSI completo + decoder DEC state-machine. TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_CONCODES_H
#define GMB_TUI_CONCODES_H

#include "gmb_tui.h"
#include "gmb_tui_mouse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- SgrCode / Mode / PrivateMode / MouseMode (specchio ConCodes.cs) ---- */
enum {
    GMB_SGR_RESET = 0, GMB_SGR_BOLD = 1, GMB_SGR_HALF_BRIGHT = 2, GMB_SGR_UNDERSCORE = 4,
    GMB_SGR_BLINK = 5, GMB_SGR_REVERSE = 7, GMB_SGR_UNDERLINE = 21, GMB_SGR_NORMAL_INTENSITY = 22,
    GMB_SGR_UNDERLINE_OFF = 24, GMB_SGR_BLINK_OFF = 25, GMB_SGR_REVERSE_OFF = 27,
    GMB_SGR_STRIKETHROUGH = 9, GMB_SGR_OVERLINE = 53, GMB_SGR_DEFAULT_FG = 39, GMB_SGR_DEFAULT_BG = 49
};

/* ---- Encoder ConCodes ---- */
int gmb_concodes_cuu(unsigned short n, char* out, int cap);
int gmb_concodes_cud(unsigned short n, char* out, int cap);
int gmb_concodes_cuf(unsigned short n, char* out, int cap);
int gmb_concodes_cub(unsigned short n, char* out, int cap);
int gmb_concodes_cnl(unsigned short n, char* out, int cap);
int gmb_concodes_cpl(unsigned short n, char* out, int cap);
int gmb_concodes_cha(unsigned short n, char* out, int cap);
int gmb_concodes_cup(unsigned short row, unsigned short col, char* out, int cap);
int gmb_concodes_ed(unsigned short mode, char* out, int cap);
int gmb_concodes_el(unsigned short mode, char* out, int cap);
int gmb_concodes_sm(unsigned int mode, char* out, int cap);
int gmb_concodes_rm(unsigned int mode, char* out, int cap);
int gmb_concodes_psm(unsigned int mode, char* out, int cap);
int gmb_concodes_prm(unsigned int mode, char* out, int cap);
int gmb_concodes_sgr_raw(const unsigned int* codes, int n, char* out, int cap);
int gmb_concodes_sgr_fg(gmb_cell_color_t c, char* out, int cap);
int gmb_concodes_sgr_bright_fg(gmb_cell_color_t c, char* out, int cap);
int gmb_concodes_sgr_bg(gmb_cell_color_t c, char* out, int cap);
int gmb_concodes_sgr_bright_bg(gmb_cell_color_t c, char* out, int cap);
int gmb_concodes_sgr_fg256(unsigned char idx, char* out, int cap);
int gmb_concodes_sgr_bg256(unsigned char idx, char* out, int cap);
int gmb_concodes_sgr_fg24(unsigned char r, unsigned char g, unsigned char b, char* out, int cap);
int gmb_concodes_sgr_bg24(unsigned char r, unsigned char g, unsigned char b, char* out, int cap);
int gmb_concodes_set_title(const char* s, char* out, int cap);
int gmb_concodes_set_icon_name(const char* s, char* out, int cap);
int gmb_concodes_set_both(const char* s, char* out, int cap);
int gmb_concodes_set_working_dir(const char* s, char* out, int cap);
int gmb_concodes_notification(const char* s, char* out, int cap);
int gmb_concodes_enable_mouse(unsigned int mode, char* out, int cap);
int gmb_concodes_disable_mouse(unsigned int mode, char* out, int cap);
int gmb_concodes_save_cursor(char* out, int cap);
int gmb_concodes_restore_cursor(char* out, int cap);
int gmb_concodes_scroll_region(unsigned short top, unsigned short bottom, char* out, int cap);
int gmb_concodes_insert_lines(unsigned short n, char* out, int cap);
int gmb_concodes_delete_lines(unsigned short n, char* out, int cap);
int gmb_concodes_insert_chars(unsigned short n, char* out, int cap);
int gmb_concodes_delete_chars(unsigned short n, char* out, int cap);
int gmb_concodes_erase_chars(unsigned short n, char* out, int cap);

/* ---- Decoder types ---- */
enum {
    GMB_KEY_BACKSPACE = 1, GMB_KEY_ENTER, GMB_KEY_LEFT, GMB_KEY_RIGHT, GMB_KEY_UP, GMB_KEY_DOWN,
    GMB_KEY_HOME, GMB_KEY_END, GMB_KEY_PAGEUP, GMB_KEY_PAGEDOWN, GMB_KEY_TAB, GMB_KEY_BACKTAB,
    GMB_KEY_DELETE, GMB_KEY_INSERT, GMB_KEY_F, GMB_KEY_CHAR, GMB_KEY_NULL, GMB_KEY_ESC,
    GMB_KEY_CAPSLOCK, GMB_KEY_SCROLLLOCK, GMB_KEY_NUMLOCK, GMB_KEY_PRINTSCREEN, GMB_KEY_PAUSE,
    GMB_KEY_MENU, GMB_KEY_SPACE
};

typedef struct gmb_keycode {
    int type;         /* GMB_KEY_* */
    char ch;
    unsigned char f_num;
} gmb_keycode_t;

/* Mouse decoded (specchio ConCodes.cs Mouse records) */
enum { GMB_MOUSE_DOWN = 1, GMB_MOUSE_UP, GMB_MOUSE_DRAG, GMB_MOUSE_SCROLLUP, GMB_MOUSE_SCROLLDOWN, GMB_MOUSE_MOVE };

typedef struct gmb_mouse_dec {
    int kind;           /* GMB_MOUSE_* */
    int button;         /* GMB_MB_* */
    unsigned short x, y;
} gmb_mouse_dec_t;

enum { GMB_SGRCOL_DEFAULT = 0, GMB_SGRCOL_INDEXED, GMB_SGRCOL_RGB };

typedef struct gmb_sgr_color {
    int type;
    unsigned char index;
    unsigned char r, g, b;
} gmb_sgr_color_t;

enum {
    GMB_SA_RESET, GMB_SA_BOLD, GMB_SA_DIM, GMB_SA_ITALIC, GMB_SA_UNDERLINE, GMB_SA_BLINK,
    GMB_SA_RAPIDBLINK, GMB_SA_REVERSE, GMB_SA_HIDDEN, GMB_SA_STRIKE, GMB_SA_PRIMARY_FONT,
    GMB_SA_ALT_FONT, GMB_SA_FRAKTUR, GMB_SA_NOT_BOLD_OR_DIM, GMB_SA_NOT_ITALIC, GMB_SA_NOT_UNDERLINE,
    GMB_SA_NOT_BLINK, GMB_SA_NOT_REVERSE, GMB_SA_NOT_HIDDEN, GMB_SA_NOT_STRIKE, GMB_SA_FG,
    GMB_SA_FG_EXTENDED, GMB_SA_DEFAULT_FG, GMB_SA_BG, GMB_SA_BG_EXTENDED, GMB_SA_DEFAULT_BG,
    GMB_SA_FRAMED, GMB_SA_ENCIRCLED, GMB_SA_OVERLINED, GMB_SA_NOT_FRAMED_OR_ENCIRCLED, GMB_SA_NOT_OVERLINED
};

typedef struct gmb_sgr_attr {
    int type;
    unsigned char alt_font_n;
    gmb_sgr_color_t color;
} gmb_sgr_attr_t;

enum {
    GMB_TM_KEYBOARD_ACTION = 2, GMB_TM_INSERT_REPLACEMENT = 4, GMB_TM_SEND_RECEIVE = 12,
    GMB_TM_KEYPAD = 1, GMB_TM_INSERT = 4, GMB_TM_ORIGIN = 6, GMB_TM_AUTOWRAP = 7,
    GMB_TM_CURSOR_VISIBLE = 25, GMB_TM_ALTSCREEN = 47, GMB_TM_MOUSE_X10 = 9, GMB_TM_MOUSE_X11 = 1000,
    GMB_TM_MOUSE_BUTTON_EVENT = 1002, GMB_TM_MOUSE_ANY_EVENT = 1003, GMB_TM_FOCUS_EVENT = 1004,
    GMB_TM_MOUSE_UTF8 = 1005, GMB_TM_MOUSE_SGR = 1006, GMB_TM_MOUSE_URXVT = 1015,
    GMB_TM_MOUSE_SGR_PIXELS = 1016, GMB_TM_BRACKETED_PASTE = 2004, GMB_TM_UNKNOWN = -1
};

enum {
    GMB_WIN_DEICONIFY = 1, GMB_WIN_ICONIFY, GMB_WIN_MOVE, GMB_WIN_RESIZE_PIXELS, GMB_WIN_RAISE,
    GMB_WIN_LOWER, GMB_WIN_REFRESH, GMB_WIN_RESIZE_CHARS, GMB_WIN_MAXIMIZE_VERT, GMB_WIN_MAXIMIZE_HORZ,
    GMB_WIN_MAXIMIZE, GMB_WIN_RESTORE, GMB_WIN_UNKNOWN
};

typedef struct gmb_window_op {
    int type;
    int x, y;
    unsigned short w, h;
    unsigned long code;
} gmb_window_op_t;

enum {
    GMB_OSC_SET_BOTH = 0, GMB_OSC_SET_ICON_NAME, GMB_OSC_SET_TITLE, GMB_OSC_SET_WORKING_DIR,
    GMB_OSC_HYPERLINK, GMB_OSC_NOTIFICATION, GMB_OSC_CLIPBOARD, GMB_OSC_SET_COLOR_INDEX,
    GMB_OSC_SET_FG_COLOR, GMB_OSC_SET_BG_COLOR, GMB_OSC_SET_CURSOR_COLOR, GMB_OSC_UNKNOWN
};

typedef struct gmb_osc {
    int type;
    char text[512];
    char selector;
    unsigned char index;
    char param[128];
    char uri[384];
} gmb_osc_t;

/* ---- Sequence (tagged union dei record Sequence) ---- */
#define GMB_SEQ_MAX 64
#define GMB_SEQ_SGR_MAX 32

enum {
    GMB_SEQ_KEY, GMB_SEQ_MOUSE, GMB_SEQ_CURSOR_POSITION, GMB_SEQ_FOCUS_IN, GMB_SEQ_FOCUS_OUT,
    GMB_SEQ_PASTE, GMB_SEQ_OSC, GMB_SEQ_WINDOW_OP, GMB_SEQ_PRINT, GMB_SEQ_EXECUTE, GMB_SEQ_SGR,
    GMB_SEQ_CURSOR_UP, GMB_SEQ_CURSOR_DOWN, GMB_SEQ_CURSOR_FORWARD, GMB_SEQ_CURSOR_BACKWARD,
    GMB_SEQ_CURSOR_NEXT_LINE, GMB_SEQ_CURSOR_PREV_LINE, GMB_SEQ_CURSOR_COLUMN, GMB_SEQ_CURSOR_LINE,
    GMB_SEQ_ERASE_IN_DISPLAY, GMB_SEQ_ERASE_IN_LINE, GMB_SEQ_SCROLL_UP, GMB_SEQ_SCROLL_DOWN,
    GMB_SEQ_INSERT_LINES, GMB_SEQ_DELETE_LINES, GMB_SEQ_INSERT_CHARS, GMB_SEQ_DELETE_CHARS,
    GMB_SEQ_SET_SCROLL_REGION, GMB_SEQ_SAVE_CURSOR, GMB_SEQ_RESTORE_CURSOR, GMB_SEQ_SET_MODE,
    GMB_SEQ_RESET_MODE, GMB_SEQ_SOFT_RESET
};

typedef struct gmb_sequence {
    int type;
    gmb_keycode_t key;
    gmb_key_modifiers_t mods;
    gmb_mouse_dec_t mouse;
    unsigned short x, y;         /* posizioni / numeri generici */
    unsigned short n1, n2;       /* regioni / colonne / righe */
    int mode;                    /* TerminalMode */
    gmb_sgr_attr_t sgr[GMB_SEQ_SGR_MAX];
    int sgr_count;
    gmb_osc_t osc;
    gmb_window_op_t window;
    char paste[512];
    char ch;                     /* GMB_SEQ_PRINT */
    unsigned char ctrl;          /* GMB_SEQ_EXECUTE */
} gmb_sequence_t;

/* ---- Parser (specchio ConCodes.cs Parser) ---- */
typedef struct gmb_parser {
    bool emulator;
    /* engine state */
    unsigned long params[30];
    int param_count;
    unsigned long parameter;
    int ignored_count;
    int state;                   /* Engine.State */
    bool priv, sgr;
    unsigned char utf8_points[4];
    int utf8_count, utf8_expected;
    unsigned char osc[1024];
    int osc_count;
    bool esc_o;
    gmb_sequence_t seqs[GMB_SEQ_MAX];
    int seq_count;
} gmb_parser_t;

void gmb_parser_init(gmb_parser_t* p, bool emulator);
void gmb_parser_advance_byte(gmb_parser_t* p, unsigned char b, bool more);
void gmb_parser_advance(gmb_parser_t* p, const char* utf8, bool more);
int  gmb_parser_pending(const gmb_parser_t* p);
int  gmb_parser_drain(gmb_parser_t* p, gmb_sequence_t* out, int max);
/* SGR decode standalone */
int  gmb_concodes_decode_sgr(const unsigned long* params, int np, gmb_sgr_attr_t* out, int max);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_CONCODES_H */
