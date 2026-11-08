/* ============================================================================
 * gmb_tui_textarea.c — gmb_tui_textarea. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== TextArea ===================== */

static void textarea_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = t->first + i;
            if (idx >= t->nitems) break;
            char line[256], cased[160];
            gmb_text_area_render_line(t, idx, line, (int)sizeof(line));
            gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
            gmb_gfx_draw_text(cased, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int idx = t->first + i;
        if (idx >= t->nitems) break;
        char line[256];
        gmb_text_area_render_line(t, idx, line, (int)sizeof(line));
        char cased[160];
        gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
        gmb_screen_setstring(buf, c->x, c->y + i, cased, fg, bg);
    }
    if (c->focused && t->blink_on)
    {
        int vis_y = t->cursor_row - t->first;
        if (vis_y >= 0 && vis_y < c->height && vis_y < t->nitems)
        {
            int cl = (int)strlen(t->items[t->cursor_row]);
            uint32_t ch = t->cursor_col < cl ? (unsigned char)t->items[t->cursor_row][t->cursor_col] : ' ';
            gmb_screen_setchar(buf, c->x + t->cursor_col, c->y + vis_y, ch, bg, fg);
        }
    }
}
static bool textarea_key(void* u, gmb_control_t* c, int key)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (t->cursor_row > 0) t->cursor_row--;
            {
                int cl = (int)strlen(t->items[t->cursor_row]);
                if (t->cursor_col > cl) t->cursor_col = cl;
            }
            if (t->cursor_row < t->first) t->first--;
            return true;
        case GMB_K_DOWN:
            if (t->cursor_row < t->nitems - 1) t->cursor_row++;
            {
                int cl = (int)strlen(t->items[t->cursor_row]);
                if (t->cursor_col > cl) t->cursor_col = cl;
            }
            if (t->cursor_row >= t->first + c->height) t->first++;
            return true;
        case GMB_K_LEFT:
            if (t->cursor_col > 0) t->cursor_col--;
            return true;
        case GMB_K_RIGHT:
            if (t->cursor_col < (int)strlen(t->items[t->cursor_row])) t->cursor_col++;
            return true;
        case GMB_K_ENTER:
            t->modified = true;
            if (t->nitems < 64)
            {
                int cl = (int)strlen(t->items[t->cursor_row]);
                for (int i = t->nitems; i > t->cursor_row + 1; i--)
                    snprintf(t->items[i], 256, "%s", t->items[i - 1]);
                snprintf(t->items[t->cursor_row + 1], 256, "%s", t->items[t->cursor_row] + t->cursor_col);
                t->items[t->cursor_row][t->cursor_col] = '\0';
                t->nitems++;
                t->cursor_row++;
                t->cursor_col = 0;
            }
            return true;
        case GMB_K_BS:
            if (t->cursor_col > 0)
            {
                t->modified = true;
                memmove(t->items[t->cursor_row] + t->cursor_col - 1, t->items[t->cursor_row] + t->cursor_col,
                        strlen(t->items[t->cursor_row] + t->cursor_col) + 1);
                t->cursor_col--;
            }
            else if (t->cursor_row > 0)
            {
                t->modified = true;
                int prev_len = (int)strlen(t->items[t->cursor_row - 1]);
                strncat(t->items[t->cursor_row - 1], t->items[t->cursor_row], 255 - prev_len);
                for (int i = t->cursor_row; i < t->nitems - 1; i++)
                    snprintf(t->items[i], 256, "%s", t->items[i + 1]);
                t->nitems--;
                t->cursor_row--;
                t->cursor_col = prev_len;
            }
            return true;
        case GMB_K_HOME: t->cursor_col = 0; return true;
        case GMB_K_END: t->cursor_col = (int)strlen(t->items[t->cursor_row]); return true;
        default:
            if (key >= 32 && key <= 126)
            {
                t->modified = true;
                int cl = (int)strlen(t->items[t->cursor_row]);
                if (cl < 255)
                {
                    memmove(t->items[t->cursor_row] + t->cursor_col + 1, t->items[t->cursor_row] + t->cursor_col,
                            (size_t)(cl - t->cursor_col) + 1);
                    t->items[t->cursor_row][t->cursor_col] = (char)key;
                    t->cursor_col++;
                }
                return true;
            }
            break;
    }
    return false;
}
static bool textarea_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_widget_t* t = (gmb_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = (y - c->y) + t->first;
    if (idx >= 0 && idx < t->nitems)
    {
        t->cursor_row = idx;
        int cl = (int)strlen(t->items[idx]);
        t->cursor_col = x - c->x;
        if (t->cursor_col < 0) t->cursor_col = 0;
        if (t->cursor_col > cl) t->cursor_col = cl;
    }
    return true;
}
static void textarea_draw_px(void* u, gmb_control_t* c)
{
    /* Editor multi-riga in pixel: card del tema + testo monospazio + caret. */
    gmb_widget_t* t = (gmb_widget_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (w < 16 || h < 16) return;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    int rh = (st.row_h > 0 ? st.row_h : 22);
    if (rh < 12) rh = 12;
    int pad = soft ? 8 : 4;
    float sc = gmb_gfx_render_scale(); if (sc <= 0) sc = 1;
    int th = (int)(gmb_gfx_text_height() / sc);
    int vis = rh > 0 ? (h - pad * 2) / rh : 0;
    if (vis < 1) vis = 1;
    if (t->cursor_row < t->first) t->first = t->cursor_row;
    if (t->cursor_row >= t->first + vis) t->first = t->cursor_row - vis + 1;
    if (t->first < 0) t->first = 0;
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    for (int r = 0; r < vis; r++)
    {
        int ln = t->first + r;
        if (ln >= t->nitems) break;
        int ry = y + pad + r * rh;
        gmb_gfx_draw_text(t->items[ln], x + pad, ry + (rh - th) / 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
        if (ln == t->cursor_row && c->focused)
        {
            int cw = (int)(gmb_gfx_text_width("M") / sc); if (cw < 2) cw = 8;
            int cx = x + pad + t->cursor_col * cw;
            if (cx + 2 > x + w - pad) cx = x + w - pad - 2;
            gmb_gfx_fill_rounded_rect(cx, ry + 3, 2, rh - 6, 1, (acc & 0xFFFFFFu) | 0x40000000u);
        }
    }
}

void gmb_text_area_init(gmb_text_area_t* ta, int x, int y, int w, int h)
{
    gmb_widget_t* t = (gmb_widget_t*)ta;
    memset(t, 0, sizeof(*t));
    gmb_control_init(&t->base, "", x, y, w, h, textarea_draw, textarea_key);
    t->base.handle_click_fn = textarea_click;
    t->base.draw_px_fn = textarea_draw_px;   /* grafica: card + testo, no fallback a celle */
    t->base.user = t;
    t->kind = W_TEXT;
    snprintf(t->items[0], 256, "");
    t->nitems = 1;
    t->blink_on = 1;
}
void gmb_text_area_set_lines(gmb_text_area_t* ta, const char* const* lines, int count)
{
    gmb_widget_t* t = (gmb_widget_t*)ta;
    t->nitems = count < 64 ? count : 64;
    for (int i = 0; i < t->nitems && lines; i++)
        snprintf(t->items[i], sizeof(t->items[i]), "%s", lines[i] ? lines[i] : "");
    if (t->nitems == 0) { snprintf(t->items[0], 256, ""); t->nitems = 1; }
    t->cursor_row = t->cursor_col = 0;
}
void gmb_text_area_scroll_up(gmb_text_area_t* ta, int lines)
{
    gmb_widget_t* t = (gmb_widget_t*)ta;
    t->first -= lines;
    if (t->first < 0) t->first = 0;
}
void gmb_text_area_scroll_down(gmb_text_area_t* ta, int lines)
{
    gmb_widget_t* t = (gmb_widget_t*)ta;
    int max_off = t->nitems - t->base.height;
    if (max_off < 0) max_off = 0;
    t->first += lines;
    if (t->first > max_off) t->first = max_off;
}
int gmb_text_area_render_line(gmb_text_area_t* ta, int idx, char* out, int cap)
{
    gmb_widget_t* t = (gmb_widget_t*)ta;
    int i = t->first + idx;
    if (i < 0 || i >= t->nitems)
    {
        for (int k = 0; k < t->base.width && k < cap - 1; k++) out[k] = ' ';
        out[t->base.width < cap - 1 ? t->base.width : cap - 1] = '\0';
        return t->base.width;
    }
    int o = snprintf(out, cap, "%.*s", t->base.width, t->items[i]);
    if (o > t->base.width) o = t->base.width;
    else for (int k = o; k < t->base.width && k < cap - 1; k++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
