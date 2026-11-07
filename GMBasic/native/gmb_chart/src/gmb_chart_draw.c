/* gmb_chart_draw.c — rendering del chart attraverso la function table dell'host.
 * Il core NON chiama primitive proprie: usa SOLO gmb_chart_renderer_t. */
#include "gmb_chart.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void R_line(const gmb_chart_renderer_t* r, int x1, int y1, int x2, int y2, int col)
{ if (r->line) r->line(r->ctx, x1, y1, x2, y2, col); }
static void R_rect(const gmb_chart_renderer_t* r, int x, int y, int w, int h, int col, int filled)
{ if (r->rect) r->rect(r->ctx, x, y, w, h, col, filled); }
static void R_text(const gmb_chart_renderer_t* r, const char* s, int x, int y, int align, int col)
{ if (r->text) r->text(r->ctx, s, x, y, align, col); }
static int  R_th(const gmb_chart_renderer_t* r) { return (r->text_height ? r->text_height(r->ctx) : (r->font_hint > 0 ? r->font_hint : 12)); }
/* Larghezza del testo logicamente (host che non la espone: stima 0.55 × altezza). */
static int  R_tw(const gmb_chart_renderer_t* r, const char* s)
{
    if (!s) return 0;
    if (r->text_width) return r->text_width(r->ctx, s);
    int th = R_th(r);
    return (int)(strlen(s) * th * 0.55f + 0.5f);
}

#define CLR_GRID 8
#define CLR_TXT  7

static int ptr_in(const gmb_chart_t* ch, int ox, int oy, int w, int h, int* mx, int* my)
{
    *mx = ch->ptr_x; *my = ch->ptr_y;
    return ch->ptr_valid && *mx > ox && *mx < ox + w - 1 && *my > oy && *my < oy + h - 1;
}

/* Passo "bello" (1/2/5×10^k) per le tacche dell'asse Y. */
static double nice_step(double range)
{
    double raw = range / 4.0;
    if (raw <= 0) return 1;
    double mag = pow(10.0, floor(log10(raw)));
    double n = raw / mag;
    double s = (n < 1.5) ? 1 : (n < 3 ? 2 : (n < 7 ? 5 : 10));
    return s * mag;
}

static void draw_grid(const gmb_chart_renderer_t* r, int ox, int oy, int w, int h,
                      double xmin, double xmax, double ymin, double ymax, int col,
                      int label_x, int label_y, int labels)
{
    int th = R_th(r);
    double step = nice_step(ymax - ymin);
    /* Tacche Y (valori "belli") con etichette NEL MARGINE sinistro: prima stavano a
     * ox+3, cioè DENTRO l'area del grafico, sopra la griglia e la prima linea. */
    for (double y = ceil(ymin / step) * step; y <= ymax + 1e-9; y += step)
    {
        int gy = oy + h - 1 - (int)((y - ymin) / (ymax - ymin) * (h - 2)) - 1;
        if (gy < oy || gy > oy + h - 1) continue;
        R_line(r, ox + 1, gy, ox + w - 2, gy, col);
        if (labels) { char lbl[40]; snprintf(lbl, sizeof(lbl), "%.6g", y); R_text(r, lbl, label_x, gy - th + 1, 0, CLR_TXT); }
    }
    /* Tacche X con passo "bello" (1/2/5x10^k) e etichette SOTTO l'area del grafico.
     * Prima erano 6 divisioni fisse con "%.5g": uscivano numeri come 2.8333 e 4.6667,
     * illeggibili in una UI. Ora il passo è tondo (1,2,5,10...) e gli interi si
     * scrivono senza decimali. */
    {
        double step = nice_step(xmax - xmin);
        /* etichette: al massimo una ogni ~60px, così non si accavallano */
        int last_x = -10000;
        for (double x = ceil(xmin / step) * step; x <= xmax + 1e-9; x += step)
        {
            int gx = ox + 1 + (int)((x - xmin) / (xmax - xmin) * (w - 2));
            if (gx < ox || gx > ox + w - 2) continue;
            R_line(r, gx, oy + 1, gx, oy + h - 2, col);
            if (!labels) continue;
            char lbl[40];
            double rv = floor(x + 0.5);
            if (fabs(x - rv) < 1e-9) snprintf(lbl, sizeof(lbl), "%.0f", rv);
            else snprintf(lbl, sizeof(lbl), "%.6g", x);
            int tw = R_tw(r, lbl);
            if (gx - last_x < tw + 10) continue;      /* salta: si sovrapporrebbe */
            R_text(r, lbl, gx + 1, label_y, 0, CLR_TXT);
            last_x = gx + tw;
        }
    }
}

static void draw_legend(const gmb_chart_t* ch, const gmb_chart_renderer_t* r, int ox, int oy, int w)
{
    (void)w;
    int th = R_th(r);
    int ly = oy + 2;
    for (int s = 0; s < ch->n_series && s < GMB_CHART_MAX_SERIES; s++)
    {
        if (!ch->labels[s][0]) { ly += th + 2; continue; }
        static const int colors[8] = { 15, 10, 14, 13, 11, 9, 12, 6 };
        R_line(r, ox + 3, ly + th / 2, ox + 3 + 8, ly + th / 2, colors[s]);
        R_text(r, ch->labels[s], ox + 3 + 12, ly, 0, CLR_TXT);
        ly += th + 2;
        if (ly > oy + 60) break;
    }
}

static void draw_objects(const gmb_chart_t* ch, const gmb_chart_renderer_t* r, int ox, int oy, int w, int h,
                         double xmin, double xmax, double ymin, double ymax)
{
    double xr = xmax - xmin; if (xr <= 0) xr = 1;
    double yr = ymax - ymin; if (yr <= 0) yr = 1;
#define XM(x) (ox + 1 + (int)(((x) - xmin) / xr * (w - 2)))
#define YM(y) (oy + h - 1 - (int)(((y) - ymin) / yr * (h - 2)) - 1)
    for (int i = 0; i < ch->nobj; i++)
    {
        const gmb_chart_obj_t* o = &ch->objs[i];
        if (!o->active) continue;
        int col = (i + 1 == ch->sel_obj) ? 15 : o->color;
        switch (o->kind)
        {
            case 1: R_line(r, XM(o->a), YM(o->b), XM(o->c), YM(o->d), col); break;
            case 2: R_line(r, ox + 1, YM(o->a), ox + w - 2, YM(o->a), col); break;
            case 3: R_line(r, XM(o->a), oy + 1, XM(o->a), oy + h - 2, col); break;
            case 4: { int x1 = XM(o->a), y1 = YM(o->b), x2 = XM(o->c), y2 = YM(o->d);
                      R_line(r, x1, y1, x2, y1, col); R_line(r, x2, y1, x2, y2, col);
                      R_line(r, x2, y2, x1, y2, col); R_line(r, x1, y2, x1, y1, col); } break;
            case 5: { static const double lv[7] = { 0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0 };
                      int x1 = XM(o->a), x2 = XM(o->c); if (x2 < x1) { int t = x1; x1 = x2; x2 = t; }
                      for (int k = 0; k < 7; k++) { int yy = YM(o->b + (o->d - o->b) * lv[k]); R_line(r, x1, yy, x2, yy, col); } } break;
            case 6: { int x = XM(o->a), y = YM(o->b);   /* BUY: triangolo su */
                      R_line(r, x, y - 7, x - 4, y + 2, col); R_line(r, x, y - 7, x + 4, y + 2, col); R_line(r, x - 4, y + 2, x + 4, y + 2, col); } break;
            case 7: { int x = XM(o->a), y = YM(o->b);   /* SELL: triangolo giù */
                      R_line(r, x, y + 7, x - 4, y - 2, col); R_line(r, x, y + 7, x + 4, y - 2, col); R_line(r, x - 4, y - 2, x + 4, y - 2, col); } break;
        }
    }
#undef XM
#undef YM
}

static void auto_drag(gmb_chart_t* ch, const gmb_chart_renderer_t* r, int ox, int oy, int w, int h,
                      double xmin, double xmax, double ymin, double ymax)
{
    (void)r;
    int mx, my;
    if (ch->ptr_down && ptr_in(ch, ox, oy, w, h, &mx, &my))
    {
        if (ch->drag_id == 0)
        {
            int id = gmb_chart_pick_px(ch, mx, my);
            if (id >= 1) { ch->drag_id = id; ch->sel_obj = id; }
            else { ch->drag_id = -1; ch->drag_ax = mx; ch->drag_ay = my; }   /* drag su area vuota → PAN */
        }
        if (ch->drag_id >= 1 && ch->drag_id <= ch->nobj)
        {
            double xd = xmin + (mx - ox - 1) / (double)(w - 2) * (xmax - xmin);
            double yd = ymin + (oy + h - 2 - my) / (double)(h - 2) * (ymax - ymin);
            gmb_chart_obj_t* o = &ch->objs[ch->drag_id - 1];
            double cx = (o->a + o->c) / 2, cy = (o->b + o->d) / 2;
            o->a += xd - cx; o->c += xd - cx; o->b += yd - cy; o->d += yd - cy;
        }
        else if (ch->drag_id == -1)
        {
            int ox2, oy2, w2, h2; double x0, x1, y0, y1;
            gmb_chart_scale(ch, &ox2, &oy2, &w2, &h2, &x0, &x1, &y0, &y1);
            double dxd = (ch->drag_ax - mx) / (double)(w2 - 2) * (x1 - x0);
            double dyd = (my - ch->drag_ay) / (double)(h2 - 2) * (y1 - y0);
            gmb_chart_pan(ch, dxd, dyd);
            ch->drag_ax = mx; ch->drag_ay = my;
        }
    }
    else ch->drag_id = 0;
}

static void crosshair(const gmb_chart_t* ch, const gmb_chart_renderer_t* r, int ox, int oy, int w, int h,
                      double xmin, double xmax, double ymin, double ymax)
{
    int mx, my;
    if (!ch->show_crosshair || !ptr_in(ch, ox, oy, w, h, &mx, &my)) return;
    R_line(r, mx, oy + 1, mx, oy + h - 2, 7);
    R_line(r, ox + 1, my, ox + w - 2, my, 7);
    double t = xmin + (double)(mx - ox - 1) / (w - 2) * (xmax - xmin);
    char info[128];
    if (ch->n_series > 0 && ch->plot_type[0] != 0 && ch->ohlc_n[0] > 0)
    {
        int bi = 0; double best = 1e300;
        for (int i = 0; i < ch->ohlc_n[0]; i++) { double dd = ch->ohlc[0][i * 5] - t; if (dd < 0) dd = -dd; if (dd < best) { best = dd; bi = i; } }
        const double* d = ch->ohlc[0] + bi * 5;
        snprintf(info, sizeof(info), "t=%.6g  O=%.5g H=%.5g L=%.5g C=%.5g", d[0], d[1], d[2], d[3], d[4]);
    }
    else
    {
        double yv = ymin + (double)(oy + h - 2 - my) / (h - 2) * (ymax - ymin);
        snprintf(info, sizeof(info), "x=%.6g  y=%.6g", t, yv);
    }
    R_text(r, info, ox + 3, oy + h - R_th(r) - 2, 0, 15);
}

void gmb_chart_draw(gmb_chart_t* ch, const gmb_chart_renderer_t* r, int ox, int oy, int w, int h)
{
    if (!ch || !r || !r->line) return;
    if (ch->n_series == 0) return;
    /* Rotella → zoom attorno al punto sotto il mouse (view della scorsa vista). */
    if (ch->wheel)
    {
        int ox0, oy0, w0, h0; double x0, x1, y0, y1;
        gmb_chart_scale(ch, &ox0, &oy0, &w0, &h0, &x0, &x1, &y0, &y1);
        double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        int mx0, my0;
        if (ptr_in(ch, ox0, oy0, w0, h0, &mx0, &my0))
        {
            cx = x0 + (mx0 - ox0 - 1) / (double)(w0 - 2) * (x1 - x0);
            cy = y0 + (oy0 + h0 - 2 - my0) / (double)(h0 - 2) * (y1 - y0);
        }
        gmb_chart_zoom(ch, ch->wheel > 0 ? 0.85 : 1.0 / 0.85, cx, cy);
        ch->wheel = 0;
    }
    if (w <= 0) w = ch->cw > 0 ? ch->cw : 400;
    if (h <= 0) h = ch->ch > 0 ? ch->ch : 220;
    if (w <= 8 || h <= 4) return;

    /* scala: autoscale sui dati oppure view window */
    double xmin, xmax, ymin, ymax;
    if (ch->autoscale)
    {
        xmin = xmax = 0; ymin = ymax = 0; int first = 1;
        for (int s = 0; s < ch->n_series; s++)
        {
            if (ch->plot_type[s] != 0 || ch->ohlc[s])
            {
                for (int i = 0; i < ch->ohlc_n[s]; i++) {
                    const double* d = ch->ohlc[s] + i * 5;
                    if (first) { xmin = xmax = d[0]; ymin = d[2]; ymax = d[3]; first = 0; }
                    if (d[0] < xmin) xmin = d[0]; if (d[0] > xmax) xmax = d[0];
                    if (d[3] < ymin) ymin = d[3]; if (d[2] > ymax) ymax = d[2];
                }
            }
            else if (ch->xs)
            {
                for (int i = 0; i < ch->lens[s]; i++) {
                    double x = ch->xs[i], y = ch->ys[s][i];
                    if (first) { xmin = xmax = x; ymin = ymax = y; first = 0; }
                    if (x < xmin) xmin = x; if (x > xmax) xmax = x;
                    if (y < ymin) ymin = y; if (y > ymax) ymax = y;
                }
            }
        }
        if (first) return;
    }
    else { xmin = ch->vx0; xmax = ch->vx1; ymin = ch->vy0; ymax = ch->vy1; }
    if (xmax == xmin) xmax = xmin + 1;
    if (ymax == ymin) ymax = ymin + 1;

    /* ── Margini per le ETICHETTE: l'asse Y vive nel margine sinistro, l'asse X sotto
     * l'area del grafico. Prima entrambe stavano DENTRO il plot, sopra la griglia. ── */
    int th_lbl = R_th(r);
    int ylw = 0, xlh = 0;
    /* Grafico PICCOLO (card KPI): niente etichette, solo la forma — le etichette a 60px
     * di altezza si sovrapponevano tra loro e alla linea. */
    int labels = !(w < 200 || h < 96);
    if (ch->show_grid && labels)
    {
        double stp = nice_step(ymax - ymin);
        for (double y = ceil(ymin / stp) * stp; y <= ymax + 1e-9; y += stp)
        {
            char lbl[40]; snprintf(lbl, sizeof(lbl), "%.6g", y);
            int tw = R_tw(r, lbl); if (tw > ylw) ylw = tw;
        }
        if (ylw > 0) ylw += 8;                 /* gap etichetta ↔ asse */
        xlh = th_lbl + 4;                      /* fascia in basso per le etichette X */
    }
    if (ylw > w / 3) ylw = w / 3;              /* grafico minuscolo: non mangiarlo tutto */
    if (xlh > h / 3) xlh = h / 3;
    int label_x = ox + 2;                      /* etichette Y nel margine */
    int label_y = oy + h - xlh + 2;             /* etichette X sotto il plot */
    ox += ylw; w -= ylw;
    h -= xlh;
    if (w < 12) { ox -= ylw; w += ylw; ylw = 0; label_x = ox + 2; }
    if (h < 8)  { h += xlh; xlh = 0; }

    /* memorizza la vista effettiva (per pick/drag in pixel) */
    ch->last_ox = ox; ch->last_oy = oy; ch->last_w = w; ch->last_h = h;
    ch->last_x0 = xmin; ch->last_x1 = xmax; ch->last_y0 = ymin; ch->last_y1 = ymax; ch->last_valid = 1;

    if (ch->title[0])  R_text(r, ch->title, ox + w / 2, oy + 2, 1, ch->axis_col);
    if (ch->ylabel[0]) R_text(r, ch->ylabel, ox + 2, oy + R_th(r) + 2, 0, ch->axis_col);

    if (ch->show_grid)   draw_grid(r, ox, oy, w, h, xmin, xmax, ymin, ymax, ch->grid_col, label_x, label_y, labels);
    if (ch->show_legend) draw_legend(ch, r, ox, oy, w);

    static const int colors[8] = { 15, 10, 14, 13, 11, 9, 12, 6 };
    double xr = xmax - xmin, yr = ymax - ymin;
#define XM(x) (ox + 1 + (int)(((x) - xmin) / xr * (w - 2)))
#define YM(y) (oy + h - 1 - (int)(((y) - ymin) / yr * (h - 2)) - 1)
    for (int s = 0; s < ch->n_series; s++)
    {
        if (ch->plot_type[s] != 0 || ch->ohlc[s])
        {
            int n = ch->ohlc_n[s];
            double sp = (n > 1) ? (double)(w - 2) / (n - 1) : (double)(w - 2);
            double cw = ch->candle_width > 0 ? ch->candle_width : 0.64;
            int hw = (int)(sp * cw * 0.5 + 0.5);   /* semi-larghezza = metà dello slot * frazione */
            if (hw < 1) hw = 1;
            if (hw > 40) hw = 40;
            for (int i = 0; i < n; i++)
            {
                const double* d = ch->ohlc[s] + i * 5;
                double o = d[1], hi = d[2], lo = d[3], cl = d[4];
                int px = XM(d[0]), po = YM(o), phi = YM(hi), plo = YM(lo), pcl = YM(cl);
                int col = (cl >= o) ? ch->up_col : ch->down_col;
                if (ch->plot_type[s] == 2) {
                    R_line(r, px, phi, px, plo, col);
                    R_line(r, px - hw, po, px, po, col);
                    R_line(r, px, pcl, px + hw, pcl, col);
                } else {
                    R_line(r, px, phi, px, plo, col);
                    int ytop = po < pcl ? po : pcl, ybot = po < pcl ? pcl : po;
                    /* Corpo come RETTANGOLO pieno (non una linea per riga): a renderScale>1
                     * le linee orizzontali a ogni y lasciavano righe nere alternate. */
                    R_rect(r, px - hw, ytop, hw * 2 + 1, ybot - ytop + 1, col, 1);
                }
            }
        }
        else if (ch->xs)
        {
            int n = ch->lens[s], prevx = 0, prevy = 0;
            int col = ch->series_col[s] ? ch->series_col[s] : colors[s];
            for (int i = 0; i < n; i++)
            {
                int px = XM(ch->xs[i]), py = YM(ch->ys[s][i]);
                if (i > 0) R_line(r, prevx, prevy, px, py, col);
                prevx = px; prevy = py;
            }
        }
    }
#undef XM
#undef YM

    draw_objects(ch, r, ox, oy, w, h, xmin, xmax, ymin, ymax);
    auto_drag(ch, r, ox, oy, w, h, xmin, xmax, ymin, ymax);
    crosshair(ch, r, ox, oy, w, h, xmin, xmax, ymin, ymax);
}
