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
static void filedialog_draw_px(void* u, gmb_control_t* c)
{
    gmb_file_dialog_t* f = (gmb_file_dialog_t*)u;
    static char _buf[32][256]; static const char* _it[32]; int n = 0;
    for (int i = 0; i < f->entries_count && n < 32; i++) { snprintf(_buf[n], 256, "%s%s", f->entries[i].is_dir ? "[D] " : "", f->entries[i].name); _it[n] = _buf[n]; n++; }
    gmb_px_draw_ex(GMB_PX_LISTBOX, c->x, c->y, c->width, c->height, NULL, _it, n, f->selected_index, 0, 0, NULL, f->scroll_offset, NULL, 0);
}
void gmb_file_dialog_init(gmb_file_dialog_t* f, int x, int y, int w, int h)
{
    memset(f, 0, sizeof(*f));
    gmb_control_init(&f->ctrl, "", x, y, w, h, filedialog_draw, filedialog_key);
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
