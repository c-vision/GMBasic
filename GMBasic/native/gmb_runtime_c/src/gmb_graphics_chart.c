/* gmb_graphics_chart.c — ADAPTER GMBasic del motore chart autonomo `gmb_chart`.
 *
 * Questo file NON contiene più il motore: è lo strato che collega il core riusabile
 * `native/gmb_chart/` al grafico di GMBasic. Mantiene l'ABI storica `gmb_gfx_chart_*`
 * (usata dai 4 backend via P/Invoke/emissione), quindi i consumatori non cambiano.
 *
 * Responsabilità dell'adapter:
 *   - registry handle↔gmb_chart_t* (nessun globale nel core);
 *   - renderer vtable che mappa le primitive del core su `gmb_gfx_*` (pixel);
 *   - inoltro dello stato puntatore (input) al core;
 *   - save su screen PNG (specifico GMBasic).
 *
 * Un'app C esterna usa direttamente `gmb_chart.h` con la PROPRIA renderer table. */
#include "gmb_chart.h"
#include "gmb_tui_widgets.h"
#include "gmb_chart_gmb.h"
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"   /* resolve_color: indice palette → RGB */
#include "gmb_gfx_input.h"
#include <stdlib.h>
#include <string.h>

#define GMB_CHART_MAX_HANDLES 32
static gmb_chart_t* g_charts[GMB_CHART_MAX_HANDLES];

static gmb_chart_t* h2c(int handle)
{
    return (handle >= 1 && handle <= GMB_CHART_MAX_HANDLES) ? g_charts[handle - 1] : NULL;
}

static uint32_t chart_px_color(int color)
{
    /* Serie con indice palette 15 (=bianco) sarebbero invisibili su card chiara:
     * la serie usa l ACCENTO del tema (grafici coerenti con la skin). */
    if (color == 15) { gmb_theme_t th = gmb_ui_theme();
        return 0xFF000000u | ((uint32_t)th.accent.r << 16) | ((uint32_t)th.accent.g << 8) | th.accent.b; }
    extern uint32_t resolve_color(int color);
    return 0xFF000000u | (resolve_color(color) & 0xFFFFFFu);
}
static void gfx_line_cb(void* ctx, int x1, int y1, int x2, int y2, int color)
{ (void)ctx; gmb_gfx_line(x1, y1, x2, y2, (int)(0x40000000u | (chart_px_color(color) & 0xFFFFFFu)), NULL); }
static void gfx_rect_cb(void* ctx, int x, int y, int w, int h, int color, int filled)
{ (void)ctx; unsigned rgb = chart_px_color(color);
  if (filled) gmb_gfx_fill_rounded_rect(x, y, w, h, 0, rgb); else gmb_gfx_stroke_rounded_rect(x, y, w, h, 0, rgb); }
static void gfx_circle_cb(void* ctx, int cx, int cy, int r, int color, int filled)
{ (void)ctx; gmb_gfx_circle(cx, cy, r, color, filled, 0.0, 360.0, 1.0); }
static void gfx_text_cb(void* ctx, const char* s, int x, int y, int align, int color)
{ (void)ctx; gmb_gfx_draw_text(s, x, y, align, color); }
/* Metriche LOGICHE: il chart lavora in coordinate logiche (ox/oy/w/h = dimensioni
 * logiche della pagina) e il testo viene scalato da gmb_gfx_draw_text. Esponendo le
 * metriche NATIVE, la legenda/assi su Retina usavano altezze doppie e le voci della
 * legenda finivano fuori posto (bug GraphicsPlotting a scale 2). */
static int  gfx_tw_cb(void* ctx, const char* s) { (void)ctx; return gmb_gfx_text_width_logical(s); }
static int  gfx_th_cb(void* ctx) { (void)ctx; return gmb_gfx_text_height_logical(); }

static const gmb_chart_renderer_t g_gfx_renderer = {
    .ctx = NULL,
    .line = gfx_line_cb, .rect = gfx_rect_cb, .circle = gfx_circle_cb,
    .text = gfx_text_cb, .text_width = gfx_tw_cb, .text_height = gfx_th_cb,
    .set_clip = NULL, .pad = 1, .font_hint = 0
};

/* Rendering PIXEL (adapter gmb_gfx_*): usato dal namespace CHART / path GRAPHICS.
 * w/h <= 0 → usa la config del chart (cw/ch). Inoltra lo stato puntatore al core. */
void gmb_chart_draw_gfx(gmb_chart_t* ch, int ox, int oy, int w, int h)
{
    if (!ch) return;
    int sx, sy;
    gmb_gfx_input_get_mouse(&sx, &sy);
    gmb_chart_set_pointer(ch, sx, sy, gmb_gfx_input_mouse_down());
    gmb_chart_set_wheel(ch, gmb_gfx_input_take_wheel());
    if (w <= 0) w = ch->cw > 0 ? ch->cw : 400;
    if (h <= 0) h = ch->ch > 0 ? ch->ch : 220;
    int pw = gmb_gfx_screen_width(), ph = gmb_gfx_screen_height();
    if (pw > 0 && ox + w > pw) w = pw - ox;
    if (ph > 0 && oy + h > ph) h = ph - oy;
    gmb_chart_draw(ch, &g_gfx_renderer, ox, oy, w, h);
}

int gmb_gfx_chart_create(void)
{
    gmb_gfx_init();
    for (int i = 0; i < GMB_CHART_MAX_HANDLES; i++)
        if (!g_charts[i]) { g_charts[i] = gmb_chart_new(); return g_charts[i] ? i + 1 : -1; }
    return -1;
}

void gmb_gfx_chart_plot(int handle, const double* xs, const double* ys, int count, const char* label)
{ gmb_chart_t* ch = h2c(handle); if (ch) gmb_chart_plot(ch, xs, ys, count, label); }

void gmb_gfx_chart_plot_financial(int handle, const double* ohlc, int count, const char* plot_type, const char* label)
{ gmb_chart_t* ch = h2c(handle); if (ch) gmb_chart_plot_financial(ch, ohlc, count, plot_type, label); }

/* Finestra DEDICATA del grafico (parità C#: CHART SHOW apre una finestra OxyPlot).
 * Con la finestra grafica attiva PRINT va sulla pagina (finestra attiva) e CPRINT
 * sempre su stdout (s_force_console in gmb_console.c). */
/* 1 = la finestra grafica è stata creata da CHART SHOW (non da un SCREEN del
 * programma): i redraw successivi possono ripulire la pagina senza cancellare
 * grafica utente. */
static int g_chart_window = 0;
/* Chiamata da gmb_gfx_screen: un nuovo SCREEN del programma riprende la finestra. */
void gmb_chart_screen_reset(void) { g_chart_window = 0; }

void gmb_gfx_chart_show(int handle, int x, int y)
{
    gmb_chart_t* ch = h2c(handle);
    if (!ch) return;
    if (!gmb_gfx_screen_on())
    {
        /* Finestra propria del grafico: 800x600 (dimensione tipica del plot OxyPlot). */
        gmb_gfx_screen(16, 800, 600, 0);
        g_chart_window = 1;
    }
    if (g_chart_window)
    {
        /* Redraw su finestra del chart: pulisce (altrimenti due CHART SHOW con viste
         * diverse si sovrappongono — bug ChartConfigDemo). */
        gmb_gfx_cls(-1);
        gmb_chart_draw_gfx(ch, 0, 0, gmb_gfx_screen_width(), gmb_gfx_screen_height());
        return;
    }
    /* Overlay su uno SCREEN già attivo: x,y = posizione del pannello nella pagina. */
    gmb_chart_draw_gfx(ch, x, y, 0, 0);
}

void gmb_gfx_chart_plot_financial_draw(int handle, int ox, int oy)
{ gmb_chart_t* ch = h2c(handle); if (ch) gmb_chart_draw_gfx(ch, ox, oy, 0, 0); }

void gmb_gfx_chart_clear(int handle) { gmb_chart_t* ch = h2c(handle); if (ch) gmb_chart_clear(ch); }

void gmb_gfx_chart_close(int handle)
{
    gmb_chart_t* ch = h2c(handle);
    if (!ch) return;
    gmb_chart_free(ch);
    g_charts[handle - 1] = NULL;
}

int gmb_gfx_chart_save(int handle, const char* path)
{
    gmb_chart_t* ch = h2c(handle);
    if (!ch) return -1;
    if (!gmb_gfx_screen_on())
    {
        gmb_gfx_screen(16, 800, 600, 0);
    }
    int w = gmb_gfx_screen_width(), h = gmb_gfx_screen_height();
    /* Il PNG contiene il solo grafico a piena pagina (parità C# CHART SAVE). */
    gmb_gfx_cls(0);
    gmb_chart_draw_gfx(ch, 0, 0, w, h);
    return gmb_gfx_save_screen_png(path);
}

void gmb_gfx_chart_set(int handle, const char* prop, double num, const char* str)
{ gmb_chart_t* ch = h2c(handle); if (ch) gmb_chart_set(ch, prop, num, str); }

double gmb_gfx_chart_get(int handle, const char* prop)
{ gmb_chart_t* ch = h2c(handle); return ch ? gmb_chart_get(ch, prop) : 0; }

const char* gmb_gfx_chart_get_str(int handle, const char* prop)
{ gmb_chart_t* ch = h2c(handle); return ch ? gmb_chart_get_str(ch, prop) : ""; }

double gmb_gfx_chart_draw(int handle, const char* op, const double* args, int nargs)
{ gmb_chart_t* ch = h2c(handle); return ch ? gmb_chart_draw_cmd(ch, op, args, nargs) : -1; }

double gmb_gfx_chart_objcount(int handle)
{ gmb_chart_t* ch = h2c(handle); return ch ? gmb_chart_objcount(ch) : 0; }

double gmb_gfx_chart_objhit(int handle, double x, double y)
{ gmb_chart_t* ch = h2c(handle); return ch ? gmb_chart_objhit_data(ch, x, y) : -1; }
