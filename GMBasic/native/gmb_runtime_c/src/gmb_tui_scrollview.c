/* ============================================================================
 * gmb_tui_scrollview.c — ScrollView.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ScrollView ===================== */

static void scrollview_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_scroll_view_t* s = (gmb_scroll_view_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = s->offset + i;
            if (idx >= s->content_count) break;
            char line[256];
            gmb_scroll_view_render_line(s, idx, line, (int)sizeof(line));
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = s->offset + i;
        if (idx >= s->content_count) break;
        char line[256];
        gmb_scroll_view_render_line(s, idx, line, (int)sizeof(line));
        gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool scrollview_key(void* u, gmb_control_t* c, int key)
{
    gmb_scroll_view_t* s = (gmb_scroll_view_t*)u;
    if (!c->enabled) return false;
    int max_offset = s->content_count - c->height;
    if (max_offset < 0) max_offset = 0;
    switch (key)
    {
        case GMB_K_UP:
            if (s->offset > 0) s->offset--;
            return true;
        case GMB_K_DOWN:
            if (s->offset < max_offset) s->offset++;
            return true;
        case GMB_K_PGUP:
            s->offset -= c->height;
            if (s->offset < 0) s->offset = 0;
            return true;
        case GMB_K_PGDN:
            s->offset += c->height;
            if (s->offset > max_offset) s->offset = max_offset;
            return true;
        case GMB_K_HOME: s->offset = 0; return true;
        case GMB_K_END: s->offset = max_offset; return true;
    }
    return false;
}
static bool scrollview_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_scroll_view_t* s = (gmb_scroll_view_t*)u;
    if (!c->enabled) return false;
    int max_offset = s->content_count - c->height;
    if (max_offset < 0) max_offset = 0;
    if (a->kind == GMB_MA_SCROLLUP) { if (s->offset > 0) s->offset--; return true; }
    if (a->kind == GMB_MA_SCROLLDOWN) { if (s->offset < max_offset) s->offset++; return true; }
    return false;
}
static void scrollview_draw_px(void* u, gmb_control_t* c)
{
    gmb_scroll_view_t* s = (gmb_scroll_view_t*)u;
    const char* its[64];
    int n = s->content_count; if (n > 64) n = 64;
    for (int i = 0; i < n; i++) its[i] = s->content[i];
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, its, n, 0, 0, n, NULL, s->offset, NULL, 0);
}
void gmb_scroll_view_init(gmb_scroll_view_t* s, int x, int y, int w, int h)
{
    gmb_control_init(&s->ctrl, "", x, y, w, h, scrollview_draw, scrollview_key);
    s->ctrl.draw_px_fn = scrollview_draw_px;
    s->ctrl.handle_mouse_fn = scrollview_mouse;
    s->ctrl.user = s;
}
void gmb_scroll_view_set_content(gmb_scroll_view_t* s, const char* const* lines, int count)
{
    s->content_count = count < 64 ? count : 64;
    for (int i = 0; i < s->content_count && lines; i++)
        snprintf(s->content[i], sizeof(s->content[i]), "%s", lines[i] ? lines[i] : "");
}
void gmb_scroll_view_push(gmb_scroll_view_t* s, const char* line)
{
    if (s->content_count >= 64) return;
    snprintf(s->content[s->content_count++], sizeof(s->content[0]), "%s", line ? line : "");
    int max_off = s->content_count - s->ctrl.height;
    if (max_off < 0) max_off = 0;
    if (s->offset > max_off) s->offset = max_off;
}
int gmb_scroll_view_render_line(gmb_scroll_view_t* s, int idx, char* out, int cap)
{
    if (idx < 0 || idx >= s->content_count)
    {
        for (int i = 0; i < s->ctrl.width && i < cap - 1; i++) out[i] = ' ';
        out[s->ctrl.width < cap - 1 ? s->ctrl.width : cap - 1] = '\0';
        return s->ctrl.width;
    }
    int o = snprintf(out, cap, "%.*s", s->ctrl.width, s->content[idx]);
    if (o > s->ctrl.width) o = s->ctrl.width;
    else for (int i = o; i < s->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
double gmb_scroll_view_scroll_fraction(gmb_scroll_view_t* s)
{
    int denom = s->content_count - s->ctrl.height;
    return denom > 0 ? (double)s->offset / denom : 0.0;
}

/* ===================== Form widget ===================== */
