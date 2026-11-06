/* ============================================================================
 * gmb_tui_datepicker.c — gmb_tui_datepicker. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_tui_datatypes.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== DatePicker ===================== */

static const char* DP_MONTHS2[12] = {
    "Gennaio","Febbraio","Marzo","Aprile","Maggio","Giugno",
    "Luglio","Agosto","Settembre","Ottobre","Novembre","Dicembre"
};

static bool dp2_is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int dp2_days_in_month(int y, int m)
{
    switch (m) { case 2: return dp2_is_leap(y) ? 29 : 28; case 4: case 6: case 9: case 11: return 30; default: return 31; }
}

static void dp2_draw_popup(gmb_date_picker_t* d, gmb_screen_buffer_t* buf, gmb_cell_color_t fg, gmb_cell_color_t bg, int focused)
{
    if (!d->open) return;
    int cal_w = d->ctrl.width;
    int cal_h = d->popup_height;
    int py = d->ctrl.y - cal_h;
    if (py < 0) py = d->ctrl.y + 1;
    int px = d->ctrl.x;
    for (int ry = py; ry < py + cal_h; ry++)
        for (int rx = px; rx < px + cal_w; rx++)
            gmb_screen_setchar(buf, rx, ry, ' ', GMB_CELLCOLOR_DEFAULT, bg);
    gmb_cell_color_t bfg, bbg;
    if (focused) { bfg = GMB_CELLCOLOR_BLACK; bbg = GMB_CELLCOLOR_YELLOW; }
    else { bfg = fg; bbg = bg; }
    gmb_screen_setchar(buf, px, py, 0x250C, bfg, bbg);
    gmb_screen_setchar(buf, px + cal_w - 1, py, 0x2510, bfg, bbg);
    gmb_screen_setchar(buf, px, py + cal_h - 1, 0x2514, bfg, bbg);
    gmb_screen_setchar(buf, px + cal_w - 1, py + cal_h - 1, 0x2518, bfg, bbg);
    for (int i = 1; i < cal_w - 1; i++)
    {
        gmb_screen_setchar(buf, px + i, py, 0x2500, bfg, bbg);
        gmb_screen_setchar(buf, px + i, py + cal_h - 1, 0x2500, bfg, bbg);
    }
    for (int i = 1; i < cal_h - 1; i++)
    {
        gmb_screen_setchar(buf, px, py + i, 0x2502, bfg, bbg);
        gmb_screen_setchar(buf, px + cal_w - 1, py + i, 0x2502, bfg, bbg);
    }
    char title[64];
    snprintf(title, sizeof(title), " %s %d ", DP_MONTHS2[d->month >= 1 && d->month <= 12 ? d->month - 1 : 0], d->year);
    int max_title = (int)strlen(title) < cal_w - 2 ? (int)strlen(title) : cal_w - 2;
    title[max_title] = '\0';
    gmb_screen_setstring(buf, px + 1, py, title, bfg, bbg);
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "Lu Ma Me Gi Ve Sa Do");
    int max_hdr = (int)strlen(hdr) < cal_w - 2 ? (int)strlen(hdr) : cal_w - 2;
    hdr[max_hdr] = '\0';
    gmb_screen_setstring(buf, px + 1, py + 1, hdr, fg, bg);
    int dow = (gmb_dates_weekday(d->year, d->month, 1) + 6) % 7;
    int days = dp2_days_in_month(d->year, d->month);
    int dy = py + 2;
    for (int dnum = 1; dnum <= days; dnum++)
    {
        bool is_sel = dnum == d->day;
        gmb_cell_color_t f = is_sel ? GMB_CELLCOLOR_WHITE : fg;
        gmb_cell_color_t b = is_sel ? GMB_CELLCOLOR_BLUE : bg;
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%02d", dnum);
        int cell_x = px + 1 + dow * 3;
        gmb_screen_setstring(buf, cell_x, dy, lbl, f, b);
        dow++;
        if (dow >= 7) { dow = 0; dy++; }
    }
}
void gmb_date_picker_draw_popup(gmb_date_picker_t* d, gmb_screen_buffer_t* buffer)
{
    if (!d->open) return;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&d->ctrl.style, d->ctrl.focused, d->ctrl.has_error, false, &fg, &bg);
    dp2_draw_popup(d, buffer, fg, bg, d->ctrl.focused);
}
static void datepicker_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_date_picker_t* d = (gmb_date_picker_t*)u;
    if (!c->enabled) return;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    char label[32];
    snprintf(label, sizeof(label), "%02d/%02d/%d", d->day, d->month, d->year);
    gmb_screen_setstring(buf, c->x, c->y, label, fg, bg);
    int arrow_x = c->x + (int)strlen(label) + 1;
    if (arrow_x < c->x + c->width)
        gmb_screen_setchar(buf, arrow_x, c->y, 0x25BC, fg, bg);
    gmb_date_picker_draw_popup(d, buf);
}
static bool datepicker_key(void* u, gmb_control_t* c, int key)
{
    gmb_date_picker_t* d = (gmb_date_picker_t*)u;
    if (!c->enabled) return false;
    if (d->open)
    {
        switch (key)
        {
            case GMB_K_LEFT:
                d->day--;
                if (d->day < 1) { d->month--; if (d->month < 1) { d->month = 12; d->year--; } d->day = dp2_days_in_month(d->year, d->month); }
                return true;
            case GMB_K_RIGHT:
                d->day++;
                if (d->day > dp2_days_in_month(d->year, d->month)) { d->day = 1; d->month++; if (d->month > 12) { d->month = 1; d->year++; } }
                return true;
            case GMB_K_UP:
                d->day -= 7;
                if (d->day < 1) { d->month--; if (d->month < 1) { d->month = 12; d->year--; } d->day += dp2_days_in_month(d->year, d->month); }
                return true;
            case GMB_K_DOWN:
            {
                int max_d = dp2_days_in_month(d->year, d->month);
                d->day += 7;
                if (d->day > max_d) { d->day -= max_d; d->month++; if (d->month > 12) { d->month = 1; d->year++; } }
                return true;
            }
            case GMB_K_ESC:
                d->open = false;
                return true;
            case GMB_K_ENTER:
                d->open = false;
                if (d->on_date_selected) d->on_date_selected(d->date_ctx, d->year, d->month, d->day);
                return true;
        }
        return false;
    }
    if (key == GMB_K_ENTER) { d->open = true; return true; }
    return false;
}
static bool datepicker_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_date_picker_t* d = (gmb_date_picker_t*)u;
    if (!c->enabled) return false;
    if (a->kind == GMB_MA_CLICK || a->kind == GMB_MA_DOUBLECLICK)
    {
        if (d->open)
        {
            if (!gmb_date_picker_contains_point(d, a->x, a->y)) { d->open = false; return true; }
        }
        else
        {
            gmb_rect_t r = gmb_control_rect(c);
            if (rect_contains(r, a->x, a->y)) { d->open = true; return true; }
        }
    }
    return false;
}
static void datepicker_draw_px(void* u, gmb_control_t* c)
{
    gmb_date_picker_t* d = (gmb_date_picker_t*)u;
    char buf[32]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d->year, d->month, d->day);
    gmb_px_draw_ex(GMB_PX_DROPDOWN, c->x, c->y, c->width, c->height, buf, NULL, 0, 0, 0, 0, NULL, 0, NULL, 0);
}
void gmb_date_picker_init(gmb_date_picker_t* d, int x, int y, int w)
{
    memset(d, 0, sizeof(*d));
    gmb_control_init(&d->ctrl, "", x, y, w, 1, datepicker_draw, datepicker_key);
    d->ctrl.draw_px_fn = datepicker_draw_px;
    d->ctrl.ctrl_kind = GMB_CK_DATEPICKER;
    d->ctrl.handle_mouse_fn = datepicker_mouse;
    d->ctrl.user = d;
    d->day = 1; d->month = 1; d->year = 2025;
    d->popup_height = 10;
}
bool gmb_date_picker_contains_point(gmb_date_picker_t* d, int x, int y)
{
    if (!d->open) return false;
    int cal_h = d->popup_height;
    int cal_w = d->ctrl.width;
    int py = d->ctrl.y - cal_h;
    if (py < 0) py = d->ctrl.y + 1;
    return x >= d->ctrl.x && x < d->ctrl.x + cal_w && y >= py && y < py + cal_h;
}
int gmb_date_picker_get_formatted(gmb_date_picker_t* d, gmb_clipper_state_t* st, char* out, int cap)
{
    const char* fmt = st ? st->set_date_format : "mm-dd-yyyy";
    bool century = st ? st->set_century : false;
    int yy = century ? d->year : d->year % 100;
    char f2[16];
    snprintf(f2, sizeof(f2), "%s", fmt);
    for (char* p = f2; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    if (strcmp(f2, "dd/mm/yyyy") == 0) snprintf(out, cap, "%02d/%02d/%d", d->day, d->month, d->year);
    else if (strcmp(f2, "dd/mm/yy") == 0) snprintf(out, cap, "%02d/%02d/%02d", d->day, d->month, yy);
    else if (strcmp(f2, "mm/dd/yyyy") == 0) snprintf(out, cap, "%02d/%02d/%d", d->month, d->day, d->year);
    else if (strcmp(f2, "mm/dd/yy") == 0) snprintf(out, cap, "%02d/%02d/%02d", d->month, d->day, yy);
    else if (strcmp(f2, "yyyy-mm-dd") == 0) snprintf(out, cap, "%d-%02d-%02d", d->year, d->month, d->day);
    else if (strcmp(f2, "yy-mm-dd") == 0) snprintf(out, cap, "%02d-%02d-%02d", yy, d->month, d->day);
    else if (strcmp(f2, "dd-mm-yyyy") == 0) snprintf(out, cap, "%02d-%02d-%d", d->day, d->month, d->year);
    else if (strcmp(f2, "dd-mm-yy") == 0) snprintf(out, cap, "%02d-%02d-%02d", d->day, d->month, yy);
    else if (strcmp(f2, "mm-dd-yyyy") == 0) snprintf(out, cap, "%02d-%02d-%d", d->month, d->day, d->year);
    else if (strcmp(f2, "mm-dd-yy") == 0) snprintf(out, cap, "%02d-%02d-%02d", d->month, d->day, yy);
    else snprintf(out, cap, "%02d/%02d/%d", d->month, d->day, d->year);
    return (int)strlen(out);
}

/* ===================== Panel / Window ===================== */

void gmb_border_chars(int border, uint32_t out[8])
{
    switch (border)
    {
        case GMB_PB_DOUBLE:
            out[0]=0x2554; out[1]=0x2550; out[2]=0x2557; out[3]=0x2551;
            out[4]=0x255D; out[5]=0x2550; out[6]=0x255A; out[7]=0x2551;
            break;
        case GMB_PB_ROUNDED:
            out[0]=0x256D; out[1]=0x2500; out[2]=0x256E; out[3]=0x2502;
            out[4]=0x256F; out[5]=0x2500; out[6]=0x2570; out[7]=0x2502;
            break;
        case GMB_PB_NONE:
            for (int i=0;i<8;i++) out[i]=' ';
            break;
        default:
            out[0]=0x250C; out[1]=0x2500; out[2]=0x2510; out[3]=0x2502;
            out[4]=0x2518; out[5]=0x2500; out[6]=0x2514; out[7]=0x2502;
            break;
    }
}

gmb_rect_t gmb_panel_inner(gmb_control_t* c, int border)
{
    gmb_rect_t r = { c->x, c->y, c->width, c->height };
    if (border == GMB_PB_NONE) return r;
    r.x = c->x + 1; r.y = c->y + 1; r.w = c->width - 2; r.h = c->height - 2;
    return r;
}

static void panel_draw_w(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_panel_w_t* p = (gmb_panel_w_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    if (p->border == GMB_PB_NONE) return;
    uint32_t bc[8];
    gmb_border_chars(p->border, bc);
    int x = c->x, y = c->y, w = c->width, h = c->height;
    gmb_screen_setchar(buf, x, y, bc[0], fg, bg);
    gmb_screen_setchar(buf, x + w - 1, y, bc[2], fg, bg);
    gmb_screen_setchar(buf, x, y + h - 1, bc[6], fg, bg);
    gmb_screen_setchar(buf, x + w - 1, y + h - 1, bc[4], fg, bg);
    for (int xx = x + 1; xx < x + w - 1; xx++)
    {
        gmb_screen_setchar(buf, xx, y, bc[1], fg, bg);
        gmb_screen_setchar(buf, xx, y + h - 1, bc[5], fg, bg);
    }
    for (int yy = y + 1; yy < y + h - 1; yy++)
    {
        gmb_screen_setchar(buf, x, yy, bc[3], fg, bg);
        for (int xx = x + 1; xx < x + w - 1; xx++) gmb_screen_setchar(buf, xx, yy, ' ', fg, bg);
        gmb_screen_setchar(buf, x + w - 1, yy, bc[7], fg, bg);
    }
    if (p->title[0])
    {
        char t[128];
        snprintf(t, sizeof(t), " %s ", p->title);
        if ((int)strlen(t) < w - 2)
            gmb_screen_setstring(buf, x + 1, y, t, fg, bg);
    }
}
static bool panel_mouse_w(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_panel_w_t* p = (gmb_panel_w_t*)u;
    switch (a->kind)
    {
        case GMB_MA_CLICK:
            if (p->draggable && a->y == c->y && a->x >= c->x && a->x < c->x + c->width)
            {
                c->focused = true;
                p->is_dragging = true;
                p->drag_off_x = a->x - c->x;
                p->drag_off_y = a->y - c->y;
                return true;
            }
            return false;
        case GMB_MA_DRAG:
            if (p->is_dragging && p->draggable)
            {
                c->x = a->x - p->drag_off_x;
                c->y = a->y - p->drag_off_y;
                return true;
            }
            return false;
        case GMB_MA_RELEASE:
            p->is_dragging = false;
            return true;
    }
    return false;
}
void gmb_panel_w_init(gmb_panel_w_t* p, int x, int y, int w, int h)
{
    memset(p, 0, sizeof(*p));
    gmb_control_init(&p->ctrl, "", x, y, w, h, panel_draw_w, NULL);
    p->ctrl.handle_mouse_fn = panel_mouse_w;
    p->ctrl.user = p;
    p->border = GMB_PB_SINGLE;
    p->draggable = true;
}

static void window_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_window_t* w = (gmb_window_t*)u;
    if (!c->visible) return;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    uint32_t bc[8];
    gmb_border_chars(w->border, bc);
    int x = c->x, y = c->y, ww = c->width, hh = c->height;
    gmb_screen_setchar(buf, x, y, bc[0], fg, bg);
    gmb_screen_setchar(buf, x + ww - 1, y, bc[2], fg, bg);
    gmb_screen_setchar(buf, x, y + hh - 1, bc[6], fg, bg);
    gmb_screen_setchar(buf, x + ww - 1, y + hh - 1, bc[4], fg, bg);
    for (int xx = x + 1; xx < x + ww - 1; xx++)
    {
        gmb_screen_setchar(buf, xx, y, bc[1], fg, bg);
        gmb_screen_setchar(buf, xx, y + hh - 1, bc[5], fg, bg);
    }
    for (int yy = y + 1; yy < y + hh - 1; yy++)
    {
        gmb_screen_setchar(buf, x, yy, bc[3], fg, bg);
        gmb_screen_setchar(buf, x + ww - 1, yy, bc[7], fg, bg);
    }
    if (w->title[0])
    {
        char t[128];
        snprintf(t, sizeof(t), " %s ", w->title);
        if ((int)strlen(t) < ww - 2)
            gmb_screen_setstring(buf, x + 1, y, t, fg, bg);
        if (w->closable && ww >= 4)
            gmb_screen_setstring(buf, x + ww - 3, y, " X ", fg, bg);
    }
}
static bool window_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_window_t* w = (gmb_window_t*)u;
    switch (a->kind)
    {
        case GMB_MA_CLICK:
            if (w->draggable && a->y == c->y && a->x >= c->x && a->x < c->x + c->width)
            {
                c->focused = true;
                w->is_dragging = true;
                w->drag_off_x = a->x - c->x;
                w->drag_off_y = a->y - c->y;
                return true;
            }
            return false;
        case GMB_MA_DRAG:
            if (w->is_dragging && w->draggable)
            {
                c->x = a->x - w->drag_off_x;
                c->y = a->y - w->drag_off_y;
                return true;
            }
            return false;
        case GMB_MA_RELEASE:
            w->is_dragging = false;
            return true;
    }
    return false;
}
void gmb_window_init(gmb_window_t* w, int x, int y, int ww, int h)
{
    memset(w, 0, sizeof(*w));
    gmb_control_init(&w->ctrl, "", x, y, ww, h, window_draw, NULL);
    w->ctrl.handle_mouse_fn = window_mouse;
    w->ctrl.user = w;
    w->border = GMB_PB_SINGLE;
    w->closable = true;
    w->draggable = true;
}
bool gmb_window_title_bar_contains(gmb_window_t* w, int cx, int cy)
{
    return cy == w->ctrl.y && cx >= w->ctrl.x && cx < w->ctrl.x + w->ctrl.width;
}
bool gmb_window_close_button_contains(gmb_window_t* w, int cx, int cy)
{
    return cy == w->ctrl.y && cx >= w->ctrl.x + w->ctrl.width - 3 && cx <= w->ctrl.x + w->ctrl.width - 1;
}
