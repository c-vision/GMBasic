/* ============================================================================
 * gmb_tui_codearea_ctrl.c — CodeArea (specchio CodeArea.cs): Control con gutter
 * (numeri + marker breakpoint/bookmark), syntax highlighting per keyword,
 * riga di esecuzione, scrollbar; draw a celle + chrome nativo gfx.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static uint32_t ca_col(gmb_cell_color_t c) { return 0x40000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

static void codearea_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_code_area_t* ca = (gmb_code_area_t*)u;
    int W = c->width, H = c->height;
    if (W <= 0 || H <= 0) return;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    /* Palette per-vista (TV): se lo stile non fissa i colori, usa quelli dell'EDITOR (blu/bianco). */
    { gmb_theme_t th_ = gmb_ui_theme(); gmb_cell_color_t tf, tb; gmb_theme_kind(&th_, 0, &tf, &tb);
      if (!c->style.has_fg || gmb_cellcolor_eq(c->style.fg, th_.text)) fg = tf; if (!c->style.has_bg || gmb_cellcolor_eq(c->style.bg, th_.window_bg)) bg = tb; }
    int gw = ca->show_line_numbers ? gmb_code_area_gutter(ca) : 0;
    int sbW = 1;
    int codeW = W - gw - sbW; if (codeW < 0) codeW = 0;

    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, W, H, 4, fg, bg, 1);
        int row = 0;
        for (int i = ca->scroll_offset; i < ca->lines_count && row < H; i++, row++)
        {
            if (i == ca->current_exec_line)
                gmb_surface_rounded_rect(buf, c->x, c->y + row, W, 1, 0, GMB_CELLCOLOR_YELLOW, GMB_CELLCOLOR_YELLOW, 1);
            if (gw > 0)
            {
                char num[32];
                gmb_codearea_format_line_number(i + 1, gw - 1, num, sizeof(num));
                gmb_gfx_draw_text(num, sk->origin_x + c->x * sk->cell_w,
                                  sk->origin_y + (c->y + row) * sk->cell_h + sk->cell_h / 4, 0, (int)ca_col(ca->gutter_color));
                gmb_gfx_draw_text(ca->lines[i], sk->origin_x + (c->x + gw) * sk->cell_w,
                                  sk->origin_y + (c->y + row) * sk->cell_h + sk->cell_h / 4, 0, (int)ca_col(fg));
            }
            else
            {
                gmb_gfx_draw_text(ca->lines[i], sk->origin_x + c->x * sk->cell_w,
                                  sk->origin_y + (c->y + row) * sk->cell_h + sk->cell_h / 4, 0, (int)ca_col(fg));
            }
        }
        return;
    }

    /* Riempi tutta l'area con lo sfondo del controllo (altrimenti le celle non scritte,
     * sotto il testo/oltre le righe, restano del colore del buffer = nere). */
    for (int yy = 0; yy < H; yy++)
        for (int xx = 0; xx < W; xx++)
            gmb_screen_setchar(buf, c->x + xx, c->y + yy, ' ', fg, bg);

    for (int i = 0; i < H; i++)
    {
        int idx = ca->scroll_offset + i;
        if (idx >= ca->lines_count) break;
        gmb_cell_color_t rowBg = (idx == ca->current_exec_line) ? GMB_CELLCOLOR_YELLOW : bg;
        if (gw > 0)
        {
            uint32_t marker = ca->breakpoints[idx] ? 0x25CF : ca->errors[idx] ? '!' : ca->bookmarks[idx] ? 0x25C6 : ' ';
            gmb_screen_setchar(buf, c->x, c->y + i, marker, ca->errors[idx] ? GMB_CELLCOLOR_RED : GMB_CELLCOLOR_RED, rowBg);
            char num[32];
            gmb_codearea_format_line_number(idx + 1, gw - 1, num, sizeof(num));
            gmb_screen_setstring(buf, c->x + 1, c->y + i, num, ca->gutter_color, rowBg);
        }
        const char* line = ca->lines[idx];
        int x = c->x + gw;
        int n = (int)strlen(line), p = 0;
        while (p < n && x < c->x + gw + codeW)
        {
            char ch = line[p];
            if (isalpha((unsigned char)ch) || ch == '_')
            {
                int j = p;
                while (j < n && (isalnum((unsigned char)line[j]) || line[j] == '_')) j++;
                char word[32];
                int wl = j - p; if (wl > 31) wl = 31;
                memcpy(word, line + p, (size_t)wl); word[wl] = '\0';
                gmb_cell_color_t col = fg;
                for (int k = 0; k < ca->keyword_count; k++)
                    if (strcmp(ca->keywords[k], word) == 0) { col = ca->keyword_color; break; }
                for (int q = p; q < j && x < c->x + gw + codeW; q++)
                    gmb_screen_setchar(buf, x++, c->y + i, (unsigned char)line[q], col, rowBg);
                p = j;
            }
            else
            {
                gmb_screen_setchar(buf, x++, c->y + i, (unsigned char)ch, fg, rowBg);
                p++;
            }
        }
    }

    gmb_cell_color_t track = { 0, 0, 100 };
    gmb_cell_color_t thumbc = { 192, 192, 192 };
    int sbX = c->x + W - 1;
    int total = ca->lines_count > H ? ca->lines_count : H;
    int thumb = H * H / total; if (thumb < 1) thumb = 1;
    int maxPos = total - H; if (maxPos < 0) maxPos = 0;
    int pos = ca->scroll_offset; if (pos > maxPos) pos = maxPos;
    int startY = maxPos > 0 ? pos * (H - thumb) / maxPos : 0;
    for (int i = 0; i < H; i++)
    {
        int inThumb = i >= startY && i < startY + thumb;
        gmb_screen_setchar(buf, sbX, c->y + i, ' ', inThumb ? thumbc : track, inThumb ? thumbc : track);
    }
}

/* Mouse: click -> posiziona il cursore sulla cella cliccata (riga/colonna). */
static bool codearea_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_code_area_t* ca = (gmb_code_area_t*)u;
    if (!c || !c->enabled || !a || a->kind != GMB_MA_CLICK) return false;
    int gw = ca->show_line_numbers ? gmb_code_area_gutter(ca) : 0;
    int row = a->y - c->y + ca->scroll_offset;
    if (row < 0) row = 0;
    if (row >= ca->lines_count) row = ca->lines_count > 0 ? ca->lines_count - 1 : 0;
    int col = a->x - c->x - gw;
    if (col < 0) col = 0;
    { int ll = (int)strlen(ca->lines[row]); if (col > ll) col = ll; }
    ca->cursor_line = row; ca->cursor_col = col;
    return true;
}

static bool codearea_key(void* u, gmb_control_t* c, int key){
    gmb_code_area_t* ca = (gmb_code_area_t*)u;
    if (!c->enabled || ca->lines_count <= 0) return false;
    char* cur = ca->lines[ca->cursor_line];
    int ll = (int)strlen(cur);
    if (ca->cursor_col > ll) ca->cursor_col = ll;
    switch (key)
    {
        case GMB_K_UP: if (ca->cursor_line > 0) ca->cursor_line--; break;
        case GMB_K_DOWN: if (ca->cursor_line < ca->lines_count - 1) ca->cursor_line++; break;
        case GMB_K_LEFT: if (ca->cursor_col > 0) ca->cursor_col--; break;
        case GMB_K_RIGHT: if (ca->cursor_col < ll) ca->cursor_col++; break;
        case GMB_K_HOME: ca->cursor_col = 0; break;
        case GMB_K_END: ca->cursor_col = ll; break;
        case GMB_K_PGUP: ca->cursor_line -= c->height; if (ca->cursor_line < 0) ca->cursor_line = 0; break;
        case GMB_K_PGDN: ca->cursor_line += c->height; if (ca->cursor_line > ca->lines_count - 1) ca->cursor_line = ca->lines_count - 1; break;
        case GMB_K_BS:  /* backspace: cancella il carattere a sinistra (o unisce alla riga precedente) */
            if (ca->cursor_col > 0)
            {
                memmove(cur + ca->cursor_col - 1, cur + ca->cursor_col, (size_t)(ll - ca->cursor_col + 1));
                ca->cursor_col--;
            }
            else if (ca->cursor_line > 0)
            {
                char* prev = ca->lines[ca->cursor_line - 1];
                int pl = (int)strlen(prev);
                if (pl + ll < GMB_CODE_MAX_COLS - 1)
                {
                    strcat(prev, cur);
                    for (int i = ca->cursor_line; i < ca->lines_count - 1; i++) strcpy(ca->lines[i], ca->lines[i + 1]);
                    ca->lines_count--; ca->cursor_line--; ca->cursor_col = pl;
                }
            }
            break;
        case GMB_K_DEL: /* cancella il carattere a destra (o unisce alla riga successiva) */
            if (ca->cursor_col < ll)
                memmove(cur + ca->cursor_col, cur + ca->cursor_col + 1, (size_t)(ll - ca->cursor_col));
            else if (ca->cursor_line < ca->lines_count - 1)
            {
                char* nxt = ca->lines[ca->cursor_line + 1];
                if (ll + (int)strlen(nxt) < GMB_CODE_MAX_COLS - 1)
                {
                    strcat(cur, nxt);
                    for (int i = ca->cursor_line + 1; i < ca->lines_count - 1; i++) strcpy(ca->lines[i], ca->lines[i + 1]);
                    ca->lines_count--;
                }
            }
            break;
        case GMB_K_ENTER: /* spezza la riga al cursore */
            if (ca->lines_count < GMB_CODE_MAX_LINES)
            {
                for (int i = ca->lines_count; i > ca->cursor_line + 1; i--) strcpy(ca->lines[i], ca->lines[i - 1]);
                strncpy(ca->lines[ca->cursor_line + 1], cur + ca->cursor_col, GMB_CODE_MAX_COLS - 1);
                ca->lines[ca->cursor_line + 1][GMB_CODE_MAX_COLS - 1] = '\0';
                cur[ca->cursor_col] = '\0';
                ca->lines_count++; ca->cursor_line++; ca->cursor_col = 0;
            }
            break;
        default:
            if (key >= 32 && key <= 126) /* inserisci il carattere digitato */
            {
                if (ll < GMB_CODE_MAX_COLS - 1)
                {
                    memmove(cur + ca->cursor_col + 1, cur + ca->cursor_col, (size_t)(ll - ca->cursor_col + 1));
                    cur[ca->cursor_col] = (char)key;
                    ca->cursor_col++;
                }
                break;
            }
            return false;
    }
    { int l2 = (int)strlen(ca->lines[ca->cursor_line]); if (ca->cursor_col > l2) ca->cursor_col = l2; }
    if (ca->cursor_line < ca->scroll_offset) ca->scroll_offset = ca->cursor_line;
    if (ca->cursor_line >= ca->scroll_offset + c->height) ca->scroll_offset = ca->cursor_line - c->height + 1;
    if (ca->scroll_offset < 0) ca->scroll_offset = 0;
    return true;
}

static void codearea_draw_px(void* u, gmb_control_t* c)
{
    /* Editor pixel: card del tema, gutter con numeri di riga, riga cursore evidenziata,
     * testo monospazio, colori mascherati (24-bit) per non finire nel palette lookup. */
    gmb_code_area_t* ca = (gmb_code_area_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    if (w < 8 || h < 8) return;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    int rh = (st.row_h > 0 ? st.row_h : 22);
    if (rh < 12) rh = 12;
    int pad = soft ? 6 : 2;
    int vis = rh > 0 ? (h - pad * 2) / rh : 0;
    int gw = ca->show_line_numbers ? (int)(gmb_gfx_text_width("0000") / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f)) + 12 : 0;
    if (soft && gw > 0) gmb_gfx_fill_rounded_rect(x + 4, y + 4, gw + 4, h - 8, 6, (0x0Au << 24) | (st.text & 0xFFFFFFu));
    int start = ca->scroll_offset; if (start < 0) start = 0;
    for (int r = 0; r < vis && start + r < ca->lines_count; r++) {
        int ln = start + r;
        int ry = y + pad + r * rh;
        if (ln == ca->cursor_line) gmb_gfx_fill_rounded_rect(x + 4, ry, w - 8, rh, soft ? rh / 2 : 0, (acc & 0xFFFFFFu) | (0x22u << 24));
        if (ca->show_line_numbers) {
            char nb[16]; snprintf(nb, sizeof(nb), "%4d ", ln + 1);
            gmb_gfx_draw_text(nb, x + pad, ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                              (int)(0x40000000u | (st.text_disabled & 0xFFFFFFu)));
        }
        gmb_gfx_draw_text(ca->lines[ln], x + pad + gw + 4, ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    }
}
void gmb_code_area_init(gmb_code_area_t* ca, int x, int y, int w, int h)
{
    memset(ca, 0, sizeof(*ca));
    gmb_control_init(&ca->ctrl, "", x, y, w, h, codearea_draw, codearea_key);
    ca->ctrl.ctrl_kind = GMB_CK_CODEAREA;
    ca->ctrl.draw_px_fn = codearea_draw_px;
    ca->ctrl.user = ca;
    ca->ctrl.handle_mouse_fn = codearea_mouse;
    ca->show_line_numbers = true;
    ca->current_exec_line = -1;
    ca->gutter_color = GMB_CELLCOLOR_GREY;
    ca->keyword_color = GMB_CELLCOLOR_YELLOW;
}

void gmb_code_area_set_lines(gmb_code_area_t* ca, const char* const* lines, int n)
{
    if (n < 0) n = 0;
    if (n > GMB_CODE_MAX_LINES) n = GMB_CODE_MAX_LINES;
    ca->lines_count = n;
    for (int i = 0; i < n; i++)
        snprintf(ca->lines[i], GMB_CODE_MAX_COLS, "%s", lines && lines[i] ? lines[i] : "");
}

void gmb_code_area_set_breakpoint(gmb_code_area_t* ca, int line, bool on)
{
    if (line >= 0 && line < GMB_CODE_MAX_LINES) ca->breakpoints[line] = on;
}
void gmb_code_area_set_bookmark(gmb_code_area_t* ca, int line, bool on)
{
    if (line >= 0 && line < GMB_CODE_MAX_LINES) ca->bookmarks[line] = on;
}
void gmb_code_area_set_error(gmb_code_area_t* ca, int line, bool on)
{
    if (line >= 0 && line < GMB_CODE_MAX_LINES) ca->errors[line] = on;
}
void gmb_code_area_clear_errors(gmb_code_area_t* ca) { if (ca) memset(ca->errors, 0, sizeof(ca->errors)); }
void gmb_code_area_set_exec_line(gmb_code_area_t* ca, int line) { ca->current_exec_line = line; }
void gmb_code_area_add_keyword(gmb_code_area_t* ca, const char* kw)
{
    if (!kw || ca->keyword_count >= GMB_CODE_MAX_KW) return;
    snprintf(ca->keywords[ca->keyword_count++], 32, "%s", kw);
}
int gmb_code_area_gutter(const gmb_code_area_t* ca) { return gmb_codearea_gutter_width(ca->lines_count); }
int gmb_code_area_cursor_line(const gmb_code_area_t* ca) { return ca ? ca->cursor_line : -1; }
