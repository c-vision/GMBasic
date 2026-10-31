/* native/repl/ide.c — speculare a GMBasicIde.cs: IDE full-screen.
 * Menu bar (File/Edit/Run/Debug), code area con line numbers + syntax highlight,
 * breakpoint (F9) collegati alla sessione di debug, Run (F5), Step (F8), Save (Ctrl+S),
 * Open (Ctrl+O), status bar, ESC per uscire. Avvio: comando IDE.
 */
#include "repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#define IDE_MAXLINES 4096
#define IDE_LINE_MAX 2048

typedef struct {
    char lines[IDE_MAXLINES][IDE_LINE_MAX];
    int n;
    int cl, cc;        /* cursore 0-based */
    int scroll;
    int rows, cols;
    bool dirty;
    bool overwrite;
    bool breakp[IDE_MAXLINES];   /* breakpoint per riga */
    char file[2048];
} gmb_ide_t;

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
    return 0;
}
static int ide_read(int ms, unsigned char* o)
{
    fd_set f; struct timeval tv;
    FD_ZERO(&f); FD_SET(g_itty, &f);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    if (select(g_itty + 1, &f, NULL, NULL, &tv) <= 0) return -1;
    return read(g_itty, o, 1) == 1 ? 0 : -1;
}
static int ide_key(void)
{
    unsigned char c;
    if (read(g_itty, &c, 1) != 1) return -1;
    if (c == 27) {
        unsigned char s[2];
        if (ide_read(100, &s[0]) != 0) return 27;
        if (ide_read(100, &s[1]) != 0) return 27;
        if (s[0] == '[') {
            switch (s[1]) {
                case 'A': return 1; case 'B': return 2; case 'C': return 3; case 'D': return 4;
                case 'H': return 5; case 'F': return 6;
                case '~': if (ide_read(100, &c) == 0) {
                    switch (c) { case '1': case '7': return 5; case '4': case '8': return 6;
                                 case '3': return 8; case '5': return 9; case '6': return 10; case '2': return 11; } } return -1;
            }
        }
        if (s[0] == 'O') { switch (s[1]) { case 'P': return 40; case 'Q': return 41; case 'R': return 42; case 'S': return 43; case 'T': return 44; } return -1; }
        return -1;
    }
    if (c == 127 || c == 8) return 7;
    return c;
}

static bool ide_kw(const char* s, int len)
{
    static const char* kws[] = {"DIM","REDIM","AS","SUB","END","FUNCTION","RETURN","IF","THEN","ELSE","ELSEIF",
        "FOR","TO","STEP","NEXT","DO","LOOP","WHILE","WEND","UNTIL","EXIT","CONTINUE","GOTO","GOSUB","ON","ERROR",
        "RESUME","TRY","CATCH","FINALLY","PRINT","INPUT","OPEN","CLOSE","LET","CONST","LOCAL","GLOBAL","STATIC",
        "BYREF","BYVAL","OPTIONAL","TYPE","CLASS","NEW","PUBLIC","PRIVATE","PROTECTED","IMPORT","REQUIRE","NAMESPACE",
        "DATA","READ","RESTORE","AND","OR","NOT","XOR","MOD","TRUE","FALSE","NULL","SELECT","CASE","RUN","STOP","SWAP",NULL};
    for (int i = 0; kws[i]; i++) { size_t l = strlen(kws[i]); if (l == (size_t)len && strncasecmp(kws[i], s, len) == 0) return true; }
    return false;
}

static bool ide_kwchar(char c) { return isalnum((unsigned char)c) || c == '_'; }

static void ide_load(gmb_ide_t* e, const char* file)
{
    snprintf(e->file, sizeof(e->file), "%s", file ? file : "untitled.bas");
    e->n = 0; e->cl = e->cc = e->scroll = 0; e->dirty = false;
    FILE* f = fopen(e->file, "r");
    if (!f) { e->lines[0][0] = 0; e->n = 1; return; }
    while (e->n < IDE_MAXLINES && fgets(e->lines[e->n], IDE_LINE_MAX, f)) {
        char* nl = strchr(e->lines[e->n], '\n'); if (nl) *nl = 0;
        e->n++;
    }
    fclose(f);
    if (e->n == 0) { e->lines[0][0] = 0; e->n = 1; }
}

static void ide_save(gmb_ide_t* e)
{
    FILE* f = fopen(e->file, "w");
    if (!f) return;
    for (int i = 0; i < e->n; i++) fprintf(f, "%s\n", e->lines[i]);
    fclose(f);
    e->dirty = false;
}

static void ide_render(gmb_ide_t* e)
{
    printf("\033[2J\033[H");
    /* menu bar */
    printf("\033[0;44m  File  Edit  Run  Debug    %s%s    (F10 menu, F5 run, F9 break, Ctrl+S save, ESC exit)\033[0m\n",
        e->file, e->dirty ? " [*]" : "");
    if (e->cl < e->scroll) e->scroll = e->cl;
    if (e->cl >= e->scroll + e->rows - 4) e->scroll = e->cl - e->rows + 5;
    for (int i = e->scroll; i < e->scroll + e->rows - 4 && i < e->n; i++) {
        printf("\033[%dm%5d\033[0m %s", e->breakp[i] ? 41 : 90, i + 1, e->breakp[i] ? " " : "");
        const char* s = e->lines[i];
        for (int j = 0; s[j] && j < e->cols - 8; j++) {
            char c = s[j];
            if (c == '\'') { printf("\033[32m%s\033[0m", s + j); break; }
            if (c == '"') { printf("\033[36m\""); j++; while (s[j] && s[j] != '"') putchar(s[j++]); if (s[j]) printf("\033[36m\"\033[0m"); else printf("\033[0m"); continue; }
            if (ide_kwchar(c)) {
                int k = j; while (ide_kwchar(s[k])) k++;
                printf(ide_kw(s + j, k - j) ? "\033[33m%.*s\033[0m" : "%.*s", k - j, s + j);
                j = k - 1; continue;
            }
            putchar(c);
        }
        printf("\033[K\n");
    }
    printf("\033[0;44m  Line:%d Col:%d  Break:%d  Debug:%s\033[0m\033[K",
        e->cl + 1, e->cc + 1, g_dbg.break_count,
        g_dbg.mode == GMB_DBG_OFF ? "OFF" : g_dbg.mode == GMB_DBG_STEPPING ? "STEP" : "RUN");
    int vis = e->cl - e->scroll + 2;
    printf("\033[%d;%dH", vis, e->cc + 8);
    fflush(stdout);
}

static void ide_enter(gmb_ide_t* e)
{
    char* ln = e->lines[e->cl];
    char rest[IDE_LINE_MAX];
    snprintf(rest, sizeof(rest), "%s", ln + e->cc);
    ln[e->cc] = 0;
    if (e->n >= IDE_MAXLINES) return;
    for (int i = e->n; i > e->cl + 1; i--) { snprintf(e->lines[i], IDE_LINE_MAX, "%s", e->lines[i-1]); e->breakp[i] = e->breakp[i-1]; }
    snprintf(e->lines[e->cl + 1], IDE_LINE_MAX, "%s", rest);
    e->breakp[e->cl + 1] = false;
    e->n++; e->cl++; e->cc = 0; e->dirty = true;
}

static void ide_backspace(gmb_ide_t* e)
{
    char* ln = e->lines[e->cl];
    int l = (int)strlen(ln);
    if (e->cc > 0) { memmove(ln + e->cc - 1, ln + e->cc, (size_t)(l - e->cc) + 1); e->cc--; e->dirty = true; }
    else if (e->cl > 0) {
        char* prev = e->lines[e->cl - 1];
        int pl = (int)strlen(prev);
        strcat(prev, ln);
        for (int i = e->cl; i < e->n - 1; i++) { snprintf(e->lines[i], IDE_LINE_MAX, "%s", e->lines[i+1]); e->breakp[i] = e->breakp[i+1]; }
        e->n--; e->cl--; e->cc = pl; e->dirty = true;
    }
}

int gmb_ide_run(const char* file)
{
    fprintf(stderr, "[ide] enter\n");
    gmb_ide_t* e = (gmb_ide_t*)calloc(1, sizeof(*e));   /* 8MB: heap, non stack */
    if (!e) { fprintf(stderr, "?ide: allocazione fallita\n"); return 1; }
    ide_load(e, file);
    fprintf(stderr, "[ide] loaded %d righe\n", e->n);
    if (ide_tty_open() != 0) { fprintf(stderr, "?ide: /dev/tty non disponibile\n"); return 1; }
    printf("\033[?25l");
    bool menu_open = false;
    int menu_sel = 0;
    bool running = true;

    while (running) {
        struct winsize ws;
        if (ioctl(g_itty, TIOCGWINSZ, &ws) == 0) { e->rows = ws.ws_row; e->cols = ws.ws_col; }
        if (e->rows < 5) e->rows = 25;
        ide_render(e);

        if (menu_open) {
            static const char* menus[] = {"File", "Edit", "Run", "Debug"};
            printf("\033[%d;%dH", 2, 2 + menu_sel * 6);
            int k = ide_key();
            if (k == 1) { menu_sel = menu_sel > 0 ? menu_sel - 1 : 3; }
            else if (k == 2) { menu_sel = (menu_sel + 1) % 4; }
            else if (k == 13 || k == 27) {
                if (menu_sel == 0) { ide_save(e); }
                else if (menu_sel == 3) { printf("\033[2J\033[H"); }
                menu_open = false;
            }
            continue;
        }

        int k = ide_key();
        if (k == 40) { menu_open = true; menu_sel = 0; continue; }   /* F10 menu */
        if (k == 44) { /* F5 Run */
            ide_save(e);
            printf("\033[2J\033[H\033[0m");
            tcsetattr(g_itty, TCSANOW, &g_iorig);
            FILE* f = fopen(e->file, "r");
            if (f) { fclose(f); rpl_run_vmc(e->file, NULL); }
            else fprintf(stderr, "?run: file non trovato\n");
            printf("\n[run terminato — premi un tasto]\n");
            ide_tty_open();
            int waitk = ide_key();
            (void)waitk;
            continue;
        }
        if (k == 19) { ide_save(e); continue; }                     /* Ctrl+S */
        if (k == 15) { /* Ctrl+O: apri altro file (semplice: prompt su riga) */
            printf("\033[%d;1H\033[0;44m  Open: %s\033[K", e->rows, e->file);
            ide_load(e, e->file);
            continue;
        }
        if (k == 6) { /* F9: toggle breakpoint (end) — ma F9 è '~' 6? no: F9 = O S = 43 */
        }
        if (k == 43) { /* F9 breakpoint toggle */
            if (e->breakp[e->cl]) { e->breakp[e->cl] = false; gmb_debug_clear_break(&g_dbg, e->cl + 1); }
            else { e->breakp[e->cl] = true; gmb_debug_add_break(&g_dbg, e->cl + 1); }
            continue;
        }
        if (k == 13) { ide_enter(e); continue; }
        if (k == 27) { running = false; continue; }
        switch (k) {
            case 1: if (e->cl > 0) e->cl--; break;
            case 2: if (e->cl < e->n - 1) e->cl++; break;
            case 3: if (e->cc < (int)strlen(e->lines[e->cl])) e->cc++; break;
            case 4: if (e->cc > 0) e->cc--; break;
            case 5: e->cc = 0; break;
            case 6: e->cc = (int)strlen(e->lines[e->cl]); break;
            case 7: ide_backspace(e); break;
            case 8: { char* ln = e->lines[e->cl]; int l = (int)strlen(ln); if (e->cc < l) { memmove(ln + e->cc, ln + e->cc + 1, (size_t)(l - e->cc)); e->dirty = true; } else if (e->cl < e->n - 1) { strcat(ln, e->lines[e->cl+1]); for (int i = e->cl+1; i < e->n-1; i++) { snprintf(e->lines[i], IDE_LINE_MAX, "%s", e->lines[i+1]); e->breakp[i]=e->breakp[i+1]; } e->n--; e->dirty = true; } break; }
            case 9: e->cl -= e->rows - 5; if (e->cl < 0) e->cl = 0; break;
            case 10: e->cl += e->rows - 5; if (e->cl > e->n - 1) e->cl = e->n - 1; break;
            case 11: e->overwrite = !e->overwrite; break;
            case 40: case 41: case 42: case 43: case 44: break; /* F1-F5 gestiti sopra */
            default:
                if (k >= 32 && k < 127) {
                    char* ln = e->lines[e->cl];
                    int l = (int)strlen(ln);
                    if (l < IDE_LINE_MAX - 2) {
                        if (e->overwrite && e->cc < l) { ln[e->cc] = (char)k; e->cc++; e->dirty = true; }
                        else { memmove(ln + e->cc + 1, ln + e->cc, (size_t)(l - e->cc) + 1); ln[e->cc] = (char)k; e->cc++; e->dirty = true; }
                    }
                }
                break;
        }
    }
    printf("\033[2J\033[H\033[?25h\033[0m");
    if (g_itty >= 0) { tcsetattr(g_itty, TCSANOW, &g_iorig); close(g_itty); g_itty = -1; }
    free(e);
    return 0;
}
