/* ============================================================================
 * gmb_tui_charts.h — Charts (specchio ClipperLib/Controls/Charts.cs):
 * Sparkline, ChartBar/BarChart, Gauge, Series/LineChart, Histogram. Puro.
 * ============================================================================ */
#ifndef GMB_TUI_CHARTS_H
#define GMB_TUI_CHARTS_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_CHART_MAX_BARS 64
#define GMB_CHART_MAX_SERIES 8
#define GMB_CHART_MAX_VALUES 512
#define GMB_CHART_MAX_BINS 64

enum { GMB_ORIENT_HORIZONTAL = 0, GMB_ORIENT_VERTICAL };

/* ---- Sparkline ---- */
int gmb_sparkline_render(const int* values, int n, int width, bool has_min, int min, bool has_max, int max, char* out, int cap);
void gmb_sparkline_render_to_buffer(gmb_screen_buffer_t* buf, int x, int y, const int* values, int n, int width);

/* ---- ChartBar / BarChart ---- */
typedef struct gmb_chart_bar {
    char label[64];
    int value;
    bool has_color;
    gmb_cell_color_t color;
} gmb_chart_bar_t;

typedef struct gmb_bar_chart {
    gmb_chart_bar_t bars[GMB_CHART_MAX_BARS];
    int bars_count;
    int orientation;
    int width, height;
    bool has_max;
    int max;
    bool show_values;
    int bar_width, bar_gap;
    bool has_default_color;
    gmb_cell_color_t default_color;
} gmb_bar_chart_t;

void gmb_bar_chart_init(gmb_bar_chart_t* c);
void gmb_bar_chart_add(gmb_bar_chart_t* c, const char* label, int value, bool has_color, gmb_cell_color_t color);
void gmb_bar_chart_render(gmb_bar_chart_t* c, gmb_screen_buffer_t* buf, int x, int y);

/* ---- Gauge ---- */
typedef struct gmb_gauge {
    double percent;
    char label[64];
    bool show_percent;
    int width;
    bool has_fill, has_empty;
    gmb_cell_color_t fill, empty;
} gmb_gauge_t;

void gmb_gauge_init(gmb_gauge_t* g, double percent);
void gmb_gauge_render(gmb_gauge_t* g, gmb_screen_buffer_t* buf, int x, int y);

/* ---- Series / LineChart ---- */
typedef struct gmb_series {
    char name[64];
    double values[GMB_CHART_MAX_VALUES];
    int values_count;
    bool has_color;
    gmb_cell_color_t color;
} gmb_series_t;

typedef struct gmb_line_chart {
    gmb_series_t series[GMB_CHART_MAX_SERIES];
    int series_count;
    bool has_xmin, has_xmax, has_ymin, has_ymax;
    double xmin, xmax, ymin, ymax;
    int width, height;
} gmb_line_chart_t;

void gmb_line_chart_init(gmb_line_chart_t* l);
void gmb_line_chart_add_series(gmb_line_chart_t* l, const char* name, const double* values, int n, bool has_color, gmb_cell_color_t color);
void gmb_line_chart_render(gmb_line_chart_t* l, gmb_screen_buffer_t* buf, int x, int y);

/* ---- Histogram ---- */
typedef struct gmb_hist_bin {
    double from, to;
    int count;
} gmb_hist_bin_t;

typedef struct gmb_histogram {
    gmb_hist_bin_t bins[GMB_CHART_MAX_BINS];
    int bins_count;
    int width, height;
    bool has_color;
    gmb_cell_color_t color;
} gmb_histogram_t;

void gmb_histogram_init(gmb_histogram_t* h);
void gmb_histogram_from_data(gmb_histogram_t* h, const double* values, int n, int bin_count);
void gmb_histogram_render(gmb_histogram_t* h, gmb_screen_buffer_t* buf, int x, int y);

/* ---- ChartUtil helpers (pubblici per test) ---- */
void gmb_chartutil_draw_hbar(gmb_screen_buffer_t* buf, int x, int y, int width, double ratio, const gmb_cell_color_t* color);
void gmb_chartutil_draw_vbar(gmb_screen_buffer_t* buf, int x, int bottom_y, int height, double ratio, const gmb_cell_color_t* color);
int  gmb_chartutil_format_value(double v, char* out, int cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_CHARTS_H */
