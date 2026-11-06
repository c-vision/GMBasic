/* ============================================================================
 * gmb_tui_fileexplorer.c — FileExplorer.cs (specchio ClipperLib/Controls).
 * Control: Draw (header + righe con icone/colori + ApplyCase), HandleKey
 * (naviga/ricerca/enter/bs), HandleClick, HandleMouse (scroll).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

enum { GMB_FE_ACTION_NONE = 0, GMB_FE_ACTION_SELECTED = 1, GMB_FE_ACTION_CHANGEDIR = 2 };

static int gmb_fe_entry_cmp(const void* a, const void* b)
{
    const gmb_fe_entry_t* ea = a;
    const gmb_fe_entry_t* eb = b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
}

static void gmb_fe_parent_of(const char* path, char* out, int cap)
{
    snprintf(out, (size_t)cap, "%s", path);
    char* slash = strrchr(out, '/');
    if (slash)
    {
        if (slash == out) slash[1] = 0;
        else *slash = 0;
    }
}

/* Indici visibili: tutti se nessuna query, altrimenti fuzzy (C# FuzzyPattern.Filter). */
static int gmb_fe_visible(const gmb_file_explorer_t* e, int* out, int cap)
{
    if (!e->search_query[0])
    {
        int n = e->entry_count < cap ? e->entry_count : cap;
        for (int i = 0; i < n; i++) out[i] = i;
        return n;
    }
    const char* names[GMB_FE_MAX_ENTRIES];
    for (int i = 0; i < e->entry_count; i++) names[i] = e->entries[i].name;
    return gmb_fuzzy_filter(e->search_query, names, e->entry_count, out, cap);
}

static int gmb_fe_count(const gmb_file_explorer_t* e)
{
    int idx[GMB_FE_MAX_ENTRIES];
    return gmb_fe_visible(e, idx, GMB_FE_MAX_ENTRIES);
}

static void gmb_fe_clamp(gmb_file_explorer_t* e, int body_rows)
{
    int idx[GMB_FE_MAX_ENTRIES];
    int vlen = gmb_fe_visible(e, idx, GMB_FE_MAX_ENTRIES);
    if (e->selected >= vlen) e->selected = vlen - 1 < 0 ? 0 : vlen - 1;
    if (e->selected < e->offset) e->offset = e->selected;
    else if (body_rows > 0 && e->selected >= e->offset + body_rows) e->offset = e->selected + 1 - body_rows;
    if (e->offset < 0) e->offset = 0;
}

static void gmb_fe_activate(gmb_file_explorer_t* e)
{
    int idx[GMB_FE_MAX_ENTRIES];
    int vlen = gmb_fe_visible(e, idx, GMB_FE_MAX_ENTRIES);
    if (vlen == 0 || e->selected >= vlen) return;
    gmb_fe_entry_t* en = &e->entries[idx[e->selected]];
    if (en->is_dir)
    {
        snprintf(e->current_path, sizeof(e->current_path), "%s", en->path);
        gmb_file_explorer_refresh(e);
        e->last_action = GMB_FE_ACTION_CHANGEDIR;
        snprintf(e->selected_path, sizeof(e->selected_path), "%s", en->path);
    }
    else
    {
        e->last_action = GMB_FE_ACTION_SELECTED;
        snprintf(e->selected_path, sizeof(e->selected_path), "%s", en->path);
    }
}

static void gmb_fe_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_file_explorer_t* e = (gmb_file_explorer_t*)u;
    gmb_cell_color_t fg, bg, sfg, sbg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sfg, &sbg);
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, c->width, c->height, 4, fg, bg, 1);
        char hdr[1152], hc[1200];
        if (e->search_mode) snprintf(hdr, sizeof(hdr), "/%s", e->search_query);
        else snprintf(hdr, sizeof(hdr), "%s", e->current_path);
        gmb_control_apply_case(c, hdr, hc, (int)sizeof(hc));
        gmb_gfx_draw_text(hc, sk->origin_x + c->x * sk->cell_w,
                          sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        int idx2[GMB_FE_MAX_ENTRIES];
        int vlen2 = gmb_fe_visible(e, idx2, GMB_FE_MAX_ENTRIES);
        int body2 = c->height - 1; if (body2 < 0) body2 = 0;
        for (int i = 0; i < body2; i++)
        {
            int vi = e->offset + i;
            if (vi >= vlen2) break;
            gmb_fe_entry_t* en = &e->entries[idx2[vi]];
            char nm[320];
            gmb_cell_color_t efg = fg;
            if (e->show_icons)
            {
                gmb_icon_info_t info = gmb_file_theme_icon_for_path(en->path);
                snprintf(nm, sizeof(nm), "%s %s", info.icon, en->name);
                if (info.has_color) efg = info.color;
            }
            else snprintf(nm, sizeof(nm), "%s", en->name);
            char raw[400];
            snprintf(raw, sizeof(raw), "%s%s%s", vi == e->selected ? ">" : " ", en->is_dir ? "D " : "  ", nm);
            int sel2 = (vi == e->selected);
            if (sel2) gmb_surface_rounded_rect(buf, c->x, c->y + 1 + i, c->width, 1, 2, sbg, sbg, 1);
            gmb_gfx_draw_text(raw, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + 1 + i) * sk->cell_h + sk->cell_h / 4, 0,
                              sel2 ? (int)(0x40000000u | ((unsigned)(sfg).r << 16) | ((unsigned)(sfg).g << 8) | (sfg).b) : (int)(0x40000000u | ((unsigned)(efg).r << 16) | ((unsigned)(efg).g << 8) | (efg).b));
        }
        return;
    }

    char header[1152], hcased[1200];
    if (e->search_mode) snprintf(header, sizeof(header), "/%s", e->search_query);
    else snprintf(header, sizeof(header), "%s", e->current_path);
    gmb_control_apply_case(c, header, hcased, (int)sizeof(hcased));
    char hline[1200]; int ho = 0;
    for (const char* p = hcased; *p && ho < c->width && ho < (int)sizeof(hline) - 1; p++) hline[ho++] = *p;
    for (; ho < c->width && ho < (int)sizeof(hline) - 1; ho++) hline[ho] = ' ';
    hline[ho] = '\0';
    gmb_screen_setstring(buf, c->x, c->y, hline, fg, bg);

    int idx[GMB_FE_MAX_ENTRIES];
    int vlen = gmb_fe_visible(e, idx, GMB_FE_MAX_ENTRIES);
    int body = c->height - 1; if (body < 0) body = 0;
    for (int i = 0; i < body; i++)
    {
        int vi = e->offset + i;
        int rowY = c->y + 1 + i;
        if (vi >= vlen)
        {
            char blank[1200];
            int n = c->width; if (n > (int)sizeof(blank) - 1) n = (int)sizeof(blank) - 1;
            for (int k = 0; k < n; k++) blank[k] = ' ';
            blank[n] = '\0';
            gmb_screen_setstring(buf, c->x, rowY, blank, fg, bg);
            continue;
        }
        gmb_fe_entry_t* en = &e->entries[idx[vi]];
        char name[320];
        gmb_cell_color_t efg = fg;
        if (e->show_icons)
        {
            gmb_icon_info_t info = gmb_file_theme_icon_for_path(en->path);
            snprintf(name, sizeof(name), "%s %s", info.icon, en->name);
            if (info.has_color) efg = info.color;
        }
        else snprintf(name, sizeof(name), "%s", en->name);
        char raw[400], cased[420];
        snprintf(raw, sizeof(raw), "%s%s%s", vi == e->selected ? ">" : " ", en->is_dir ? "D " : "  ", name);
        gmb_control_apply_case(c, raw, cased, (int)sizeof(cased));
        char line[1200]; int o = 0;
        for (const char* p = cased; *p && o < c->width && o < (int)sizeof(line) - 1; p++) line[o++] = *p;
        for (; o < c->width && o < (int)sizeof(line) - 1; o++) line[o] = ' ';
        line[o] = '\0';
        if (vi == e->selected) gmb_screen_setstring(buf, c->x, rowY, line, sfg, sbg);
        else gmb_screen_setstring(buf, c->x, rowY, line, efg, bg);
    }
}

static bool gmb_fe_search_key(gmb_file_explorer_t* e, gmb_control_t* c, int key)
{
    int body = c->height - 1; if (body < 0) body = 0;
    switch (key)
    {
        case GMB_K_ESC:
            e->search_query[0] = '\0'; e->search_mode = false; e->selected = 0; e->offset = 0; return true;
        case GMB_K_ENTER:
            e->search_mode = false; e->selected = 0; e->offset = 0; return true;
        case GMB_K_BS:
        {
            int l = (int)strlen(e->search_query);
            if (l > 0) e->search_query[l - 1] = '\0';
            e->selected = 0; e->offset = 0; return true;
        }
        case GMB_K_UP:
            if (e->selected > 0) { e->selected--; gmb_fe_clamp(e, body); }
            return true;
        case GMB_K_DOWN:
        {
            int v = gmb_fe_count(e);
            if (v > 0 && e->selected + 1 < v) { e->selected++; gmb_fe_clamp(e, body); }
            return true;
        }
        default:
            if (key >= 32 && key < 127)
            {
                int l = (int)strlen(e->search_query);
                if (l < 127) { e->search_query[l] = (char)key; e->search_query[l + 1] = '\0'; }
                e->selected = 0; e->offset = 0;
            }
            return true;
    }
}

static bool gmb_fe_key(void* u, gmb_control_t* c, int key)
{
    gmb_file_explorer_t* e = (gmb_file_explorer_t*)u;
    if (!c->enabled) return false;
    e->last_action = GMB_FE_ACTION_NONE;
    e->selected_path[0] = '\0';
    int body = c->height - 1; if (body < 0) body = 0;
    if (e->search_mode) return gmb_fe_search_key(e, c, key);
    switch (key)
    {
        case GMB_K_UP:
            if (e->selected > 0) { e->selected--; gmb_fe_clamp(e, body); }
            return true;
        case GMB_K_DOWN:
        {
            int v = gmb_fe_count(e);
            if (v > 0 && e->selected + 1 < v) { e->selected++; gmb_fe_clamp(e, body); }
            return true;
        }
        case GMB_K_PGUP:
            e->selected -= body; if (e->selected < 0) e->selected = 0; gmb_fe_clamp(e, body); return true;
        case GMB_K_PGDN:
        {
            int v = gmb_fe_count(e);
            if (v > 0) { e->selected += body; if (e->selected > v - 1) e->selected = v - 1; gmb_fe_clamp(e, body); }
            return true;
        }
        case GMB_K_HOME: e->selected = 0; gmb_fe_clamp(e, body); return true;
        case GMB_K_END:
        {
            int v = gmb_fe_count(e);
            if (v > 0) { e->selected = v - 1; gmb_fe_clamp(e, body); }
            return true;
        }
        case '/': e->search_mode = true; return true;
        case GMB_K_ENTER: gmb_fe_activate(e); return true;
        case GMB_K_BS:
        {
            char parent[1024];
            gmb_fe_parent_of(e->current_path, parent, sizeof(parent));
            if (strlen(parent) > 0 && strcmp(parent, e->current_path) != 0)
            {
                snprintf(e->current_path, sizeof(e->current_path), "%s", parent);
                gmb_file_explorer_refresh(e);
                e->last_action = GMB_FE_ACTION_CHANGEDIR;
                snprintf(e->selected_path, sizeof(e->selected_path), "%s", parent);
            }
            return true;
        }
    }
    return false;
}


static bool gmb_fe_click_px(void* u, gmb_control_t* c, int x, int y);
static bool gmb_fe_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_file_explorer_t* e = (gmb_file_explorer_t*)u;
    if (!c->enabled) return false;
    if (a->kind == GMB_MA_SCROLLUP) { if (e->offset > 0) e->offset--; return true; }
    if (a->kind == GMB_MA_SCROLLDOWN)
    {
        int maxoff = gmb_fe_count(e) - (c->height - 2);
        if (maxoff < 0) maxoff = 0;
        if (e->offset < maxoff) e->offset++;
        return true;
    }
    if (a->kind == GMB_MA_CLICK || a->kind == GMB_MA_DOUBLECLICK)
        return gmb_fe_click_px(u, c, a->x, a->y);
    return false;
}

static void fileexplorer_draw_px(void* u, gmb_control_t* c)
{
    gmb_file_explorer_t* e = (gmb_file_explorer_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    unsigned acc = (st.accent & 0xFFFFFFu) ? st.accent : st.fill_press;
    int rh = st.row_h > 0 ? st.row_h : 22;
    int hh = rh + (soft ? 6 : 4);
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    /* barra del percorso */
    gmb_gfx_fill_rounded_rect(x + (soft ? 4 : 0), y + (soft ? 4 : 0), w - (soft ? 8 : 0), hh, soft ? hh / 2 : 0,
                              (st.head_bg & 0xFFFFFFu) | (0xFFu << 24));
    gmb_gfx_draw_text(e->current_path, x + (soft ? 14 : 6), y + (soft ? 4 : 0) + (hh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                      (int)(0x40000000u | (st.head_fg & 0xFFFFFFu)));
    int vis = rh > 0 ? (h - hh - (soft ? 6 : 0)) / rh : 0;
    int start = e->offset; if (start < 0) start = 0;
    for (int r = 0; r < vis && start + r < e->entry_count; r++) {
        int i = start + r, ry = y + hh + (soft ? 2 : 0) + r * rh;
        if (i == e->selected) gmb_gfx_fill_rounded_rect(x + 4, ry, w - 8, rh - 1, soft ? (rh - 1) / 2 : 0, (acc & 0xFFFFFFu) | (0x22u << 24));
        char nm[280]; snprintf(nm, sizeof(nm), "%s%s", e->entries[i].is_dir ? "[D] " : "    ", e->entries[i].name);
        gmb_gfx_draw_text(nm, x + (soft ? 14 : 6), ry + (rh - (int)(gmb_gfx_text_height() / (gmb_gfx_render_scale() > 0 ? gmb_gfx_render_scale() : 1.0f))) / 2, 0,
                          (int)(0x40000000u | ((e->entries[i].is_dir ? acc : st.text) & 0xFFFFFFu)));
    }
}
static bool gmb_fe_click_px(void* u, gmb_control_t* c, int x, int y)
{
    gmb_file_explorer_t* e = (gmb_file_explorer_t*)u;
    if (!c->enabled) return false;
    int rx = c->px_w > 0 ? c->px_x : c->x;
    int ry0 = c->px_h > 0 ? c->px_y : c->y;
    int rw = c->px_w > 0 ? c->px_w : c->width;
    int rhh = c->px_h > 0 ? c->px_h : c->height;
    if (x < rx || x >= rx + rw || y < ry0 || y >= ry0 + rhh) return false;
    c->focused = true;
    gmb_widget_style_t st = gmb_widget_style_current();
    int rh = st.row_h > 0 ? st.row_h : 22;
    int hh = rh + (st.shape == 1 ? 6 : 4);
    int row = (y - ry0 - hh - (st.shape == 1 ? 2 : 0)) / rh;
    if (row < 0) return true;
    int v = gmb_fe_count(e);
    int idx = e->offset + row;
    if (idx >= 0 && idx < v) { e->selected = idx; gmb_fe_clamp(e, rhh - 1); gmb_fe_activate(e); }
    return true;
}

void gmb_file_explorer_init(gmb_file_explorer_t* e, const char* path)
{
    memset(e, 0, sizeof(*e));
    memset(e, 0, sizeof(*e));
    gmb_control_init(&e->ctrl, "", 0, 0, 0, 10, gmb_fe_draw, gmb_fe_key);
    e->ctrl.draw_px_fn = fileexplorer_draw_px;
    e->ctrl.user = e;
    e->ctrl.handle_click_fn = gmb_fe_click_px;
    e->ctrl.handle_mouse_fn = gmb_fe_mouse;
    e->show_icons = true;
    e->selected = 0;
    e->last_action = GMB_FE_ACTION_NONE;
    snprintf(e->current_path, sizeof(e->current_path), "%s", path ? path : ".");
    gmb_file_explorer_refresh(e);
}

void gmb_file_explorer_set_bounds(gmb_file_explorer_t* e, int x, int y, int width, int height)
{
    e->ctrl.x = x; e->ctrl.y = y; e->ctrl.width = width; e->ctrl.height = height;
}
int gmb_file_explorer_last_action(const gmb_file_explorer_t* e) { return e ? e->last_action : 0; }
const char* gmb_file_explorer_selected_path(const gmb_file_explorer_t* e) { return e ? e->selected_path : NULL; }
bool gmb_file_explorer_search_mode(const gmb_file_explorer_t* e) { return e ? e->search_mode : false; }

void gmb_file_explorer_refresh(gmb_file_explorer_t* e)
{
    e->entry_count = 0;
    e->selected = 0;
    e->offset = 0;
    char parent[1024];
    gmb_fe_parent_of(e->current_path, parent, sizeof(parent));
    if (strlen(parent) > 0 && strcmp(parent, e->current_path) != 0)
    {
        gmb_fe_entry_t* up = &e->entries[e->entry_count++];
        snprintf(up->name, sizeof(up->name), "..");
        up->is_dir = true;
        snprintf(up->path, sizeof(up->path), "%s", parent);
    }
    DIR* dir = opendir(e->current_path);
    if (!dir) return;
    struct dirent* de;
    while ((de = readdir(dir)) != NULL && e->entry_count < GMB_FE_MAX_ENTRIES)
    {
        if (de->d_name[0] == '.')
        {
            if (!e->show_hidden || strcmp(de->d_name, ".") == 0) continue;
        }
        gmb_fe_entry_t* en = &e->entries[e->entry_count];
        snprintf(en->name, sizeof(en->name), "%s", de->d_name);
        snprintf(en->path, sizeof(en->path), "%s/%s", e->current_path, de->d_name);
        en->is_dir = de->d_type == DT_DIR;
        e->entry_count++;
    }
    closedir(dir);
    if (e->entry_count > 0)
        qsort(e->entries, (size_t)e->entry_count, sizeof(gmb_fe_entry_t), gmb_fe_entry_cmp);
}

void gmb_file_explorer_set_path(gmb_file_explorer_t* e, const char* path)
{
    snprintf(e->current_path, sizeof(e->current_path), "%s", path ? path : ".");
    gmb_file_explorer_refresh(e);
}
int gmb_file_explorer_entry_count(gmb_file_explorer_t* e) { return e->entry_count; }
int gmb_file_explorer_selected_index(gmb_file_explorer_t* e) { return e->selected; }
const char* gmb_file_explorer_get_selected(gmb_file_explorer_t* e)
{
    if (e->selected < 0 || e->selected >= e->entry_count) return NULL;
    return e->entries[e->selected].path;
}
void gmb_file_explorer_next(gmb_file_explorer_t* e)
{
    if (e->entry_count == 0) return;
    if (e->selected < e->entry_count - 1) e->selected++;
}
void gmb_file_explorer_prev(gmb_file_explorer_t* e)
{
    if (e->entry_count == 0) return;
    if (e->selected > 0) e->selected--;
}
void gmb_file_explorer_set_search(gmb_file_explorer_t* e, const char* query)
{
    snprintf(e->search_query, sizeof(e->search_query), "%s", query ? query : "");
}
bool gmb_file_explorer_navigate_into(gmb_file_explorer_t* e)
{
    const char* sel = gmb_file_explorer_get_selected(e);
    if (!sel) return false;
    for (int i = 0; i < e->entry_count; i++)
    {
        if (strcmp(e->entries[i].path, sel) == 0)
        {
            if (!e->entries[i].is_dir) return false;
            gmb_file_explorer_set_path(e, sel);
            return true;
        }
    }
    return false;
}
int gmb_file_explorer_offset(const gmb_file_explorer_t* e) { return e ? e->offset : 0; }
int gmb_file_explorer_filter_entries(const gmb_file_explorer_t* e, const char* query,
                                     int* out_idx, int* out_is_dir, int cap)
{
    if (!e || !query || !out_idx || cap <= 0) return 0;
    int n = 0;
    for (int i = 0; i < e->entry_count && n < cap; i++) {
        if (strcasestr(e->entries[i].name, query)) {
            out_idx[n] = i;
            if (out_is_dir) out_is_dir[n] = e->entries[i].is_dir ? 1 : 0;
            n++;
        }
    }
    return n;
}
bool gmb_file_explorer_navigate(gmb_file_explorer_t* e, int delta)
{
    if (!e || e->entry_count == 0) return false;
    int next = e->selected + delta;
    if (next < 0) next = 0;
    if (next >= e->entry_count) next = e->entry_count - 1;
    e->selected = next;
    return true;
}
