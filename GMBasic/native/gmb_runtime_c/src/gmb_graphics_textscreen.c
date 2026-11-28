/* gmb_graphics_textscreen.c — P7 blocco 4: schermo di TESTO 80×25 a celle 8×12.
 *
 * Riferimento (Engine1999, docs pubblici): il text screen è una griglia di 80×25 celle da
 * 8×12 px (640×300), ogni cella con il proprio carattere e i propri colori — «penna» (testo)
 * e «carta» (sfondo) — più un cursore che va a capo e scorre quando arriva in fondo.
 *
 * Perché un modulo e non le primitive esistenti: `gmb_gfx_render_cells*`/`gmb_gfx_draw_cell`
 * (gmb_gfx_cellfont.c) DISEGNANO una griglia che il chiamante possiede; qui manca il MODELLO
 * — nessuna griglia persistente, nessun cursore, nessuna scrittura con a capo e scroll. Il
 * blocco 4 consegna quello: le primitive restano dove sono e il TUI continua a usarle.
 *
 * Scelte dichiarate (differenze dal riferimento, tutte volute):
 *  - i glifi vengono dal bitmap ASCII 8×16 già nel runtime (`gmb_gfx_cellfont_bitmap.h`,
 *    Terminus / OFL), campionato a 12 righe (riga destinazione y → sorgente y*16/12): niente
 *    dipendenza da un font TTF, quindi resa IDENTICA in headless e su tutte le gambe C;
 *  - il modulo possiede SOLO la griglia: il disegno in pagina è esplicito
 *    (`gmb_gfx_textscreen_render`), così chi compone sceglie l'ordine con sprite e layer
 *    (la superficie BASIC lo chiamerà a fine frame);
 *  - attivando la modalità, se c'è uno schermo la pagina diventa esattamente 80*8 × 25*12
 *    pixel NATIVI (stessa regola di `render_cells`: si divide per il render-scale, così il
 *    target resta 640×300 anche su un display Retina);
 *  - penna/carta predefinite: bianco su nero; si cambiano con `gmb_gfx_textscreen_pen`;
 *  - TAB = colonna successiva multipla di 8, riempita di spazi con penna/carta correnti;
 *  - `\r` ignorato; ogni altro byte è memorizzato come codepoint (i codepoint fuori da
 *    32..126 non hanno glifo nel bitmap → cella con la sola carta, nessun carattere finto).
 */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include "gmb_gfx_cellfont_bitmap.h"

#include <string.h>

#define TS_COLS 80
#define TS_ROWS 25
#define TS_CW   8
#define TS_CH   12

typedef struct {
    int cp;
    uint32_t fg;   /* penna: 0x00RRGGBB */
    uint32_t bg;   /* carta: 0x00RRGGBB */
} ts_cell_t;

typedef struct {
    int init, on;
    int cx, cy;
    uint32_t pen, paper;
    ts_cell_t cells[TS_ROWS][TS_COLS];
} ts_screen_t;

static ts_screen_t g_ts;

static void ts_fill_cells(int cp, uint32_t fg, uint32_t bg)
{
    for (int y = 0; y < TS_ROWS; y++)
        for (int x = 0; x < TS_COLS; x++)
        {
            g_ts.cells[y][x].cp = cp;
            g_ts.cells[y][x].fg = fg;
            g_ts.cells[y][x].bg = bg;
        }
}

static void ts_init(void)
{
    if (g_ts.init) return;
    g_ts.init = 1;
    g_ts.pen = 0xFFFFFFu;      /* bianco su nero, il default del testo del riferimento */
    g_ts.paper = 0x000000u;
    g_ts.cx = 0;
    g_ts.cy = 0;
    ts_fill_cells(' ', g_ts.pen, g_ts.paper);
}

/* Scorre di `lines` righe verso l'alto; le righe liberate in fondo prendono carta e penna
 * correnti (è quello che il riferimento fa con lo scroll del text screen). */
static void ts_scroll(int lines)
{
    if (lines <= 0) return;
    if (lines > TS_ROWS) lines = TS_ROWS;
    if (lines < TS_ROWS)
        memmove(&g_ts.cells[0][0], &g_ts.cells[lines][0],
                (size_t)(TS_ROWS - lines) * (size_t)TS_COLS * sizeof(ts_cell_t));
    for (int y = TS_ROWS - lines; y < TS_ROWS; y++)
        for (int x = 0; x < TS_COLS; x++)
        {
            g_ts.cells[y][x].cp = ' ';
            g_ts.cells[y][x].fg = g_ts.pen;
            g_ts.cells[y][x].bg = g_ts.paper;
        }
}

static void ts_newline(void)
{
    if (++g_ts.cy >= TS_ROWS) { ts_scroll(1); g_ts.cy = TS_ROWS - 1; }
}

/* Scrive un carattere alla posizione del cursore e avanza (a capo automatico a fine riga). */
static void ts_put(int cp)
{
    g_ts.cells[g_ts.cy][g_ts.cx].cp = cp;
    g_ts.cells[g_ts.cy][g_ts.cx].fg = g_ts.pen;
    g_ts.cells[g_ts.cy][g_ts.cx].bg = g_ts.paper;
    if (++g_ts.cx >= TS_COLS) { g_ts.cx = 0; ts_newline(); }
}

int gmb_gfx_textscreen_set(int on)
{
    ts_init();
    g_ts.on = on ? 1 : 0;
    if (!on) return 0;   /* la griglia resta: si può riaccendere senza perdere il contenuto */
    ts_fill_cells(' ', g_ts.pen, g_ts.paper);
    g_ts.cx = 0;
    g_ts.cy = 0;
    if (gmb_gfx_screen_on())
    {
        float sc = gmb_gfx_render_scale();
        int w, h;
        if (sc < 1.0f) sc = 1.0f;
        w = (int)((TS_COLS * TS_CW) / sc + 0.5f);
        h = (int)((TS_ROWS * TS_CH) / sc + 0.5f);
        if (w > 0 && h > 0 && (gmb_gfx_width() != w || gmb_gfx_height() != h))
            gmb_gfx_resize_screen(w, h);
    }
    return 1;
}

int gmb_gfx_textscreen_on(void) { ts_init(); return g_ts.on; }
int gmb_gfx_textscreen_cols(void) { return TS_COLS; }
int gmb_gfx_textscreen_rows(void) { return TS_ROWS; }
int gmb_gfx_textscreen_cell_w(void) { return TS_CW; }
int gmb_gfx_textscreen_cell_h(void) { return TS_CH; }

void gmb_gfx_textscreen_pen(int fg, int bg)
{
    ts_init();
    if (fg >= 0) g_ts.pen = resolve_color(fg) & 0xFFFFFFu;
    if (bg >= 0) g_ts.paper = resolve_color(bg) & 0xFFFFFFu;
}

void gmb_gfx_textscreen_locate(int col, int row)
{
    ts_init();
    if (col < 0) col = 0;
    if (col >= TS_COLS) col = TS_COLS - 1;
    if (row < 0) row = 0;
    if (row >= TS_ROWS) row = TS_ROWS - 1;
    g_ts.cx = col;
    g_ts.cy = row;
}

int gmb_gfx_textscreen_cursor_x(void) { ts_init(); return g_ts.cx; }
int gmb_gfx_textscreen_cursor_y(void) { ts_init(); return g_ts.cy; }

/* Ritorna il numero di caratteri scritti (controlli esclusi); 0 a modo spento. */
int gmb_gfx_textscreen_write(const char* text)
{
    const unsigned char* s;
    int n = 0;
    ts_init();
    if (!g_ts.on || !text) return 0;
    for (s = (const unsigned char*)text; *s; s++)
    {
        if (*s == '\r') continue;
        if (*s == '\n') { g_ts.cx = 0; ts_newline(); continue; }
        if (*s == '\t')
        {
            do { ts_put(' '); n++; } while ((g_ts.cx % 8) != 0);
            continue;
        }
        ts_put((int)*s);
        n++;
    }
    return n;
}

/* Righe scorse; 0 a modo spento o con `lines <= 0`. */
int gmb_gfx_textscreen_scroll(int lines)
{
    ts_init();
    if (!g_ts.on || lines <= 0) return 0;
    if (lines > TS_ROWS) lines = TS_ROWS;
    ts_scroll(lines);
    return lines;
}

int gmb_gfx_textscreen_clear(void)
{
    ts_init();
    if (!g_ts.on) return 0;
    ts_fill_cells(' ', g_ts.pen, g_ts.paper);
    g_ts.cx = 0;
    g_ts.cy = 0;
    return 1;
}

/* Lettura di una cella: 1 = scritta, 0 = coordinate fuori dalla griglia. Gli out opzionali
 * (NULL) si possono omettere; i colori sono 0x00RRGGBB. */
int gmb_gfx_textscreen_cell(int col, int row, int* ch, unsigned int* fg, unsigned int* bg)
{
    ts_init();
    if (col < 0 || col >= TS_COLS || row < 0 || row >= TS_ROWS) return 0;
    if (ch) *ch = g_ts.cells[row][col].cp;
    if (fg) *fg = g_ts.cells[row][col].fg;
    if (bg) *bg = g_ts.cells[row][col].bg;
    return 1;
}

static void ts_pixel(gmb_gfx_page_t* p, int x, int y, uint32_t rgb)
{
    if (x < 0 || y < 0 || x >= p->w || y >= p->h) return;
    p->pixels[(size_t)y * (size_t)p->w + x] = 0xFF000000u | (rgb & 0xFFFFFFu);
}

static void ts_rect(gmb_gfx_page_t* p, int x, int y, int w, int h, uint32_t rgb)
{
    int x0 = x, y0 = y, x1 = x + w - 1, y1 = y + h - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= p->w) x1 = p->w - 1;
    if (y1 >= p->h) y1 = p->h - 1;
    for (int yy = y0; yy <= y1; yy++)
        for (int xx = x0; xx <= x1; xx++)
            p->pixels[(size_t)yy * (size_t)p->w + xx] = 0xFF000000u | (rgb & 0xFFFFFFu);
}

/* Disegna la griglia sulla pagina ATTIVA (canvas se c'è, altrimenti pagina dello schermo)
 * dall'angolo in alto a sinistra, una cella = 8×12 pixel nativi. Ritorna le celle disegnate,
 * 0 a modo spento o senza pagina. */
int gmb_gfx_textscreen_render(void)
{
    gmb_gfx_page_t* p;
    ts_init();
    if (!g_ts.on) return 0;
    p = active_page();
    if (!p || !p->pixels) return 0;
    for (int y = 0; y < TS_ROWS; y++)
        for (int x = 0; x < TS_COLS; x++)
        {
            ts_cell_t* c = &g_ts.cells[y][x];
            int px = x * TS_CW, py = y * TS_CH;
            ts_rect(p, px, py, TS_CW, TS_CH, c->bg);
            if (c->cp < 32 || c->cp > 126) continue;
            {
                const unsigned char* rows = gmb_cellfont_ascii[c->cp - 32];
                for (int gy = 0; gy < TS_CH; gy++)
                {
                    /* 16 righe di font → 12 righe di cella (nearest): niente font TTF */
                    unsigned char bits = rows[gy * 16 / TS_CH];
                    if (!bits) continue;
                    for (int gx = 0; gx < TS_CW; gx++)
                        if (bits & (0x80u >> gx)) ts_pixel(p, px + gx, py + gy, c->fg);
                }
            }
        }
    return TS_ROWS * TS_COLS;
}
