/* ============================================================================
 * gmb_tui_candlestick.c — Candlestick (widget NUOVO): candele OHLC.
 * Control: Draw a celle (TUI) + chrome nativo (surface gfx); navigazione cursore.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_graphics.h"
#include "gmb_chart.h"
#include "gmb_chart_gmb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void candlestick_bounds(const gmb_candlestick_t* k, double* mn, double* mx)
{
    double lo = 1e300, hi = -1e300;
    for (int i = 0; i < k->count; i++)
    {
        if (k->candles[i].l < lo) lo = k->candles[i].l;
        if (k->candles[i].h > hi) hi = k->candles[i].h;
    }
    if (lo > hi) { lo = 0; hi = 1; }
    *mn = lo; *mx = hi;
}

static void candlestick_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_candlestick_t* k = (gmb_candlestick_t*)u;
    int W = c->width, H = c->height;
    if (W <= 0 || H <= 0 || k->count <= 0) return;
    double mn, mx;
    candlestick_bounds(k, &mn, &mx);
    double range = mx - mn;
    if (range <= 0) range = 1;

    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    (void)fg;
    const gmb_cell_color_t up = GMB_CELLCOLOR_GREEN;
    const gmb_cell_color_t down = GMB_CELLCOLOR_RED;

    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, W, H, 4, fg, bg, 1);
        double px_per_unit = (double)(H * sk->cell_h) / range;
        int ox = sk->origin_x, oy = sk->origin_y;
        for (int x = 0; x < W; x++)
        {
            int bi = k->count <= W ? x : (int)((long)x * k->count / W);
            if (bi >= k->count) bi = k->count - 1;
            gmb_candle_t cd = k->candles[bi];
            int cxpix = ox + (c->x + x) * sk->cell_w + sk->cell_w / 2;
            int yH = oy + c->y * sk->cell_h + (int)((mx - cd.h) * px_per_unit);
            int yL = oy + c->y * sk->cell_h + (int)((mx - cd.l) * px_per_unit);
            int yO = oy + c->y * sk->cell_h + (int)((mx - cd.o) * px_per_unit);
            int yC = oy + c->y * sk->cell_h + (int)((mx - cd.c) * px_per_unit);
            unsigned int rgb = (cd.c >= cd.o) ? 0x00A000u : 0xD00000u;
            gmb_gfx_line(cxpix, yH, cxpix, yL, (int)(0x40000000u | rgb), 0);
            int bt = yO < yC ? yO : yC;
            int bb = yO < yC ? yC : yO;
            if (bb <= bt) bb = bt + 1;
            int bw = sk->cell_w > 2 ? sk->cell_w - 2 : 1;
            gmb_gfx_fill_rounded_rect(cxpix - bw / 2, bt, bw, bb - bt, 0,
                                      (cd.c >= cd.o) ? 0xFF00A000u : 0xFFD00000u);
        }
        return;
    }

    for (int x = 0; x < W; x++)
    {
        int bi = k->count <= W ? x : (int)((long)x * k->count / W);
        if (bi >= k->count) bi = k->count - 1;
        gmb_candle_t cd = k->candles[bi];
        int rH = (int)((mx - cd.h) / range * H);
        int rL = (int)((mx - cd.l) / range * H);
        int rO = (int)((mx - cd.o) / range * H);
        int rC = (int)((mx - cd.c) / range * H);
        int bt = rO < rC ? rO : rC;
        int bb = rO < rC ? rC : rO;
        gmb_cell_color_t col = cd.c >= cd.o ? up : down;
        for (int r = 0; r < H; r++)
        {
            if (r < rH || r > rL) continue;
            if (r >= bt && r <= bb) gmb_screen_setchar(buf, c->x + x, c->y + r, 0x2588, col, col);
            else gmb_screen_setchar(buf, c->x + x, c->y + r, 0x2502, col, bg);
        }
    }
}

static bool candlestick_key(void* u, gmb_control_t* c, int key)
{
    gmb_candlestick_t* k = (gmb_candlestick_t*)u;
    if (!c->enabled || k->count <= 0) return false;
    switch (key)
    {
        case GMB_K_LEFT: if (k->cursor > 0) k->cursor--; return true;
        case GMB_K_RIGHT: if (k->cursor < k->count - 1) k->cursor++; return true;
        case GMB_K_HOME: k->cursor = 0; return true;
        case GMB_K_END: k->cursor = k->count - 1; return true;
    }
    return false;
}

void gmb_candlestick_init(gmb_candlestick_t* k, int x, int y, int w, int h)
{
    memset(k, 0, sizeof(*k));
    gmb_control_init(&k->ctrl, "", x, y, w, h, candlestick_draw, candlestick_key);
    k->ctrl.user = k;
}

void gmb_candlestick_set_candles(gmb_candlestick_t* k, const gmb_candle_t* candles, int n)
{
    if (n < 0) n = 0;
    if (n > GMB_CANDLE_MAX) n = GMB_CANDLE_MAX;
    k->count = n;
    if (candles) memcpy(k->candles, candles, (size_t)n * sizeof(gmb_candle_t));
    if (k->cursor >= k->count) k->cursor = k->count - 1;
    if (k->cursor < 0) k->cursor = 0;
}

int gmb_candlestick_cursor(const gmb_candlestick_t* k) { return k ? k->cursor : -1; }

/* ===================== Control factory — usa il MOTORE AUTONOMO gmb_chart =====================
 * Verbo CONTROLS `CANDLESTICK x,y,w,h[,"o,h,l,c;..."]`: crea un `gmb_chart_t` (un solo motore),
 * lo registra nel registry TUI; reso a CELLE in TEXT e a PIXEL in GRAPHICS tramite gli adapter
 * (`gmb_chart_draw_cells` / `gmb_chart_draw_gfx`). CSV vuoto → dataset demo. */
typedef struct { gmb_control_t ctrl; gmb_chart_t* chart; } gmb_candlestick_ctrl_t;

static void cctrl_cell_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_candlestick_ctrl_t* g = (gmb_candlestick_ctrl_t*)u;
    gmb_chart_draw_cells(g->chart, buf, c->x, c->y, c->width, c->height);
}
static void cctrl_px_draw(void* u, gmb_control_t* c)
{
    gmb_candlestick_ctrl_t* g = (gmb_candlestick_ctrl_t*)u;
    gmb_chart_draw_gfx(g->chart, c->x, c->y, c->width, c->height);
}

gmb_control_t* gmb_tui_add_candlestick(int x, int y, int w, int h, const char* csv)
{
    gmb_candlestick_ctrl_t* g = (gmb_candlestick_ctrl_t*)calloc(1, sizeof(*g));
    if (!g) return NULL;
    gmb_chart_t* ch = gmb_chart_new();
    if (!ch) { free(g); return NULL; }
    gmb_chart_set(ch, "width", w, "");
    gmb_chart_set(ch, "height", h, "");
    /* CSV "o,h,l,c;..." → flat OHLC [t,o,h,l,c]*n (il core usa il timestamp). */
    double flat[GMB_CANDLE_MAX * 5]; int n = 0;
    if (csv && *csv)
    {
        char buf[4096]; snprintf(buf, sizeof(buf), "%s", csv);
        char* s1 = NULL;
        for (char* grp = strtok_r(buf, ";", &s1); grp && n < GMB_CANDLE_MAX; grp = strtok_r(NULL, ";", &s1))
        {
            double v[4] = {0,0,0,0}; int vi = 0;
            char* s2 = NULL;
            for (char* t = strtok_r(grp, ",", &s2); t && vi < 4; t = strtok_r(NULL, ",", &s2)) v[vi++] = atof(t);
            if (vi >= 4) { int b = n * 5; flat[b] = n + 1; flat[b+1] = v[0]; flat[b+2] = v[1]; flat[b+3] = v[2]; flat[b+4] = v[3]; n++; }
        }
    }
    if (n == 0)
    {
        static const double demo[12][4] = {
            {10,12,9,11},{11,13,10,12},{12,12.5,11,11.5},{11.5,12,10,10.5},
            {10.5,13,10,12.5},{12.5,14,12,13.5},{13.5,14,12.5,13},{13,13.5,12,12.5},
            {12.5,15,12,14.5},{14.5,15,13,13.5},{13.5,14,12.5,13.5},{13.5,16,13,15.5} };
        for (int i = 0; i < 12; i++) { int b = i * 5; flat[b] = i + 1; flat[b+1] = demo[i][0]; flat[b+2] = demo[i][1]; flat[b+3] = demo[i][2]; flat[b+4] = demo[i][3]; n++; }
    }
    gmb_chart_plot_financial(ch, flat, n, "CANDLESTICK", "");
    gmb_control_init(&g->ctrl, "", x, y, w, h, cctrl_cell_draw, NULL);
    g->ctrl.draw_px_fn = cctrl_px_draw;
    g->ctrl.ctrl_kind = GMB_CK_CANDLESTICK;
    g->ctrl.user = g;
    g->chart = ch;
    gmb_tui_control_add(&g->ctrl);
    return &g->ctrl;
}
