/* native/repl/editor.c — buffer di programma della REPL (LIST/NEW/SAVE/LOAD/AUTO/DELETE/RENUM).
 *
 * L'EDITOR interattivo NON vive più qui: c'è UNA sola TUI, `ide.c` (parità di comandi con
 * l'IDE della REPL C#); il vecchio editor full-screen duplicato è stato rimosso (2026-11-01).
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
