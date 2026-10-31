/* ============================================================================
 * gmb_tui_io.c — I/O console (porting RawTerminal.cs/TerminalReader.cs/
 * RawInputReader.cs). I/O reale via stdin e /dev/tty (termios).
 * ============================================================================ */
#include "gmb_tui_io.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#include <io.h>
#include <windows.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

/* Dimensione del terminale (colonne/righe) via ioctl(TIOCGWINSZ) su stdout; fallback 80x25. */
void gmb_terminal_size(int* w, int* h)
{
#if defined(_WIN32) || defined(_WIN64)
    if (w) *w = 80;
    if (h) *h = 25;
#else
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
    {
        if (w) *w = (int)ws.ws_col;
        if (h) *h = (int)ws.ws_row;
    }
    else { if (w) *w = 80; if (h) *h = 25; }
#endif
}

/* ===================== RawTerminal ===================== */

bool gmb_raw_terminal_is_platform_supported(void)
{
#if defined(_WIN32) || defined(_WIN64)
    /* Windows 10+ console VT: le funzioni input raw sono disponibili ma il
     * fallback conio (_kbhit/_getch) copre tutte le versioni. */
    return true;
#else
    return true;
#endif
}
int gmb_raw_terminal_tty_fd(void) { return -1; }

static bool g_raw = false;
void gmb_raw_terminal_enable_raw(void) { g_raw = true; }
void gmb_raw_terminal_disable_raw(void) { g_raw = false; }

/* ===================== TerminalReader ===================== */

#if defined(_WIN32) || defined(_WIN64)
/* Windows: _kbhit per disponibilità, _getch per la lettura (console raw).
 * Nota: _getch consuma un byte; per i tasti estesi (0xE0/0x00) il secondo byte
 * va letto subito. Le sequenze ESC (mouse/SGR) sono gestite dal decode generico. */
static bool win_has_byte(void) { return _kbhit() != 0; }
static int win_get_byte(void)
{
    int c = _getch();
    if (c == 0 || c == 0xE0) { int c2 = _getch(); (void)c2; return 0; }
    return c;
}
#endif

bool gmb_terminal_reader_is_event_available(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return win_has_byte();
#else
    fd_set set;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    return select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0;
#endif
}

bool gmb_terminal_reader_is_event_available_timeout(int timeout_ms)
{
    if (timeout_ms <= 0) return gmb_terminal_reader_is_event_available();
    if (gmb_terminal_reader_is_event_available()) return true;
#if defined(_WIN32) || defined(_WIN64)
    Sleep(10);
#else
    usleep(10000);
#endif
    return gmb_terminal_reader_is_event_available();
}

static void set_stdin_raw(bool on)
{
#if defined(_WIN32) || defined(_WIN64)
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    static DWORD saved_mode = 0;
    static bool have_saved = false;
    DWORD m;
    if (h == INVALID_HANDLE_VALUE) return;
    if (on)
    {
        if (!have_saved) { GetConsoleMode(h, &saved_mode); have_saved = true; }
        m = saved_mode & ~(DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
        SetConsoleMode(h, m);
    }
    else if (have_saved)
    {
        SetConsoleMode(h, saved_mode);
    }
#else
    static struct termios saved;
    static bool have_saved = false;
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) != 0) return;
    if (on)
    {
        if (!have_saved) { saved = t; have_saved = true; }
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
    else if (have_saved)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    }
#endif
}

int gmb_terminal_reader_decode_csi_key_len(const char* seq, int len)
{
    if (len < 2) return 0;
    char last = seq[len - 1];
    switch (last)
    {
        case 'A': return GMB_K_UP;
        case 'B': return GMB_K_DOWN;
        case 'C': return GMB_K_RIGHT;
        case 'D': return GMB_K_LEFT;
        case 'H': return GMB_K_HOME;
        case 'F': return GMB_K_END;
        case 'Z': return GMB_K_BTAB;
        /* SS3 (ESC O <lettera>): F1-F4. */
        case 'P': return GMB_K_F1;
        case 'Q': return GMB_K_F2;
        case 'R': return GMB_K_F3;
        case 'S': return GMB_K_F4;
        case '~':
        {
            /* CSI ESC [ <num> ~ : num -> tasto (xterm). */
            int v = 0, i = 2, seen = 0;
            while (i < len - 1 && seq[i] >= '0' && seq[i] <= '9') { v = v * 10 + (seq[i] - '0'); i++; seen = 1; }
            if (!seen) return 0;
            switch (v)
            {
                case 11: return GMB_K_F1;  case 12: return GMB_K_F2;  case 13: return GMB_K_F3;
                case 14: return GMB_K_F4;  case 15: return GMB_K_F5;  case 17: return GMB_K_F6;
                case 18: return GMB_K_F7;  case 19: return GMB_K_F8;  case 20: return GMB_K_F9;
                case 21: return GMB_K_F10; case 23: return GMB_K_F11; case 24: return GMB_K_F12;
                default: return 0;
            }
        }
        default: return 0;
    }
}

int gmb_terminal_reader_decode_csi_key(const char* seq)
{
    if (!seq) return 0;
    return gmb_terminal_reader_decode_csi_key_len(seq, (int)strlen(seq));
}

/* legge un carattere non bloccante da stdin, -1 se assente */
static int peek_byte(int timeout_us)
{
#if defined(_WIN32) || defined(_WIN64)
    if (!win_has_byte()) return -1;
    return win_get_byte();
#else
    fd_set set;
    struct timeval tv;
    tv.tv_sec = timeout_us / 1000000;
    tv.tv_usec = timeout_us % 1000000;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) <= 0) return -1;
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    return c;
#endif
}

/* colleziona la sequenza ESC (fino a 64 byte) */
static int collect_esc_seq(char* out, int cap)
{
    int o = 0;
    out[o++] = 0x1B;
    for (int retry = 0; retry < 10 && o < cap; retry++)
    {
        int b = peek_byte(4000);
        if (b < 0) continue;
        out[o++] = (char)b;
        /* sequenze SGR mouse complete */
        if (o >= 3 && out[0] == 0x1B && out[1] == '[' && out[2] == '<')
        {
            for (int i = 3; i < o; i++)
                if (out[i] == 'M' || out[i] == 'm') return o;
        }
        if (o >= 3 && out[0] == 0x1B && out[1] == '[' && out[2] == 'M' && o >= 6)
            return o;
        char last = out[o - 1];
        if ((last >= 'a' && last <= 'z') || (last >= 'A' && last <= 'Z'))
            return o;
        retry = 0;
    }
    return o;
}

static int read_event_impl(gmb_clipper_state_t* st, gmb_ui_event_t* out)
{
    set_stdin_raw(true);
    int b = peek_byte(200000);
    if (b < 0)
    {
        set_stdin_raw(false);
        out->type = GMB_UIEVENT_NONE;
        return 0;
    }
    if (b != 0x1B)
    {
        set_stdin_raw(false);
        out->type = GMB_UIEVENT_KEY;
        switch (b)
        {
            case '\r': case '\n': out->key = GMB_K_ENTER; break;
            case 9: out->key = GMB_K_TAB; break;
            case 127: out->key = GMB_K_BS; break;
            default: out->key = (b >= 32 && b <= 126) ? b : 0; break;
        }
        out->mods = GMB_MODKEY_NONE;
        return 1;
    }
    /* ESC: attende la sequenza */
    char seq[64];
    int sl = collect_esc_seq(seq, 64);
    set_stdin_raw(false);

    gmb_mouse_action_t ma;
    if (gmb_mouse_decode_and_process(seq, &ma))
    {
        if (st)
        {
            st->mouse_x = ma.x;
            st->mouse_y = ma.y;
            st->mouse_double_clicked = (ma.kind == GMB_MA_DOUBLECLICK);
            bool shift = (ma.mods & GMB_MODKEY_SHIFT) != 0;
            bool ctrl = (ma.mods & GMB_MODKEY_CONTROL) != 0;
            for (int i = 0; i < 3; i++) { st->mouse_shift[i] = shift; st->mouse_ctrl[i] = ctrl; }
        }
        out->type = GMB_UIEVENT_MOUSE;
        out->mouse = ma;
        return 1;
    }
    if (sl >= 2 && seq[1] == '[')
    {
        int key = gmb_terminal_reader_decode_csi_key_len(seq, sl);
        if (key > 0)
        {
            out->type = GMB_UIEVENT_KEY;
            out->key = key;
            out->mods = GMB_MODKEY_NONE;
            return 1;
        }
    }
    out->type = GMB_UIEVENT_KEY;
    out->key = GMB_K_ESC;
    out->mods = GMB_MODKEY_NONE;
    return 1;
}

int gmb_terminal_reader_read_event_core(gmb_clipper_state_t* st, gmb_ui_event_t* out)
{
    return read_event_impl(st, out);
}

/* ===================== RawInputReader (termios /dev/tty) ===================== */

static int g_tty_fd = -1;
static bool g_is_raw = false;

bool gmb_raw_input_reader_has_byte(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return win_has_byte();
#else
    if (g_tty_fd < 0)
    {
        gmb_raw_input_reader_ensure_raw();
        if (g_tty_fd < 0) return gmb_terminal_reader_is_event_available();
    }
    fd_set set;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&set);
    FD_SET(g_tty_fd, &set);
    return select(g_tty_fd + 1, &set, NULL, NULL, &tv) > 0;
#endif
}

bool gmb_raw_input_reader_is_event_available(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return win_has_byte();
#else
    if (g_tty_fd < 0) return gmb_terminal_reader_is_event_available();
    char c;
    fd_set set;
    struct timeval tv = { 0, 10000 };
    FD_ZERO(&set);
    FD_SET(g_tty_fd, &set);
    if (select(g_tty_fd + 1, &set, NULL, NULL, &tv) <= 0) return false;
    if (read(g_tty_fd, &c, 1) == 1)
    {
        lseek(g_tty_fd, -1, SEEK_CUR);
        return true;
    }
    return false;
#endif
}

void gmb_raw_input_reader_ensure_raw(void)
{
#if defined(_WIN32) || defined(_WIN64)
    set_stdin_raw(true);
#else
    if (g_is_raw || g_tty_fd >= 0) return;
    g_tty_fd = open("/dev/tty", O_RDONLY);
    if (g_tty_fd < 0) return;
    struct termios t;
    if (tcgetattr(g_tty_fd, &t) != 0) { close(g_tty_fd); g_tty_fd = -1; return; }
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 20;
    if (tcsetattr(g_tty_fd, TCSANOW, &t) != 0) { close(g_tty_fd); g_tty_fd = -1; return; }
    g_is_raw = true;
#endif
}

int gmb_raw_input_reader_read_event_core(gmb_clipper_state_t* st, gmb_ui_event_t* out)
{
#if defined(_WIN32) || defined(_WIN64)
    if (!win_has_byte()) { out->type = GMB_UIEVENT_NONE; return 0; }
    int b = win_get_byte();
    if (b == 0) { out->type = GMB_UIEVENT_NONE; return 0; }
    if (b == 27)
    {
        out->type = GMB_UIEVENT_KEY;
        out->key = GMB_K_ESC;
        out->mods = GMB_MODKEY_NONE;
        return 1;
    }
    out->type = GMB_UIEVENT_KEY;
    switch (b)
    {
        case '\r': case '\n': out->key = GMB_K_ENTER; break;
        case 9: out->key = GMB_K_TAB; break;
        case 8: out->key = GMB_K_BS; break;
        default: out->key = (b >= 32 && b <= 126) ? b : 0; break;
    }
    out->mods = GMB_MODKEY_NONE;
    return 1;
#else
    if (g_tty_fd < 0)
    {
        gmb_raw_input_reader_ensure_raw();
        if (g_tty_fd < 0) return gmb_terminal_reader_read_event_core(st, out);
    }
    unsigned char buf[64];
    int n = (int)read(g_tty_fd, buf, 64);
    if (n <= 0) { out->type = GMB_UIEVENT_NONE; return 0; }
    char seq[64];
    for (int i = 0; i < n && i < 63; i++) seq[i] = (char)buf[i];
    seq[n < 63 ? n : 63] = '\0';
    int sl = n < 63 ? n : 63;

    gmb_mouse_action_t ma;
    if (gmb_mouse_decode_and_process(seq, &ma))
    {
        if (st) { st->mouse_x = ma.x; st->mouse_y = ma.y; }
        out->type = GMB_UIEVENT_MOUSE;
        out->mouse = ma;
        return 1;
    }
    if (sl >= 3 && seq[0] == 0x1B && seq[1] == '[')
    {
        int key = gmb_terminal_reader_decode_csi_key_len(seq, sl);
        if (key > 0)
        {
            out->type = GMB_UIEVENT_KEY;
            out->key = key;
            out->mods = GMB_MODKEY_NONE;
            return 1;
        }
    }
    if (n >= 1)
    {
        /* Un byte (o più: es. digitazione veloce/paste). Decodifica il PRIMO e rimetti il
         * resto nella typeahead — mai scartare i caratteri né trasformarli in ESC. */
        out->type = GMB_UIEVENT_KEY;
        switch (buf[0])
        {
            case '\r': case '\n': out->key = GMB_K_ENTER; break;
            case 27: out->key = GMB_K_ESC; break;
            case 9: out->key = GMB_K_TAB; break;
            case 127: out->key = GMB_K_BS; break;
            default: out->key = (buf[0] >= 32 && buf[0] <= 126) ? buf[0] : 0; break;
        }
        out->mods = GMB_MODKEY_NONE;
        if (n > 1 && st)
        {
            char rest[64]; int r = 0;
            for (int i = 1; i < n && r < 63; i++) rest[r++] = (char)buf[i];
            rest[r] = '\0';
            if (r) gmb_clipper_input_keyboard(st, rest);
        }
        return 1;
    }
    out->type = GMB_UIEVENT_NONE;
    return 0;
#endif
}

void gmb_raw_input_reader_shutdown(void)
{
#if defined(_WIN32) || defined(_WIN64)
    set_stdin_raw(false);
    g_is_raw = false;
#else
    if (g_tty_fd >= 0) { close(g_tty_fd); g_tty_fd = -1; }
    g_is_raw = false;
#endif
}

/* Lettura eventi per i loop interattivi (TBROWSE): prima la typeahead scriptata
 * GMB_TUI_SCRIPT (la harness CI la valorizza con N ESC), poi il lettore raw.
 * Senza questa priorità i loop TBROWSE pendono sul tty in headless (timeout CI). */
int gmb_browse_read_event(gmb_clipper_state_t* st, gmb_ui_event_t* out)
{
    static int sidx = 0;
    const char* script = gmb_opt_env("GMB_TUI_SCRIPT");
    if (script && script[sidx])
    {
        unsigned char c = (unsigned char)script[sidx++];
        out->type = GMB_UIEVENT_KEY;
        out->mods = GMB_MODKEY_NONE;
        switch (c)
        {
            case 27: out->key = GMB_K_ESC; break;
            case '\r': case '\n': out->key = GMB_K_ENTER; break;
            case 9: out->key = GMB_K_TAB; break;
            case 8: case 127: out->key = GMB_K_BS; break;
            default: out->key = (c >= 32 && c <= 126) ? c : 0; break;
        }
        return 1;
    }
    return gmb_raw_input_reader_read_event_core(st, out);
}
