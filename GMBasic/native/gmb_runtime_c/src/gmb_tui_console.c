/* ============================================================================
 * gmb_tui_console.c — costanti/eventi/adattatori terminale (porting di
 * ClipperLib/Clipper Consts.cs, UiEvents.cs, InkeyAdapter.cs, TermUtils.cs).
 * ============================================================================ */
#include "gmb_tui_console.h"
#include "gmb_tui_clipper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gmb_inkey_to_event(const gmb_ui_event_t* ev)
{
    if (!ev) return -1;
    switch (ev->type)
    {
        case GMB_UIEVENT_KEY: return ev->key;
        case GMB_UIEVENT_MOUSE: return gmb_mouse_action_inkey(&ev->mouse);
        default: return -1;
    }
}

void gmb_inkey_to_uievent(int inkey, gmb_clipper_state_t* st, gmb_ui_event_t* out)
{
    memset(out, 0, sizeof(*out));
    if (inkey <= GMB_MOUSE_BASE)
    {
        gmb_mouse_action_t ma;
        if (gmb_mouse_action_from_inkey(inkey, st, &ma))
        {
            out->type = GMB_UIEVENT_MOUSE;
            out->mouse = ma;
            return;
        }
    }
    out->type = GMB_UIEVENT_KEY;
    out->key = inkey;
    out->mods = GMB_MODKEY_NONE;
}

int gmb_termutils_show_cursor(bool on, char* out, int cap)
{
    const char* s = on ? GMB_ANSI_CURSORSHOW : GMB_ANSI_CURSORHIDE;
    snprintf(out, cap, "%s", s);
    return (int)strlen(out);
}

int gmb_termutils_alternate_screen(bool on, char* out, int cap)
{
    const char* s = on ? GMB_ANSI_ALTSCREENIN : GMB_ANSI_ALTSCREENOUT;
    snprintf(out, cap, "%s", s);
    return (int)strlen(out);
}

int gmb_termutils_set_title(const char* t, char* out, int cap)
{
    /* OSC 2;title ST */
    snprintf(out, cap, "\x1b]2;%s\x1b\\", t ? t : "");
    return (int)strlen(out);
}

int gmb_termutils_enable_mouse_capture(char* out, int cap)
{
    snprintf(out, cap, "%s%s", "\x1b[?1000h", "\x1b[?1006h");
    return (int)strlen(out);
}

int gmb_termutils_disable_mouse_capture(char* out, int cap)
{
    snprintf(out, cap, "%s%s%s", "\x1b[?1006l", "\x1b[?1000l", "\x1b[?1004l");
    return (int)strlen(out);
}

int gmb_termutils_enable_bracketed_paste(char* out, int cap)
{
    snprintf(out, cap, "%s", "\x1b[?2004h");
    return (int)strlen(out);
}

int gmb_termutils_disable_bracketed_paste(char* out, int cap)
{
    snprintf(out, cap, "%s", "\x1b[?2004l");
    return (int)strlen(out);
}

int gmb_termutils_move_to(int x, int y, char* out, int cap)
{
    int col = x < 1 ? 1 : x;
    int row = y < 1 ? 1 : y;
    snprintf(out, cap, "\x1b[%d;%dH", row, col);
    return (int)strlen(out);
}

int gmb_termutils_home(char* out, int cap)
{
    snprintf(out, cap, "%s", GMB_ANSI_HOME);
    return (int)strlen(out);
}

int gmb_termutils_clear_screen(char* out, int cap)
{
    snprintf(out, cap, "%s", "\x1b[2J");
    return (int)strlen(out);
}

int gmb_termutils_clear_line(char* out, int cap)
{
    snprintf(out, cap, "%s", "\x1b[2K");
    return (int)strlen(out);
}

void gmb_termutils_query_size(int* w, int* h)
{
    int W = 80, H = 24;
    const char* c = getenv("COLUMNS");
    const char* l = getenv("LINES");
    if (c) { int v = atoi(c); if (v > 0) W = v; }
    if (l) { int v = atoi(l); if (v > 0) H = v; }
    if (w) *w = W;
    if (h) *h = H;
}

int gmb_consts_fg256(uint8_t color, char* out, int cap)
{
    snprintf(out, cap, "\x1b[38;5;%um", color);
    return (int)strlen(out);
}

int gmb_consts_bg256(uint8_t color, char* out, int cap)
{
    snprintf(out, cap, "\x1b[48;5;%um", color);
    return (int)strlen(out);
}

int gmb_consts_fg_rgb(uint8_t r, uint8_t g, uint8_t b, char* out, int cap)
{
    snprintf(out, cap, "\x1b[38;2;%u;%u;%um", r, g, b);
    return (int)strlen(out);
}

int gmb_consts_bg_rgb(uint8_t r, uint8_t g, uint8_t b, char* out, int cap)
{
    snprintf(out, cap, "\x1b[48;2;%u;%u;%um", r, g, b);
    return (int)strlen(out);
}

int gmb_consts_cursor_goto(int line, int column, char* out, int cap)
{
    snprintf(out, cap, "\x1b[%d;%dH", line, column);
    return (int)strlen(out);
}

int gmb_consts_cursor_up(int rows, char* out, int cap)
{
    snprintf(out, cap, "\x1b[%dA", rows);
    return (int)strlen(out);
}

int gmb_consts_cursor_down(int rows, char* out, int cap)
{
    snprintf(out, cap, "\x1b[%dB", rows);
    return (int)strlen(out);
}

int gmb_consts_cursor_right(int cols, char* out, int cap)
{
    snprintf(out, cap, "\x1b[%dC", cols);
    return (int)strlen(out);
}

int gmb_consts_cursor_left(int cols, char* out, int cap)
{
    snprintf(out, cap, "\x1b[%dD", cols);
    return (int)strlen(out);
}

int gmb_consts_screen_change(uint8_t value, char* out, int cap)
{
    snprintf(out, cap, "\x1b[=%uh", value);
    return (int)strlen(out);
}

int gmb_termutils_enable_mouse_capture_motion(char* out, int cap)
{
    snprintf(out, cap, "%s%s", "\x1b[?1003h", "\x1b[?1006h");
    return (int)strlen(out);
}
