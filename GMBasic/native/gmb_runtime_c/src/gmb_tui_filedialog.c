/* ============================================================================
 * gmb_tui_filedialog.c — gmb_tui_filedialog. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"
#include <dirent.h>
#include <sys/stat.h>

/* ===================== FileDialog ===================== */

static void filedialog_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_file_dialog_t* f = (gmb_file_dialog_t*)u;
    gmb_cell_color_t fg, bg, sf, sb;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sf, &sb);
    for (int i = 0; i < c->height; i++)
    {
        int idx = f->scroll_offset + i;
        if (idx >= f->entries_count) break;
        char line[256];
        gmb_file_dialog_render_line(f, idx, line, (int)sizeof(line));
        if (idx == f->selected_index) gmb_screen_setstring(buf, c->x, c->y + i, line, sf, sb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool filedialog_key(void* u, gmb_control_t* c, int key)
{
    gmb_file_dialog_t* f = (gmb_file_dialog_t*)u;
    if (!c->enabled) return false;
    switch (key)
    {
        case GMB_K_UP:
            if (f->selected_index > 0) { f->selected_index--; if (f->selected_index < f->scroll_offset) f->scroll_offset--; }
            return true;
        case GMB_K_DOWN:
            if (f->selected_index < f->entries_count - 1) { f->selected_index++; if (f->selected_index >= f->scroll_offset + c->height) f->scroll_offset++; }
            return true;
        case GMB_K_PGUP:
            f->selected_index -= c->height; if (f->selected_index < 0) f->selected_index = 0;
            f->scroll_offset -= c->height; if (f->scroll_offset < 0) f->scroll_offset = 0;
            return true;
        case GMB_K_PGDN:
            f->selected_index += c->height; if (f->selected_index >= f->entries_count) f->selected_index = f->entries_count - 1;
            f->scroll_offset += c->height; if (f->scroll_offset > f->entries_count - c->height) f->scroll_offset = f->entries_count - c->height > 0 ? f->entries_count - c->height : 0;
            return true;
        case GMB_K_HOME: f->selected_index = 0; f->scroll_offset = 0; return true;
        case GMB_K_END:
            f->selected_index = f->entries_count - 1;
            f->scroll_offset = f->entries_count - c->height > 0 ? f->entries_count - c->height : 0;
            return true;
        case GMB_K_ENTER: case GMB_K_ESC: return true;
    }
    return false;
}
static bool filedialog_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_file_dialog_t* f = (gmb_file_dialog_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = (y - c->y) + f->scroll_offset;
    if (idx >= 0 && idx < f->entries_count) f->selected_index = idx;
    return true;
}
/* Render PIXEL (grafica): finestra di dialogo vera, non un blit di celle.
 * Card + barra del titolo + righe (con prefisso [D] per le cartelle) + selezione. */
static void filedialog_draw_px(void* u, gmb_control_t* c)
{
    gmb_file_dialog_t* f = (gmb_file_dialog_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = (c->px_w > 0) ? c->px_x : c->x;
    int y = (c->px_h > 0) ? c->px_y : c->y;
    int w = (c->px_w > 0) ? c->px_w : c->width;
    int h = (c->px_h > 0) ? c->px_h : c->height;
    if (w < 120 || h < 90) return;
    int rr = st.radius > 0 ? st.radius : 8;
    int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;
    int rh = st.row_h > 0 ? st.row_h : 24; if (rh < th + 6) rh = th + 6;
    int tb = 34, pad = 12;

    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, st.fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    /* Barra del titolo. */
    gmb_gfx_fill_rounded_rect(x + 1, y + 1, w - 2, tb, rr > 2 ? rr - 1 : 0, st.head_bg);
    gmb_gfx_draw_text("Apri file", x + pad, y + 1 + (tb - th) / 2, 0,
                      (int)(0x40000000u | (st.head_fg & 0xFFFFFFu)));
    /* Percorso/mascherina. */
    gmb_gfx_draw_text("*.bas", x + w - pad - 70, y + 1 + (tb - th) / 2, 0,
                      (int)(0x40000000u | (st.text_disabled & 0xFFFFFFu)));

    int ly = y + tb + 6;
    int lh = h - tb - 6 - 44;                 /* spazio per il footer con OK/Annulla */
    int rows = lh / rh; if (rows < 1) rows = 1;
    gmb_gfx_fill_rounded_rect(x + pad - 4, ly - 2, w - 2 * pad + 8, lh + 4, 6, (st.border & 0xFFFFFFu) | 0x22000000u);
    for (int r = 0; r < rows; r++)
    {
        int i = f->scroll_offset + r;
        if (i >= f->entries_count) break;
        int ry = ly + r * rh;
        int sel = (i == f->selected_index);
        if (sel) gmb_gfx_fill_rounded_rect(x + pad - 2, ry, w - 2 * pad + 4, rh, 5, (st.accent & 0xFFFFFFu) | 0x55000000u);
        else if (r & 1) gmb_gfx_fill_rounded_rect(x + pad - 2, ry, w - 2 * pad + 4, rh, 0, (st.border & 0xFFFFFFu) | 0x14000000u);
        char nm[300];
        snprintf(nm, sizeof(nm), "%s%s", f->entries[i].is_dir ? "[D] " : "     ", f->entries[i].name);
        int avail = w - 2 * pad - 12, len = (int)strlen(nm);
        while (len > 0 && gmb_gfx_text_width_logical(nm) > avail) nm[--len] = 0;
        gmb_gfx_draw_text(nm, x + pad, ry + (rh - th) / 2, 0,
                          (int)(0x40000000u | ((sel ? 0xFFFFFFu : (st.text & 0xFFFFFFu)))));
    }
    /* Footer: pulsanti. */
    int by = y + h - 40;
    gmb_gfx_fill_rounded_rect(x + w - 220, by, 100, 28, 14, (st.accent & 0xFFFFFFu) | 0xFF000000u);
    gmb_gfx_draw_text("Apri", x + w - 220 + 50 - gmb_gfx_text_width_logical("Apri") / 2,
                      by + (28 - th) / 2, 0, (int)(0x40000000u | 0xFFFFFFu));
    gmb_gfx_fill_rounded_rect(x + w - 108, by, 96, 28, 14, st.fill_hover);
    gmb_gfx_stroke_rounded_rect(x + w - 108, by, 96, 28, 14, st.border);
    gmb_gfx_draw_text("Annulla", x + w - 108 + 48 - gmb_gfx_text_width_logical("Annulla") / 2,
                      by + (28 - th) / 2, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
}

void gmb_file_dialog_init(gmb_file_dialog_t* f, int x, int y, int w, int h)
{
    memset(f, 0, sizeof(*f));
    /* Bounds: se non specificate, dialogo CENTRATO (520x420) sulla pagina logica. */
    if (w <= 0 || h <= 0) {
        extern int gmb_gfx_screen_width(void); extern int gmb_gfx_screen_height(void);
        int sw = gmb_gfx_screen_width(), sh = gmb_gfx_screen_height();
        w = 520; h = 420;
        if (sw > 0) x = (sw - w) / 2;
        if (sh > 0) y = (sh - h) / 2;
    }
    gmb_control_init(&f->ctrl, "", x, y, w, h, filedialog_draw, filedialog_key);
    f->ctrl.coord_auto_w = 1; f->ctrl.coord_auto_h = 1;   /* dimensioni proprie, non da celle */
    f->ctrl.draw_px_fn = filedialog_draw_px;
    f->ctrl.handle_click_fn = filedialog_click;
    f->ctrl.user = f;
}
void gmb_file_dialog_set_entries(gmb_file_dialog_t* f, const gmb_file_entry_t* entries, int count)
{
    f->entries_count = count < 64 ? count : 64;
    for (int i = 0; i < f->entries_count && entries; i++)
        f->entries[i] = entries[i];
}

/* Popola il dialogo leggendo la cartella: cartelle prima, poi i file che combaciano
 * con la maschera ("*.bas"). Senza questo il dialogo restava sempre VUOTO. */
void gmb_file_dialog_load(gmb_file_dialog_t* f, const char* dir, const char* mask)
{
    if (!f) return;
    f->entries_count = 0; f->selected_index = 0; f->scroll_offset = 0;
    const char* base = (dir && *dir) ? dir : ".";
    DIR* d = opendir(base);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d)) != NULL && f->entries_count < 64)
    {
        if (de->d_name[0] == '.') continue;
        char p[1200]; snprintf(p, sizeof(p), "%s/%s", base, de->d_name);
        struct stat stt; if (stat(p, &stt) != 0) continue;
        int is_dir = S_ISDIR(stt.st_mode) ? 1 : 0;
        if (!is_dir && mask && *mask && strcmp(mask, "*.*") != 0 && strcmp(mask, "*") != 0)
        {
            if (mask[0] == '*')
            {
                const char* dot = strrchr(de->d_name, '.');
                if (!dot || strcasecmp(dot, mask + 1) != 0) continue;
            }
        }
        snprintf(f->entries[f->entries_count].name, sizeof(f->entries[0].name), "%s", de->d_name);
        f->entries[f->entries_count].is_dir = is_dir ? true : false;
        f->entries_count++;
    }
    closedir(d);
    /* Ordina: cartelle prima, poi alfabetico (bubble, max 64 elementi). */
    for (int i = 0; i < f->entries_count - 1; i++)
        for (int j = 0; j < f->entries_count - 1 - i; j++)
        {
            int swap = 0;
            if (f->entries[j].is_dir != f->entries[j + 1].is_dir) swap = f->entries[j + 1].is_dir ? 1 : 0;
            else swap = strcasecmp(f->entries[j].name, f->entries[j + 1].name) > 0;
            if (swap) { gmb_file_entry_t t = f->entries[j]; f->entries[j] = f->entries[j + 1]; f->entries[j + 1] = t; }
        }
}
int gmb_file_dialog_render_line(gmb_file_dialog_t* f, int idx, char* out, int cap)
{
    int i = f->scroll_offset + idx;
    if (i < 0 || i >= f->entries_count)
    {
        for (int k = 0; k < f->ctrl.width && k < cap - 1; k++) out[k] = ' ';
        out[f->ctrl.width < cap - 1 ? f->ctrl.width : cap - 1] = '\0';
        return f->ctrl.width;
    }
    const char* prefix = f->entries[i].is_dir ? "[D]" : "[F]";
    /* C# FileDialog: $"{prefix} {ApplyCase(e.Name)}" (solo il nome, non il prefisso). */
    char nm[160];
    gmb_control_apply_case(&f->ctrl, f->entries[i].name, nm, (int)sizeof(nm));
    int o = snprintf(out, cap, "%s %s", prefix, nm);
    if (o > f->ctrl.width) o = f->ctrl.width;
    else for (int k = o; k < f->ctrl.width && k < cap - 1; k++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
