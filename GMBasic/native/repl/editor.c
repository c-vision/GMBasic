/* native/repl/editor.c — speculare a GWBasicEditor.cs (buffer di programma).
 * Stato del buffer (LIST/NEW/SAVE/LOAD/EDIT/AUTO/DELETE/RENUM).
 */
#include "repl.h"

#define RPL_BUF_MAX 4096
#define RPL_LINE_MAX 1024
/* Buffer di programma: righe COMPLETE "numero testo" ordinate per numero
 * (parità ReplEngine.cs: SortedDictionary<int,string> _programLines). */
static char g_buf[RPL_BUF_MAX][RPL_LINE_MAX];
static int g_buf_n = 0;
static int g_auto_inc = 10;

/* Numero di riga iniziale della stringa; -1 se non numerata. */
static int rpl_line_num(const char* s, const char** body)
{
    while (*s == ' ' || *s == '\t') s++;
    if (*s < '0' || *s > '9') { if (body) *body = s; return -1; }
    long n = 0; while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    while (*s == ' ' || *s == '\t') s++;
    if (body) *body = s;
    return (int)n;
}
/* Posizione della riga `num` se esiste (>=0), altrimenti -(inserimento+1). */
static int rpl_line_pos(int num)
{
    for (int i = 0; i < g_buf_n; i++) {
        int n = rpl_line_num(g_buf[i], NULL);
        if (n == num) return i;
        if (n < 0 || n > num) return -(i + 1);
    }
    return -(g_buf_n + 1);
}

void rpl_editor_set_line(int num, const char* body)
{
    char tmp[RPL_LINE_MAX];
    snprintf(tmp, sizeof(tmp), "%d %s", num, body ? body : "");
    int pos = rpl_line_pos(num);
    if (pos >= 0) { snprintf(g_buf[pos], RPL_LINE_MAX, "%s", tmp); return; }
    if (g_buf_n >= RPL_BUF_MAX) return;
    int ins = -pos - 1;
    for (int i = g_buf_n; i > ins; i--) snprintf(g_buf[i], RPL_LINE_MAX, "%s", g_buf[i - 1]);
    snprintf(g_buf[ins], RPL_LINE_MAX, "%s", tmp);
    g_buf_n++;
}

void rpl_editor_remove_line(int num)
{
    int pos = rpl_line_pos(num);
    if (pos < 0) return;
    for (int i = pos; i < g_buf_n - 1; i++) snprintf(g_buf[i], RPL_LINE_MAX, "%s", g_buf[i + 1]);
    g_buf_n--;
}

/* Sorgente completo del buffer (righe unite) per RUN. Buffer statico riusabile. */
const char* rpl_editor_source(void)
{
    static char src[RPL_BUF_MAX * 8];
    src[0] = 0;
    size_t off = 0;
    for (int i = 0; i < g_buf_n; i++) {
        int n = snprintf(src + off, sizeof(src) - off, "%s\n", g_buf[i]);
        if (n < 0 || (size_t)n >= sizeof(src) - off) break;
        off += (size_t)n;
    }
    return src;
}

void rpl_editor_load(const char* path)
{
    g_buf_n = 0;
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "?file non trovato: %s\n", path); return; }
    char raw[RPL_LINE_MAX];
    while (fgets(raw, sizeof(raw), f)) {
        char* e = raw + strlen(raw);
        while (e > raw && (e[-1]=='\n'||e[-1]=='\r')) *--e=0;
        const char* body = NULL;
        int num = rpl_line_num(raw, &body);
        if (num >= 0) rpl_editor_set_line(num, body);
    }
    fclose(f);
}

void rpl_editor_save(const char* path)
{
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "?impossibile salvare: %s\n", path); return; }
    for (int i = 0; i < g_buf_n; i++) fprintf(f, "%s\n", g_buf[i]);
    fclose(f);
}

void rpl_editor_list(void)
{
    if (g_buf_n == 0) { printf("(nessuna riga)\n"); return; }
    for (int i = 0; i < g_buf_n; i++) printf("%s\n", g_buf[i]);
}

void rpl_editor_new(void) { g_buf_n = 0; }

void rpl_editor_delete(const char* range)
{
    int from = 0, to = 0;
    const char* dash = strchr(range, '-');
    if (dash) { from = atoi(range); to = atoi(dash + 1); }
    else { from = to = atoi(range); }
    if (range[0] == 0) { printf("?DELETE <linea>|<inizio>-<fine>\n"); return; }
    for (int i = g_buf_n - 1; i >= 0; i--) {
        int n = rpl_line_num(g_buf[i], NULL);
        if (n >= from && n <= to) rpl_editor_remove_line(n);
    }
}

void rpl_editor_auto(int inc)
{
    if (inc < 1) inc = 10;
    g_auto_inc = inc;
    printf("AUTO incremento=%d\n", g_auto_inc);
}

void rpl_editor_renum(void)
{
    if (g_buf_n == 0) return;
    char tmp[RPL_BUF_MAX][RPL_LINE_MAX];
    int n = 0, newnum = 10;
    for (int i = 0; i < g_buf_n; i++) {
        const char* body = NULL;
        rpl_line_num(g_buf[i], &body);
        snprintf(tmp[n], RPL_LINE_MAX, "%d %s", newnum, body);
        newnum += 10; n++;
    }
    for (int i = 0; i < n; i++) snprintf(g_buf[i], RPL_LINE_MAX, "%s", tmp[i]);
    g_buf_n = n;
    printf("Renumbered.\n");
}

int rpl_editor_count(void) { return g_buf_n; }

/* ════════════════════════════════════════════════════════════════════
 * FULL-SCREEN EDITOR (speculare a GWBasicEditor.cs — CodeArea + sintassi)
 * Editor ANSI + termios raw: navigazione, insert/overwrite, split/delete,
 * salva (Ctrl+S), esci (ESC ×2), line numbers, syntax highlight, status bar.
 * ════════════════════════════════════════════════════════════════════ */
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>

#define GMB_ED_MAXL 4096
typedef struct {
    char** lines;
    int n, cap;
    int cl, cc;          /* cursore (0-based) */
    int scroll;          /* prima riga visibile */
    int rows, cols;
    bool dirty, overwrite;
    char file[2048];
} gmb_ed_t;

static void gmb_ed_grow(gmb_ed_t* e, int need)
{
    if (need >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 128;
        while (e->cap < need) e->cap *= 2;
        e->lines = (char**)realloc(e->lines, (size_t)e->cap * sizeof(char*));
    }
}

static void gmb_ed_load(gmb_ed_t* e, const char* file)
{
    snprintf(e->file, sizeof(e->file), "%s", file ? file : "untitled.bas");
    FILE* f = fopen(e->file, "r");
    if (!f) { gmb_ed_grow(e, 1); e->lines[0] = strdup(""); e->n = 1; return; }
    char line[GMB_ED_MAXL];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        gmb_ed_grow(e, e->n + 1);
        e->lines[e->n++] = strdup(line);
    }
    fclose(f);
    if (e->n == 0) { gmb_ed_grow(e, 1); e->lines[0] = strdup(""); e->n = 1; }
}

static void gmb_ed_save(gmb_ed_t* e)
{
    if (getenv("GMBASIC_DEBUG")) fprintf(stderr, "[ed] SAVE '%s' (%d righe)\n", e->file, e->n);
    FILE* f = fopen(e->file, "w");
    if (!f) return;
    for (int i = 0; i < e->n; i++) fprintf(f, "%s\n", e->lines[i]);
    fclose(f);
    e->dirty = false;
}

/* syntax highlight: parola chiave gialla, 'commento verde, "stringa" ciano */
static bool is_kw_char(char c) { return isalnum((unsigned char)c) || c == '_'; }
static bool gmb_ed_keyword(const char* s, int len)
{
    static const char* kws[] = {"DIM","REDIM","AS","SUB","END","FUNCTION","RETURN","IF","THEN","ELSE",
        "ELSEIF","FOR","TO","STEP","NEXT","DO","LOOP","WHILE","WEND","UNTIL","EXIT","CONTINUE","GOTO",
        "GOSUB","ON","ERROR","RESUME","TRY","CATCH","FINALLY","PRINT","INPUT","OPEN","CLOSE","LET",
        "CONST","LOCAL","GLOBAL","STATIC","BYREF","BYVAL","OPTIONAL","TYPE","CLASS","NEW","PUBLIC",
        "PRIVATE","PROTECTED","IMPORT","REQUIRE","NAMESPACE","DATA","READ","RESTORE","AND","OR","NOT",
        "XOR","MOD","TRUE","FALSE","NULL","SELECT","CASE","RUN","STOP","SWAP", NULL};
    for (int i = 0; kws[i]; i++) {
        size_t l = strlen(kws[i]);
        if (l == (size_t)len && strncasecmp(kws[i], s, len) == 0) return true;
    }
    return false;
}

static void gmb_ed_render_line(gmb_ed_t* e, int idx)
{
    const char* s = e->lines[idx];
    /* gutter con numero riga */
    printf("\033[%dm%5d\033[0m ", 90, idx + 1);
    int in_str = 0;
    for (int i = 0; s[i] && i < e->cols - 8; i++) {
        char c = s[i];
        if (c == '\'') { printf("\033[32m%s\033[0m", s + i); return; }   /* commento verde fino a fine */
        if (c == '"') { in_str = !in_str; printf("\033[36m\""); continue; }
        if (in_str) { putchar(c); continue; }
        if (is_kw_char(c)) {
            int j = i; while (is_kw_char(s[j])) j++;
            printf(gmb_ed_keyword(s + i, j - i) ? "\033[33m%.*s\033[0m" : "%.*s", j - i, s + i);
            i = j - 1; continue;
        }
        putchar(c);
    }
    if (in_str) printf("\033[0m");
    printf("\033[K");   /* cancella a fine riga */
}

static void gmb_ed_render(gmb_ed_t* e)
{
    printf("\033[2J\033[H");
    /* titolo */
    printf("\033[1;44m  %s%s%s\033[0m\n", e->file, e->dirty ? " [*]" : "", "  (Ctrl+S salva, ESC ESC esce)");
    if (e->cl < e->scroll) e->scroll = e->cl;
    if (e->cl >= e->scroll + e->rows - 3) e->scroll = e->cl - e->rows + 4;
    for (int i = e->scroll; i < e->scroll + e->rows - 3 && i < e->n; i++)
        gmb_ed_render_line(e, i);
    for (int i = e->scroll + e->rows - 3 - e->n + e->scroll; i > 0 && e->n < e->scroll + e->rows - 3; i--) { /* nop */
        break;
    }
    /* status bar */
    printf("\033[%d;1H\033[0;44m  %s  Line:%d  Col:%d%s\033[0m", e->rows, e->overwrite ? "INS" : "OVR", e->cl + 1, e->cc + 1, "");
    /* posiziona cursore */
    int vis = e->cl - e->scroll + 2;
    printf("\033[%d;%dH", vis, e->cc + 7);
    fflush(stdout);
}

/* legge 1 byte dal tty con timeout (ms). Ritorna -2 su timeout, -1 su errore. */
static int tty_read_timeout(int tfd, unsigned char* out, int ms)
{
    fd_set fds; struct timeval tv;
    FD_ZERO(&fds); FD_SET(tfd, &fds);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    if (select(tfd + 1, &fds, NULL, NULL, &tv) <= 0) return -2;
    return read(tfd, out, 1) == 1 ? 0 : -1;
}

static int gmb_ed_readkey(int tfd)
{
    unsigned char c;
    if (read(tfd, &c, 1) != 1) return -1;
    if (c == 27) {
        unsigned char seq[3];
        if (tty_read_timeout(tfd, &seq[0], 100) != 0) return 27;   /* ESC nudo */
        if (tty_read_timeout(tfd, &seq[1], 100) != 0) return 27;
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 1;  /* up */
                case 'B': return 2;  /* down */
                case 'C': return 3;  /* right */
                case 'D': return 4;  /* left */
                case 'H': return 5;  /* home */
                case 'F': return 6;  /* end */
                case '~':
                    if (tty_read_timeout(tfd, &seq[2], 100) != 0) return -1;
                    switch (seq[2]) {
                        case '1': case '7': return 5;   /* home */
                        case '4': case '8': return 6;   /* end */
                        case '3': return 8;             /* delete */
                        case '5': return 9;             /* pgup */
                        case '6': return 10;            /* pgdn */
                        case '2': return 11;            /* insert */
                    }
                    return -1;
            }
        }
        if (seq[0] == 'O') {
            switch (seq[1]) { case 'P': return 40; case 'S': return 43; case 'T': return 44; }
            return -1;
        }
        return -1;
    }
    if (c == 127) return 7;   /* backspace */
    return c;
}

static void gmb_ed_insert_char(gmb_ed_t* e, char c)
{
    char* ln = e->lines[e->cl];
    int l = (int)strlen(ln);
    if (e->cc > l) e->cc = l;
    if (e->cc < GMB_ED_MAXL - 1) {
        memmove(ln + e->cc + 1, ln + e->cc, (size_t)(l - e->cc) + 1);
        ln[e->cc] = c;
        e->cc++;
        e->dirty = true;
    }
}

static void gmb_ed_enter(gmb_ed_t* e)
{
    char* ln = e->lines[e->cl];
    int l = (int)strlen(ln);
    char* rest = strdup(ln + e->cc);
    ln[e->cc] = 0;
    gmb_ed_grow(e, e->n + 1);
    for (int i = e->n; i > e->cl + 1; i--) e->lines[i] = e->lines[i - 1];
    e->lines[e->cl + 1] = rest;
    e->n++;
    e->cl++;
    e->cc = 0;
    e->dirty = true;
}

static void gmb_ed_backspace(gmb_ed_t* e)
{
    char* ln = e->lines[e->cl];
    if (e->cc > 0) {
        memmove(ln + e->cc - 1, ln + e->cc, strlen(ln + e->cc) + 1);
        e->cc--;
        e->dirty = true;
    } else if (e->cl > 0) {
        char* prev = e->lines[e->cl - 1];
        int pl = (int)strlen(prev);
        strcat(prev, ln);
        free(ln);
        for (int i = e->cl; i < e->n - 1; i++) e->lines[i] = e->lines[i + 1];
        e->n--;
        e->cl--;
        e->cc = pl;
        e->dirty = true;
    }
}

static void gmb_ed_delete(gmb_ed_t* e)
{
    char* ln = e->lines[e->cl];
    int l = (int)strlen(ln);
    if (e->cc < l) {
        memmove(ln + e->cc, ln + e->cc + 1, (size_t)(l - e->cc));
        e->dirty = true;
    } else if (e->cl < e->n - 1) {
        strcat(ln, e->lines[e->cl + 1]);
        free(e->lines[e->cl + 1]);
        for (int i = e->cl + 1; i < e->n - 1; i++) e->lines[i] = e->lines[i + 1];
        e->n--;
        e->dirty = true;
    }
}

int gmb_editor_run(const char* file)
{
    gmb_ed_t ed = {0};
    gmb_ed_load(&ed, file);
    int tfd = open("/dev/tty", O_RDWR);
    if (tfd < 0) { fprintf(stderr, "?editor: /dev/tty non disponibile\n"); return 1; }
    struct termios orig, raw;
    tcgetattr(tfd, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    tcsetattr(tfd, TCSANOW, &raw);
    printf("\033[?25l");  /* nascondi cursore sistema */

    bool exiting = false;
    while (!exiting) {
        struct winsize ws;
        if (ioctl(0, TIOCGWINSZ, &ws) == 0) { ed.rows = ws.ws_row; ed.cols = ws.ws_col; }
        if (ed.rows < 5) ed.rows = 25;
        gmb_ed_render(&ed);
        int k = gmb_ed_readkey(tfd);
        if (getenv("GMBASIC_DEBUG") && (k == 19 || k == 27 || k == 13 || k < 32)) fprintf(stderr, "[ed] key=%d\n", k);
        switch (k) {
            case 1: if (ed.cl > 0) ed.cl--; break;                          /* up */
            case 2: if (ed.cl < ed.n - 1) ed.cl++; break;                   /* down */
            case 3: if (ed.cc < (int)strlen(ed.lines[ed.cl])) ed.cc++; break;/* right */
            case 4: if (ed.cc > 0) ed.cc--; break;                          /* left */
            case 5: ed.cc = 0; break;                                       /* home */
            case 6: ed.cc = (int)strlen(ed.lines[ed.cl]); break;            /* end */
            case 7: gmb_ed_backspace(&ed); break;
            case 8: gmb_ed_delete(&ed); break;
            case 9: ed.cl -= ed.rows - 4; if (ed.cl < 0) ed.cl = 0; break;  /* pgup */
            case 10: ed.cl += ed.rows - 4; if (ed.cl > ed.n - 1) ed.cl = ed.n - 1; break; /* pgdn */
            case 11: ed.overwrite = !ed.overwrite; break;                   /* insert */
            case 13: gmb_ed_enter(&ed); break;                              /* enter */
            case 40: break;                                                 /* F1 */
            case 44: break;                                                 /* F5 */
            case 27:                                                        /* ESC */
                if (exiting) { exiting = false; } else exiting = true;
                break;
            case 19:                                                        /* Ctrl+S */
                gmb_ed_save(&ed);
                break;
            default:
                if (k >= 32 && k < 127) {
                    if (ed.overwrite) {
                        char* ln = ed.lines[ed.cl];
                        int l = (int)strlen(ln);
                        if (ed.cc < l) { ln[ed.cc] = (char)k; ed.cc++; ed.dirty = true; }
                        else gmb_ed_insert_char(&ed, (char)k);
                    } else gmb_ed_insert_char(&ed, (char)k);
                }
                break;
        }
    }
    printf("\033[2J\033[H\033[?25h\033[0m");
    tcsetattr(tfd, TCSANOW, &orig);
    close(tfd);
    /* salva il buffer di programma della REPL per compatibilità (EDIT usa g_buf) */
    for (int i = 0; i < ed.n && i < RPL_BUF_MAX; i++) {
        snprintf(g_buf[i], RPL_LINE_MAX, "%s", ed.lines[i]);
    }
    g_buf_n = ed.n < RPL_BUF_MAX ? ed.n : RPL_BUF_MAX;
    for (int i = 0; i < ed.n; i++) free(ed.lines[i]);
    free(ed.lines);
    printf("editor chiuso: %d righe (%s)\n", g_buf_n, ed.dirty ? "non salvato" : "salvato");
    return 0;
}
