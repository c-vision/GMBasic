/* ============================================================================
 * gmb_tui_logview.c — gmb_tui_logview. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== LogView ===================== */

static const char* log_level_label(int l)
{
    switch (l)
    {
        case GMB_LOG_TRACE: return "TRACE";
        case GMB_LOG_DEBUG: return "DEBUG";
        case GMB_LOG_INFO: return "INFO ";
        case GMB_LOG_WARN: return "WARN ";
        case GMB_LOG_ERROR: return "ERROR";
        default: return "?????";
    }
}
static void logview_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_log_view_t* l = (gmb_log_view_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        int r2 = 0;
        for (int i = l->offset; i < l->records_count && r2 < c->height; i++, r2++)
        {
            if (l->records[i].level < l->min_level) continue;
            gmb_cell_color_t lc2 = fg;
            switch (l->records[i].level)
            {
                case GMB_LOG_ERROR: lc2 = GMB_CELLCOLOR_RED; break;
                case GMB_LOG_WARN: lc2 = GMB_CELLCOLOR_YELLOW; break;
                case GMB_LOG_INFO: lc2 = GMB_CELLCOLOR_GREEN; break;
                default: lc2 = GMB_CELLCOLOR_GREY; break;
            }
            char ln[256];
            snprintf(ln, sizeof(ln), "[%s] %s: %s", log_level_label(l->records[i].level), l->records[i].target, l->records[i].message);
            gmb_gfx_draw_text(ln, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + r2) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(lc2).r << 16) | ((unsigned)(lc2).g << 8) | (lc2).b));
        }
        return;
    }
    int row = 0;
    for (int i = l->offset; i < l->records_count && row < c->height; i++, row++)
    {
        if (l->records[i].level < l->min_level) continue;
        gmb_cell_color_t lc = fg;
        switch (l->records[i].level)
        {
            case GMB_LOG_ERROR: lc = GMB_CELLCOLOR_RED; break;
            case GMB_LOG_WARN: lc = GMB_CELLCOLOR_YELLOW; break;
            case GMB_LOG_INFO: lc = GMB_CELLCOLOR_GREEN; break;
            default: lc = GMB_CELLCOLOR_GREY; break;
        }
        char line[256];
        snprintf(line, sizeof(line), "[%s] %s: %s", log_level_label(l->records[i].level),
                 l->records[i].target, l->records[i].message);
        int ll = (int)strlen(line);
        if (ll > c->width) line[c->width] = '\0';
        else for (int k = ll; k < c->width && k < 255; k++) line[k] = ' ';
        if (ll < c->width) line[c->width < 255 ? c->width : 255] = '\0';
        gmb_screen_setstring(buf, c->x, c->y + row, line, lc, bg);
    }
}
static bool logview_key(void* u, gmb_control_t* c, int key)
{
    gmb_log_view_t* l = (gmb_log_view_t*)u;
    if (!c->enabled) return false;
    int max_off = gmb_log_view_filtered_count(l) - c->height;
    if (max_off < 0) max_off = 0;
    switch (key)
    {
        case GMB_K_UP: if (l->offset > 0) l->offset--; return true;
        case GMB_K_DOWN: if (l->offset < max_off) l->offset++; return true;
        case GMB_K_PGUP: l->offset -= c->height; if (l->offset < 0) l->offset = 0; return true;
        case GMB_K_PGDN: l->offset += c->height; if (l->offset > max_off) l->offset = max_off; return true;
        case GMB_K_HOME: l->offset = 0; return true;
        case GMB_K_END: l->offset = max_off; return true;
    }
    return false;
}
static bool logview_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_log_view_t* l = (gmb_log_view_t*)u;
    if (!c->enabled) return false;
    int max_off = gmb_log_view_filtered_count(l) - c->height;
    if (max_off < 0) max_off = 0;
    if (a->kind == GMB_MA_SCROLLUP) { if (l->offset > 0) l->offset--; return true; }
    if (a->kind == GMB_MA_SCROLLDOWN) { if (l->offset < max_off) l->offset++; return true; }
    return false;
}
static void logview_draw_px(void* u, gmb_control_t* c)
{
    gmb_log_view_t* l = (gmb_log_view_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    static const unsigned lvcol[5] = { 0x888888u, 0x44AAFFu, 0x44CC66u, 0xFFAA33u, 0xFF4444u };
    static const char* lvn[5] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    if (w < 8 || h < 8) return;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    int lrh = (st.row_h > 0 ? st.row_h : 22);
    int pad = soft ? 4 : 0;
    int vis = lrh > 0 ? (h - pad * 2) / lrh : 0;
    int start = l->offset; if (start < 0) start = 0;
    for (int r = 0; r < vis && start + r < l->records_count; r++) {
        gmb_log_record_t* rec = &l->records[start + r];
        int li = rec->level; if (li < 0 || li > 4) li = 2;
        int ry = y + pad + r * lrh;
        gmb_gfx_draw_text(lvn[li], x + (soft ? 12 : 4), ry + (lrh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                          (int)(0x40000000u | lvcol[li]));
        gmb_gfx_draw_text(rec->message, x + (soft ? 64 : 56), ry + 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    }
}
void gmb_log_view_init(gmb_log_view_t* l, int x, int y, int w, int h)
{
    memset(l, 0, sizeof(*l));
    gmb_control_init(&l->ctrl, "", x, y, w, h, logview_draw, logview_key);
    l->ctrl.ctrl_kind = GMB_CK_LOGVIEW;
    l->ctrl.draw_px_fn = logview_draw_px;
    l->ctrl.handle_mouse_fn = logview_mouse;
    l->ctrl.user = l;
    l->capacity = 1000;
    l->min_level = GMB_LOG_TRACE;
}
void gmb_log_view_push(gmb_log_view_t* l, int level, const char* target, const char* message)
{
    if (l->records_count < 256)
    {
        gmb_log_record_t* r = &l->records[l->records_count++];
        r->level = level;
        snprintf(r->target, sizeof(r->target), "%s", target ? target : "");
        snprintf(r->message, sizeof(r->message), "%s", message ? message : "");
    }
    else
    {
        for (int i = 0; i < l->records_count - 1; i++) l->records[i] = l->records[i + 1];
        l->records[l->records_count - 1].level = level;
        snprintf(l->records[l->records_count - 1].target, sizeof(l->records[l->records_count - 1].target), "%s", target ? target : "");
        snprintf(l->records[l->records_count - 1].message, sizeof(l->records[l->records_count - 1].message), "%s", message ? message : "");
    }
}
int gmb_log_view_filtered_count(gmb_log_view_t* l)
{
    int n = 0;
    for (int i = 0; i < l->records_count; i++)
        if (l->records[i].level >= l->min_level) n++;
    return n;
}
int gmb_log_view_render_line(gmb_log_view_t* l, int idx, char* out, int cap)
{
    int seen = 0;
    for (int i = 0; i < l->records_count; i++)
    {
        if (l->records[i].level < l->min_level) continue;
        if (seen == idx)
        {
            snprintf(out, cap, "[%s] %s: %s", log_level_label(l->records[i].level),
                     l->records[i].target, l->records[i].message);
            int ll = (int)strlen(out);
            if (ll > l->ctrl.width) out[l->ctrl.width] = '\0';
            else for (int k = ll; k < l->ctrl.width && k < cap - 1; k++) out[k] = ' ';
            if (ll < l->ctrl.width) out[l->ctrl.width < cap - 1 ? l->ctrl.width : cap - 1] = '\0';
            return (int)strlen(out);
        }
        seen++;
    }
    for (int i = 0; i < l->ctrl.width && i < cap - 1; i++) out[i] = ' ';
    out[l->ctrl.width < cap - 1 ? l->ctrl.width : cap - 1] = '\0';
    return l->ctrl.width;
}
double gmb_log_view_scroll_fraction(gmb_log_view_t* l)
{
    int f = gmb_log_view_filtered_count(l);
    int denom = f - l->ctrl.height;
    return denom > 0 ? (double)l->offset / denom : 0.0;
}
