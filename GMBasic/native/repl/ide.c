/* native/repl/ide.c — IDE TUI del REPL nativo.
 *
 * PARITÀ DI COMANDI con `GMBasicSharp/REPL/GMBasicIde.cs` (l'IDE della REPL C#): stessi
 * menu, stessi tasti, stesse funzioni. La logica "intelligente" NON è duplicata: outline,
 * completion, go-to-definition, find-references, lint, rename, snippet e make
 * (bas → .a1 / libreria) vivono nel modello C condiviso `gmb_ide` (`gmb_runtime_c/src/gmb_ide.c`),
 * gli stessi che la REPL C# usa via `CIdeNative`. Il debug usa `gmb_debug` (breakpoint,
 * step, trace, watch) come i comandi testuali del REPL.
 *
 * Layout (come QuickVB): TitleBar, MenuBar, CodeArea (line numbers + highlight + gutter
 * breakpoint), pannello inferiore (Output / Watch / Call Stack), StatusBar con posizione
 * e modalità.
 *
 * Comandi (menu):
 *   File   New/Open/Merge/Save/Save As/Save All/Print/Exit
 *   Edit   Undo/Redo, Cut/Copy/Paste/Clear, New SUB/FUNCTION/GENERATOR, Comment/Uncomment,
 *          Select All, Completion, Go To Definition, Find References, Rename, Snippet
 *   View   Outline, Next SUB, Output Screen, Line Numbers, Output/Watch/Call Stack panel
 *   Search Find, Selected Text, Repeat, Change (replace), Go To Line
 *   Run    Start/Restart/Continue, Make EXE, Make Library
 *   Build  Build/Rebuild, Clean, Error List
 *   Debug  Toggle Breakpoint, Clear All, Step Into/Over/Out, Run to Cursor, Add Watch,
 *          Delete All Watch, Watch panel, Call Stack
 *   Opts   Options (tab, line numbers, highlight, backup, theme), Theme
 *   Help   Index, About
 *
 * Tasti: F1 help · F2 outline · F3 repeat find · F4 output · F5 run · F7 build · F8 step ·
 * F9 breakpoint · F10 menu · F12 goto-def · Ctrl+N/O/S/Y/F/H/G/R/L/Space · ESC esci.
 */
#include "repl.h"
#include "gmb_debug.h"
#include "gmb_ide.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>

#define IDE_MAX_TABS     32
#define IDE_MAX_LINES  20000
#define IDE_TAB_NAME     512
#define IDE_LINE_CAP     2048
#define IDE_PANEL_H      6
#define IDE_UNDO_MAX     60
#define IDE_OUT_MAX      400
#define IDE_MSG_MAX      256
#define IDE_CLIP_MAX     (1024 * 1024)

/* ─────────────────────────── tasti ─────────────────────────── */
enum {
    KEY_UP = 1, KEY_DOWN, KEY_RIGHT, KEY_LEFT, KEY_HOME, KEY_END, KEY_BS, KEY_DEL,
    KEY_PGUP, KEY_PGDN, KEY_INS, KEY_TAB, KEY_STAB,
    KEY_SF1, KEY_SF2, KEY_SF3, KEY_SF4, KEY_SDEL, KEY_SINS, KEY_SF10, KEY_SF12,
    KEY_F1 = 40, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_F11, KEY_F12,
    KEY_CTRL = 100   /* + codice ASCII: Ctrl+A = 101 … */
};

/* ─────────────────────────── stato ─────────────────────────── */
typedef struct {
    char  name[IDE_TAB_NAME];
    char** lines;
    int   n, cap;
    int   cl, cc, scroll;
    bool  dirty;
} tab_t;

typedef struct {
    tab_t* tabs[IDE_MAX_TABS];
    int    ntab, active;

    int    rows, cols;
    bool   show_lines, highlight, backup;
    int    tab_width;
    int    theme;                 /* 0 qbasic · 1 dark · 2 light */
    bool   overwrite;
    bool   running;
    int    autosave;              /* secondi, 0 = off */
    long   last_autosave;
    char   msg[IDE_MSG_MAX];

    /* pannello inferiore */
    bool   panel_visible;
    int    panel;                 /* 0 output · 1 watch · 2 call stack */
    char*  out[IDE_OUT_MAX];
    int    nout;

    /* undo (snapshot testuali dell'intero buffer) */
    char** undo;
    int    nundo;
    char** redo;
    int    nredo;
    bool   typing;

    /* clipboard interno (testo multi-riga) */
    char*  clip;

    /* ultima ricerca */
    char   find_term[IDE_MSG_MAX];
    gmb_ide_t ide;                /* modello condiviso (outline/completion/refs/…) */
} ide_t;

typedef struct { const char* label; int action; const char* hotkey; } ide_item_t;
typedef struct { const char* label; const ide_item_t* items; int n; } ide_menu_t;

/* azioni */
enum {
    A_NONE = 0,
    A_NEW, A_OPEN, A_MERGE, A_SAVE, A_SAVEAS, A_SAVEALL, A_PRINT, A_EXIT,
    A_UNDO, A_REDO, A_CUT, A_COPY, A_PASTE, A_CLEARLINE, A_NEWSUB, A_NEWFUNC, A_NEWGEN,
    A_COMMENT, A_UNCOMMENT, A_SELECTALL, A_COMPLETE, A_GOTODEF, A_REFS, A_RENAME, A_SNIPPET,
    A_OUTLINE, A_NEXTSUB, A_OUTPUT, A_LINENO, A_PANEL_OUT, A_PANEL_WATCH, A_PANEL_STACK,
    A_FIND, A_FINDSEL, A_REPEAT, A_REPLACE, A_GOTOLINE,
    A_RUN, A_RESTART, A_CONTINUE, A_MAKEEXE, A_MAKELIB,
    A_BUILD, A_REBUILD, A_CLEAN, A_ERRORS,
    A_BREAK, A_BREAKCLR, A_STEPIN, A_STEPOVER, A_STEPOUT, A_RUNTO, A_ADDWATCH, A_DELWATCH,
    A_OPTIONS, A_THEME,
    A_HELP, A_ABOUT
};

static const ide_item_t m_file[] = {
    {"New Program", A_NEW, "Ctrl+N"}, {"Open Program...", A_OPEN, "Ctrl+O"},
    {"Merge...", A_MERGE, NULL}, {"Save", A_SAVE, "Ctrl+S"},
    {"Save As...", A_SAVEAS, NULL}, {"Save All", A_SAVEALL, NULL},
    {"(sep)", A_NONE, NULL}, {"Print...", A_PRINT, NULL},
    {"(sep)", A_NONE, NULL}, {"Exit", A_EXIT, "Esc"},
};
static const ide_item_t m_edit[] = {
    {"Undo", A_UNDO, "Ctrl+Z"}, {"Redo", A_REDO, "Ctrl+Y"},
    {"(sep)", A_NONE, NULL},
    {"Cut Line", A_CUT, "Shift+Del"}, {"Copy Line", A_COPY, "Ctrl+Ins"},
    {"Paste", A_PASTE, "Shift+Ins"}, {"Clear Line", A_CLEARLINE, "Del"},
    {"(sep)", A_NONE, NULL},
    {"New SUB...", A_NEWSUB, NULL}, {"New FUNCTION...", A_NEWFUNC, NULL},
    {"New GENERATOR...", A_NEWGEN, NULL},
    {"(sep)", A_NONE, NULL},
    {"Comment Block", A_COMMENT, "Ctrl+/"}, {"Uncomment", A_UNCOMMENT, NULL},
    {"Select All", A_SELECTALL, "Ctrl+A"},
    {"(sep)", A_NONE, NULL},
    {"Code Completion", A_COMPLETE, "Ctrl+Space"}, {"Go to Definition", A_GOTODEF, "F12"},
    {"Find References", A_REFS, "Shift+F12"}, {"Rename...", A_RENAME, "Ctrl+R"},
    {"Insert Snippet...", A_SNIPPET, NULL},
};
static const ide_item_t m_view[] = {
    {"SUBs...", A_OUTLINE, "F2"}, {"Next SUB", A_NEXTSUB, "Shift+F2"},
    {"(sep)", A_NONE, NULL},
    {"Output Screen", A_OUTPUT, "F4"}, {"Line Numbers", A_LINENO, "Ctrl+L"},
    {"Output Panel", A_PANEL_OUT, NULL}, {"Watch Panel", A_PANEL_WATCH, NULL},
    {"Call Stack", A_PANEL_STACK, NULL},
};
static const ide_item_t m_search[] = {
    {"Find...", A_FIND, "Ctrl+F"}, {"Selected Text", A_FINDSEL, "Ctrl+\\"},
    {"Repeat Last Find", A_REPEAT, "F3"}, {"Change...", A_REPLACE, "Ctrl+H"},
    {"Go To Line...", A_GOTOLINE, "Ctrl+G"},
};
static const ide_item_t m_run[] = {
    {"Start", A_RUN, "Shift+F5"}, {"Restart", A_RESTART, "Ctrl+F5"},
    {"Continue", A_CONTINUE, "F5"},
    {"(sep)", A_NONE, NULL},
    {"Make EXE File...", A_MAKEEXE, NULL}, {"Make Library...", A_MAKELIB, NULL},
};
static const ide_item_t m_build[] = {
    {"Build", A_BUILD, "F7"}, {"Rebuild", A_REBUILD, "Ctrl+F7"},
    {"Clean", A_CLEAN, NULL}, {"Error List", A_ERRORS, "Ctrl+E"},
};
static const ide_item_t m_debug[] = {
    {"Toggle Breakpoint", A_BREAK, "F9"}, {"Clear All Breakpoints", A_BREAKCLR, NULL},
    {"(sep)", A_NONE, NULL},
    {"Step Into", A_STEPIN, "F8"}, {"Step Over", A_STEPOVER, "F10"},
    {"Step Out", A_STEPOUT, "Shift+F10"}, {"Run to Cursor", A_RUNTO, "Ctrl+F8"},
    {"(sep)", A_NONE, NULL},
    {"Add Watch...", A_ADDWATCH, NULL}, {"Delete All Watch", A_DELWATCH, NULL},
    {"(sep)", A_NONE, NULL},
    {"Watch Panel", A_PANEL_WATCH, NULL}, {"Call Stack", A_PANEL_STACK, NULL},
};
static const ide_item_t m_opts[] = {
    {"Options...", A_OPTIONS, NULL}, {"Theme...", A_THEME, NULL},
};
static const ide_item_t m_help[] = {
    {"Index", A_HELP, "F1"}, {"About...", A_ABOUT, NULL},
};

static const ide_menu_t g_menus[] = {
    {" File ", m_file, (int)(sizeof(m_file) / sizeof(m_file[0]))},
    {" Edit ", m_edit, (int)(sizeof(m_edit) / sizeof(m_edit[0]))},
    {" View ", m_view, (int)(sizeof(m_view) / sizeof(m_view[0]))},
    {" Search", m_search, (int)(sizeof(m_search) / sizeof(m_search[0]))},
    {"  Run ", m_run, (int)(sizeof(m_run) / sizeof(m_run[0]))},
    {" Build", m_build, (int)(sizeof(m_build) / sizeof(m_build[0]))},
    {" Debug", m_debug, (int)(sizeof(m_debug) / sizeof(m_debug[0]))},
    {" Opts ", m_opts, (int)(sizeof(m_opts) / sizeof(m_opts[0]))},
    {" Help ", m_help, (int)(sizeof(m_help) / sizeof(m_help[0]))},
};
#define IDE_NMENUS ((int)(sizeof(g_menus) / sizeof(g_menus[0])))

/* sovrapposizioni (menu, popup, prompt) */
enum { OV_NONE = 0, OV_MENU, OV_POPUP, OV_PROMPT, OV_CONFIRM };
static int  g_ov;
static int  g_menu, g_menu_item;
static char* g_popup[400];
static int  g_popup_n, g_popup_sel;
static char g_popup_title[128];
static char g_prompt[IDE_MSG_MAX];
static int  g_prompt_cap, g_prompt_cc;
static const char* g_prompt_title;
static bool g_prompt_ok;
static char g_confirm_msg[IDE_MSG_MAX];
static int  g_confirm_action;

extern gmb_debug_session_t g_dbg;

/* Output dell'IDE: stream dedicato sul terminale (/dev/tty), NON stdout — il console a celle
 * della REPL installa un hook su stdout che tradurrebbe il layout in celle. */
static FILE* g_out;
#define printf(...) fprintf(g_out, __VA_ARGS__)
#define putchar(c)  fputc((c), g_out)


static ide_t* g_ide;

/* ─────────────────────────── tty / input ─────────────────────────── */
static int g_itty = -1;
static struct termios g_iorig;

static int ide_tty_open(void)
{
    g_itty = open("/dev/tty", O_RDWR);
    if (g_itty < 0) return -1;
    struct termios raw;
    tcgetattr(g_itty, &g_iorig);
    raw = g_iorig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    tcsetattr(g_itty, TCSANOW, &raw);
    if (g_out) { fclose(g_out); g_out = NULL; }
    g_out = fdopen(dup(g_itty), "w");
    if (!g_out) return -1;
    return 0;
}
static void ide_tty_close(void)
{
    if (g_out) { fflush(g_out); fclose(g_out); g_out = NULL; }
    if (g_itty >= 0) { tcsetattr(g_itty, TCSANOW, &g_iorig); close(g_itty); g_itty = -1; }
}
static int ide_read(int ms, unsigned char* o)
{
    fd_set f; struct timeval tv;
    FD_ZERO(&f); FD_SET(g_itty, &f);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    if (select(g_itty + 1, &f, NULL, NULL, &tv) <= 0) return -1;
    return read(g_itty, o, 1) == 1 ? 0 : -1;
}
/* Decodifica un tasto. Ctrl+A..Ctrl+Z → 101..126; i tasti speciali → KEY_*. */
static int ide_key(void)
{
    unsigned char c;
    if (read(g_itty, &c, 1) != 1) return -1;
    if (c == 27) {
        unsigned char s0, last = 0;
        if (ide_read(60, &s0) != 0) return 27;
        if (s0 == '[') {
            unsigned char b;
            if (ide_read(60, &b) != 0) return 27;
            if (b >= '0' && b <= '9') {
                /* ESC [ <num> [;<mod>] ~  /  ESC [ 1 ; 2 X  (shift-…) */
                char digits[6]; int nd = 0;
                digits[nd++] = (char)b;
                while (nd < 5) {
                    unsigned char t;
                    if (ide_read(60, &t) != 0) break;
                    if (t >= '0' && t <= '9') { digits[nd++] = (char)t; continue; }
                    last = t; break;
                }
                digits[nd] = 0;
                int code = atoi(digits);
                int mod = 0;
                if (last == ';') {
                    unsigned char m;
                    if (ide_read(60, &m) == 0 && m >= '0' && m <= '9') mod = m - '0';
                    if (ide_read(60, &last) != 0) return -1;
                }
                bool shift = (mod == 2);
                if (last == '~') {
                    switch (code) {
                        case 1: case 7: return KEY_HOME;
                        case 2: return shift ? KEY_SINS : KEY_INS;
                        case 3: return shift ? KEY_SDEL : KEY_DEL;
                        case 4: case 8: return KEY_END;
                        case 5: return KEY_PGUP;
                        case 6: return KEY_PGDN;
                        case 11: return KEY_F1; case 12: return KEY_F2; case 13: return KEY_F3;
                        case 14: return KEY_F4; case 15: return KEY_F5; case 17: return KEY_F6;
                        case 18: return KEY_F7; case 19: return KEY_F8; case 20: return KEY_F9;
                        case 21: return KEY_F10; case 23: return KEY_F11; case 24: return shift ? KEY_SF12 : KEY_F12;
                    }
                    return -1;
                }
                if (last >= 'A' && last <= 'Z') {
                    /* ESC [ 1 ; 2 P/Q/R/S = Shift+F1..F4 */
                    if (shift) {
                        switch (last) {
                            case 'P': return KEY_SF1; case 'Q': return KEY_SF2;
                            case 'R': return KEY_SF3; case 'S': return KEY_SF4;
                            case 'A': return KEY_UP; case 'B': return KEY_DOWN;
                            case 'C': return KEY_RIGHT; case 'D': return KEY_LEFT;
                            case 'H': return KEY_HOME; case 'F': return KEY_END;
                        }
                    }
                    switch (last) {
                        case 'A': return KEY_UP; case 'B': return KEY_DOWN;
                        case 'C': return KEY_RIGHT; case 'D': return KEY_LEFT;
                        case 'H': return KEY_HOME; case 'F': return KEY_END;
                    }
                }
                return -1;
            }
            switch (b) {
                case 'A': return KEY_UP; case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT; case 'D': return KEY_LEFT;
                case 'H': return KEY_HOME; case 'F': return KEY_END;
                case 'Z': return KEY_STAB;
            }
            return -1;
        }
        if (s0 == 'O') {
            unsigned char t;
            if (ide_read(60, &t) != 0) return 27;
            switch (t) {
                case 'P': return KEY_F1; case 'Q': return KEY_F2; case 'R': return KEY_F3;
                case 'S': return KEY_F4; case 'T': return KEY_F5;
            }
        }
        return 27;
    }
    /* Backspace = DEL (0x7F) sui terminali moderni. 0x08 è Ctrl+H: lo si tiene come
     * Ctrl+H (Change/replace, parità con l'IDE C#) — un terminale che manda 0x08 per
     * Backspace userebbe Change al posto di cancellare (documentato). */
    if (c == 127) return KEY_BS;
    if (c == 13) return 13;
    if (c == 9) return KEY_TAB;
    if (c == 8) return KEY_CTRL + 8;
    if (c < 32) return KEY_CTRL + c;
    return (int)c;
}

/* ─────────────────────────── colori / temi ─────────────────────────── */
typedef struct { const char* reset, *title, *menu, *menu_sel, *gutter, *kw, *str, *com, *status, *sel; } theme_t;
static const theme_t g_themes[] = {
    /* qbasic */ {"\x1b[0m", "\x1b[0;44m", "\x1b[0;44m", "\x1b[0;30;47m", "\x1b[90m", "\x1b[33m", "\x1b[36m", "\x1b[32m", "\x1b[0;44m", "\x1b[0;30;47m"},
    /* dark   */ {"\x1b[0m", "\x1b[48;5;236m\x1b[97m", "\x1b[48;5;236m\x1b[97m", "\x1b[48;5;63m\x1b[97m", "\x1b[90m", "\x1b[38;5;221m", "\x1b[38;5;114m", "\x1b[38;5;245m", "\x1b[48;5;236m\x1b[97m", "\x1b[48;5;63m\x1b[97m"},
    /* light  */ {"\x1b[0m", "\x1b[0;30;47m", "\x1b[0;30;47m", "\x1b[0;30;46m", "\x1b[90m", "\x1b[34m", "\x1b[32m", "\x1b[90m", "\x1b[0;30;47m", "\x1b[0;30;46m"},
};
static const theme_t* th(void) { return &g_themes[(g_ide && g_ide->theme >= 0 && g_ide->theme <= 2) ? g_ide->theme : 0]; }

/* ─────────────────────────── utility righe ─────────────────────────── */
static void tab_init(tab_t* t, const char* name)
{
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name ? name : "untitled.bas");
    t->cap = 64;
    t->lines = (char**)calloc((size_t)t->cap, sizeof(char*));
    t->lines[0] = strdup("");
    t->n = 1;
}
static void tab_reserve(tab_t* t, int need)
{
    if (need <= t->cap) return;
    int nc = t->cap;
    while (nc < need) nc *= 2;
    t->lines = (char**)realloc(t->lines, (size_t)nc * sizeof(char*));
    for (int i = t->cap; i < nc; i++) t->lines[i] = NULL;
    t->cap = nc;
}
static void tab_insert(tab_t* t, int at, const char* s)
{
    if (t->n >= IDE_MAX_LINES) return;
    tab_reserve(t, t->n + 2);
    for (int i = t->n; i > at; i--) t->lines[i] = t->lines[i - 1];
    t->lines[at] = strdup(s ? s : "");
    t->n++;
}
static void tab_delete(tab_t* t, int at)
{
    if (t->n <= 1 || at < 0 || at >= t->n) { if (t->n == 1) { free(t->lines[0]); t->lines[0] = strdup(""); } return; }
    free(t->lines[at]);
    for (int i = at; i < t->n - 1; i++) t->lines[i] = t->lines[i + 1];
    t->lines[t->n - 1] = NULL;
    t->n--;
}
static void tab_free(tab_t* t)
{
    if (!t) return;
    for (int i = 0; i < t->n; i++) free(t->lines[i]);
    free(t->lines);
    free(t);
}

/* ─────────────────────────── undo ─────────────────────────── */
static char* tab_serialize(tab_t* t)
{
    size_t cap = 1024;
    for (int i = 0; i < t->n; i++) cap += strlen(t->lines[i]) + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    size_t used = 0;
    for (int i = 0; i < t->n; i++) {
        size_t l = strlen(t->lines[i]);
        memcpy(out + used, t->lines[i], l); used += l;
        out[used++] = '\n'; out[used] = 0;
    }
    return out;
}
static void tab_deserialize(tab_t* t, const char* text, int* out_line, int* out_col)
{
    for (int i = 0; i < t->n; i++) free(t->lines[i]);
    t->n = 0;
    const char* p = text ? text : "";
    while (*p || t->n == 0) {
        const char* nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > IDE_LINE_CAP - 1) len = IDE_LINE_CAP - 1;
        char buf[IDE_LINE_CAP];
        memcpy(buf, p, (size_t)len); buf[len] = 0;
        tab_insert(t, t->n, buf);
        if (!nl) break;
        p = nl + 1;
    }
    if (t->n == 0) tab_insert(t, 0, "");
    if (out_line) *out_line = 0;
    if (out_col) *out_col = 0;
}
static void undo_push(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    char* snap = tab_serialize(t);
    if (!snap) return;
    if (e->nundo >= IDE_UNDO_MAX) {
        free(e->undo[0]);
        memmove(e->undo, e->undo + 1, (size_t)(e->nundo - 1) * sizeof(char*));
        e->nundo--;
    }
    e->undo[e->nundo++] = snap;
    for (int i = 0; i < e->nredo; i++) free(e->redo[i]);
    e->nredo = 0;
    t->dirty = true;
}
static void undo_do(ide_t* e)
{
    if (e->nundo == 0) { snprintf(e->msg, sizeof(e->msg), "Nothing to undo"); return; }
    tab_t* t = e->tabs[e->active];
    char* cur = tab_serialize(t);
    if (cur) {
        if (e->nredo >= IDE_UNDO_MAX) { free(e->redo[0]); memmove(e->redo, e->redo + 1, (size_t)(e->nredo - 1) * sizeof(char*)); e->nredo--; }
        e->redo[e->nredo++] = cur;
    }
    char* prev = e->undo[--e->nundo];
    tab_deserialize(t, prev, &t->cl, &t->cc);
    free(prev);
    snprintf(e->msg, sizeof(e->msg), "Undo");
}
static void redo_do(ide_t* e)
{
    if (e->nredo == 0) { snprintf(e->msg, sizeof(e->msg), "Nothing to redo"); return; }
    tab_t* t = e->tabs[e->active];
    char* cur = tab_serialize(t);
    if (cur) {
        if (e->nundo >= IDE_UNDO_MAX) { free(e->undo[0]); memmove(e->undo, e->undo + 1, (size_t)(e->nundo - 1) * sizeof(char*)); e->nundo--; }
        e->undo[e->nundo++] = cur;
    }
    char* next = e->redo[--e->nredo];
    tab_deserialize(t, next, &t->cl, &t->cc);
    free(next);
    snprintf(e->msg, sizeof(e->msg), "Redo");
}

/* ─────────────────────────── pannello output ─────────────────────────── */
static void out_add(ide_t* e, const char* line)
{
    if (e->nout >= IDE_OUT_MAX) {
        free(e->out[0]);
        memmove(e->out, e->out + 1, (size_t)(IDE_OUT_MAX - 1) * sizeof(char*));
        e->nout--;
    }
    e->out[e->nout++] = strdup(line && line[0] ? line : " ");
}
static void out_clear(ide_t* e)
{
    for (int i = 0; i < e->nout; i++) free(e->out[i]);
    e->nout = 0;
}

/* ─────────────────────────── modello gmb_ide ─────────────────────────── */
static int mod_sync(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    int idx = gmb_ide_tab_open(&e->ide, t->name, NULL);
    if (idx < 0) return -1;
    char* text = tab_serialize(t);
    gmb_ide_set_text(&e->ide, gmb_ide_active(&e->ide), text);
    free(text);
    gmb_ide_analyze(&e->ide);
    return gmb_ide_active(&e->ide);
}
static char* word_before_cursor(tab_t* t)
{
    char* ln = t->lines[t->cl];
    int end = t->cc, start = end;
    while (start > 0 && (isalnum((unsigned char)ln[start - 1]) || ln[start - 1] == '_')) start--;
    char* w = (char*)malloc((size_t)(end - start) + 1);
    memcpy(w, ln + start, (size_t)(end - start));
    w[end - start] = 0;
    return w;
}
static void mod_reload_from_ide(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    int idx = gmb_ide_active(&e->ide);
    char* buf = (char*)malloc(1024 * 1024);
    if (!buf) return;
    gmb_ide_doc_text(&e->ide, idx, buf, 1024 * 1024);
    tab_deserialize(t, buf, &t->cl, &t->cc);
    free(buf);
    t->dirty = true;
}

/* ─────────────────────────── prompt / popup ─────────────────────────── */
static void prompt_draw(ide_t* e)
{
    int y = e->rows;
    printf("\x1b[%d;1H", y);
    printf("\x1b[0;44m%s%s%s\x1b[K", th()->menu_sel, g_prompt_title ? g_prompt_title : "",
        th()->reset);
    printf("\x1b[%d;1H\x1b[0;44m> %s", y, g_prompt);
    printf("\x1b[%d;%dH", y, 3 + g_prompt_cc);
    fflush(g_out);
}
static bool prompt_run(ide_t* e, const char* title, char* buf, int cap, const char* initial)
{
    snprintf(g_prompt, sizeof(g_prompt), "%s", initial ? initial : "");
    g_prompt_cc = (int)strlen(g_prompt);
    g_prompt_title = title;
    g_prompt_cap = cap;
    g_ov = OV_PROMPT;
    for (;;) {
        prompt_draw(e);
        int k = ide_key();
        if (k == 27) { g_ov = OV_NONE; return false; }
        if (k == 13) { snprintf(buf, (size_t)cap, "%s", g_prompt); g_ov = OV_NONE; return true; }
        if (k == KEY_BS) { if (g_prompt_cc > 0) g_prompt[--g_prompt_cc] = 0; continue; }
        if (k == KEY_LEFT) { if (g_prompt_cc > 0) g_prompt_cc--; continue; }
        if (k == KEY_RIGHT) { if (g_prompt[g_prompt_cc]) g_prompt_cc++; continue; }
        if (k == KEY_HOME) { g_prompt_cc = 0; continue; }
        if (k == KEY_END) { g_prompt_cc = (int)strlen(g_prompt); continue; }
        if (k >= 32 && k < 256) {
            int len = (int)strlen(g_prompt);
            if (len < (int)sizeof(g_prompt) - 1) {
                memmove(g_prompt + g_prompt_cc + 1, g_prompt + g_prompt_cc, (size_t)(len - g_prompt_cc) + 1);
                g_prompt[g_prompt_cc++] = (char)k;
            }
        }
    }
}
static int popup_run(ide_t* e, const char* title, char** items, int n)
{
    if (n <= 0) return -1;
    snprintf(g_popup_title, sizeof(g_popup_title), "%s", title ? title : "");
    g_popup_n = n;
    for (int i = 0; i < n && i < 400; i++) g_popup[i] = items[i];
    g_popup_sel = 0;
    g_ov = OV_POPUP;
    for (;;) {
        /* render: titolo + lista, larghezza minima 40 */
        int w = 40;
        for (int i = 0; i < g_popup_n; i++) { int l = (int)strlen(g_popup[i]) + 4; if (l > w) w = l; }
        if (w > e->cols - 4) w = e->cols - 4;
        int h = g_popup_n + 2;
        if (h > e->rows - 4) h = e->rows - 4;
        int y0 = 2, x0 = 2;
        printf("\x1b[%d;%dH\x1b[0;44m+", y0, x0);
        for (int i = 0; i < w - 2; i++) putchar('-');
        putchar('+');
        printf("\x1b[%d;%dH\x1b[0;44m|%-*.*s|", y0 + 1, x0, w - 2, w - 2, g_popup_title);
        for (int i = 0; i < h - 2; i++) {
            int idx = i + (g_popup_sel >= h - 2 ? g_popup_sel - (h - 3) : 0);
            const char* it = idx < g_popup_n ? g_popup[idx] : "";
            printf("\x1b[%d;%dH", y0 + 2 + i, x0);
            if (idx == g_popup_sel) printf("%s| %-*.*s|%s", th()->menu_sel, w - 4, w - 4, it, th()->reset);
            else printf("\x1b[0;44m| %-*.*s|", w - 4, w - 4, it);
        }
        printf("\x1b[%d;%dH\x1b[0;44m+", y0 + h - 1, x0);
        for (int i = 0; i < w - 2; i++) putchar('-');
        putchar('+');
        fflush(g_out);
        int k = ide_key();
        if (k == 27) { g_ov = OV_NONE; return -1; }
        if (k == 13) { g_ov = OV_NONE; return g_popup_sel; }
        if (k == KEY_UP) { if (g_popup_sel > 0) g_popup_sel--; continue; }
        if (k == KEY_DOWN) { if (g_popup_sel < g_popup_n - 1) g_popup_sel++; continue; }
        if (k == KEY_PGUP) { g_popup_sel -= 10; if (g_popup_sel < 0) g_popup_sel = 0; continue; }
        if (k == KEY_PGDN) { g_popup_sel += 10; if (g_popup_sel > g_popup_n - 1) g_popup_sel = g_popup_n - 1; continue; }
    }
}

/* ─────────────────────────── rendering ─────────────────────────── */
static bool is_kw(const char* s, int len)
{
    char w[64];
    if (len <= 0 || len >= 64) return false;
    memcpy(w, s, (size_t)len); w[len] = 0;
    return gmb_ide_is_keyword(w) != 0;
}
static void draw_line_hl(const theme_t* t, const char* s, int max)
{
    int j = 0;
    while (s[j] && j < max) {
        char c = s[j];
        if (c == '\'') { printf("%s%s%s", t->com, s + j, t->reset); return; }
        if (c == '"') {
            int k = j + 1;
            while (s[k] && s[k] != '"') k++;
            printf("%s%.*s%s", t->str, k - j + (s[k] ? 1 : 0), s + j, t->reset);
            j = s[k] ? k + 1 : k;
            continue;
        }
        if (isalnum((unsigned char)c) || c == '_') {
            int k = j;
            while (s[k] && (isalnum((unsigned char)s[k]) || s[k] == '_')) k++;
            if (is_kw(s + j, k - j)) printf("%s%.*s%s", t->kw, k - j, s + j, t->reset);
            else printf("%.*s", k - j, s + j);
            j = k;
            continue;
        }
        putchar(c);
        j++;
    }
}
static void render(ide_t* e)
{
    const theme_t* t = th();
    tab_t* tb = e->tabs[e->active];
    int code_h = e->rows - 2 - (e->panel_visible ? IDE_PANEL_H : 0);
    if (code_h < 3) code_h = 3;
    if (tb->cl < tb->scroll) tb->scroll = tb->cl;
    if (tb->cl >= tb->scroll + code_h) tb->scroll = tb->cl - code_h + 1;

    printf("\x1b[2J\x1b[H");
    /* title bar: tab (nome, * se sporco, attivo evidenziato) */
    printf("%s ", t->title);
    {
        int used = 1;
        for (int i = 0; i < e->ntab && used < e->cols - 2; i++) {
            char label[160];
            snprintf(label, sizeof(label), " %s%s ", e->tabs[i]->name, e->tabs[i]->dirty ? " *" : "");
            int w = (int)strlen(label);
            if (used + w >= e->cols - 1) break;
            if (i == e->active) printf("%s%s", t->menu_sel, label);
            else printf("%s", label);
            used += w;
        }
        if (e->ntab > 1) printf("%s  [%d/%d] ", t->reset, e->active + 1, e->ntab);
        else printf("%s", t->reset);
    }
    printf("%*s", e->cols > 60 ? 0 : 0, "");
    printf("\n");
    /* menu bar */
    printf("%s", t->menu);
    for (int i = 0; i < IDE_NMENUS; i++) {
        if (g_ov == OV_MENU && i == g_menu) printf("%s", t->menu_sel);
        printf("%s", g_menus[i].label);
        if (g_ov == OV_MENU && i == g_menu) printf("%s%s", t->reset, t->menu);
    }
    printf("%*s%5d:%02d  %s%s", e->cols > 60 ? e->cols - 60 : 1, "",
        tb->cl + 1, tb->cc + 1, tb->dirty ? "[*] " : "", t->reset);
    putchar('\n');
    /* code area + gutter */
    for (int i = 0; i < code_h; i++) {
        int line = tb->scroll + i;
        if (line >= tb->n) { printf("%s\x1b[K%s\n", t->reset, t->reset); continue; }
        if (e->show_lines) printf("%s%5d%s ", t->gutter, line + 1, t->reset);
        if (gmb_debug_has_break(&g_dbg, line + 1)) printf("\x1b[41m \x1b[0m "); else printf("  ");
        if (e->highlight) draw_line_hl(t, tb->lines[line], e->cols - (e->show_lines ? 9 : 4));
        else printf("%.*s", e->cols - (e->show_lines ? 9 : 4), tb->lines[line]);
        printf("\x1b[K\n");
    }
    /* pannello inferiore */
    if (e->panel_visible) {
        int y = e->rows - IDE_PANEL_H;
        printf("\x1b[%d;1H%s", y, t->title);
        printf(" Output | Watch | Call Stack %*s", e->cols > 40 ? e->cols - 30 : 1, "");
        printf("%s\x1b[K", t->reset);
        for (int i = 0; i < IDE_PANEL_H - 1; i++) {
            printf("\x1b[%d;1H\x1b[K", y + 1 + i);
            if (e->panel == 0) {
                int idx = e->nout - (IDE_PANEL_H - 1) + i;
                if (idx >= 0 && idx < e->nout) printf("%s", e->out[idx]);
            } else if (e->panel == 1) {
                if (i < g_dbg.watch_count) printf("WATCH %s", g_dbg.watches[i].name);
                else if (i == 0 && g_dbg.watch_count == 0) printf("(no watch — Debug > Add Watch...)");
            } else {
                if (i == 0) printf("Call Stack: unavailable in the native REPL (VM/C debugger prints pauses)");
            }
        }
    }
    /* status bar */
    {
        const char* mode = g_dbg.mode == GMB_DBG_OFF ? "OFF" : g_dbg.mode == GMB_DBG_STEPPING ? "STEP" : "RUN";
        printf("\x1b[%d;1H%s", e->rows, t->status);
        printf(" %s  Ln %d, Col %d  %s  Break:%d  Debug:%s ", tb->name, tb->cl + 1, tb->cc + 1,
            e->overwrite ? "OVR" : "INS", g_dbg.break_count, mode);
        if (e->msg[0]) printf("| %s ", e->msg);
        printf("%*s%s", e->cols > 90 ? e->cols - 90 : 1, "", t->reset);
    }
    /* overlay menu */
    if (g_ov == OV_MENU) {
        int x = 1;
        for (int i = 0; i < g_menu; i++) x += (int)strlen(g_menus[i].label);
        const ide_menu_t* m = &g_menus[g_menu];
        int w = 24;
        for (int i = 0; i < m->n; i++) { int l = (int)strlen(m->items[i].label) + (m->items[i].hotkey ? (int)strlen(m->items[i].hotkey) + 2 : 0) + 3; if (l > w) w = l; }
        if (x + w > e->cols) x = e->cols - w - 1;
        if (x < 1) x = 1;
        printf("\x1b[2;%dH%s", x, t->menu_sel);
        printf("+"); for (int i = 0; i < w - 2; i++) putchar('-'); putchar('+');
        for (int i = 0; i < m->n; i++) {
            printf("\x1b[%d;%dH%s|", 3 + i, x, t->menu_sel);
            if (m->items[i].action == A_NONE) { printf("%-*s", w - 2, ""); printf("|"); continue; }
            if (i == g_menu_item) printf("%s %-*.*s ", t->menu_sel, w - 4, w - 4, m->items[i].label);
            else printf(" %-*.*s ", w - 4, w - 4, m->items[i].label);
            if (m->items[i].hotkey) printf("%s%*s", i == g_menu_item ? t->menu_sel : t->menu, 0, "");
            printf("|");
        }
        printf("\x1b[%d;%dH%s+", 3 + m->n, x, t->menu_sel);
        for (int i = 0; i < w - 2; i++) putchar('-'); putchar('+');
        printf("%s", t->reset);
        fflush(g_out);
    }
    /* cursore */
    {
        int vis = tb->cl - tb->scroll + 1;
        int x = (tb->cc + 1) + (e->show_lines ? 8 : 1) + (gmb_debug_has_break(&g_dbg, tb->cl + 1) ? -1 : 0);
        if (x < 1) x = 1;
        printf("\x1b[%d;%dH", vis, x);
    }
    fflush(g_out);
}

/* ─────────────────────────── azioni ─────────────────────────── */
static void act_save(ide_t* e, bool as)
{
    tab_t* tb = e->tabs[e->active];
    char name[IDE_TAB_NAME];
    snprintf(name, sizeof(name), "%s", tb->name);
    if (as) {
        char buf[IDE_TAB_NAME];
        if (!prompt_run(e, "Save as", buf, sizeof(buf), tb->name)) return;
        snprintf(name, sizeof(name), "%s", buf);
    }
    if (e->backup) {
        char bak[IDE_TAB_NAME + 8];
        snprintf(bak, sizeof(bak), "%s.bak", name);
        FILE* src = fopen(name, "r");
        if (src) { FILE* dst = fopen(bak, "w"); if (dst) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), src)) > 0) fwrite(b, 1, n, dst); fclose(dst); } fclose(src); }
    }
    FILE* f = fopen(name, "w");
    if (!f) { snprintf(e->msg, sizeof(e->msg), "?Save: cannot write %s", name); return; }
    for (int i = 0; i < tb->n; i++) fprintf(f, "%s\n", tb->lines[i]);
    fclose(f);
    tb->dirty = false;
    snprintf(tb->name, sizeof(tb->name), "%s", name);
    snprintf(e->msg, sizeof(e->msg), "Saved %s", name);
}
static void act_open_file(ide_t* e, const char* path, bool merge)
{
    FILE* f = fopen(path, "r");
    if (!f) { snprintf(e->msg, sizeof(e->msg), "?Open: %s", path); return; }
    if (e->ntab >= IDE_MAX_TABS) { fclose(f); snprintf(e->msg, sizeof(e->msg), "?Too many tabs"); return; }
    tab_t* t = (tab_t*)calloc(1, sizeof(tab_t));
    tab_init(t, path);
    char buf[IDE_LINE_CAP];
    t->n = 0;
    while (fgets(buf, sizeof(buf), f)) {
        size_t l = strlen(buf);
        while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
        tab_insert(t, t->n, buf);
    }
    fclose(f);
    if (t->n == 0) tab_insert(t, 0, "");
    if (merge) {
        tab_t* cur = e->tabs[e->active];
        for (int i = 0; i < t->n; i++) tab_insert(cur, cur->n, t->lines[i]);
        cur->dirty = true;
        tab_free(t);
        snprintf(e->msg, sizeof(e->msg), "Merged %s", path);
    } else {
        e->tabs[e->ntab++] = t;
        e->active = e->ntab - 1;
        snprintf(e->msg, sizeof(e->msg), "Opened %s", path);
    }
}
static void act_open(ide_t* e, bool merge)
{
    char* items[400];
    int n = 0;
    DIR* d = opendir(".");
    if (d) {
        struct dirent* de;
        while ((de = readdir(d)) && n < 400) {
            const char* nm = de->d_name;
            size_t l = strlen(nm);
            if (l > 4 && (strcasecmp(nm + l - 4, ".bas") == 0 || strcasecmp(nm + l - 4, ".inc") == 0))
                items[n++] = strdup(nm);
        }
        closedir(d);
    }
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (strcasecmp(items[i], items[j]) > 0) { char* t = items[i]; items[i] = items[j]; items[j] = t; }
    if (n == 0) { snprintf(e->msg, sizeof(e->msg), "No .bas/.inc in current dir"); return; }
    int pick = popup_run(e, merge ? "Merge .bas file" : "Open .bas file", items, n);
    if (pick >= 0) act_open_file(e, items[pick], merge);
    for (int i = 0; i < n; i++) free(items[i]);
}
static void act_new(ide_t* e)
{
    if (e->ntab >= IDE_MAX_TABS) return;
    char nm[64];
    snprintf(nm, sizeof(nm), "untitled%d.bas", e->ntab + 1);
    tab_t* t = (tab_t*)calloc(1, sizeof(tab_t));
    tab_init(t, nm);
    e->tabs[e->ntab++] = t;
    e->active = e->ntab - 1;
}
static void act_close_tab(ide_t* e)
{
    if (e->ntab <= 1) return;
    tab_free(e->tabs[e->active]);
    for (int i = e->active; i < e->ntab - 1; i++) e->tabs[i] = e->tabs[i + 1];
    e->ntab--;
    if (e->active >= e->ntab) e->active = e->ntab - 1;
}
static void act_outline(ide_t* e)
{
    int idx = mod_sync(e);
    if (idx < 0) return;
    char buf[64 * 1024];
    gmb_ide_outline(&e->ide, buf, (int)sizeof(buf));
    char* items[400]; int n = 0;
    char* save = NULL; char* tok = strtok_r(buf, ";", &save);
    while (tok && n < 400) {
        int line = atoi(tok);
        char* bar = strchr(tok, '|');
        char* bar2 = bar ? strchr(bar + 1, '|') : NULL;
        const char* name = bar2 ? bar2 + 1 : tok;
        char row[256];
        snprintf(row, sizeof(row), "%5d  %-10s %s", line + 1,
            bar && bar2 ? (bar[1] == '0' ? "SUB" : "FUNC") : "", name);
        items[n++] = strdup(row);
        tok = strtok_r(NULL, ";", &save);
    }
    if (n == 0) { snprintf(e->msg, sizeof(e->msg), "No SUB/FUNCTION"); return; }
    int pick = popup_run(e, "Outline (SUBs/Functions)", items, n);
    if (pick >= 0) {
        /* ritrova la riga dall'item scelto */
        int line = atoi(items[pick]);
        if (line > 0 && line <= e->tabs[e->active]->n) e->tabs[e->active]->cl = line - 1;
        e->tabs[e->active]->cc = 0;
    }
    for (int i = 0; i < n; i++) free(items[i]);
}
static void act_complete(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    int idx = mod_sync(e);
    if (idx < 0) return;
    char* pre = word_before_cursor(t);
    char buf[32 * 1024];
    gmb_ide_complete(&e->ide, pre, buf, (int)sizeof(buf));
    free(pre);
    char* items[400]; int n = 0;
    char* save = NULL; char* tok = strtok_r(buf, ";", &save);
    while (tok && n < 400) { items[n++] = strdup(tok); tok = strtok_r(NULL, ";", &save); }
    if (n == 0) { snprintf(e->msg, sizeof(e->msg), "No completion"); return; }
    int pick = popup_run(e, "Code Completion", items, n);
    if (pick >= 0) {
        const char* w = items[pick];
        int len = (int)strlen(t->lines[t->cl]);
        char* ln = t->lines[t->cl];
        /* sostituisci la parola prima del cursore */
        int end = t->cc, start = end;
        while (start > 0 && (isalnum((unsigned char)ln[start - 1]) || ln[start - 1] == '_')) start--;
        char rest[IDE_LINE_CAP];
        snprintf(rest, sizeof(rest), "%s", ln + end);
        char out[IDE_LINE_CAP];
        snprintf(out, sizeof(out), "%.*s%s%s", start, ln, w, rest);
        if ((int)strlen(out) < IDE_LINE_CAP) { strcpy(ln, out); t->cc = start + (int)strlen(w); t->dirty = true; e->typing = false; }
        (void)len;
    }
    for (int i = 0; i < n; i++) free(items[i]);
}
static void act_gotodef(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    char* w = word_before_cursor(t);
    if (!w[0]) { free(w); snprintf(e->msg, sizeof(e->msg), "No identifier at cursor"); return; }
    mod_sync(e);
    int line = gmb_ide_goto_def(&e->ide, w);
    free(w);
    if (line >= 0 && line < t->n) { t->cl = line; t->cc = 0; snprintf(e->msg, sizeof(e->msg), "Go to definition"); }
    else snprintf(e->msg, sizeof(e->msg), "?Definition not found");
}
static void act_refs(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    char* w = word_before_cursor(t);
    if (!w[0]) { free(w); snprintf(e->msg, sizeof(e->msg), "No identifier at cursor"); return; }
    mod_sync(e);
    char buf[32 * 1024];
    gmb_ide_find_refs(&e->ide, w, buf, (int)sizeof(buf));
    char items[400][256];
    char* ptrs[400]; int n = 0;
    char* save = NULL; char* tok = strtok_r(buf, ";", &save);
    while (tok && n < 400) {
        int line = atoi(tok);
        snprintf(items[n], sizeof(items[n]), "%5d  %s", line + 1, line < t->n ? t->lines[line] : "");
        ptrs[n] = items[n];
        n++;
        tok = strtok_r(NULL, ";", &save);
    }
    free(w);
    if (n == 0) { snprintf(e->msg, sizeof(e->msg), "No references"); return; }
    int pick = popup_run(e, "References", ptrs, n);
    if (pick >= 0) { int line = atoi(items[pick]); if (line > 0 && line <= t->n) { t->cl = line - 1; t->cc = 0; } }
}
static void act_rename(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    char* from = word_before_cursor(t);
    char to[IDE_MSG_MAX];
    if (!from[0]) { free(from); snprintf(e->msg, sizeof(e->msg), "No identifier at cursor"); return; }
    if (!prompt_run(e, "Rename to", to, sizeof(to), "")) { free(from); return; }
    undo_push(e);
    mod_sync(e);
    int n = gmb_ide_rename(&e->ide, from, to);
    if (n > 0) { mod_reload_from_ide(e); snprintf(e->msg, sizeof(e->msg), "Renamed %d occurrence(s)", n); }
    else snprintf(e->msg, sizeof(e->msg), "?Rename: identifier not found");
    free(from);
}
static void act_snippet(ide_t* e)
{
    static char* kinds[] = {"SUB", "FUNCTION", "GENERATOR", "FOR", "IF", "WHILE", "SELECT", "CLASS"};
    int pick = popup_run(e, "Insert snippet", kinds, (int)(sizeof(kinds) / sizeof(kinds[0])));
    if (pick < 0) return;
    char buf[8 * 1024];
    gmb_ide_snippet(kinds[pick], buf, (int)sizeof(buf));
    undo_push(e);
    tab_t* t = e->tabs[e->active];
    char* save = NULL;
    char line[IDE_LINE_CAP];
    char* tok = strtok_r(buf, "\n", &save);
    int at = t->cl;
    while (tok) { snprintf(line, sizeof(line), "%s", tok); tab_insert(t, at++, line); tok = strtok_r(NULL, "\n", &save); }
    t->cl = at - 1;
}
static void act_comment(ide_t* e, bool on)
{
    tab_t* t = e->tabs[e->active];
    undo_push(e);
    int a = t->cl, b = t->cl;
    if (t->cc == 0) { b = t->n - 1; }
    for (int i = a; i <= b && i < t->n; i++) {
        char out[IDE_LINE_CAP];
        if (on) snprintf(out, sizeof(out), "' %s", t->lines[i]);
        else {
            const char* s = t->lines[i];
            while (*s == ' ' || *s == '\t') s++;
            if (s[0] == '\'') { s++; if (*s == ' ') s++; snprintf(out, sizeof(out), "%s", s); }
            else snprintf(out, sizeof(out), "%s", t->lines[i]);
        }
        free(t->lines[i]);
        t->lines[i] = strdup(out);
    }
    t->dirty = true;
}
static bool tab_has_unsaved(ide_t* e)
{
    for (int i = 0; i < e->ntab; i++) if (e->tabs[i]->dirty) return true;
    return false;
}
static void tab_run(ide_t* e, bool debug_step)
{
    tab_t* t = e->tabs[e->active];
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "/tmp/gmb_ide_%ld_%d.bas", (long)getpid(), e->active);
    char name[512];
    snprintf(name, sizeof(name), "%s", t->name);
    /* salva su file reale se il nome non è un untitled, altrimenti su temp */
    if (strncmp(name, "untitled", 8) != 0) {
        act_save(e, false);
        snprintf(tmp, sizeof(tmp), "%s", name);
    } else {
        FILE* f = fopen(tmp, "w");
        if (!f) { snprintf(e->msg, sizeof(e->msg), "?Run: cannot write temp"); return; }
        for (int i = 0; i < t->n; i++) fprintf(f, "%s\n", t->lines[i]);
        fclose(f);
    }
    if (debug_step) g_dbg.mode = GMB_DBG_STEPPING;
    e->running = true;
    printf("\x1b[2J\x1b[H\x1b[0m");
    fflush(g_out);
    ide_tty_close();
    rpl_run_vmc(tmp, NULL);
    printf("\n[press a key to return to the IDE]\n");
    fflush(g_out);
    ide_tty_open();
    printf("\x1b[?25l");
    int k = ide_key(); (void)k;
    g_dbg.mode = GMB_DBG_OFF;
    e->running = false;
    snprintf(e->msg, sizeof(e->msg), "Run finished");
    out_add(e, "Run finished");
}
static void act_build(ide_t* e, bool show_errors)
{
    mod_sync(e);
    char lint[32 * 1024];
    gmb_ide_lint(&e->ide, lint, (int)sizeof(lint));
    char items[400][256]; char* ptrs[400]; int n = 0;
    char* save = NULL; char* tok = strtok_r(lint, ";", &save);
    while (tok && n < 400) {
        int line = atoi(tok);
        char* bar = strchr(tok, '|');
        snprintf(items[n], sizeof(items[n]), "%5d  %s", line + 1, bar ? bar + 1 : tok);
        ptrs[n] = items[n]; n++;
        tok = strtok_r(NULL, ";", &save);
    }
    char out[512];
    snprintf(out, sizeof(out), "/tmp/gmb_ide_%ld.a1", (long)getpid());
    int rc = gmb_ide_make_bas(&e->ide, out);
    char msg[IDE_MSG_MAX];
    snprintf(msg, sizeof(msg), "%s: %s (%d error(s))", show_errors ? "Errors" : "Build", rc == 0 ? "OK" : "FAILED", n);
    snprintf(e->msg, sizeof(e->msg), "%s", msg);
    out_add(e, msg);
    for (int i = 0; i < n; i++) out_add(e, items[i]);
    if (show_errors && n > 0) {
        int pick = popup_run(e, "Error List", ptrs, n);
        if (pick >= 0) { int line = atoi(items[pick]); if (line > 0 && line <= e->tabs[e->active]->n) e->tabs[e->active]->cl = line - 1; }
    }
}
static void act_make(ide_t* e, bool lib)
{
    mod_sync(e);
    char out[512];
    snprintf(out, sizeof(out), "%s.%s", e->tabs[e->active]->name, lib ? "lib.a1" : "a1");
    int rc = lib ? gmb_ide_make_lib(&e->ide, out) : gmb_ide_make_bas(&e->ide, out);
    char msg[IDE_MSG_MAX];
    snprintf(msg, sizeof(msg), "%s -> %s : %s", lib ? "Make Library" : "Make EXE", out, rc == 0 ? "OK" : "FAILED");
    snprintf(e->msg, sizeof(e->msg), "%s", msg);
    out_add(e, msg);
}
static void act_find(ide_t* e, bool replace)
{
    tab_t* t = e->tabs[e->active];
    char term[IDE_MSG_MAX];
    /* Change parte VUOTO (Find ripropone l'ultimo termine): pre-riempire Change col termine
     * precedente faceva appendere il nuovo testo a quello vecchio ("PRINThi"). */
    if (!prompt_run(e, replace ? "Change: find" : "Find", term, sizeof(term),
                    replace ? "" : e->find_term)) return;
    snprintf(e->find_term, sizeof(e->find_term), "%s", term);
    if (!replace) {
        mod_sync(e);
        int line = gmb_ide_doc_find(&e->ide, gmb_ide_active(&e->ide), term, t->cl);
        if (line >= 0) { t->cl = line; t->cc = 0; snprintf(e->msg, sizeof(e->msg), "Found"); }
        else snprintf(e->msg, sizeof(e->msg), "?Not found");
        return;
    }
    char rep[IDE_MSG_MAX];
    if (!prompt_run(e, "Change: replace with", rep, sizeof(rep), "")) return;
    undo_push(e);
    int count = 0;
    for (int i = 0; i < t->n; i++) {
        char out[IDE_LINE_CAP]; out[0] = 0;
        const char* p = t->lines[i];
        size_t used = 0;
        while (*p) {
            const char* hit = strstr(p, term);
            if (!hit) { size_t l = strlen(p); if (used + l < sizeof(out)) { memcpy(out + used, p, l); used += l; out[used] = 0; } break; }
            size_t pre = (size_t)(hit - p);
            if (used + pre + strlen(rep) < sizeof(out)) { memcpy(out + used, p, pre); used += pre; memcpy(out + used, rep, strlen(rep)); used += strlen(rep); out[used] = 0; }
            p = hit + strlen(term);
            count++;
        }
        if (out[0] || t->lines[i][0] == 0) { free(t->lines[i]); t->lines[i] = strdup(out); }
    }
    snprintf(e->msg, sizeof(e->msg), "Replaced %d occurrence(s)", count);
}
static void act_gotoline(ide_t* e)
{
    char buf[64];
    if (!prompt_run(e, "Go to line", buf, sizeof(buf), "")) return;
    int line = atoi(buf);
    tab_t* t = e->tabs[e->active];
    if (line > 0 && line <= t->n) { t->cl = line - 1; t->cc = 0; }
    else snprintf(e->msg, sizeof(e->msg), "?Line out of range");
}
static void act_options(ide_t* e)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", e->tab_width);
    if (prompt_run(e, "Tab width", buf, sizeof(buf), buf)) e->tab_width = atoi(buf) > 0 ? atoi(buf) : 2;
    snprintf(buf, sizeof(buf), "%d", e->show_lines ? 1 : 0);
    if (prompt_run(e, "Line numbers (1/0)", buf, sizeof(buf), buf)) e->show_lines = atoi(buf) != 0;
    snprintf(buf, sizeof(buf), "%d", e->highlight ? 1 : 0);
    if (prompt_run(e, "Syntax highlighting (1/0)", buf, sizeof(buf), buf)) e->highlight = atoi(buf) != 0;
    snprintf(buf, sizeof(buf), "%d", e->backup ? 1 : 0);
    if (prompt_run(e, "Backup on save (1/0)", buf, sizeof(buf), buf)) e->backup = atoi(buf) != 0;
    snprintf(buf, sizeof(buf), "%d", e->autosave);
    if (prompt_run(e, "Auto-save seconds (0=off)", buf, sizeof(buf), buf)) e->autosave = atoi(buf);
    snprintf(e->msg, sizeof(e->msg), "Options saved (session)");
}
static void act_theme(ide_t* e)
{
    char* items[] = {"qbasic", "dark", "light"};
    int pick = popup_run(e, "Theme", items, 3);
    if (pick >= 0) e->theme = pick;
}
static void act_help(ide_t* e)
{
    (void)e;
    printf("\x1b[2J\x1b[H\x1b[0m");
    fflush(g_out);
    ide_tty_close();
    gmb_help_browse();
    ide_tty_open();
    printf("\x1b[?25l");
}
static void act_about(ide_t* e)
{
    char* items[] = {"GMBasic IDE (native C)", "Commands: see the menu bar (F10)", "Help: F1 / Help > Index", "Esc closes the IDE"};
    popup_run(e, "About", items, 4);
}
static void act_watch(ide_t* e, bool add)
{
    if (add) {
        char buf[IDE_MSG_MAX];
        if (!prompt_run(e, "Add watch (variable)", buf, sizeof(buf), "")) return;
        if (gmb_debug_add_watch(&g_dbg, buf) < 0) snprintf(e->msg, sizeof(e->msg), "?Watch set full");
        else e->panel = 1;
    } else {
        g_dbg.watch_count = 0;
        snprintf(e->msg, sizeof(e->msg), "All watch deleted");
    }
    e->panel_visible = true;
}
static void act_cut_copy(ide_t* e, bool cut)
{
    tab_t* t = e->tabs[e->active];
    free(e->clip);
    e->clip = strdup(t->lines[t->cl]);
    if (cut) {
        undo_push(e);
        tab_delete(t, t->cl);
        if (t->cl >= t->n) t->cl = t->n - 1;
        t->cc = 0;
        t->dirty = true;
        snprintf(e->msg, sizeof(e->msg), "Cut line");
    } else snprintf(e->msg, sizeof(e->msg), "Copy line");
}
static void act_paste(ide_t* e)
{
    if (!e->clip) { snprintf(e->msg, sizeof(e->msg), "Clipboard empty"); return; }
    undo_push(e);
    tab_t* t = e->tabs[e->active];
    char* save = NULL;
    char* line = strtok_r(e->clip, "\n", &save);
    int at = t->cl;
    if (!line) { tab_insert(t, at, ""); }
    while (line) { tab_insert(t, at++, line); line = strtok_r(NULL, "\n", &save); }
    t->cl = at - 1;
    t->dirty = true;
    snprintf(e->msg, sizeof(e->msg), "Paste");
}
static void act_select_all(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    size_t cap = 1024;
    for (int i = 0; i < t->n; i++) cap += strlen(t->lines[i]) + 1;
    free(e->clip);
    e->clip = (char*)malloc(cap);
    if (!e->clip) return;
    e->clip[0] = 0;
    for (int i = 0; i < t->n; i++) { strcat(e->clip, t->lines[i]); strcat(e->clip, "\n"); }
    snprintf(e->msg, sizeof(e->msg), "Copied whole buffer to clipboard");
}
static void act_print(ide_t* e)
{
    char path[512];
    snprintf(path, sizeof(path), "/tmp/gmb_ide_print_%ld.txt", (long)getpid());
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < e->tabs[e->active]->n; i++) fprintf(f, "%s\n", e->tabs[e->active]->lines[i]);
    fclose(f);
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "lpr \"%s\" 2>/dev/null", path);
    int rc = system(cmd);
    snprintf(e->msg, sizeof(e->msg), rc == 0 ? "Sent to printer" : "?Print: lpr failed (file %s)", path);
    out_add(e, e->msg);
}
static void act_clean(ide_t* e)
{
    char path[512];
    snprintf(path, sizeof(path), "/tmp/gmb_ide_%ld.a1", (long)getpid());
    unlink(path);
    char msg[IDE_MSG_MAX];
    snprintf(msg, sizeof(msg), "Clean: removed %s", path);
    out_add(e, msg);
    snprintf(e->msg, sizeof(e->msg), "%s", msg);
}
static void act_run_to_cursor(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    int line = t->cl + 1;
    gmb_debug_add_break(&g_dbg, line);
    g_dbg.mode = GMB_DBG_RUNNING;
    tab_run(e, false);
}
static void act_break(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    int line = t->cl + 1;
    if (gmb_debug_has_break(&g_dbg, line)) { gmb_debug_clear_break(&g_dbg, line); snprintf(e->msg, sizeof(e->msg), "Breakpoint removed (line %d)", line); }
    else if (gmb_debug_add_break(&g_dbg, line) >= 0) snprintf(e->msg, sizeof(e->msg), "Breakpoint set (line %d)", line);
    else snprintf(e->msg, sizeof(e->msg), "?Breakpoint set full");
}

static void do_action(ide_t* e, int a)
{
    if (a == A_NONE) return;
    if (a != A_UNDO) e->typing = false;
    switch (a) {
        case A_NEW: act_new(e); break;
        case A_OPEN: act_open(e, false); break;
        case A_MERGE: act_open(e, true); break;
        case A_SAVE: act_save(e, false); break;
        case A_SAVEAS: act_save(e, true); break;
        case A_SAVEALL: for (int i = 0; i < e->ntab; i++) { e->active = i; act_save(e, false); } break;
        case A_PRINT: act_print(e); break;
        case A_EXIT: snprintf(g_confirm_msg, sizeof(g_confirm_msg), "Save changes before exiting? (Y=save, N=discard, Esc=cancel)"); g_confirm_action = A_EXIT; g_ov = OV_CONFIRM; break;
        case A_UNDO: undo_do(e); break;
        case A_REDO: redo_do(e); break;
        case A_CUT: act_cut_copy(e, true); break;
        case A_COPY: act_cut_copy(e, false); break;
        case A_PASTE: act_paste(e); break;
        case A_CLEARLINE: { tab_t* t = e->tabs[e->active]; undo_push(e); free(t->lines[t->cl]); t->lines[t->cl] = strdup(""); t->cc = 0; t->dirty = true; } break;
        case A_NEWSUB: case A_NEWFUNC: case A_NEWGEN:
        {
            char buf[8 * 1024];
            gmb_ide_snippet(a == A_NEWSUB ? "SUB" : a == A_NEWFUNC ? "FUNCTION" : "GENERATOR", buf, (int)sizeof(buf));
            undo_push(e);
            tab_t* t = e->tabs[e->active];
            char* save = NULL; char line[IDE_LINE_CAP]; int at = t->n;
            char* tok = strtok_r(buf, "\n", &save);
            while (tok) { snprintf(line, sizeof(line), "%s", tok); tab_insert(t, at++, line); tok = strtok_r(NULL, "\n", &save); }
            t->cl = at - 1;
            break;
        }
        case A_COMMENT: act_comment(e, true); break;
        case A_UNCOMMENT: act_comment(e, false); break;
        case A_SELECTALL: act_select_all(e); break;
        case A_COMPLETE: act_complete(e); break;
        case A_GOTODEF: act_gotodef(e); break;
        case A_REFS: act_refs(e); break;
        case A_RENAME: act_rename(e); break;
        case A_SNIPPET: act_snippet(e); break;
        case A_OUTLINE: act_outline(e); break;
        case A_NEXTSUB: { tab_t* t = e->tabs[e->active]; for (int i = t->cl + 1; i < t->n; i++) { const char* s = t->lines[i]; while (*s == ' ') s++; if (!strncasecmp(s, "SUB ", 4) || !strncasecmp(s, "FUNCTION ", 9)) { t->cl = i; t->cc = 0; break; } } } break;
        case A_OUTPUT: e->panel_visible = !e->panel_visible; break;
        case A_LINENO: e->show_lines = !e->show_lines; break;
        case A_PANEL_OUT: e->panel = 0; e->panel_visible = true; break;
        case A_PANEL_WATCH: e->panel = 1; e->panel_visible = true; break;
        case A_PANEL_STACK: e->panel = 2; e->panel_visible = true; break;
        case A_FIND: act_find(e, false); break;
        case A_FINDSEL: act_find(e, false); break;
        case A_REPEAT: if (e->find_term[0]) { tab_t* t = e->tabs[e->active]; mod_sync(e); int line = gmb_ide_doc_find(&e->ide, gmb_ide_active(&e->ide), e->find_term, t->cl + 1); if (line >= 0) { t->cl = line; t->cc = 0; } else snprintf(e->msg, sizeof(e->msg), "?Not found"); } break;
        case A_REPLACE: act_find(e, true); break;
        case A_GOTOLINE: act_gotoline(e); break;
        case A_RUN: case A_RESTART: case A_CONTINUE: tab_run(e, false); break;
        case A_MAKEEXE: act_make(e, false); break;
        case A_MAKELIB: act_make(e, true); break;
        case A_BUILD: act_build(e, false); break;
        case A_REBUILD: act_build(e, false); break;
        case A_CLEAN: act_clean(e); break;
        case A_ERRORS: act_build(e, true); break;
        case A_BREAK: act_break(e); break;
        case A_BREAKCLR: gmb_debug_clear_all_breaks(&g_dbg); snprintf(e->msg, sizeof(e->msg), "All breakpoints cleared"); break;
        case A_STEPIN: tab_run(e, true); break;
        case A_STEPOVER: case A_STEPOUT: snprintf(e->msg, sizeof(e->msg), "%s: VM/C debugger supports single-step only (F8 = Step Into)", a == A_STEPOVER ? "Step Over" : "Step Out"); break;
        case A_RUNTO: act_run_to_cursor(e); break;
        case A_ADDWATCH: act_watch(e, true); break;
        case A_DELWATCH: act_watch(e, false); break;
        case A_OPTIONS: act_options(e); break;
        case A_THEME: act_theme(e); break;
        case A_HELP: act_help(e); break;
        case A_ABOUT: act_about(e); break;
    }
}

/* ─────────────────────────── input editing ─────────────────────────── */
static void ensure_undo(ide_t* e) { if (!e->typing) { undo_push(e); e->typing = true; } }
static void ed_insert(ide_t* e, int ch)
{
    tab_t* t = e->tabs[e->active];
    char* ln = t->lines[t->cl];
    int len = (int)strlen(ln);
    if (len >= IDE_LINE_CAP - 2) return;
    ensure_undo(e);
    if (e->overwrite && t->cc < len) ln[t->cc] = (char)ch;
    else { memmove(ln + t->cc + 1, ln + t->cc, (size_t)(len - t->cc) + 1); ln[t->cc] = (char)ch; }
    t->cc++;
    t->dirty = true;
}
static void ed_enter(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    undo_push(e);
    char rest[IDE_LINE_CAP];
    snprintf(rest, sizeof(rest), "%s", t->lines[t->cl] + t->cc);
    t->lines[t->cl][t->cc] = 0;
    tab_insert(t, t->cl + 1, rest);
    t->cl++; t->cc = 0; t->dirty = true;
}
static void ed_backspace(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    undo_push(e);
    char* ln = t->lines[t->cl];
    int len = (int)strlen(ln);
    if (t->cc > 0) { memmove(ln + t->cc - 1, ln + t->cc, (size_t)(len - t->cc) + 1); t->cc--; }
    else if (t->cl > 0) {
        int pl = (int)strlen(t->lines[t->cl - 1]);
        char joined[IDE_LINE_CAP];
        snprintf(joined, sizeof(joined), "%s%s", t->lines[t->cl - 1], ln);
        free(t->lines[t->cl - 1]);
        t->lines[t->cl - 1] = strdup(joined);
        tab_delete(t, t->cl);
        t->cl--; t->cc = pl;
    }
    t->dirty = true;
}
static void ed_delete(ide_t* e)
{
    tab_t* t = e->tabs[e->active];
    undo_push(e);
    char* ln = t->lines[t->cl];
    int len = (int)strlen(ln);
    if (t->cc < len) memmove(ln + t->cc, ln + t->cc + 1, (size_t)(len - t->cc));
    else if (t->cl < t->n - 1) {
        char joined[IDE_LINE_CAP];
        snprintf(joined, sizeof(joined), "%s%s", ln, t->lines[t->cl + 1]);
        free(t->lines[t->cl]);
        t->lines[t->cl] = strdup(joined);
        tab_delete(t, t->cl + 1);
    }
    t->dirty = true;
}

/* ─────────────────────────── menu ─────────────────────────── */
static void menu_key(ide_t* e, int k)
{
    const ide_menu_t* m = &g_menus[g_menu];
    if (k == 27) { g_ov = OV_NONE; return; }
    if (k == KEY_LEFT) { g_menu = (g_menu + IDE_NMENUS - 1) % IDE_NMENUS; g_menu_item = 0; return; }
    if (k == KEY_RIGHT) { g_menu = (g_menu + 1) % IDE_NMENUS; g_menu_item = 0; return; }
    if (k == KEY_UP) { do { g_menu_item = (g_menu_item + m->n - 1) % m->n; } while (m->items[g_menu_item].action == A_NONE); return; }
    if (k == KEY_DOWN) { do { g_menu_item = (g_menu_item + 1) % m->n; } while (m->items[g_menu_item].action == A_NONE); return; }
    if (k == 13) {
        int a = m->items[g_menu_item].action;
        if (a == A_NONE) return;
        g_ov = OV_NONE;
        do_action(e, a);
    }
}

/* ─────────────────────────── entry ─────────────────────────── */
int gmb_ide_run(const char* file)
{
    ide_t* e = (ide_t*)calloc(1, sizeof(ide_t));
    if (!e) { fprintf(stderr, "?ide: out of memory\n"); return 1; }
    g_ide = e;
    gmb_ide_init(&e->ide);
    e->show_lines = true;
    e->highlight = true;
    e->backup = true;
    e->tab_width = 2;
    e->theme = 0;
    e->panel_visible = true;
    e->panel = 0;
    e->clip = NULL;
    e->undo = (char**)calloc(IDE_UNDO_MAX, sizeof(char*));
    e->redo = (char**)calloc(IDE_UNDO_MAX, sizeof(char*));

    /* apri il file richiesto (o un untitled) */
    {
        tab_t* t = (tab_t*)calloc(1, sizeof(tab_t));
        tab_init(t, file && *file ? file : "untitled.bas");
        if (file && *file) {
            FILE* f = fopen(file, "r");
            if (f) {
                char buf[IDE_LINE_CAP];
                t->n = 0;
                while (fgets(buf, sizeof(buf), f)) {
                    size_t l = strlen(buf);
                    while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
                    tab_insert(t, t->n, buf);
                }
                fclose(f);
                if (t->n == 0) tab_insert(t, 0, "");
            } else {
                snprintf(e->msg, sizeof(e->msg), "New file: %s", file);
            }
        }
        e->tabs[e->ntab++] = t;
    }

    if (ide_tty_open() != 0) { fprintf(stderr, "?ide: /dev/tty not available\n"); return 1; }
    printf("\x1b[?25l");
    bool running = true;

    while (running) {
        struct winsize ws;
        if (ioctl(g_itty, TIOCGWINSZ, &ws) == 0) { e->rows = ws.ws_row; e->cols = ws.ws_col; }
        if (e->rows < 8) e->rows = 25;
        if (e->cols < 40) e->cols = 80;
        if (e->autosave > 0) {
            long now = (long)time(NULL);
            if (now - e->last_autosave >= e->autosave && tab_has_unsaved(e)) {
                e->last_autosave = now;
                int save_active = e->active;
                for (int i = 0; i < e->ntab; i++) { e->active = i; act_save(e, false); }
                e->active = save_active;
            }
        }
        render(e);

        int k = ide_key();
        if (g_ov == OV_MENU) { menu_key(e, k); continue; }
        if (g_ov == OV_CONFIRM) {
            if (k == 'y' || k == 'Y') { for (int i = 0; i < e->ntab; i++) { e->active = i; act_save(e, false); } running = false; }
            else if (k == 'n' || k == 'N') running = false;
            else { g_ov = OV_NONE; }
            continue;
        }
        if (g_ov == OV_POPUP || g_ov == OV_PROMPT) continue;   /* gestiti nei rispettivi loop */

        tab_t* t = e->tabs[e->active];
        if (k == KEY_F1) { do_action(e, A_HELP); continue; }
        if (k == KEY_F2) { do_action(e, A_OUTLINE); continue; }
        if (k == KEY_F3) { do_action(e, A_REPEAT); continue; }
        if (k == KEY_F4) { do_action(e, A_OUTPUT); continue; }
        if (k == KEY_F5) { do_action(e, A_RUN); continue; }
        if (k == KEY_F7) { do_action(e, A_BUILD); continue; }
        if (k == KEY_F8) { do_action(e, A_STEPIN); continue; }
        if (k == KEY_F9) { do_action(e, A_BREAK); continue; }
        if (k == KEY_F10) { g_ov = OV_MENU; g_menu = 0; g_menu_item = 0; continue; }
        if (k == KEY_F12) { do_action(e, A_GOTODEF); continue; }
        if (k == KEY_CTRL + 14) { do_action(e, A_NEW); continue; }          /* Ctrl+N */
        if (k == KEY_CTRL + 15) { do_action(e, A_OPEN); continue; }         /* Ctrl+O */
        if (k == KEY_CTRL + 19) { do_action(e, A_SAVE); continue; }         /* Ctrl+S */
        if (k == KEY_CTRL + 6)  { do_action(e, A_FIND); continue; }         /* Ctrl+F */
        if (k == KEY_CTRL + 7)  { do_action(e, A_GOTOLINE); continue; }     /* Ctrl+G */
        if (k == KEY_CTRL + 8)  { do_action(e, A_REPLACE); continue; }      /* Ctrl+H */
        if (k == KEY_CTRL + 12) { do_action(e, A_LINENO); continue; }       /* Ctrl+L */
        if (k == KEY_CTRL + 18) { do_action(e, A_RENAME); continue; }       /* Ctrl+R */
        if (k == KEY_CTRL + 25) { do_action(e, A_REDO); continue; }         /* Ctrl+Y */
        if (k == KEY_CTRL + 0)  { do_action(e, A_COMPLETE); continue; }     /* Ctrl+Space */
        if (k == KEY_CTRL + 26) { do_action(e, A_UNDO); continue; }         /* Ctrl+Z */
        if (k == KEY_TAB && e->ntab > 1) { e->active = (e->active + 1) % e->ntab; continue; }
        if (k == KEY_STAB && e->ntab > 1) { e->active = (e->active + e->ntab - 1) % e->ntab; continue; }
        if (k == KEY_CTRL + 23) { act_close_tab(e); continue; }             /* Ctrl+W */

        switch (k) {
            case 27: if (tab_has_unsaved(e)) { snprintf(g_confirm_msg, sizeof(g_confirm_msg), "Save changes before exiting? (Y=save, N=discard, Esc=cancel)"); g_confirm_action = A_EXIT; g_ov = OV_CONFIRM; }
                     else running = false; break;
            case 13: e->typing = false; ed_enter(e); break;
            case KEY_BS: e->typing = false; ed_backspace(e); break;
            case KEY_DEL: e->typing = false; ed_delete(e); break;
            case KEY_INS: e->overwrite = !e->overwrite; break;
            case KEY_UP: e->typing = false; if (t->cl > 0) t->cl--; if (t->cc > (int)strlen(t->lines[t->cl])) t->cc = (int)strlen(t->lines[t->cl]); break;
            case KEY_DOWN: e->typing = false; if (t->cl < t->n - 1) t->cl++; if (t->cc > (int)strlen(t->lines[t->cl])) t->cc = (int)strlen(t->lines[t->cl]); break;
            case KEY_LEFT: e->typing = false; if (t->cc > 0) t->cc--; else if (t->cl > 0) { t->cl--; t->cc = (int)strlen(t->lines[t->cl]); } break;
            case KEY_RIGHT: e->typing = false; { int len = (int)strlen(t->lines[t->cl]); if (t->cc < len) t->cc++; else if (t->cl < t->n - 1) { t->cl++; t->cc = 0; } } break;
            case KEY_HOME: e->typing = false; t->cc = 0; break;
            case KEY_END: e->typing = false; t->cc = (int)strlen(t->lines[t->cl]); break;
            case KEY_PGUP: e->typing = false; t->cl -= e->rows - IDE_PANEL_H - 4; if (t->cl < 0) t->cl = 0; break;
            case KEY_PGDN: e->typing = false; t->cl += e->rows - IDE_PANEL_H - 4; if (t->cl > t->n - 1) t->cl = t->n - 1; break;
            default:
                if (k == 9) { for (int i = 0; i < 4; i++) ed_insert(e, ' '); }
                else if (k >= 32 && k < 256) ed_insert(e, k);
                break;
        }
    }
    /* chiusura */
    printf("\x1b[2J\x1b[H\x1b[?25h\x1b[0m");
    ide_tty_close();
    for (int i = 0; i < e->ntab; i++) tab_free(e->tabs[i]);
    for (int i = 0; i < e->nundo; i++) free(e->undo[i]);
    free(e->undo);
    for (int i = 0; i < e->nredo; i++) free(e->redo[i]);
    free(e->redo);
    out_clear(e);
    free(e->clip);
    free(e);
    g_ide = NULL;
    return 0;
}
