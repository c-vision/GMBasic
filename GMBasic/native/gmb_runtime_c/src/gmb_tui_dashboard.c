/* ============================================================================
 * gmb_tui_dashboard.c — Dashboard (porting del runtime C, specchio ClipperLib).
 *
 * MODELLO UNICO: dashboard = gmb_widget_t. Etichette in `metrics`, valori in
 * `metric_v`, massimi in `metric_m`, unita' in `metric_u`, scelta in `ival`.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Dashboard ===================== */
#define DB_SEL(d) ((int)((gmb_widget_t*)(d))->ival)

static void dashboard_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height && i < d->metrics_count; i++)
        {
            char line[256];
            gmb_dashboard_render_line(d, i, line, (int)sizeof(line));
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height && i < d->metrics_count; i++)
    {
        char line[256];
        gmb_dashboard_render_line(d, i, line, (int)sizeof(line));
        gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool dashboard_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_widget_t* d = (gmb_widget_t*)u;
    if (key == GMB_K_DOWN) { if (DB_SEL(d) < d->metrics_count - 1) d->ival = DB_SEL(d) + 1; return true; }
    if (key == GMB_K_UP) { if (DB_SEL(d) > 0) d->ival = DB_SEL(d) - 1; return true; }
    return false;
}
static void dashboard_draw_px(void* u, gmb_control_t* c)
{
    gmb_widget_t* d = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    gmb_gfx_fill_rounded_rect(x, y, w, h, 6, st.fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, 6, st.border);
    int n = d->metrics_count; if (n > 16) n = 16; if (n <= 0) return;
    int rh = h / n; if (rh < 10) rh = 10;
    for (int i = 0; i < n; i++) {
        const char* lab_src = d->metrics[i];
        float mv = d->metric_v[i], mm = d->metric_m[i];
        const char* mu = d->metric_u[i];
        int ry = y + i * rh;
        char lab[96]; snprintf(lab, sizeof(lab), "%s  %.1f %s", lab_src, (double)mv, mu);
        gmb_gfx_draw_text(lab, x + 6, ry + 2, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
        double t = (mm > 0.0f) ? ((double)mv / (double)mm) : 0.0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        int barw = w - 12; if (barw < 4) barw = 4;
        int by = ry + rh - 9;
        gmb_gfx_fill_rounded_rect(x + 6, by, barw, 5, 2, (int)(0x40000000u | (st.border & 0xFFFFFFu)));
        int fw = (int)(barw * t);
        if (fw > 0) gmb_gfx_fill_rounded_rect(x + 6, by, fw, 5, 2, (int)(0x40000000u | (((i == DB_SEL(d) ? st.fill_hover : st.accent)) & 0xFFFFFFu)));
    }
}
void gmb_dashboard_init(gmb_dashboard_t* db, int x, int y, int w, int h)
{
    gmb_widget_t* d = (gmb_widget_t*)db;
    memset(d, 0, sizeof(*d));
    gmb_control_init(&d->base, "", x, y, w, h, dashboard_draw, dashboard_key);
    d->base.draw_px_fn = dashboard_draw_px;
    d->base.user = d;
    d->kind = W_LISTBOX;
    d->bar_width = 12;
}
int gmb_dashboard_add_metric(gmb_dashboard_t* db, const char* label, float value, float max, const char* unit)
{
    gmb_widget_t* d = (gmb_widget_t*)db;
    if (d->metrics_count >= 16) return -1;
    int i = d->metrics_count++;
    snprintf(d->metrics[i], sizeof(d->metrics[i]), "%s", label ? label : "");
    d->metric_v[i] = value;
    d->metric_m[i] = max > 0 ? max : 1;
    snprintf(d->metric_u[i], sizeof(d->metric_u[i]), "%s", unit ? unit : "");
    return i;
}
int gmb_dashboard_render_line(gmb_dashboard_t* db, int idx, char* out, int cap)
{
    gmb_widget_t* d = (gmb_widget_t*)db;
    if (idx < 0 || idx >= d->metrics_count)
    {
        out[0] = '\0';
        return 0;
    }
    const char* label = d->metrics[idx];
    float value = d->metric_v[idx], max = d->metric_m[idx];
    const char* unit = d->metric_u[idx];
    int filled = (int)(value / max * d->bar_width);
    if (filled > d->bar_width) filled = d->bar_width;
    if (filled < 0) filled = 0;
    int o = 0;
    int lbl = (int)strlen(label);
    for (int i = 0; i < lbl && o < cap - 1; i++) out[o++] = label[i];
    if (o < cap - 1) out[o++] = ' ';
    if (o < cap - 1) out[o++] = '[';
    for (int i = 0; i < d->bar_width && o < cap - 1; i++) out[o++] = (i < filled) ? '#' : ' ';
    if (o < cap - 1) out[o++] = ']';
    if (o < cap - 1) out[o++] = ' ';
    o += snprintf(out + o, (size_t)(cap - o), "%.0f%s", value, unit);
    out[o] = '\0';
    return o;
}
