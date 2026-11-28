/* ============================================================================
 * gmb_widget_model.c — MODELLO UNICO dei controlli (gmb_widget_t).
 *
 * Un controllo ha UN solo stato (gmb_tui_widgets.h: gmb_widget_t). Qui vive il
 * RENDERER a CELLE dello stesso modello; quello PIXEL è gmb_widget_draw_px()
 * (gmb_form.c). Nessun adattatore, nessuna struct parallela.
 * ========================================================================== */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_tui_app.h"
#include "gmb_textview.h"
#include "gmb_theme_roles.h"
#include "gmb_a11y.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Colore di una riga della vista testuale (`mode=markdown|diff`). Lo stile è
 * simbolico (gmb_textview.h): qui si sceglie il ruolo del tema. Con il contrasto
 * alto attivo la soglia di leggibilità resta quella degli altri testi. */
static void tv_row_colors(int st, gmb_cell_color_t base_fg, gmb_cell_color_t base_bg,
                          gmb_cell_color_t* fg, gmb_cell_color_t* bg)
{
    gmb_theme_t th = gmb_ui_theme();
    *fg = base_fg; *bg = base_bg;
    switch (st)
    {
        case GMB_TV_S_HEAD1: *fg = th.accent; break;
        case GMB_TV_S_HEAD2: *fg = th.accent; break;
        case GMB_TV_S_HEAD3: *fg = th.accent; break;
        case GMB_TV_S_CODE:
            *fg = (th.editor_fg.r | th.editor_fg.g | th.editor_fg.b) ? th.editor_fg : th.text;
            *bg = (th.editor_bg.r | th.editor_bg.g | th.editor_bg.b) ? th.editor_bg : th.window_bg;
            break;
        case GMB_TV_S_QUOTE: *fg = th.dim; break;
        case GMB_TV_S_RULE:  *fg = th.dim; break;
        case GMB_TV_S_META:  *fg = th.dim; break;
        case GMB_TV_S_HUNK:  *fg = th.accent; break;
        case GMB_TV_S_ADD:   { gmb_cell_color_t c = { 0x2E, 0xA0, 0x4E, 0 }; *fg = c; break; }
        case GMB_TV_S_DEL:   { gmb_cell_color_t c = { 0xC0, 0x3A, 0x3A, 0 }; *fg = c; break; }
        default: return;
    }
    if (gmb_a11y_get().high_contrast)
        *fg = gmb_theme_ensure_contrast(*fg, *bg, gmb_a11y_min_text_ratio());
}

/* ── RENDERER a CELLE ───────────────────────────────────────────────────────
 * Legge lo STESSO modello del renderer pixel: cambia solo la superficie. */
void gmb_widget_draw_cells(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* g = (gmb_widget_t*)(u ? u : c);
    if (!g || !c || !buf) return;
    switch (g->kind)
    {
        case W_STATUSBAR:
        {
            /* TStatusLine: i separatori sono `│` (0x2502), non `|`. */
            gmb_cell_color_t fg, bg;
            gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
            {
                gmb_theme_t th_ = gmb_ui_theme(); gmb_cell_color_t tf, tb;
                gmb_theme_kind(&th_, 3, &tf, &tb);
                if (!c->style.has_fg || gmb_cellcolor_eq(c->style.fg, th_.text)) fg = tf;
                if (!c->style.has_bg || gmb_cellcolor_eq(c->style.bg, th_.window_bg)) bg = tb;
            }
            char line[256], cased[320], plain[256];
            snprintf(line, sizeof(line), "%s", g->label);
            for (char* p = line; *p; p++) if (*p == '|') *p = ' ';
            gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
            snprintf(plain, sizeof(plain), "%s", g->label);
            gmb_cell_color_t red = { 170, 0, 0, 0 };
            gmb_draw_string_keys(buf, c->x, c->y, cased, c->width, fg, bg, red);
            for (int i = 0; i < (int)strlen(plain); i++)
                if (plain[i] == '|' && i < c->width)
                    gmb_screen_setchar(buf, c->x + i, c->y, 0x2502, fg, bg);
            break;
        }
        case W_PANEL:
        {
            gmb_cell_color_t fg, bg;
            gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
            int w = c->width, h = c->height;
            if (w < 2 || h < 2) break;
            /* Surface gfx: chrome nativo (rounded) invece del bordo a celle. */
            gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
            if (sk && sk->is_gfx)
            {
                gmb_surface_rounded_rect(buf, c->x, c->y, w, h, 6, fg, bg, 1);
                if (g->title[0])
                {
                    char t[128], ct[160];
                    snprintf(t, sizeof(t), "%s", g->title);
                    gmb_control_apply_case(c, t, ct, (int)sizeof(ct));
                    int px = sk->origin_x + (c->x + 1) * sk->cell_w;
                    int py = sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4;
                    gmb_gfx_draw_text(ct, px, py, 0, (int)(0x40000000u | ((unsigned)fg.r << 16) | ((unsigned)fg.g << 8) | fg.b));
                }
                break;
            }
            gmb_screen_setchar(buf, c->x, c->y, 0x250C, fg, bg);
            gmb_screen_setchar(buf, c->x + w - 1, c->y, 0x2510, fg, bg);
            gmb_screen_setchar(buf, c->x, c->y + h - 1, 0x2514, fg, bg);
            gmb_screen_setchar(buf, c->x + w - 1, c->y + h - 1, 0x2518, fg, bg);
            for (int i = 1; i < w - 1; i++)
            {
                gmb_screen_setchar(buf, c->x + i, c->y, 0x2500, fg, bg);
                gmb_screen_setchar(buf, c->x + i, c->y + h - 1, 0x2500, fg, bg);
            }
            for (int i = 1; i < h - 1; i++)
            {
                gmb_screen_setchar(buf, c->x, c->y + i, 0x2502, fg, bg);
                gmb_screen_setchar(buf, c->x + w - 1, c->y + i, 0x2502, fg, bg);
            }
            if (g->title[0])
            {
                char t[128], ct[160];
                snprintf(t, sizeof(t), " %s ", g->title);
                gmb_control_apply_case(c, t, ct, (int)sizeof(ct));
                gmb_screen_setstring(buf, c->x + 1, c->y, ct, fg, bg);
            }
            break;
        }
        case W_TOGGLE:
        {
            gmb_cell_color_t fg, bg;
            gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
            char line[128], cased[160];
            snprintf(line, sizeof(line), "%s %s", g->ival != 0 ? "[X]" : "[ ]", g->caption);
            int l = (int)strlen(line);
            if (l > c->width) line[c->width] = '\0';
            else for (int i = l; i < c->width && i < 127; i++) line[i] = ' ';
            if (l < c->width) line[c->width < 127 ? c->width : 127] = '\0';
            gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
            gmb_screen_setstring(buf, c->x, c->y, cased, fg, bg);
            break;
        }
        case W_RADIO:
        {
            gmb_cell_color_t fg, bg;
            gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
            int sel = (int)g->ival;
            for (int i = 0; i < g->nitems && i < c->height; i++)
            {
                char line[160], cased[192];
                const char* mark = (i == sel) ? "(\xE2\x80\xA2)" : "( )";
                int o = snprintf(line, sizeof(line), " %s %s", mark, g->items[i]);
                if (o > c->width) o = c->width;
                else for (int k = o; k < c->width && k < (int)sizeof(line) - 1; k++) line[o++] = ' ';
                line[o] = '\0';
                gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
                gmb_screen_setstring(buf, c->x, c->y + i, cased, fg, bg);
            }
            break;
        }
        case W_TABS:
        {
            /* C# Draw: per ogni tab " {ApplyCase(name)} "; attivo = Effective(selected:true). */
            gmb_cell_color_t fg, bg, afg, abg;
            gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
            gmb_control_style_effective(&c->style, false, false, true, &afg, &abg);
            int sel = (int)g->ival, cx = c->x;
            for (int i = 0; i < g->nitems; i++)
            {
                char raw[80], cased[160];
                snprintf(raw, sizeof(raw), " %s ", g->items[i]);
                gmb_control_apply_case(c, raw, cased, (int)sizeof(cased));
                int tl = (int)strlen(cased);
                if (cx + tl > c->x + c->width) break;
                int active = (i == sel);
                gmb_screen_setstring(buf, cx, c->y, cased, active ? afg : fg, active ? abg : bg);
                cx += tl;
            }
            break;
        }
        case W_TRACK:
        {
            gmb_cell_color_t fg, bg;
            gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
            char line[256];
            gmb_slider_render_line(g, line, (int)sizeof(line));
            gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
            break;
        }
        case W_LISTBOX:
        {
            if (g->tv) gmb_listbox_sync(g, c->width);
            for (int i = 0; i < c->height; i++)
            {
                int idx = g->first + i;
                if (idx >= g->nitems) break;
                bool sel = idx == (int)g->ival;
                gmb_cell_color_t fg, bg;
                gmb_control_style_effective(&c->style, c->focused, false, sel, &fg, &bg);
                if (!sel && g->tv) tv_row_colors(gmb_textview_row_style((gmb_textview_t*)g->tv, idx), fg, bg, &fg, &bg);
                char line[256], cased[320];
                gmb_listbox_render_line(g, idx, line, (int)sizeof(line));
                gmb_control_apply_case(c, line, cased, (int)sizeof(cased));
                gmb_screen_setstring(buf, c->x, c->y + i, cased, fg, bg);
                /* TV TListViewer: barra verticale `│` sull'ultima colonna. */
                if (c->width > 1)
                {
                    gmb_theme_t th = gmb_ui_theme();
                    gmb_screen_setchar(buf, c->x + c->width - 1, c->y + i, 0x2502, th.dim, th.window_bg);
                }
            }
            break;
        }
        case W_DROPDOWN:
        {
            gmb_cell_color_t fg, bg;
            gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
            char line[160];
            gmb_combobox_render_line(g, line, (int)sizeof(line));
            gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
            /* TV THistory: icona `▐↓▌` (tre celle); `↓` se lo spazio è ridotto. */
            if (c->width >= 4) {
                gmb_screen_setchar(buf, c->x + c->width - 3, c->y, 0x2590, fg, bg);
                gmb_screen_setchar(buf, c->x + c->width - 2, c->y, 0x2193, fg, bg);
                gmb_screen_setchar(buf, c->x + c->width - 1, c->y, 0x258C, fg, bg);
            } else if (c->width >= 1) {
                gmb_screen_setchar(buf, c->x + c->width - 1, c->y, 0x2193, fg, bg);
            }
            if (g->open && g->nitems > 0)
            {
                gmb_cell_color_t popFg, popBg, selFg, selBg;
                gmb_control_style_effective(&c->style, false, false, false, &popFg, &popBg);
                gmb_control_style_effective(&c->style, false, false, true, &selFg, &selBg);
                int maxH = g->drop_down_height < g->nitems ? g->drop_down_height : g->nitems;
                for (int i = 0; i < maxH; i++)
                {
                    const char* it = g->items[i];
                    int il = (int)strlen(it);
                    char tmp[128], item[160];
                    if (il > c->width) snprintf(tmp, sizeof(tmp), "%.*s", c->width, it);
                    else snprintf(tmp, sizeof(tmp), "%-*s", c->width, it);
                    gmb_control_apply_case(c, tmp, item, (int)sizeof(item));
                    gmb_cell_color_t f = (i == g->highlight) ? selFg : popFg;
                    gmb_cell_color_t b = (i == g->highlight) ? selBg : popBg;
                    gmb_screen_setstring(buf, c->x, c->y + 1 + i, item, f, b);
                }
            }
            break;
        }
        case W_COLORPICKER:
        {
            int rows = (256 + g->columns - 1) / (g->columns > 0 ? g->columns : 1);
            for (int r = 0; r < c->height; r++)
            {
                int pr = g->top_row + r;
                if (pr >= rows) break;
                for (int cc = 0; cc < g->columns && cc < c->width; cc++)
                {
                    int idx = pr * g->columns + cc;
                    if (idx >= 256) break;
                    gmb_cell_color_t swatch = g->palette[idx];
                    if (idx == (int)g->ival)
                    {
                        int lum = swatch.r * 299 + swatch.g * 587 + swatch.b * 114;
                        gmb_cell_color_t mfg = lum > 140000 ? GMB_CELLCOLOR_BLACK : GMB_CELLCOLOR_WHITE;
                        gmb_screen_setchar(buf, c->x + cc, c->y + r, 0x25CF, mfg, swatch);
                    }
                    else
                    {
                        gmb_screen_setchar(buf, c->x + cc, c->y + r, 0x2588, swatch, swatch);
                    }
                }
            }
            break;
        }
        case W_GRID:
        {
            gmb_cell_color_t fg, bg, sf, sb;
            gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
            gmb_control_style_effective(&c->style, false, false, true, &sf, &sb);
            for (int r = 0; r < c->height; r++)
            {
                char line[256];
                gmb_grid_render_line(g, r, line, (int)sizeof(line));
                if (r == g->cursor_row) gmb_screen_setstring(buf, c->x, c->y + r, line, sf, sb);
                else gmb_screen_setstring(buf, c->x, c->y + r, line, fg, bg);
            }
            break;
        }
        default:
            break;
    }
}
