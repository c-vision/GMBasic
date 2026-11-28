/* ============================================================================
 * gmb_ui_palette.c — command palette (vedi include/gmb_ui_palette.h).
 *
 * Tre pezzi separati, come nell'inspector:
 *   1. REGISTRO delle azioni + FILTRO deterministico (nessun I/O, testabile);
 *   2. STATO dell'overlay (query, selezione) e gestione dei tasti;
 *   3. ESECUZIONE: l'azione scelta passa dal GPM (`gmb_task_spawn`), mai inline
 *      sul thread che legge il tasto.
 * Il disegno esiste in due forme (celle / pixel) e non contiene logica.
 * ============================================================================ */
#include "gmb_ui_palette.h"
#include "gmb_c99gpm.h"       /* gmb_c99gpm_active, gmb_task_spawn */
#include "gmb_tui_widgets.h"  /* tema a celle, stile corrente */
#include "gmb_ui_gfx.h"       /* GMB_GFX_* per il percorso pixel */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    char id[48];
    char title[64];
    char shortcut[24];
    char category[32];
    int  used;                 /* slot occupato */
    int  order;                /* ordine di registrazione (pareggio del filtro) */
} pal_item_t;

static pal_item_t g_items[GMB_PALETTE_MAX];
static int g_nitems = 0;
static int g_next_order = 1;

static int  g_open = 0;
static char g_query[GMB_PALETTE_QUERY_MAX];
static int  g_sel = 0;                 /* posizione nell'ELENCO FILTRATO */

static gmb_palette_runner_t g_runner = NULL;
static void* g_runner_ctx = NULL;
static int   g_pending = -1;           /* azione accodata, -1 = nessuna */

/* Estrae il tasto da un carattere UTF-8 (le lettere accentate restano intere). */
static int utf8_next(const char** p)
{
    const unsigned char* s = (const unsigned char*)*p;
    if (!*s) return 0;
    int c = *s;
    int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
    *p += len;
    return c;   /* il byte di testa basta per l'apposizione ed è stabile */
}

/* Copia normalizzata (minuscole ASCII, spazi singoli) per il confronto. */
static void norm_copy(char* dst, int cap, const char* src)
{
    int n = 0, sp = 0;
    for (const char* p = src ? src : ""; *p && n < cap - 1; p++) {
        unsigned char c = (unsigned char)*p;
        if (isspace(c)) { if (!sp && n) { dst[n++] = ' '; sp = 1; } continue; }
        sp = 0;
        dst[n++] = (char)tolower(c);
    }
    while (n > 0 && dst[n - 1] == ' ') n--;
    dst[n] = '\0';
}

static int starts_with(const char* s, const char* pre)
{
    size_t n = strlen(pre);
    return n == 0 || strncmp(s, pre, n) == 0;
}

/* Pertinenza: 0 = titolo che inizia con la query, 1 = titolo che la contiene,
 * 2 = scorciatoia/categoria/parola che la contiene, 3 = fuori elenco. */
static int pal_score(const pal_item_t* it, const char* q, char* title_n, char* misc_n)
{
    if (!q[0]) return 0;
    if (starts_with(title_n, q)) return 0;
    if (strstr(title_n, q)) return 1;
    if (strstr(misc_n, q)) return 2;
    return 3;
}

int gmb_palette_register(const char* id, const char* title,
                         const char* shortcut, const char* category)
{
    if (!id || !*id || !title || !*title) return -1;
    int idx = -1;
    for (int i = 0; i < g_nitems; i++) if (g_items[i].used && !strcmp(g_items[i].id, id)) { idx = i; break; }
    if (idx < 0) {
        if (g_nitems >= GMB_PALETTE_MAX) return -2;
        idx = g_nitems++;
        g_items[idx].used = 1;
        g_items[idx].order = g_next_order++;
    }
    snprintf(g_items[idx].id, sizeof g_items[idx].id, "%s", id);
    snprintf(g_items[idx].title, sizeof g_items[idx].title, "%s", title);
    snprintf(g_items[idx].shortcut, sizeof g_items[idx].shortcut, "%s", shortcut ? shortcut : "");
    snprintf(g_items[idx].category, sizeof g_items[idx].category, "%s", category ? category : "");
    return 0;
}

int  gmb_palette_count(void) { return g_nitems; }

void gmb_palette_clear(void)
{
    memset(g_items, 0, sizeof g_items);
    g_nitems = 0;
    g_next_order = 1;
    g_sel = 0;
    g_query[0] = '\0';
    g_pending = -1;
}

void gmb_palette_open(void)  { g_open = 1; g_query[0] = '\0'; g_sel = 0; }
void gmb_palette_close(void) { g_open = 0; g_pending = -1; }
int  gmb_palette_is_open(void) { return g_open; }

void gmb_palette_set_query(const char* text) { norm_copy(g_query, sizeof g_query, text); g_sel = 0; }
const char* gmb_palette_query(void) { return g_query; }

void gmb_palette_type(const char* text)
{
    if (!text) return;
    size_t n = strlen(g_query);
    for (const char* p = text; *p && n < sizeof g_query - 1; p++) {
        unsigned char c = (unsigned char)*p;
        g_query[n++] = (char)tolower(c);
    }
    g_query[n] = '\0';
    g_sel = 0;
}

void gmb_palette_backspace(void)
{
    /* toglie un carattere UTF-8 intero (mai mezzo glifo) */
    size_t n = strlen(g_query);
    while (n > 0 && ((unsigned char)g_query[n - 1] & 0xC0) == 0x80) g_query[--n] = '\0';
    if (n > 0) g_query[--n] = '\0';
    g_sel = 0;
}

int gmb_palette_results(int* out, int cap)
{
    if (!out || cap <= 0) return 0;
    /* ordinamento stabile per punteggio: inserimento su 3 bucket */
    static int idx[GMB_PALETTE_MAX];
    int n_total = 0;
    for (int i = 0; i < g_nitems && n_total < GMB_PALETTE_MAX; i++)
        if (g_items[i].used) idx[n_total++] = i;
    int n = 0;
    int best[3][GMB_PALETTE_MAX];
    int nb[3] = {0, 0, 0};
    for (int k = 0; k < n_total; k++) {
        const pal_item_t* it = &g_items[idx[k]];
        char tn[160], misc[96];
        norm_copy(tn, sizeof tn, it->title);
        char m[96];
        snprintf(m, sizeof m, "%s %s", it->category, it->shortcut);
        norm_copy(misc, sizeof misc, m);
        int sc = pal_score(it, g_query, tn, misc);
        if (sc >= 3) continue;
        /* a pari merito: prima l'ordine di registrazione */
        int pos = nb[sc];
        while (pos > 0 && g_items[best[sc][pos - 1]].order > it->order) {
            best[sc][pos] = best[sc][pos - 1];
            pos--;
        }
        best[sc][pos] = idx[k];
        nb[sc]++;
    }
    for (int b = 0; b < 3 && n < cap; b++)
        for (int k = 0; k < nb[b] && n < cap; k++) out[n++] = best[b][k];
    return n;
}

static int pal_filtered_count(void)
{
    int tmp[GMB_PALETTE_MAX];
    return gmb_palette_results(tmp, GMB_PALETTE_MAX);
}

void gmb_palette_move(int delta)
{
    int n = pal_filtered_count();
    if (n <= 0) { g_sel = 0; return; }
    g_sel = (g_sel + delta) % n;
    if (g_sel < 0) g_sel += n;
}

int  gmb_palette_selected(void) { return g_sel; }

void gmb_palette_set_selected(int idx)
{
    int n = pal_filtered_count();
    if (n <= 0) { g_sel = 0; return; }
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    g_sel = idx;
}

void gmb_palette_set_runner(gmb_palette_runner_t fn, void* ctx) { g_runner = fn; g_runner_ctx = ctx; }

/* Overlay abilitato dal programma (emesso dal driver quando compare `PALETTE …`):
 * da lì il frame live disegna la palette e pompa le azioni. In headless resta
 * disabilitato — il test pilotato lo accende scaricando gli stati a mano. */
static int g_wired = 0;
void gmb_ui_palette_wire(int on) { g_wired = on ? 1 : 0; }
int  gmb_ui_palette_wired(void) { return g_wired; }

int gmb_palette_register3(const char* id, const char* title, const char* shortcut)
{ return gmb_palette_register(id, title, shortcut, ""); }
int gmb_palette_register4(const char* id, const char* title, const char* shortcut, const char* category)
{ return gmb_palette_register(id, title, shortcut, category); }

/* ---- esecuzione: sempre attraverso il GPM ------------------------------------ */

typedef struct { int idx; } pal_job_t;

static void pal_execute(void* arg)
{
    pal_job_t job = *(pal_job_t*)arg;
    free(arg);
    if (job.idx < 0 || job.idx >= g_nitems || !g_items[job.idx].used) return;
    char id[48];
    snprintf(id, sizeof id, "%s", g_items[job.idx].id);
    g_open = 0;
    g_query[0] = '\0';
    if (g_runner) g_runner(id, g_runner_ctx);
}

void gmb_palette_run_selected(void)
{
    int sel[GMB_PALETTE_MAX];
    int n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    if (n <= 0) return;
    if (g_sel >= n) g_sel = n - 1;
    int idx = sel[g_sel];
    if (gmb_c99gpm_active() && gmb_cur() != NULL) {
        /* dentro un task: il comando parte come TASK separato (non inline nel
         * percorso del tasto) e il frame continua. */
        pal_job_t* job = (pal_job_t*)malloc(sizeof *job);
        if (job) { job->idx = idx; gmb_task_spawn(pal_execute, job); g_pending = -1; return; }
    }
    /* Programma lineare (nessuno scheduler, nessun frame che pompi): l'azione si
     * esegue subito. È il programma stesso a eseguirla — nessun thread esterno, e
     * quando c'è un frame resta l'accodamento qui sopra. */
    pal_job_t* job = (pal_job_t*)malloc(sizeof *job);
    if (!job) return;
    job->idx = idx;
    pal_execute(job);
}

void gmb_palette_pump(void)
{
    if (g_pending < 0) return;
    pal_job_t* job = (pal_job_t*)malloc(sizeof *job);
    if (!job) { pal_execute(NULL); return; }
    job->idx = g_pending;
    g_pending = -1;
    pal_execute(job);
}

int gmb_palette_key(int key)
{
    if (!g_open) return 0;
    switch (key) {
        case 27:  gmb_palette_close(); return 1;                    /* ESC */
        case 13: case 10: gmb_palette_run_selected(); return 1;     /* Enter */
        case 8: case 127: gmb_palette_backspace(); return 1;        /* Backspace */
        case 0x1A01: gmb_palette_move(-1); return 1;                /* SU   (codice TUI) */
        case 0x1A02: gmb_palette_move(1);  return 1;                /* GIU  (codice TUI) */
        case 0x1A04: gmb_palette_move(-GMB_PALETTE_MAXROWS); return 1;
        case 0x1A05: gmb_palette_move(GMB_PALETTE_MAXROWS); return 1;
        default: break;
    }
    if (key >= 32 && key < 0x110000) { char b[8]; int n = 0; b[n++] = (char)key; b[n] = '\0'; gmb_palette_type(b); return 1; }
    return 1;   /* qualunque altro tasto: l'overlay lo cattura (modalità) */
}

/* ---- testo e disegno --------------------------------------------------------- */

int gmb_palette_lines(char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int off = 0;
    off += snprintf(out + off, (size_t)(cap - off), "PALETTE> %s\n", g_query);
    if (off >= cap) return cap;
    int sel[GMB_PALETTE_MAX];
    int n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    if (n == 0) {
        off += snprintf(out + off, (size_t)(cap - off), "(nessun comando)\n");
        return off > cap ? cap : off;
    }
    if (g_sel >= n) g_sel = n - 1;
    int rows = n < GMB_PALETTE_MAXROWS ? n : GMB_PALETTE_MAXROWS;
    int first = 0;
    if (g_sel >= rows) first = g_sel - rows + 1;
    for (int i = first; i < first + rows && i < n && off < cap; i++) {
        const pal_item_t* it = &g_items[sel[i]];
        char cat[32];
        snprintf(cat, sizeof cat, "%s", it->category);
        off += snprintf(out + off, (size_t)(cap - off), "%c %-34.34s %-16.16s %s\n",
                        i == g_sel ? '>' : ' ', it->title, cat, it->shortcut);
    }
    if (off > cap) off = cap;
    return off;
}

void gmb_palette_draw_cells(gmb_screen_buffer_t* b)
{
    if (!b || !g_open) return;
    char text[GMB_PALETTE_MAXROWS * GMB_PALETTE_ROW_MAX];
    int n = gmb_palette_lines(text, (int)sizeof text);
    int nl = 0;
    for (int i = 0; i < n; i++) if (text[i] == '\n') nl++;
    if (nl <= 0) return;

    int sw = gmb_screen_width(b), sh = gmb_screen_height(b);
    if (sw < 24 || sh < 8) return;
    int W = 58;
    if (W > sw - 4) W = sw - 4;
    int H = nl + 2;
    if (H > sh - 2) H = sh - 2;
    int X = (sw - W) / 2, Y = sh / 4;
    if (Y < 0) Y = 0;

    gmb_theme_t th = gmb_ui_theme();
    gmb_cell_color_t fg = th.text, bg = th.window_bg;
    gmb_cell_color_t bd = (th.border.r || th.border.g || th.border.b) ? th.border : fg;
    gmb_cell_color_t tf = (th.title_focus.r || th.title_focus.g || th.title_focus.b) ? th.title_focus : fg;
    gmb_cell_color_t sf = (th.active_bg.r || th.active_bg.g || th.active_bg.b) ? th.active_bg : fg;
    gmb_cell_color_t sb = (th.accent.r || th.accent.g || th.accent.b) ? th.accent : bg;

    for (int y = Y + 1; y < Y + H - 1; y++)
        for (int x = X + 1; x < X + W - 1; x++) gmb_screen_setchar(b, x, y, ' ', fg, bg);
    gmb_screen_setchar(b, X, Y, 0x250C, bd, bg);
    gmb_screen_setchar(b, X + W - 1, Y, 0x2510, bd, bg);
    gmb_screen_setchar(b, X, Y + H - 1, 0x2514, bd, bg);
    gmb_screen_setchar(b, X + W - 1, Y + H - 1, 0x2518, bd, bg);
    for (int x = X + 1; x < X + W - 1; x++) {
        gmb_screen_setchar(b, x, Y, 0x2500, bd, bg);
        gmb_screen_setchar(b, x, Y + H - 1, 0x2500, bd, bg);
    }
    for (int y = Y + 1; y < Y + H - 1; y++) {
        gmb_screen_setchar(b, X, y, 0x2502, bd, bg);
        gmb_screen_setchar(b, X + W - 1, y, 0x2502, bd, bg);
    }
    gmb_screen_setstring_n(b, X + 1, Y, " COMANDI  (Ctrl+K) ", W - 2, tf, bg);

    int row = 0;
    for (int i = 0; i < n && Y + 1 + row < Y + H - 1; i++) {
        int len = 0;
        while (i + len < n && text[i + len] != '\n') len++;
        char line[GMB_PALETTE_ROW_MAX];
        int k = len < (int)sizeof line - 1 ? len : (int)sizeof line - 1;
        memcpy(line, text + i, (size_t)k); line[k] = '\0';
        int is_sel = line[0] == '>';
        if (is_sel) for (int x = X + 1; x < X + W - 1; x++) gmb_screen_setchar(b, x, Y + 1 + row, ' ', sf, sb);
        gmb_screen_setstring_n(b, X + 1, Y + 1 + row, line, W - 2, is_sel ? sf : fg, is_sel ? sb : bg);
        row++;
        i += len;
    }
}

void gmb_palette_draw_pixels(void)
{
    if (!g_open || !GMB_GFX_SCREEN_ON()) return;
    char text[GMB_PALETTE_MAXROWS * GMB_PALETTE_ROW_MAX];
    int n = gmb_palette_lines(text, (int)sizeof text);
    int nl = 0;
    for (int i = 0; i < n; i++) if (text[i] == '\n') nl++;
    if (nl <= 0) return;

    gmb_widget_style_t st = gmb_widget_style_current();
    int sw = GMB_GFX_SCREEN_W(), sh = GMB_GFX_SCREEN_H();
    int lh = GMB_GFX_TEXT_H();
    if (lh < 8) lh = 14;
    lh += 2;
    int W = 620;
    if (W > sw - 32) W = sw - 32;
    int H = nl * lh + 12;
    if (H > sh - 32) H = sh - 32;
    if (W < 80 || H < lh * 3) return;
    int X = (sw - W) / 2, Y = sh / 5;

    unsigned int bg = st.body ? st.body : st.fill;
    unsigned int fg = st.text ? st.text : 0xFFFFFF;
    unsigned int bd = st.accent ? st.accent : fg;
    unsigned int sh_c = st.fill_press ? st.fill_press : bg;
    GMB_GFX_FILL_RRECT(X, Y, W, H, 6, bg);
    GMB_GFX_STROKE_RRECT(X, Y, W, H, 6, bd);

    /* riga di ricerca (la prima) enfatizzata, poi l'elenco */
    int row = 0, i = 0;
    while (i < n && row * lh < H - 6) {
        int len = 0;
        while (i + len < n && text[i + len] != '\n') len++;
        char line[GMB_PALETTE_ROW_MAX];
        int k = len < (int)sizeof line - 1 ? len : (int)sizeof line - 1;
        memcpy(line, text + i, (size_t)k); line[k] = '\0';
        if (row == 0) GMB_GFX_FILL_RRECT(X + 4, Y + 4, W - 8, lh, 3, sh_c);
        GMB_GFX_DRAW_TEXT(line, X + 8, Y + 4 + row * lh, 0, fg);
        row++;
        i += len + 1;
    }
}