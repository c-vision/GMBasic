/* ============================================================================
 * gmb_tui_printerdialog.c — PrinterDialog.cs (specchio ClipperLib/Controls).
 * Control: Draw (bordo/titolo/lista/copie/duplex/bottoni), HandleKey, HandleClick.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GMB_PD_FOCUS_PRINTERS = 0, GMB_PD_FOCUS_OK, GMB_PD_FOCUS_CANCEL };

static void pd_border_chars(int border, uint32_t* tl, uint32_t* th, uint32_t* tr,
                            uint32_t* vl, uint32_t* br, uint32_t* bh, uint32_t* bl, uint32_t* vr,
                            uint32_t* sepl, uint32_t* sepr)
{
    if (border == GMB_WM_BORDER_DOUBLE)
    {
        *tl = 0x2554; *th = 0x2550; *tr = 0x2557; *vl = 0x2551;
        *bl = 0x255A; *bh = 0x2550; *br = 0x255D; *vr = 0x2551;
        *sepl = 0x2560; *sepr = 0x2563;
    }
    else if (border == GMB_WM_BORDER_ROUNDED)
    {
        *tl = 0x256D; *th = 0x2500; *tr = 0x256E; *vl = 0x2502;
        *bl = 0x2570; *bh = 0x2500; *br = 0x256F; *vr = 0x2502;
        *sepl = 0x251C; *sepr = 0x2524;
    }
    else
    {
        *tl = 0x250C; *th = 0x2500; *tr = 0x2510; *vl = 0x2502;
        *bl = 0x2514; *bh = 0x2500; *br = 0x2518; *vr = 0x2502;
        *sepl = 0x251C; *sepr = 0x2524;
    }
}

static void pd_draw_sep(gmb_screen_buffer_t* buf, int x, int y, int width, uint32_t th,
                        uint32_t sepl, uint32_t sepr, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    gmb_screen_setchar(buf, x, y, sepl, fg, bg);
    for (int i = x + 1; i < x + width - 1; i++) gmb_screen_setchar(buf, i, y, th, fg, bg);
    gmb_screen_setchar(buf, x + width - 1, y, sepr, fg, bg);
}

static void pd_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_printer_dialog_t* d = (gmb_printer_dialog_t*)u;
    gmb_cell_color_t fg, bg, sfg, sbg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &sfg, &sbg);
    uint32_t tl, th, tr, vl, br, bh, bl, vr, sepl, sepr;
    pd_border_chars(d->border, &tl, &th, &tr, &vl, &br, &bh, &bl, &vr, &sepl, &sepr);
    int W = c->width, H = c->height;
    if (W < 2 || H < 2) return;
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        gmb_surface_rounded_rect(buf, c->x, c->y, W, H, 6, fg, bg, 1);
        if (d->title[0])
            gmb_gfx_draw_text(d->title, sk->origin_x + (c->x + 1) * sk->cell_w,
                              sk->origin_y + c->y * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        int lr = d->printers_count < H - 4 ? d->printers_count : H - 4;
        if (lr < 0) lr = 0;
        for (int i = 0; i < lr; i++)
        {
            int sel2 = (i == d->selected_index);
            if (sel2) gmb_surface_rounded_rect(buf, c->x + 1, c->y + 1 + i, W - 2, 1, 2, sbg, sbg, 1);
            gmb_gfx_draw_text(d->printers[i], sk->origin_x + (c->x + 1) * sk->cell_w,
                              sk->origin_y + (c->y + 1 + i) * sk->cell_h + sk->cell_h / 4, 0,
                              sel2 ? (int)(0x40000000u | ((unsigned)(sfg).r << 16) | ((unsigned)(sfg).g << 8) | (sfg).b) : (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        }
        char info2[160];
        snprintf(info2, sizeof(info2), "Copie: [-] %3d [+]  %s R/F (D)", (int)d->copies, d->duplex ? "[X]" : "[ ]");
        gmb_gfx_draw_text(info2, sk->origin_x + (c->x + 1) * sk->cell_w,
                          sk->origin_y + (c->y + 1 + lr + 1) * sk->cell_h + sk->cell_h / 4, 0, (int)(0x40000000u | ((unsigned)(fg).r << 16) | ((unsigned)(fg).g << 8) | (fg).b));
        gmb_button_t ok2, cancel2;
        gmb_button_init(&ok2, c->x + 2, c->y + H - 2, 8, d->ok_label);
        ok2.ctrl.focused = (d->btn_focus == GMB_PD_FOCUS_OK);
        gmb_button_init(&cancel2, c->x + W - 10, c->y + H - 2, 8, d->cancel_label);
        cancel2.ctrl.focused = (d->btn_focus == GMB_PD_FOCUS_CANCEL);
        gmb_control_draw(&ok2.ctrl, buf);
        gmb_control_draw(&cancel2.ctrl, buf);
        return;
    }

    gmb_screen_setchar(buf, c->x, c->y, tl, fg, bg);
    gmb_screen_setchar(buf, c->x + W - 1, c->y, tr, fg, bg);
    gmb_screen_setchar(buf, c->x, c->y + H - 1, bl, fg, bg);
    gmb_screen_setchar(buf, c->x + W - 1, c->y + H - 1, br, fg, bg);
    for (int x = c->x + 1; x < c->x + W - 1; x++)
    {
        gmb_screen_setchar(buf, x, c->y, th, fg, bg);
        gmb_screen_setchar(buf, x, c->y + H - 1, bh, fg, bg);
    }
    for (int y = c->y + 1; y < c->y + H - 1; y++)
    {
        gmb_screen_setchar(buf, c->x, y, vl, fg, bg);
        for (int x = c->x + 1; x < c->x + W - 1; x++) gmb_screen_setchar(buf, x, y, ' ', fg, bg);
        gmb_screen_setchar(buf, c->x + W - 1, y, vr, fg, bg);
    }
    if (d->title[0])
    {
        char t[96];
        snprintf(t, sizeof(t), " %s ", d->title);
        if ((int)strlen(t) < W - 2) gmb_screen_setstring(buf, c->x + 1, c->y, t, fg, bg);
    }
    int listRows = d->printers_count < H - 4 ? d->printers_count : H - 4;
    if (listRows < 0) listRows = 0;
    for (int i = 0; i < listRows; i++)
    {
        int selected = (i == d->selected_index);
        char raw[160], line[160];
        snprintf(raw, sizeof(raw), "%s %s", selected ? ">" : " ", d->printers[i]);
        int o = 0;
        for (const char* p = raw; *p && o < W - 2; p++) line[o++] = *p;
        for (; o < W - 2; o++) line[o] = ' ';
        line[o] = '\0';
        gmb_screen_setstring(buf, c->x + 1, c->y + 1 + i, line, selected ? sfg : fg, selected ? sbg : bg);
    }
    int sep1Y = c->y + 1 + listRows;
    pd_draw_sep(buf, c->x, sep1Y, W, th, sepl, sepr, fg, bg);
    int infoY = sep1Y + 1;
    char info[160];
    snprintf(info, sizeof(info), "Copie: [-] %3d [+]  %s R/F (D)", (int)d->copies, d->duplex ? "[X]" : "[ ]");
    gmb_screen_setstring(buf, c->x + 1, infoY, info, fg, bg);

    int sep2Y = c->y + H - 3;
    pd_draw_sep(buf, c->x, sep2Y, W, th, sepl, sepr, fg, bg);

    gmb_button_t ok, cancel;
    gmb_button_init(&ok, c->x + 2, c->y + H - 2, 8, d->ok_label);
    ok.ctrl.focused = (d->btn_focus == GMB_PD_FOCUS_OK);
    gmb_button_init(&cancel, c->x + W - 10, c->y + H - 2, 8, d->cancel_label);
    cancel.ctrl.focused = (d->btn_focus == GMB_PD_FOCUS_CANCEL);
    gmb_control_draw(&ok.ctrl, buf);
    gmb_control_draw(&cancel.ctrl, buf);
}

static bool pd_key(void* u, gmb_control_t* c, int key)
{
    (void)c;
    gmb_printer_dialog_t* d = (gmb_printer_dialog_t*)u;
    switch (key)
    {
        case GMB_K_UP: if (d->selected_index > 0) d->selected_index--; return true;
        case GMB_K_DOWN: if (d->selected_index + 1 < d->printers_count) d->selected_index++; return true;
        case '+': d->copies = (unsigned short)(d->copies + 1 > 999 ? 999 : d->copies + 1); return true;
        case '-': d->copies = (unsigned short)(d->copies > 1 ? d->copies - 1 : 1); return true;
        case 'd': case 'D': d->duplex = !d->duplex; return true;
        case GMB_K_TAB:
            if (d->btn_focus == GMB_PD_FOCUS_PRINTERS) { d->btn_focus = GMB_PD_FOCUS_OK; return true; }
            if (d->btn_focus == GMB_PD_FOCUS_OK) { d->btn_focus = GMB_PD_FOCUS_CANCEL; return true; }
            return false;
        case GMB_K_BTAB:
            if (d->btn_focus == GMB_PD_FOCUS_CANCEL) { d->btn_focus = GMB_PD_FOCUS_OK; return true; }
            if (d->btn_focus == GMB_PD_FOCUS_OK) { d->btn_focus = GMB_PD_FOCUS_PRINTERS; return true; }
            return false;
        case GMB_K_ENTER: case ' ':
            if (d->btn_focus == GMB_PD_FOCUS_CANCEL) d->confirmed = false;
            else d->confirmed = true;
            return true;
        case GMB_K_ESC: d->confirmed = false; return true;
    }
    return false;
}

static bool pd_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_printer_dialog_t* d = (gmb_printer_dialog_t*)u;
    if (!c->enabled) return false;
    int W = c->width, H = c->height;
    gmb_rect_t okr = { c->x + 2, c->y + H - 2, 8, 1 };
    gmb_rect_t car = { c->x + W - 10, c->y + H - 2, 8, 1 };
    if (rect_contains(okr, x, y)) { c->focused = true; d->confirmed = true; return true; }
    if (rect_contains(car, x, y)) { c->focused = false; d->confirmed = false; return true; }
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int relY = y - (c->y + 1);
    int listRows = d->printers_count < H - 4 ? d->printers_count : H - 4;
    if (listRows < 0) listRows = 0;
    if (relY == listRows + 1)
    {
        int rx = x - (c->x + 1);
        if (rx >= 7 && rx <= 9) { d->copies = (unsigned short)(d->copies > 1 ? d->copies - 1 : 1); return true; }
        if (rx >= 15 && rx <= 17) { d->copies = (unsigned short)(d->copies + 1 > 999 ? 999 : d->copies + 1); return true; }
        if (rx >= 19 && rx <= 21) { d->duplex = !d->duplex; return true; }
    }
    if (relY >= 0 && relY < listRows) { d->selected_index = relY; return true; }
    return true;
}

void gmb_printer_dialog_init(gmb_printer_dialog_t* d)
{
    memset(d, 0, sizeof(*d));
    gmb_control_init(&d->ctrl, "", 0, 0, 40, 8, pd_draw, pd_key);
    d->ctrl.user = d;
    d->ctrl.handle_click_fn = pd_click;
    d->copies = 1;
    d->border = GMB_WM_BORDER_DOUBLE;
    d->btn_focus = GMB_PD_FOCUS_PRINTERS;
    snprintf(d->title, sizeof(d->title), "Select Printer");
    snprintf(d->ok_label, sizeof(d->ok_label), "OK");
    snprintf(d->cancel_label, sizeof(d->cancel_label), "Cancel");
    /* C# PrinterDialog.Printers default. */
    gmb_printer_dialog_add_printer(d, "Default");
    gmb_printer_dialog_add_printer(d, "PDF");
    gmb_printer_dialog_add_printer(d, "Office Laser");
    gmb_printer_dialog_add_printer(d, "Lab Inkjet");
}

void gmb_printer_dialog_set_bounds(gmb_printer_dialog_t* d, int x, int y, int width, int height)
{
    d->ctrl.x = x; d->ctrl.y = y; d->ctrl.width = width; d->ctrl.height = height;
}

void gmb_printer_dialog_add_printer(gmb_printer_dialog_t* d, const char* name)
{
    if (d->printers_count >= GMB_PRINTERS_MAX) return;
    snprintf(d->printers[d->printers_count++], 128, "%s", name);
}

const char* gmb_printer_dialog_selected_printer(gmb_printer_dialog_t* d)
{
    if (d->printers_count == 0) return NULL;
    int idx = d->selected_index < 0 ? 0 : (d->selected_index >= d->printers_count ? d->printers_count - 1 : d->selected_index);
    return d->printers[idx];
}

void gmb_printer_dialog_centered(gmb_printer_dialog_t* d, int term_w, int term_h, int rows)
{
    int w = 40;
    int h = 8 + rows;
    int x = (term_w - w) / 2;
    int y = (term_h - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    gmb_printer_dialog_set_bounds(d, x, y, w, h);
}
