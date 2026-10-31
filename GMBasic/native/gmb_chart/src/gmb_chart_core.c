/* gmb_chart_core.c — modello, configurazione, oggetti, scala, pick/drag, input.
 * Nessuna dipendenza da GMBasic/SDL/`g_gfx`: pura libc. Vedi include/gmb_chart.h. */
#include "gmb_chart.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>

gmb_chart_t* gmb_chart_new(void)
{
    gmb_chart_t* ch = (gmb_chart_t*)calloc(1, sizeof(*ch));
    if (!ch) return NULL;
    ch->in_use = 1;
    /* Crosshair OFF di default (nel motore C# non c'è): seguendo il mouse disegnava
     * una riga verticale+orizzontale che attraversava il grafico (bug ChartDrawDemo:
     * "una riga verticale e orizzontale che si incrociano"). Si abilita con
     * CHART SET h, "crosshair", 1. */
    ch->show_grid = 1; ch->show_legend = 1; ch->show_crosshair = 0; ch->autoscale = 1;
    ch->px = 0; ch->py = 0; ch->cw = 400; ch->ch = 220;
    ch->grid_col = 8; ch->up_col = 10; ch->down_col = 12; ch->axis_col = 7;
    ch->candle_width = 0.64;
    return ch;
}

void gmb_chart_clear(gmb_chart_t* ch)
{
    if (!ch) return;
    free(ch->xs); ch->xs = NULL;
    for (int i = 0; i < ch->n_series; i++) { free(ch->ys[i]); free(ch->ohlc[i]); }
    memset(ch->ys, 0, sizeof(ch->ys));
    memset(ch->ohlc, 0, sizeof(ch->ohlc));
    ch->n_series = 0;
    ch->nobj = 0; ch->sel_obj = 0; ch->drag_id = 0;
}

void gmb_chart_free(gmb_chart_t* ch)
{
    if (!ch) return;
    gmb_chart_clear(ch);
    free(ch);
}

void gmb_chart_plot(gmb_chart_t* ch, const double* xs, const double* ys, int n, const char* label)
{
    if (!ch || ch->n_series >= GMB_CHART_MAX_SERIES) return;
    int idx = ch->n_series++;
    if (idx == 0 && xs)
    {
        free(ch->xs);
        ch->xs = (double*)malloc((size_t)n * sizeof(double));
        for (int i = 0; i < n; i++) ch->xs[i] = xs[i];
    }
    ch->ys[idx] = (double*)malloc((size_t)n * sizeof(double));
    for (int i = 0; i < n; i++) ch->ys[idx][i] = ys[i];
    ch->lens[idx] = n;
    ch->plot_type[idx] = 0;
    snprintf(ch->labels[idx], sizeof(ch->labels[idx]), "%s", label ? label : "");
}

void gmb_chart_plot_financial(gmb_chart_t* ch, const double* ohlc, int n, const char* plot_type, const char* label)
{
    if (!ch || !ohlc || n <= 0 || ch->n_series >= GMB_CHART_MAX_SERIES) return;
    int idx = ch->n_series++;
    double* buf = (double*)malloc((size_t)n * 5 * sizeof(double));
    if (!buf) { ch->n_series--; return; }
    for (int i = 0; i < n * 5; i++) buf[i] = ohlc[i];
    ch->ohlc[idx] = buf;
    ch->ohlc_n[idx] = n;
    ch->plot_type[idx] = (plot_type && strncasecmp(plot_type, "OHLC", 4) == 0) ? 2 : 1;
    snprintf(ch->labels[idx], sizeof(ch->labels[idx]), "%s", label ? label : "");
    if (idx == 0)
    {
        free(ch->xs);
        ch->xs = (double*)malloc((size_t)n * sizeof(double));
        for (int i = 0; i < n; i++) ch->xs[i] = ohlc[i * 5];
    }
}

/* Valori base per gli indicatori: close della serie 0 (finanziaria) o ys[0] (line). */
static int chart_base_values(const gmb_chart_t* ch, double* out, int cap)
{
    int n = 0;
    if (ch->n_series == 0) return 0;
    if (ch->ohlc[0]) { n = ch->ohlc_n[0]; for (int i = 0; i < n && i < cap; i++) out[i] = ch->ohlc[0][i * 5 + 4]; }
    else if (ch->ys[0]) { n = ch->lens[0]; for (int i = 0; i < n && i < cap; i++) out[i] = ch->ys[0][i]; }
    return n;
}

int gmb_chart_indicator(gmb_chart_t* ch, const char* kind, int period, int color)
{
    if (!ch || !kind || period <= 0) return -1;
    if (ch->n_series >= GMB_CHART_MAX_SERIES) return -1;
    double base[8192];
    int n = chart_base_values(ch, base, 8192);
    if (n <= 0) return -1;
    double* ys = (double*)malloc((size_t)n * sizeof(double));
    if (!ys) return -1;
    if (!strcasecmp(kind, "ema"))
    {
        double k = 2.0 / (period + 1), e = base[0];
        for (int i = 0; i < n; i++) { e = (i == 0) ? base[0] : base[i] * k + e * (1.0 - k); ys[i] = e; }
    }
    else   /* sma */
    {
        double sum = 0;
        for (int i = 0; i < n; i++) { sum += base[i]; if (i >= period) sum -= base[i - period]; ys[i] = (i >= period - 1) ? sum / period : base[i]; }
    }
    int idx = ch->n_series++;
    if (!ch->xs) { ch->xs = (double*)malloc((size_t)n * sizeof(double)); for (int i = 0; i < n; i++) ch->xs[i] = i + 1; }
    ch->ys[idx] = ys;
    ch->lens[idx] = n;
    ch->plot_type[idx] = 0;
    ch->series_col[idx] = color;
    snprintf(ch->labels[idx], sizeof(ch->labels[idx]), "%s%d", (!strcasecmp(kind, "ema") ? "EMA" : "SMA"), period);
    return idx + 1;
}

void gmb_chart_set(gmb_chart_t* ch, const char* prop, double num, const char* str)
{
    if (!ch || !prop) return;
    if      (!strcasecmp(prop, "grid"))      ch->show_grid = (num != 0);
    else if (!strcasecmp(prop, "legend"))    ch->show_legend = (num != 0);
    else if (!strcasecmp(prop, "crosshair")) ch->show_crosshair = (num != 0);
    else if (!strcasecmp(prop, "autoscale")) ch->autoscale = (num != 0);
    else if (!strcasecmp(prop, "width")  || !strcasecmp(prop, "w")) ch->cw = (int)num;
    else if (!strcasecmp(prop, "height") || !strcasecmp(prop, "h")) ch->ch = (int)num;
    else if (!strcasecmp(prop, "px") || !strcasecmp(prop, "x")) ch->px = (int)num;
    else if (!strcasecmp(prop, "py") || !strcasecmp(prop, "y")) ch->py = (int)num;
    else if (!strcasecmp(prop, "xmin")) { ch->vx0 = num; ch->autoscale = 0; }
    else if (!strcasecmp(prop, "xmax")) { ch->vx1 = num; ch->autoscale = 0; }
    else if (!strcasecmp(prop, "ymin")) { ch->vy0 = num; ch->autoscale = 0; }
    else if (!strcasecmp(prop, "ymax")) { ch->vy1 = num; ch->autoscale = 0; }
    else if (!strcasecmp(prop, "gridcolor")) ch->grid_col = (int)num;
    else if (!strcasecmp(prop, "upcolor"))   ch->up_col = (int)num;
    else if (!strcasecmp(prop, "downcolor")) ch->down_col = (int)num;
    else if (!strcasecmp(prop, "axiscolor")) ch->axis_col = (int)num;
    else if (!strcasecmp(prop, "candlewidth") || !strcasecmp(prop, "candlefrac"))
    {
        if (num > 0) { if (num > 1) num = 1; ch->candle_width = num; }
    }
    else if (!strcasecmp(prop, "title"))  { if (str) snprintf(ch->title, sizeof(ch->title), "%s", str); }
    else if (!strcasecmp(prop, "xlabel")) { if (str) snprintf(ch->xlabel, sizeof(ch->xlabel), "%s", str); }
    else if (!strcasecmp(prop, "ylabel")) { if (str) snprintf(ch->ylabel, sizeof(ch->ylabel), "%s", str); }
}

double gmb_chart_get(const gmb_chart_t* ch, const char* prop)
{
    if (!ch || !prop) return 0;
    if      (!strcasecmp(prop, "grid"))      return ch->show_grid;
    else if (!strcasecmp(prop, "legend"))    return ch->show_legend;
    else if (!strcasecmp(prop, "crosshair")) return ch->show_crosshair;
    else if (!strcasecmp(prop, "autoscale")) return ch->autoscale;
    else if (!strcasecmp(prop, "width")  || !strcasecmp(prop, "w")) return ch->cw;
    else if (!strcasecmp(prop, "height") || !strcasecmp(prop, "h")) return ch->ch;
    else if (!strcasecmp(prop, "px") || !strcasecmp(prop, "x")) return ch->px;
    else if (!strcasecmp(prop, "py") || !strcasecmp(prop, "y")) return ch->py;
    else if (!strcasecmp(prop, "xmin")) return ch->vx0;
    else if (!strcasecmp(prop, "xmax")) return ch->vx1;
    else if (!strcasecmp(prop, "ymin")) return ch->vy0;
    else if (!strcasecmp(prop, "ymax")) return ch->vy1;
    else if (!strcasecmp(prop, "candlewidth") || !strcasecmp(prop, "candlefrac")) return ch->candle_width;
    else if (!strcasecmp(prop, "series") || !strcasecmp(prop, "seriescount")) return ch->n_series;
    else if (!strcasecmp(prop, "objs") || !strcasecmp(prop, "objcount")) return gmb_chart_objcount(ch);
    return 0;
}

const char* gmb_chart_get_str(const gmb_chart_t* ch, const char* prop)
{
    if (!ch || !prop) return "";
    if (!strcasecmp(prop, "title"))  return ch->title;
    if (!strcasecmp(prop, "xlabel")) return ch->xlabel;
    if (!strcasecmp(prop, "ylabel")) return ch->ylabel;
    return "";
}

int gmb_chart_objcount(const gmb_chart_t* ch)
{
    if (!ch) return 0;
    int n = 0;
    for (int i = 0; i < ch->nobj; i++) if (ch->objs[i].active) n++;
    return n;
}

int gmb_chart_objhit_data(const gmb_chart_t* ch, double x, double y)
{
    if (!ch) return -1;
    double best = 1e300; int bi = -1;
    for (int i = 0; i < ch->nobj; i++)
    {
        const gmb_chart_obj_t* o = &ch->objs[i];
        if (!o->active) continue;
        double dx = o->c - o->a, dy = o->d - o->b, L2 = dx * dx + dy * dy;
        double t = L2 > 0 ? ((x - o->a) * dx + (y - o->b) * dy) / L2 : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double px = o->a + t * dx, py = o->b + t * dy, d = (x - px) * (x - px) + (y - py) * (y - py);
        if (d < best) { best = d; bi = i; }
    }
    return bi + 1;
}

void gmb_chart_scale(const gmb_chart_t* ch, int* ox, int* oy, int* w, int* h,
                     double* x0, double* x1, double* y0, double* y1)
{
    if (ch->last_valid)
    {
        *ox = ch->last_ox; *oy = ch->last_oy; *w = ch->last_w; *h = ch->last_h;
        *x0 = ch->last_x0; *x1 = ch->last_x1; *y0 = ch->last_y0; *y1 = ch->last_y1;
    }
    else
    {
        *ox = ch->px; *oy = ch->py; *w = ch->cw > 0 ? ch->cw : 400; *h = ch->ch > 0 ? ch->ch : 220;
        if (ch->autoscale && ch->n_series > 0)
        {
            double xmn = ch->ohlc[0][0], xmx = ch->ohlc[0][0], ymn = ch->ohlc[0][2], ymx = ch->ohlc[0][2];
            for (int s = 0; s < ch->n_series; s++)
                for (int i = 0; i < ch->ohlc_n[s]; i++) {
                    const double* d = ch->ohlc[s] + i * 5;
                    if (d[0] < xmn) xmn = d[0]; if (d[0] > xmx) xmx = d[0];
                    if (d[3] < ymn) ymn = d[3]; if (d[2] > ymx) ymx = d[2];
                }
            *x0 = xmn; *x1 = xmx; *y0 = ymn; *y1 = ymx;
        }
        else { *x0 = ch->vx0; *x1 = ch->vx1; *y0 = ch->vy0; *y1 = ch->vy1; }
    }
    if (*x1 == *x0) (*x1)++;
    if (*y1 == *y0) (*y1)++;
}

int gmb_chart_pick_px(const gmb_chart_t* ch, int px, int py)
{
    if (!ch) return -1;
    int ox, oy, w, h; double xmin, xmax, ymin, ymax;
    gmb_chart_scale(ch, &ox, &oy, &w, &h, &xmin, &xmax, &ymin, &ymax);
    double xr = xmax - xmin, yr = ymax - ymin;
#define PXX(x) (ox + 1 + (int)(((x) - xmin) / xr * (w - 2)))
#define PYY(y) (oy + h - 1 - (int)(((y) - ymin) / yr * (h - 2)) - 1)
    double best = 1e18; int bi = -1;
    for (int i = 0; i < ch->nobj; i++)
    {
        const gmb_chart_obj_t* o = &ch->objs[i];
        if (!o->active) continue;
        int x1 = PXX(o->a), y1 = PYY(o->b), x2 = PXX(o->c), y2 = PYY(o->d);
        double dx = x2 - x1, dy = y2 - y1, L2 = dx * dx + dy * dy;
        double t = L2 > 0 ? ((px - x1) * dx + (py - y1) * dy) / L2 : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double qx = x1 + t * dx, qy = y1 + t * dy, d = (px - qx) * (px - qx) + (py - qy) * (py - qy);
        if (d < best) { best = d; bi = i; }
    }
#undef PXX
#undef PYY
    return best <= 400.0 ? bi + 1 : -1;
}

double gmb_chart_draw_cmd(gmb_chart_t* ch, const char* op, const double* args, int nargs)
{
    if (!ch || !op) return -1;
#define GAR(i) ((i) < nargs ? args[i] : 0.0)
    if (!strcasecmp(op, "count")) return gmb_chart_objcount(ch);
    if (!strcasecmp(op, "hit"))   return gmb_chart_objhit_data(ch, GAR(0), GAR(1));
    if (!strcasecmp(op, "pick"))  { int id = gmb_chart_pick_px(ch, (int)GAR(0), (int)GAR(1)); ch->sel_obj = id; return id; }
    if (!strcasecmp(op, "drag"))
    {
        int id = (int)GAR(0);
        if (id >= 1 && id <= ch->nobj && ch->objs[id - 1].active)
        {
            int ox, oy, w, h; double xmin, xmax, ymin, ymax;
            gmb_chart_scale(ch, &ox, &oy, &w, &h, &xmin, &xmax, &ymin, &ymax);
            double xd = xmin + (GAR(1) - ox - 1) / (w - 2) * (xmax - xmin);
            double yd = ymin + (oy + h - 2 - GAR(2)) / (h - 2) * (ymax - ymin);
            gmb_chart_obj_t* o = &ch->objs[id - 1];
            double cx = (o->a + o->c) / 2, cy = (o->b + o->d) / 2;
            o->a += xd - cx; o->c += xd - cx; o->b += yd - cy; o->d += yd - cy;
            ch->sel_obj = id;
        }
        return id;
    }
    if (!strcasecmp(op, "clear")) { ch->nobj = 0; ch->sel_obj = 0; return 0; }
    if (!strcasecmp(op, "remove"))
    {
        int id = (int)GAR(0);
        if (id >= 1 && id <= ch->nobj) ch->objs[id - 1].active = 0;
        return id;
    }
    if (!strcasecmp(op, "move"))
    {
        int id = (int)GAR(0);
        if (id >= 1 && id <= ch->nobj && ch->objs[id - 1].active)
        {
            gmb_chart_obj_t* o = &ch->objs[id - 1];
            o->a = GAR(1); o->b = GAR(2); o->c = GAR(3); o->d = GAR(4);
        }
        return id;
    }
    if (!strcasecmp(op, "zoom")) { gmb_chart_zoom(ch, GAR(0) > 0 ? GAR(0) : 1.0, NAN, NAN); return 0; }
    if (!strcasecmp(op, "pan"))  { gmb_chart_pan(ch, GAR(0), GAR(1)); return 0; }
    if (!strcasecmp(op, "sma") || !strcasecmp(op, "ema"))
        return gmb_chart_indicator(ch, op, (int)GAR(0), (nargs > 1) ? (int)args[1] : 0);
    if (!strcasecmp(op, "buy") || !strcasecmp(op, "sell"))
    {
        if (ch->nobj >= GMB_CHART_MAX_OBJ) return -1;
        int idx = ch->nobj++;
        gmb_chart_obj_t* o = &ch->objs[idx];
        o->kind = (!strcasecmp(op, "buy")) ? 6 : 7;
        o->a = GAR(0); o->b = GAR(1); o->c = o->a; o->d = o->b; o->active = 1;
        o->color = (nargs > 2) ? (int)args[2] : (o->kind == 6 ? ch->up_col : ch->down_col);
        return idx + 1;
    }
    int kind = 0;
    if      (!strcasecmp(op, "line"))  kind = 1;
    else if (!strcasecmp(op, "hline")) kind = 2;
    else if (!strcasecmp(op, "vline")) kind = 3;
    else if (!strcasecmp(op, "rect"))  kind = 4;
    else if (!strcasecmp(op, "fib"))   kind = 5;
    else return -1;
    if (ch->nobj >= GMB_CHART_MAX_OBJ) return -1;
    int idx = ch->nobj++;
    gmb_chart_obj_t* o = &ch->objs[idx];
    o->kind = kind; o->active = 1;
    o->a = GAR(0); o->b = GAR(1); o->c = GAR(2); o->d = GAR(3);
    int cidx = (kind == 2 || kind == 3) ? 1 : 4;
    o->color = (nargs > cidx) ? (int)args[cidx] : 11;
    return idx + 1;
#undef GAR
}

void gmb_chart_set_pointer(gmb_chart_t* ch, int px, int py, int down)
{
    if (!ch) return;
    ch->ptr_x = px; ch->ptr_y = py; ch->ptr_down = down; ch->ptr_valid = 1;
}

void gmb_chart_set_wheel(gmb_chart_t* ch, int delta) { if (ch) ch->wheel += delta; }

/* Materializza la view (se autoscale) nei campi vx/vy e spegne autoscale. */
static void chart_ensure_view(gmb_chart_t* ch)
{
    if (!ch->autoscale) return;
    int ox, oy, w, h; double x0, x1, y0, y1;
    gmb_chart_scale(ch, &ox, &oy, &w, &h, &x0, &x1, &y0, &y1);
    ch->vx0 = x0; ch->vx1 = x1; ch->vy0 = y0; ch->vy1 = y1; ch->autoscale = 0;
}

void gmb_chart_zoom(gmb_chart_t* ch, double factor, double cx, double cy)
{
    if (!ch || factor <= 0) return;
    chart_ensure_view(ch);
    if (cx != cx) cx = (ch->vx0 + ch->vx1) / 2;   /* NaN → centro view */
    if (cy != cy) cy = (ch->vy0 + ch->vy1) / 2;
    ch->vx0 = cx + (ch->vx0 - cx) * factor; ch->vx1 = cx + (ch->vx1 - cx) * factor;
    ch->vy0 = cy + (ch->vy0 - cy) * factor; ch->vy1 = cy + (ch->vy1 - cy) * factor;
}

void gmb_chart_pan(gmb_chart_t* ch, double dx, double dy)
{
    if (!ch) return;
    chart_ensure_view(ch);
    ch->vx0 += dx; ch->vx1 += dx; ch->vy0 += dy; ch->vy1 += dy;
}
