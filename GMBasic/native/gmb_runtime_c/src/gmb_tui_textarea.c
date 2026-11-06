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
    gmb_text_area_t* t = (gmb_text_area_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int idx = t->scroll_offset + i;
            if (idx >= t->lines_count) break;
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
        int idx = t->scroll_offset + i;
        if (idx >= t->lines_count) break;
        char line[256];
        gmb_text_area_render_line(t, idx, line, (int)sizeof(line));
        char cased[160];
        gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
        gmb_screen_setstring(buf, c->x, c->y + i, cased, fg, bg);
    }
    if (c->focused && t->blink_on)
    {
        int vis_y = t->cursor_line - t->scroll_offset;
        if (vis_y >= 0 && vis_y < c->height && vis_y < t->lines_count)
        {
            int cl = (int)strlen(t->lines[t->cursor_line]);
            uint32_t ch = t->cursor_col < cl ? (unsigned char)t->lines[t->cursor_line][t->cursor_col] : ' ';
            gmb_screen_setchar(buf, c->x + t->cursor_col, c->y + vis_y, ch, bg, fg);
        }
    }
}
static bool textarea_key(void* u, gmb_control_t* c, int key)
{
    gmb_text_area_t* t = (gmb_text_area_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (t->cursor_line > 0) t->cursor_line--;
            {
                int cl = (int)strlen(t->lines[t->cursor_line]);
                if (t->cursor_col > cl) t->cursor_col = cl;
            }
            if (t->cursor_line < t->scroll_offset) t->scroll_offset--;
            return true;
        case GMB_K_DOWN:
            if (t->cursor_line < t->lines_count - 1) t->cursor_line++;
            {
                int cl = (int)strlen(t->lines[t->cursor_line]);
                if (t->cursor_col > cl) t->cursor_col = cl;
            }
            if (t->cursor_line >= t->scroll_offset + c->height) t->scroll_offset++;
            return true;
        case GMB_K_LEFT:
            if (t->cursor_col > 0) t->cursor_col--;
            return true;
        case GMB_K_RIGHT:
            if (t->cursor_col < (int)strlen(t->lines[t->cursor_line])) t->cursor_col++;
            return true;
        case GMB_K_ENTER:
            t->modified = true;
            if (t->lines_count < 64)
            {
                int cl = (int)strlen(t->lines[t->cursor_line]);
                for (int i = t->lines_count; i > t->cursor_line + 1; i--)
                    snprintf(t->lines[i], 256, "%s", t->lines[i - 1]);
                snprintf(t->lines[t->cursor_line + 1], 256, "%s", t->lines[t->cursor_line] + t->cursor_col);
                t->lines[t->cursor_line][t->cursor_col] = '\0';
                t->lines_count++;
                t->cursor_line++;
                t->cursor_col = 0;
            }
            return true;
        case GMB_K_BS:
            if (t->cursor_col > 0)
            {
                t->modified = true;
                memmove(t->lines[t->cursor_line] + t->cursor_col - 1, t->lines[t->cursor_line] + t->cursor_col,
                        strlen(t->lines[t->cursor_line] + t->cursor_col) + 1);
                t->cursor_col--;
            }
            else if (t->cursor_line > 0)
            {
                t->modified = true;
                int prev_len = (int)strlen(t->lines[t->cursor_line - 1]);
                strncat(t->lines[t->cursor_line - 1], t->lines[t->cursor_line], 255 - prev_len);
                for (int i = t->cursor_line; i < t->lines_count - 1; i++)
                    snprintf(t->lines[i], 256, "%s", t->lines[i + 1]);
                t->lines_count--;
                t->cursor_line--;
                t->cursor_col = prev_len;
            }
            return true;
        case GMB_K_HOME: t->cursor_col = 0; return true;
        case GMB_K_END: t->cursor_col = (int)strlen(t->lines[t->cursor_line]); return true;
        default:
            if (key >= 32 && key <= 126)
            {
                t->modified = true;
                int cl = (int)strlen(t->lines[t->cursor_line]);
                if (cl < 255)
                {
                    memmove(t->lines[t->cursor_line] + t->cursor_col + 1, t->lines[t->cursor_line] + t->cursor_col,
                            (size_t)(cl - t->cursor_col) + 1);
                    t->lines[t->cursor_line][t->cursor_col] = (char)key;
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
    gmb_text_area_t* t = (gmb_text_area_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = (y - c->y) + t->scroll_offset;
    if (idx >= 0 && idx < t->lines_count)
    {
        t->cursor_line = idx;
        int cl = (int)strlen(t->lines[idx]);
        t->cursor_col = x - c->x;
        if (t->cursor_col < 0) t->cursor_col = 0;
        if (t->cursor_col > cl) t->cursor_col = cl;
    }
    return true;
}
void gmb_text_area_init(gmb_text_area_t* t, int x, int y, int w, int h)
{
    memset(t, 0, sizeof(*t));
    gmb_control_init(&t->ctrl, "", x, y, w, h, textarea_draw, textarea_key);
    t->ctrl.handle_click_fn = textarea_click;
    t->ctrl.user = t;
    snprintf(t->lines[0], 256, "");
    t->lines_count = 1;
    t->blink_on = true;
}
void gmb_text_area_set_lines(gmb_text_area_t* t, const char* const* lines, int count)
{
    t->lines_count = count < 64 ? count : 64;
    for (int i = 0; i < t->lines_count && lines; i++)
        snprintf(t->lines[i], sizeof(t->lines[i]), "%s", lines[i] ? lines[i] : "");
    if (t->lines_count == 0) { snprintf(t->lines[0], 256, ""); t->lines_count = 1; }
    t->cursor_line = t->cursor_col = 0;
}
void gmb_text_area_scroll_up(gmb_text_area_t* t, int lines)
{
    t->scroll_offset -= lines;
    if (t->scroll_offset < 0) t->scroll_offset = 0;
}
void gmb_text_area_scroll_down(gmb_text_area_t* t, int lines)
{
    int max_off = t->lines_count - t->ctrl.height;
    if (max_off < 0) max_off = 0;
    t->scroll_offset += lines;
    if (t->scroll_offset > max_off) t->scroll_offset = max_off;
}
int gmb_text_area_render_line(gmb_text_area_t* t, int idx, char* out, int cap)
{
    int i = t->scroll_offset + idx;
    if (i < 0 || i >= t->lines_count)
    {
        for (int k = 0; k < t->ctrl.width && k < cap - 1; k++) out[k] = ' ';
        out[t->ctrl.width < cap - 1 ? t->ctrl.width : cap - 1] = '\0';
        return t->ctrl.width;
    }
    int o = snprintf(out, cap, "%.*s", t->ctrl.width, t->lines[i]);
    if (o > t->ctrl.width) o = t->ctrl.width;
    else for (int k = o; k < t->ctrl.width && k < cap - 1; k++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
