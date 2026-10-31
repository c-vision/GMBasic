/* ============================================================================
 * gmb_chart.h — Motore chart AUTONOMO e riusabile (C puro, zero dipendenze).
 *
 * Progettato per essere linkato da host diversi:
 *   - GMBasic (runtime C# / C / VM/C / C99): via adapter che mappa le primitive su
 *     `gmb_gfx_*` (pixel) o su un buffer a celle (TEXT);
 *   - una applicazione C di trading standalone: fornisce la PROPRIA
 *     `gmb_chart_renderer_t` (SDL/Metal/ImGui/…) e linka questa libreria.
 *
 * Il core NON conosce GMBasic, SDL, `g_gfx`, gli handle o l'input di GMBasic:
 *   - lo stato è un'istanza `gmb_chart_t` (nessun globale);
 *   - il disegno passa SOLO per la function table `gmb_chart_renderer_t` fornita dall'host;
 *   - l'input (puntatore) è inoltrato dall'host con `gmb_chart_set_pointer`.
 *
 * Solo dipendenze: libc (stdlib/string/stdio/strings/math).
 * ============================================================================ */
#ifndef GMB_CHART_H
#define GMB_CHART_H

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_CHART_MAX_OBJ     64
#define GMB_CHART_MAX_SERIES  8
#define GMB_CHART_MAX_LABEL   64

/* ---- Interfaccia di disegno (function table) fornita dall'HOST -----------------
 * Tutte le coordinate sono PIXEL nella superficie di destinazione; i colori sono
 * interpretati dall'host (indice palette, ARGB, …). Il core NON conosce il backend.
 * Solo `line` è obbligatoria; le altre, se NULL, vengono degradate dal core. */
typedef struct gmb_chart_renderer {
    void* ctx;   /* stato dell'host (surface/buffer/…) passato com'è alle funzioni */

    void (*line)(void* ctx, int x1, int y1, int x2, int y2, int color);
    void (*rect)(void* ctx, int x, int y, int w, int h, int color, int filled);
    void (*circle)(void* ctx, int cx, int cy, int r, int color, int filled);
    void (*text)(void* ctx, const char* s, int x, int y, int align, int color);
    int  (*text_width)(void* ctx, const char* s);   /* larghezza in pixel; 0 se ignoto */
    int  (*text_height)(void* ctx);                 /* altezza riga in pixel; 0 se ignoto */
    void (*set_clip)(void* ctx, int x, int y, int w, int h);

    /* Coordinate di tuning (default ragionevoli se 0/NULL). */
    int     pad;          /* margine interna pannello (px), default 1 */
    int     font_hint;    /* altezza carattere stimata se text_height ignoto, default 12 */
} gmb_chart_renderer_t;

/* ---- Oggetto di disegno (CHART DRAW) ---- */
typedef struct gmb_chart_obj {
    int kind;               /* 1=line 2=hline 3=vline 4=rect 5=fib */
    double a, b, c, d;      /* geometria in coordinate DATI */
    int color;
    int active;
} gmb_chart_obj_t;

/* ---- Istanza chart (stato proprio, nessun globale) ---- */
typedef struct gmb_chart {
    int in_use;
    /* serie line */
    int n_series;
    double* xs;                          /* asse x condiviso (prima serie) */
    int     lens[GMB_CHART_MAX_SERIES];
    double* ys[GMB_CHART_MAX_SERIES];
    char    labels[GMB_CHART_MAX_SERIES][GMB_CHART_MAX_LABEL];
    /* serie OHLC */
    double* ohlc[GMB_CHART_MAX_SERIES];  /* flat [t,o,h,l,c] * ohlc_n */
    int     ohlc_n[GMB_CHART_MAX_SERIES];
    int     plot_type[GMB_CHART_MAX_SERIES]; /* 0=line 1=candlestick 2=ohlc */
    int     series_col[GMB_CHART_MAX_SERIES]; /* colore per-serie (0 = default palette) */
    /* configurazione (CHART SET/GET) */
    int show_grid, show_legend, show_crosshair, autoscale;
    int px, py, cw, ch;                  /* pannello (default 0,0,400,220) */
    double vx0, vx1, vy0, vy1;           /* view window se autoscale=0 */
    int grid_col, up_col, down_col, axis_col;
    double candle_width;                 /* corpo candela: frazione dello slot 0..1 (default 0.64) */
    char title[GMB_CHART_MAX_LABEL], xlabel[32], ylabel[32];
    /* oggetti di disegno */
    gmb_chart_obj_t objs[GMB_CHART_MAX_OBJ];
    int nobj;
    int sel_obj;                         /* oggetto selezionato (evidenziato) */
    int drag_id;                         /* oggetto in trascinamento (auto-drag); -1 = pan */
    double drag_ax, drag_ay;             /* ultimo punto mouse del drag (pan) */
    /* ultima vista disegnata (per pick/drag in pixel) */
    int last_ox, last_oy, last_w, last_h;
    double last_x0, last_x1, last_y0, last_y1; int last_valid;
    /* stato puntatore inoltrato dall'host */
    int ptr_x, ptr_y, ptr_down, ptr_valid;
    int wheel;                           /* delta rotella accumulato (consumato dal draw) */
} gmb_chart_t;

/* ---- Ciclo di vita ---- */
gmb_chart_t* gmb_chart_new(void);
void         gmb_chart_free(gmb_chart_t* ch);
void         gmb_chart_clear(gmb_chart_t* ch);

/* ---- Dati ---- */
void gmb_chart_plot(gmb_chart_t* ch, const double* xs, const double* ys, int n, const char* label);
void gmb_chart_plot_financial(gmb_chart_t* ch, const double* ohlc, int n, const char* plot_type, const char* label);
/* Overlay indicatore calcolato sui dati base (close della serie 0, o ys[0]):
 * kind = "sma" | "ema"; period; color (0 = palette). Ritorna l'indice serie (>=1) o -1. */
int  gmb_chart_indicator(gmb_chart_t* ch, const char* kind, int period, int color);

/* ---- Configurazione (CHART SET/GET/GETS) ---- */
void        gmb_chart_set(gmb_chart_t* ch, const char* prop, double num, const char* str);
double      gmb_chart_get(const gmb_chart_t* ch, const char* prop);
const char* gmb_chart_get_str(const gmb_chart_t* ch, const char* prop);

/* ---- Oggetti di disegno + query (CHART DRAW) ---- */
double gmb_chart_draw_cmd(gmb_chart_t* ch, const char* op, const double* args, int nargs);
int    gmb_chart_objcount(const gmb_chart_t* ch);
int    gmb_chart_objhit_data(const gmb_chart_t* ch, double x, double y);   /* coord. DATI */
int    gmb_chart_pick_px(const gmb_chart_t* ch, int px, int py);           /* coord. PIXEL */

/* ---- Vista effettiva (pannello + scala) — utile anche all'host ---- */
void gmb_chart_scale(const gmb_chart_t* ch, int* ox, int* oy, int* w, int* h,
                     double* x0, double* x1, double* y0, double* y1);

/* ---- Input (l'host inoltra lo stato del puntatore prima del draw) ---- */
void gmb_chart_set_pointer(gmb_chart_t* ch, int px, int py, int down);
/* Delta rotella accumulato (positivo = zoom in); consumato al draw successivo. */
void gmb_chart_set_wheel(gmb_chart_t* ch, int delta);

/* ---- View window: zoom/pan (pilotabili da BASIC o dall'host) ----
 * `zoom`: moltiplica l'ampiezza della view per `factor` (<1 = zoom in) attorno al
 *   punto dati (cx,cy); se cx/cy non finiti, usa il centro della view.
 * `pan`:  trasla la view di (dx,dy) in unità DATI. Entrambi spengono autoscale. */
void gmb_chart_zoom(gmb_chart_t* ch, double factor, double cx, double cy);
void gmb_chart_pan(gmb_chart_t* ch, double dx, double dy);

/* ---- Rendering: il core disegna SOLO attraverso il renderer dell'host ----
 * ox,oy,w,h = rettangolo bersaglio (pixel). Se w<=0 o h<=0 usa la config (cw/ch). */
void gmb_chart_draw(gmb_chart_t* ch, const gmb_chart_renderer_t* r, int ox, int oy, int w, int h);

#ifdef __cplusplus
}
#endif
#endif /* GMB_CHART_H */
