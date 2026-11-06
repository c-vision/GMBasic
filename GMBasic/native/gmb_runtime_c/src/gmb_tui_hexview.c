/* ============================================================================
 * gmb_tui_hexview.c — gmb_tui_hexview. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== HexView ===================== */

static int hex_addr_cols(gmb_hex_view_t* h)
{
    return h->address_format == 0 ? 8 : 5;
}
static int hex_hex_cols(gmb_hex_view_t* h)
{
    int groups = (h->bytes_per_row + h->bytes_per_group - 1) / h->bytes_per_group;
    return h->bytes_per_row * 2 + (groups - 1) * (int)strlen(h->group_separator);
}
static void hexview_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_hex_view_t* h = (gmb_hex_view_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        for (int i = 0; i < c->height; i++)
        {
            int row = h->offset + i;
            int start_byte = row * h->bytes_per_row;
            if (start_byte >= h->data_len) break;
            char line[256];
            gmb_hex_view_render_line(h, row, line, (int)sizeof(line));
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        return;
    }
    for (int i = 0; i < c->height; i++)
    {
        int row = h->offset + i;
        int start_byte = row * h->bytes_per_row;
        if (start_byte >= h->data_len) break;
        char line[256];
        gmb_hex_view_render_line(h, row, line, (int)sizeof(line));
        gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
        if (h->show_address)
        {
            char addr[16];
            if (h->address_format == 0)
                snprintf(addr, sizeof(addr), "%08X", (unsigned)(h->start_addr + start_byte));
            else
                snprintf(addr, sizeof(addr), "%5d", (int)(h->start_addr + start_byte));
            gmb_screen_setstring(buf, c->x, c->y + i, addr, GMB_CELLCOLOR_CYAN, bg);
        }
        if (h->show_ascii)
        {
            int ascii_start = (h->show_address ? hex_addr_cols(h) + 1 : 0) + hex_hex_cols(h) + 1;
            for (int b = 0; b < h->bytes_per_row; b++)
            {
                int idx = start_byte + b;
                char ch = idx < h->data_len ? (char)h->data[idx] : ' ';
                if (ch < 32 || ch >= 127) ch = '.';
                gmb_screen_setchar(buf, c->x + ascii_start + b, c->y + i, (unsigned char)ch, GMB_CELLCOLOR_GREEN, bg);
            }
        }
    }
}
static bool hexview_key(void* u, gmb_control_t* c, int key)
{
    gmb_hex_view_t* h = (gmb_hex_view_t*)u;
    if (!c->enabled) return false;
    int max_row = (h->data_len + h->bytes_per_row - 1) / h->bytes_per_row - c->height;
    if (max_row < 0) max_row = 0;
    switch (key)
    {
        case GMB_K_UP: if (h->offset > 0) h->offset--; return true;
        case GMB_K_DOWN: if (h->offset < max_row) h->offset++; return true;
        case GMB_K_PGUP: h->offset -= c->height; if (h->offset < 0) h->offset = 0; return true;
        case GMB_K_PGDN: h->offset += c->height; if (h->offset > max_row) h->offset = max_row; return true;
        case GMB_K_HOME: h->offset = 0; return true;
        case GMB_K_END: h->offset = max_row; return true;
    }
    return false;
}
static bool hexview_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_hex_view_t* h = (gmb_hex_view_t*)u;
    if (!c->enabled) return false;
    int max_row = (h->data_len + h->bytes_per_row - 1) / h->bytes_per_row - c->height;
    if (max_row < 0) max_row = 0;
    if (a->kind == GMB_MA_SCROLLUP) { if (h->offset > 0) h->offset--; return true; }
    if (a->kind == GMB_MA_SCROLLDOWN) { if (h->offset < max_row) h->offset++; return true; }
    return false;
}
static void hexview_draw_px(void* u, gmb_control_t* c)
{
    gmb_hex_view_t* hv = (gmb_hex_view_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    if (w < 8 || h < 8) return;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    int hrh = (st.row_h > 0 ? st.row_h : 22);
    int pad = soft ? 4 : 0;
    int vis = hrh > 0 ? (h - pad * 2) / hrh : 0;
    int bpr = hv->bytes_per_row > 0 ? hv->bytes_per_row : 16;
    int start = hv->offset; if (start < 0) start = 0;
    for (int r = 0; r < vis; r++) {
        int base = (start + r) * bpr; if (base >= hv->data_len) break;
        char line[256]; int o = 0;
        o += snprintf(line + o, sizeof(line) - o, "%04lX  ", hv->start_addr + (unsigned long)base);
        for (int k = 0; k < bpr; k++) { int idx = base + k; if (idx < hv->data_len) o += snprintf(line + o, sizeof(line) - o, "%02X ", (unsigned)hv->data[idx]); else o += snprintf(line + o, sizeof(line) - o, "   "); }
        {   /* clip: la riga hex non deve uscire dalla card */
            int cw1 = (int)(gmb_gfx_text_width("0") / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f));
            if (cw1 < 1) cw1 = 8;
            int maxc = (w - (soft ? 24 : 8)) / cw1;
            if (maxc > 3 && (int)strlen(line) > maxc) { line[maxc - 3] = (char)0xE2; line[maxc - 2] = (char)0x80; line[maxc - 1] = (char)0xA6; line[maxc] = 0; }
        }
        gmb_gfx_draw_text(line, x + (soft ? 12 : 4), y + pad + r * hrh + (hrh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    }
}
void gmb_hex_view_init(gmb_hex_view_t* h, int x, int y, int w, int hgt)
{
    memset(h, 0, sizeof(*h));
    gmb_control_init(&h->ctrl, "", x, y, w, hgt, hexview_draw, hexview_key);
    h->ctrl.ctrl_kind = GMB_CK_HEXVIEW;
    h->ctrl.draw_px_fn = hexview_draw_px;
    h->ctrl.handle_mouse_fn = hexview_mouse;
    h->ctrl.user = h;
    h->bytes_per_row = 16;
    h->bytes_per_group = 2;
    h->group_separator = " ";
    h->show_address = true;
    h->show_ascii = true;
}
void gmb_hex_view_set_data(gmb_hex_view_t* h, const unsigned char* data, int len)
{
    h->data_len = len < 1024 ? len : 1024;
    for (int i = 0; i < h->data_len; i++) h->data[i] = data[i];
}
int gmb_hex_view_render_line(gmb_hex_view_t* h, int row, char* out, int cap)
{
    int start_byte = (h->offset + row) * h->bytes_per_row;
    if (start_byte >= h->data_len) { out[0] = '\0'; return 0; }
    int o = 0;
    if (h->show_address)
    {
        char addr[16];
        if (h->address_format == 0)
            snprintf(addr, sizeof(addr), "%08X", (unsigned)(h->start_addr + start_byte));
        else
            snprintf(addr, sizeof(addr), "%5d", (int)(h->start_addr + start_byte));
        for (char* p = addr; *p && o < cap - 1; p++) out[o++] = *p;
        if (o < cap - 1) out[o++] = ' ';
    }
    for (int b = 0; b < h->bytes_per_row; b++)
    {
        int idx = start_byte + b;
        if (b > 0 && b % h->bytes_per_group == 0 && b < h->bytes_per_row)
            for (const char* p = h->group_separator; *p && o < cap - 1; p++) out[o++] = *p;
        if (idx < h->data_len)
            o += snprintf(out + o, cap - o, "%02X", h->data[idx]);
        else
        {
            out[o++] = ' '; out[o++] = ' ';
        }
    }
    if (h->show_ascii)
    {
        if (o < cap - 1) out[o++] = ' ';
        for (int b = 0; b < h->bytes_per_row; b++)
        {
            int idx = start_byte + b;
            char ch = idx < h->data_len ? (char)h->data[idx] : ' ';
            if (ch < 32 || ch >= 127) ch = '.';
            if (o < cap - 1) out[o++] = ch;
        }
    }
    if (o > h->ctrl.width) o = h->ctrl.width;
    else for (int i = o; i < h->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
