/* gmb_tui_chart.c — Adapter a CELLE del motore chart autonomo `gmb_chart`.
 *
 * Implementa `gmb_chart_renderer_t` mappando le primitive su `gmb_screen_buffer_t`
 * (gmb_screen_setchar). Così il chart (un solo motore) rende anche in TEXT/console.
 * Auto-incluso in GMB_TUI_MODULES (file `gmb_tui_*.c`). */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_chart.h"
#include "gmb_chart_gmb.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* EGA index (0-15) → colore cella. */
static gmb_cell_color_t ega_color(int idx)
{
    static const uint8_t p[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
        {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
    };
    int i = idx & 15;
    gmb_cell_color_t c = { p[i][0], p[i][1], p[i][2], 0 };
    return c;
}

static void cell_put(void* ctx, int x, int y, uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    gmb_screen_buffer_t* b = (gmb_screen_buffer_t*)ctx;
    if (!b || x < 0 || y < 0 || x >= gmb_screen_width(b)) return;
    gmb_screen_setchar(b, x, y, ch, fg, bg);
}

static void cell_line(void* ctx, int x1, int y1, int x2, int y2, int color)
{
    gmb_cell_color_t c = ega_color(color);
    int dx = x2 - x1 < 0 ? x1 - x2 : x2 - x1;
    int dy = y2 - y1 < 0 ? y1 - y2 : y2 - y1;
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    for (;;)
    {
        cell_put(ctx, x1, y1, 0x2588, c, c);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static void cell_rect(void* ctx, int x, int y, int w, int h, int color, int filled)
{
    gmb_cell_color_t c = ega_color(color);
    if (filled)
    {
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) cell_put(ctx, x + i, y + j, 0x2588, c, c);
        return;
    }
    for (int i = 0; i < w; i++) { cell_put(ctx, x + i, y, 0x2588, c, c); cell_put(ctx, x + i, y + h - 1, 0x2588, c, c); }
    for (int j = 0; j < h; j++) { cell_put(ctx, x, y + j, 0x2588, c, c); cell_put(ctx, x + w - 1, y + j, 0x2588, c, c); }
}

static void cell_circle(void* ctx, int cx, int cy, int r, int color, int filled)
{
    (void)filled;
    gmb_cell_color_t c = ega_color(color);
    int x = r, y = 0, err = 0;
    while (x >= y)
    {
        cell_put(ctx, cx + x, cy + y, 0x2588, c, c); cell_put(ctx, cx + y, cy + x, 0x2588, c, c);
        cell_put(ctx, cx - y, cy + x, 0x2588, c, c); cell_put(ctx, cx - x, cy + y, 0x2588, c, c);
        cell_put(ctx, cx - x, cy - y, 0x2588, c, c); cell_put(ctx, cx - y, cy - x, 0x2588, c, c);
        cell_put(ctx, cx + y, cy - x, 0x2588, c, c); cell_put(ctx, cx + x, cy - y, 0x2588, c, c);
        if (err <= 0) { y++; err += 2 * y + 1; }
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

static void cell_text(void* ctx, const char* s, int x, int y, int align, int color)
{
    gmb_cell_color_t c = ega_color(color), bg = ega_color(0);
    int n = (int)strlen(s);
    if (align == 1) x -= n / 2; else if (align == 2) x -= n;
    for (int i = 0; i < n; i++) { unsigned ch = (unsigned char)s[i]; if (ch >= 32) cell_put(ctx, x + i, y, (uint32_t)ch, c, bg); }
}

static int cell_tw(void* ctx, const char* s) { (void)ctx; return (int)strlen(s); }
static int cell_th(void* ctx) { (void)ctx; return 1; }

static const gmb_chart_renderer_t g_cell_renderer = {
    NULL, cell_line, cell_rect, cell_circle, cell_text, cell_tw, cell_th, NULL, 1, 1
};

void gmb_chart_draw_cells(gmb_chart_t* ch, gmb_screen_buffer_t* buf, int ox, int oy, int w, int h)
{
    if (!ch || !buf) return;
    gmb_chart_renderer_t r = g_cell_renderer;
    r.ctx = buf;
    gmb_chart_draw(ch, &r, ox, oy, w, h);
}

/* ===================== Controllo CHART (verbo CONTROLS) =====================
 * `CONTROLS CHART x,y,w,h[,"v1;v2;..."]`: controllo a celle che ospita il motore
 * autonomo `gmb_chart` (una serie di linee). Reso a celle in TEXT e a pixel in
 * GRAPHICS dallo stesso core (ridimensiona col controllo). csv = "v" (x=indice)
 * oppure "x,y"; csv vuoto → sinusoide demo. */
typedef struct { gmb_control_t ctrl; gmb_chart_t* chart; } gmb_chart_ctrl_t;

static void cchart_cell_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_chart_ctrl_t* g = (gmb_chart_ctrl_t*)u;
    gmb_chart_draw_cells(g->chart, buf, c->x, c->y, c->width, c->height);
}
static void cchart_px_draw(void* u, gmb_control_t* c)
{
    gmb_chart_ctrl_t* g = (gmb_chart_ctrl_t*)u;
    /* margine interno: le etichette degli assi restano DENTRO la card */
    int mx = 36, my = 10;
    int cw = c->width - mx - 8; if (cw < 40) cw = c->width;
    int chh = c->height - my - 8; if (chh < 40) chh = c->height;
    gmb_chart_draw_gfx(g->chart, c->x + mx, c->y + my, cw, chh);
}

gmb_control_t* gmb_tui_add_chart(int x, int y, int w, int h, const char* csv)
{
    gmb_chart_ctrl_t* g = (gmb_chart_ctrl_t*)calloc(1, sizeof(*g));
    if (!g) return NULL;
    gmb_chart_t* ch = gmb_chart_new();
    if (!ch) { free(g); return NULL; }
    gmb_chart_set(ch, "width", w, "");
    gmb_chart_set(ch, "height", h, "");
    gmb_chart_set(ch, "legend", 0, "");   /* legenda off: spazio prezioso in un controllo */

    double xs[1024], ys[1024]; int n = 0;
    if (csv && *csv)
    {
        char buf[4096]; snprintf(buf, sizeof(buf), "%s", csv);
        char* s1 = NULL;
        for (char* grp = strtok_r(buf, ";", &s1); grp && n < 1024; grp = strtok_r(NULL, ";", &s1))
        {
            double a = 0, b = 0; int k = 0; char* s2 = NULL;
            for (char* t = strtok_r(grp, ",", &s2); t && k < 2; t = strtok_r(NULL, ",", &s2)) { if (k == 0) a = atof(t); else b = atof(t); k++; }
            if (k >= 2) { xs[n] = a; ys[n] = b; }
            else { xs[n] = n + 1; ys[n] = a; }
            n++;
        }
    }
    if (n == 0)
    {
        for (int i = 0; i < 20; i++) { xs[i] = i + 1; ys[i] = 50 + 30 * sin(i * 0.5) + i; }
        n = 20;
    }
    gmb_chart_plot(ch, xs, ys, n, "");

    gmb_control_init(&g->ctrl, "", x, y, w, h, cchart_cell_draw, NULL);
    g->ctrl.draw_px_fn = cchart_px_draw;
    g->ctrl.ctrl_kind = GMB_CK_CHART;
    g->ctrl.user = g;
    g->chart = ch;
    gmb_tui_control_add(&g->ctrl);
    return &g->ctrl;
}
