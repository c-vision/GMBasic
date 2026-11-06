/* ============================================================================
 * gmb_tui_charts.c — Charts (porting Charts.cs). Puro.
 * ============================================================================ */
#include "gmb_tui_charts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const uint32_t V_BLOCKS[9] = { ' ', 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588 };
static const uint32_t H_BLOCKS[9] = { ' ', 0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589, 0x2588 };
static const uint32_t DOT_BITS[8] = { 0x01, 0x08, 0x02, 0x10, 0x04, 0x20, 0x40, 0x80 };

int gmb_chartutil_format_value(double v, char* out, int cap)
{
    double rounded = round(v);
    if (fabs(v - rounded) < 1e-10)
        return snprintf(out, cap, "%lld", (long long)rounded);
    return snprintf(out, cap, "%.1f", v);
}

static void chart_truncate(const char* s, int max_len, char* out, int cap)
{
    int l = (int)strlen(s);
    if (l <= max_len || max_len < 0) { snprintf(out, cap, "%s", s); return; }
    snprintf(out, cap, "%.*s", max_len, s);
}

void gmb_chartutil_draw_hbar(gmb_screen_buffer_t* buf, int x, int y, int width, double ratio, const gmb_cell_color_t* color)
{
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int eighths = (int)(ratio * width * 8.0 + 0.5);
    gmb_cell_color_t c = color ? *color : GMB_CELLCOLOR_DEFAULT;
    for (int col = 0; col < width; col++)
    {
        int level = eighths - col * 8;
        if (level < 0) level = 0;
        if (level > 8) level = 8;
        if (level > 0)
            gmb_screen_setchar(buf, x + col, y, H_BLOCKS[level], c, GMB_CELLCOLOR_DEFAULT);
    }
}

void gmb_chartutil_draw_vbar(gmb_screen_buffer_t* buf, int x, int bottom_y, int height, double ratio, const gmb_cell_color_t* color)
{
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int eighths = (int)(ratio * height * 8.0 + 0.5);
    gmb_cell_color_t c = color ? *color : GMB_CELLCOLOR_DEFAULT;
    for (int row = 0; row < height; row++)
    {
        int level = eighths - row * 8;
        if (level < 0) level = 0;
        if (level > 8) level = 8;
        if (level > 0)
            gmb_screen_setchar(buf, x, bottom_y - row, V_BLOCKS[level], c, GMB_CELLCOLOR_DEFAULT);
    }
}

/* ===================== Sparkline ===================== */

static int spark_level(const int* values, int n, int width, bool has_min, int min, bool has_max, int max)
{
    (void)n;
    return 0;
}

int gmb_sparkline_render(const int* values, int n, int width, bool has_min, int min, bool has_max, int max, char* out, int cap)
{
    if (!values || n <= 0 || width <= 0) { if (out && cap > 0) out[0] = '\0'; return 0; }
    int data_min = values[0], data_max = values[0];
    for (int i = 1; i < n; i++)
    {
        if (values[i] < data_min) data_min = values[i];
        if (values[i] > data_max) data_max = values[i];
    }
    int lo = has_min ? min : (data_min < 0 ? data_min : 0);
    int hi = has_max ? max : data_max;
    if (hi <= lo) hi = lo + 1;
    int nn = n < width ? n : width;
    int o = 0;
    for (int i = 0; i < nn; i++)
    {
        double t = (double)(values[i] - lo) / (hi - lo);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        int level = (int)(t * 8.0 + 0.5);
        if (level > 8) level = 8;
        uint32_t ch = V_BLOCKS[level];
        /* scrivi UTF-8 */
        if (ch < 0x80) { if (o < cap - 1) out[o++] = (char)ch; }
        else if (ch < 0x800) { if (o < cap - 2) { out[o++] = (char)(0xC0 | (ch >> 6)); out[o++] = (char)(0x80 | (ch & 0x3F)); } }
        else { if (o < cap - 3) { out[o++] = (char)(0xE0 | (ch >> 12)); out[o++] = (char)(0x80 | ((ch >> 6) & 0x3F)); out[o++] = (char)(0x80 | (ch & 0x3F)); } }
    }
    out[o] = '\0';
    return o;
}

void gmb_sparkline_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, const int* values, int n, int width)
{
    char s[512];
    int sl = gmb_sparkline_render(values, n, width, false, 0, false, 0, s, (int)sizeof(s));
    for (int i = 0; i < sl; i++)
    {
        if ((unsigned char)s[i] >= 0x80)
        {
            /* copia il carattere UTF-8 completo */
            uint32_t ch = 0;
            int len = 0;
            unsigned char c0 = (unsigned char)s[i];
            if ((c0 >> 3) == 0x1E && s[i+1] && s[i+2] && s[i+3]) { ch = ((c0 & 7) << 18) | ((s[i+1]&0x3F)<<12) | ((s[i+2]&0x3F)<<6) | (s[i+3]&0x3F); len = 4; }
            else if ((c0 >> 4) == 0x0E && s[i+1] && s[i+2]) { ch = ((c0 & 15) << 12) | ((s[i+1]&0x3F)<<6) | (s[i+2]&0x3F); len = 3; }
            else if ((c0 >> 5) == 0x06 && s[i+1]) { ch = ((c0 & 31) << 6) | (s[i+1]&0x3F); len = 2; }
            gmb_screen_setchar(buf, x + i, y, ch, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
            i += len - 1;
        }
        else
            gmb_screen_setchar(buf, x + i, y, (unsigned char)s[i], GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
    }
}

/* ===================== BarChart ===================== */

void gmb_bar_chart_init(gmb_bar_chart_t* c)
{
    memset(c, 0, sizeof(*c));
    memset(c, 0, sizeof(*c));
    c->orientation = GMB_ORIENT_HORIZONTAL;
    c->width = 20;
    c->height = 10;
    c->bar_width = 1;
    c->bar_gap = 1;
}

void gmb_bar_chart_add(gmb_bar_chart_t* c, const char* label, int value, bool has_color, gmb_cell_color_t color)
{
    if (c->bars_count >= GMB_CHART_MAX_BARS) return;
    gmb_chart_bar_t* b = &c->bars[c->bars_count++];
    snprintf(b->label, sizeof(b->label), "%s", label ? label : "");
    b->value = value;
    b->has_color = has_color;
    b->color = color;
}

static double barchart_max_val(gmb_bar_chart_t* c)
{
    if (c->has_max) return c->max;
    if (c->bars_count == 0) return 0.0;
    double m = 0.0;
    for (int i = 0; i < c->bars_count; i++)
        if ((double)c->bars[i].value > m) m = (double)c->bars[i].value;
    return m;
}

static void barchart_render_horizontal(gmb_bar_chart_t* c, gmb_screen_buffer_t* buf, int x, int y)
{
    double max = barchart_max_val(c);
    int label_w = 0;
    for (int i = 0; i < c->bars_count; i++)
    {
        int l = (int)strlen(c->bars[i].label);
        if (l > label_w) label_w = l;
    }
    if (label_w > c->width / 2) label_w = c->width / 2;
    int value_w = 0;
    if (c->show_values)
    {
        char tmp[32];
        for (int i = 0; i < c->bars_count; i++)
        {
            gmb_chartutil_format_value(c->bars[i].value, tmp, sizeof(tmp));
            int l = (int)strlen(tmp) + 1;
            if (l > value_w) value_w = l;
        }
    }
    int sep = label_w > 0 ? 1 : 0;
    int bar_x = x + label_w + sep;
    int bar_w = c->width - label_w - sep - value_w;
    if (bar_w <= 0) return;
    int rows = c->bars_count < c->height ? c->bars_count : c->height;
    for (int i = 0; i < rows; i++)
    {
        gmb_chart_bar_t* bar = &c->bars[i];
        int ry = y + i;
        if (label_w > 0)
        {
            char lbl[64];
            chart_truncate(bar->label, label_w, lbl, sizeof(lbl));
            gmb_screen_setstring(buf, x, ry, lbl, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
        }
        double ratio = max > 0.0 ? bar->value / max : 0.0;
        const gmb_cell_color_t* color = bar->has_color ? &bar->color : (c->has_default_color ? &c->default_color : NULL);
        gmb_chartutil_draw_hbar(buf, bar_x, ry, bar_w, ratio, color);
        if (c->show_values)
        {
            char tmp[32];
            gmb_chartutil_format_value(bar->value, tmp, sizeof(tmp));
            gmb_screen_setstring(buf, bar_x + bar_w + 1, ry, tmp, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
        }
    }
}

static void barchart_render_vertical(gmb_bar_chart_t* c, gmb_screen_buffer_t* buf, int x, int y)
{
    double max = barchart_max_val(c);
    bool has_labels = false;
    for (int i = 0; i < c->bars_count; i++)
        if (c->bars[i].label[0]) { has_labels = true; break; }
    int label_row = has_labels ? 1 : 0;
    int chart_h = c->height - label_row;
    if (chart_h <= 0) return;
    int bottom_y = y + chart_h - 1;
    int step = c->bar_width + c->bar_gap;
    int n = c->bars_count < (c->width / (step > 0 ? step : 1)) ? c->bars_count : (c->width / (step > 0 ? step : 1));
    for (int i = 0; i < n; i++)
    {
        gmb_chart_bar_t* bar = &c->bars[i];
        int bx = x + i * step;
        double ratio = max > 0.0 ? bar->value / max : 0.0;
        const gmb_cell_color_t* color = bar->has_color ? &bar->color : (c->has_default_color ? &c->default_color : NULL);
        for (int cc = 0; cc < c->bar_width; cc++)
        {
            if (bx + cc >= x + c->width) break;
            gmb_chartutil_draw_vbar(buf, bx + cc, bottom_y, chart_h, ratio, color);
        }
        if (has_labels && bar->label[0])
        {
            char lbl[64];
            chart_truncate(bar->label, c->bar_width, lbl, sizeof(lbl));
            gmb_screen_setstring(buf, bx, bottom_y + 1, lbl, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
        }
    }
}

void gmb_bar_chart_render(gmb_bar_chart_t* c, gmb_screen_buffer_t* buf, int x, int y)
{
    if (c->bars_count == 0 || c->width <= 0 || c->height <= 0) return;
    if (c->orientation == GMB_ORIENT_HORIZONTAL) barchart_render_horizontal(c, buf, x, y);
    else barchart_render_vertical(c, buf, x, y);
}

/* ===================== Gauge ===================== */

void gmb_gauge_init(gmb_gauge_t* g, double percent)
{
    memset(g, 0, sizeof(*g));
    memset(g, 0, sizeof(*g));
    g->percent = percent < 0.0 ? 0.0 : (percent > 100.0 ? 100.0 : percent);
    g->width = 20;
}

void gmb_gauge_render(gmb_gauge_t* g, gmb_screen_buffer_t* buf, int x, int y)
{
    if (g->width <= 0) return;
    double ratio = g->percent / 100.0;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    double exact_fill = ratio * g->width;
    int full_cells = (int)exact_fill;
    int partial_eighths = (int)((exact_fill - full_cells) * 8.0 + 0.5);
    gmb_cell_color_t f = g->has_fill ? g->fill : GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t e = g->has_empty ? g->empty : GMB_CELLCOLOR_DEFAULT;
    for (int i = 0; i < g->width; i++)
    {
        if (i < full_cells) gmb_screen_setchar(buf, x + i, y, 0x2588, f, GMB_CELLCOLOR_DEFAULT);
        else if (i == full_cells && partial_eighths > 0) gmb_screen_setchar(buf, x + i, y, H_BLOCKS[partial_eighths], f, GMB_CELLCOLOR_DEFAULT);
        else gmb_screen_setchar(buf, x + i, y, 0x2591, e, GMB_CELLCOLOR_DEFAULT);
    }
    char text[64] = "";
    if (g->label[0]) snprintf(text, sizeof(text), "%s", g->label);
    else if (g->show_percent) snprintf(text, sizeof(text), "%d%%", (int)(g->percent + 0.5));
    if (text[0] && (int)strlen(text) <= g->width)
    {
        int tx = x + (g->width - (int)strlen(text)) / 2;
        gmb_screen_setstring(buf, tx, y, text, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
    }
}

/* ===================== LineChart ===================== */

void gmb_line_chart_init(gmb_line_chart_t* l)
{
    memset(l, 0, sizeof(*l));
    memset(l, 0, sizeof(*l));
    l->width = 20;
    l->height = 10;
}

void gmb_line_chart_add_series(gmb_line_chart_t* l, const char* name, const double* values, int n, bool has_color, gmb_cell_color_t color)
{
    if (l->series_count >= GMB_CHART_MAX_SERIES) return;
    gmb_series_t* s = &l->series[l->series_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "%s", name ? name : "");
    s->values_count = n > GMB_CHART_MAX_VALUES ? GMB_CHART_MAX_VALUES : n;
    for (int i = 0; i < s->values_count; i++) s->values[i] = values[i];
    s->has_color = has_color;
    s->color = color;
}

static void linechart_draw_line(gmb_line_chart_t* l, uint32_t* canvas, int cw,
                                double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    double steps = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
    if (steps < 1.0) steps = 1.0;
    dx /= steps;
    dy /= steps;
    double xx = x1, yy = y1;
    int total = (int)(steps + 0.5);
    for (int i = 0; i <= total; i++)
    {
        int px = (int)(xx + 0.5);
        int py = (int)(yy + 0.5);
        int bx = px / 2;
        int by = py / 4;
        if (bx >= 0 && bx < l->width && by >= 0 && by < l->height)
        {
            int dx2 = px % 2;
            int dy2 = py % 4;
            int bit_idx = dy2 * 2 + dx2;
            canvas[by * cw + bx] |= DOT_BITS[bit_idx];
        }
        xx += dx;
        yy += dy;
    }
}

void gmb_line_chart_render(gmb_line_chart_t* l, gmb_screen_buffer_t* buf, int x, int y)
{
    if (l->series_count == 0 || l->width <= 0 || l->height <= 0) return;
    double x_min = 1e308, x_max = -1e308, y_min = 1e308, y_max = -1e308;
    for (int s = 0; s < l->series_count; s++)
    {
        gmb_series_t* sr = &l->series[s];
        for (int i = 0; i < sr->values_count; i++)
        {
            if ((double)i < x_min) x_min = i;
            if ((double)i > x_max) x_max = i;
            double v = sr->values[i];
            if (v < y_min) y_min = v;
            if (v > y_max) y_max = v;
        }
    }
    if (x_min >= 1e308 || x_max <= -1e308) { x_min = 0; x_max = 1; y_min = 0; y_max = 1; }
    if (x_max - x_min < 1e-10) x_max = x_min + 1.0;
    if (y_max - y_min < 1e-10) y_max = y_min + 1.0;
    if (l->has_xmin) x_min = l->xmin;
    if (l->has_xmax) x_max = l->xmax;
    if (l->has_ymin) y_min = l->ymin;
    if (l->has_ymax) y_max = l->ymax;

    uint32_t canvas[64 * 64];
    memset(canvas, 0, sizeof(uint32_t) * 64 * 64);
    int cw = l->width;

    double dot_w = l->width * 2.0;
    double dot_h = l->height * 4.0;
    for (int s = 0; s < l->series_count; s++)
    {
        gmb_series_t* sr = &l->series[s];
        for (int i = 1; i < sr->values_count; i++)
        {
            double xx1 = (double)(i - 1 - x_min) / (x_max - x_min) * (dot_w - 1);
            double yy1 = (1.0 - (sr->values[i - 1] - y_min) / (y_max - y_min)) * (dot_h - 1);
            double xx2 = (double)(i - x_min) / (x_max - x_min) * (dot_w - 1);
            double yy2 = (1.0 - (sr->values[i] - y_min) / (y_max - y_min)) * (dot_h - 1);
            linechart_draw_line(l, canvas, cw, xx1, yy1, xx2, yy2);
        }
        if (sr->values_count == 1)
        {
            double cx = (0.0 - x_min) / (x_max - x_min) * (dot_w - 1);
            double cy = (1.0 - (sr->values[0] - y_min) / (y_max - y_min)) * (dot_h - 1);
            int px = (int)(cx + 0.5);
            int py = (int)(cy + 0.5);
            int bx = px / 2, by = py / 4;
            if (bx >= 0 && bx < l->width && by >= 0 && by < l->height)
                canvas[by * cw + bx] |= DOT_BITS[(py % 4) * 2 + (px % 2)];
        }
    }

    for (int by = 0; by < l->height; by++)
        for (int bx = 0; bx < l->width; bx++)
        {
            uint32_t dots = canvas[by * cw + bx];
            if (dots != 0)
            {
                uint32_t ch = 0x2800 + dots;
                gmb_screen_setchar(buf, x + bx, y + by, ch, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
            }
        }
}

/* ===================== Histogram ===================== */

void gmb_histogram_init(gmb_histogram_t* h)
{
    memset(h, 0, sizeof(*h));
    memset(h, 0, sizeof(*h));
    h->width = 20;
    h->height = 10;
}

void gmb_histogram_from_data(gmb_histogram_t* h, const double* values, int n, int bin_count)
{
    h->bins_count = 0;
    if (bin_count < 1) bin_count = 1;
    if (bin_count > GMB_CHART_MAX_BINS) bin_count = GMB_CHART_MAX_BINS;
    if (n <= 0 || !values)
    {
        for (int i = 0; i < bin_count && h->bins_count < GMB_CHART_MAX_BINS; i++)
        {
            gmb_hist_bin_t* b = &h->bins[h->bins_count++];
            b->from = i; b->to = i + 1; b->count = 0;
        }
        return;
    }
    double min = values[0], max = values[0];
    for (int i = 1; i < n; i++)
    {
        if (values[i] < min) min = values[i];
        if (values[i] > max) max = values[i];
    }
    double span = fabs(max - min) < 1e-10 ? 1.0 : max - min;
    int counts[GMB_CHART_MAX_BINS];
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < n; i++)
    {
        int idx = (int)((values[i] - min) / span * bin_count);
        if (idx >= bin_count) idx = bin_count - 1;
        if (idx < 0) idx = 0;
        counts[idx]++;
    }
    for (int i = 0; i < bin_count && h->bins_count < GMB_CHART_MAX_BINS; i++)
    {
        gmb_hist_bin_t* b = &h->bins[h->bins_count++];
        b->from = min + span * i / bin_count;
        b->to = min + span * (i + 1) / bin_count;
        b->count = counts[i];
    }
}

void gmb_histogram_render(gmb_histogram_t* h, gmb_screen_buffer_t* buf, int x, int y)
{
    if (h->bins_count == 0 || h->width <= 0 || h->height <= 0) return;
    int max_count = 0;
    for (int i = 0; i < h->bins_count; i++)
        if (h->bins[i].count > max_count) max_count = h->bins[i].count;
    if (max_count == 0) max_count = 1;
    int label_row = 1;
    int chart_h = h->height - label_row;
    if (chart_h <= 0) return;
    int bottom_y = y + chart_h - 1;
    int bar_w = (h->width - (h->bins_count - 1)) / h->bins_count;
    if (bar_w < 1) bar_w = 1;
    int gap = h->bins_count > 1 ? (h->width - bar_w * h->bins_count) / (h->bins_count - 1) : 0;
    if (gap < 0) gap = 0;
    int step = bar_w + gap;
    int n = h->bins_count < (h->width / (step > 0 ? step : 1)) ? h->bins_count : (h->width / (step > 0 ? step : 1));
    for (int i = 0; i < n; i++)
    {
        gmb_hist_bin_t* bin = &h->bins[i];
        int bx = x + i * step;
        double ratio = (double)bin->count / max_count;
        for (int cc = 0; cc < bar_w; cc++)
        {
            if (bx + cc >= x + h->width) break;
            gmb_chartutil_draw_vbar(buf, bx + cc, bottom_y, chart_h, ratio, h->has_color ? &h->color : NULL);
        }
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "%d", bin->count);
        chart_truncate(lbl, bar_w, lbl, sizeof(lbl));
        gmb_screen_setstring(buf, bx, bottom_y + 1, lbl, GMB_CELLCOLOR_DEFAULT, GMB_CELLCOLOR_DEFAULT);
    }
}
