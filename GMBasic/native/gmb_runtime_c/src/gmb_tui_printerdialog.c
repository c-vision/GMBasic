/* ============================================================================
 * gmb_tui_printerdialog.c — PrinterDialog.cs (specchio ClipperLib/Controls).
 * Control: Draw (bordo/titolo/lista/copie/duplex/bottoni), HandleKey, HandleClick.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"      /* gmb_form_modal_close() */
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

/* ── Layout PIXEL del PrinterDialog (condiviso da render e hit-test) ─────────── */
#define PD_PX_W    520
#define PD_PX_H    360
#define PD_PX_TB   34          /* barra titolo */
#define PD_PX_PAD  14
#define PD_PX_ROW  26

static void pd_px_layout(const gmb_control_t* c, int* x, int* y, int* w, int* h)
{
    *x = (c->px_w > 0) ? c->px_x : c->x;
    *y = (c->px_h > 0) ? c->px_y : c->y;
    *w = (c->px_w > 0) ? c->px_w : c->width;
    *h = (c->px_h > 0) ? c->px_h : c->height;
}

/* Render PIXEL: dialogo con titolo, elenco stampanti, copie +/-, duplex, OK/Annulla. */
static void pd_draw_px(void* u, gmb_control_t* c)
{
    gmb_printer_dialog_t* d = (gmb_printer_dialog_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x, y, w, h; pd_px_layout(c, &x, &y, &w, &h);
    if (w < 200 || h < 160) return;
    int rr = st.radius > 0 ? st.radius : 8;
    int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;

    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, st.fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);
    gmb_gfx_fill_rounded_rect(x + 1, y + 1, w - 2, PD_PX_TB, rr > 2 ? rr - 1 : 0, st.head_bg);
    gmb_gfx_draw_text(d->title, x + PD_PX_PAD, y + 1 + (PD_PX_TB - th) / 2, 0,
                      (int)(0x40000000u | (st.head_fg & 0xFFFFFFu)));

    /* Elenco stampanti. */
    int ly = y + PD_PX_TB + 10;
    int lh = h - PD_PX_TB - 10 - 96;
    int rows = lh / PD_PX_ROW; if (rows < 1) rows = 1;
    gmb_gfx_fill_rounded_rect(x + PD_PX_PAD - 6, ly - 4, w - 2 * PD_PX_PAD + 12, lh + 8, 6,
                              (st.border & 0xFFFFFFu) | 0x22000000u);
    for (int r = 0; r < rows && r < d->printers_count; r++)
    {
        int ry = ly + r * PD_PX_ROW;
        int sel = (r == d->selected_index);
        if (sel) gmb_gfx_fill_rounded_rect(x + PD_PX_PAD - 4, ry, w - 2 * PD_PX_PAD + 8, PD_PX_ROW, 5,
                                           (st.accent & 0xFFFFFFu) | 0x55000000u);
        else if (r & 1) gmb_gfx_fill_rounded_rect(x + PD_PX_PAD - 4, ry, w - 2 * PD_PX_PAD + 8, PD_PX_ROW, 0,
                                                  (st.border & 0xFFFFFFu) | 0x14000000u);
        gmb_gfx_draw_text(d->printers[r], x + PD_PX_PAD, ry + (PD_PX_ROW - th) / 2, 0,
                          (int)(0x40000000u | ((sel ? 0xFFFFFFu : (st.text & 0xFFFFFFu)))));
    }

    /* Copie: -  N  +   e casella duplex. */
    int cy = y + h - 88;
    gmb_gfx_draw_text("Copie", x + PD_PX_PAD, cy + 8, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    int bx = x + PD_PX_PAD + 60;
    gmb_gfx_fill_rounded_rect(bx, cy, 30, 28, 8, st.fill_hover);
    gmb_gfx_stroke_rounded_rect(bx, cy, 30, 28, 8, st.border);
    gmb_gfx_draw_text("-", bx + 15 - gmb_gfx_text_width_logical("-") / 2, cy + (28 - th) / 2, 0,
                      (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    char cb[16]; snprintf(cb, sizeof(cb), "%u", (unsigned)d->copies);
    gmb_gfx_draw_text(cb, bx + 40, cy + (28 - th) / 2, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    int bx2 = bx + 30 + 24;
    gmb_gfx_fill_rounded_rect(bx2, cy, 30, 28, 8, st.fill_hover);
    gmb_gfx_stroke_rounded_rect(bx2, cy, 30, 28, 8, st.border);
    gmb_gfx_draw_text("+", bx2 + 15 - gmb_gfx_text_width_logical("+") / 2, cy + (28 - th) / 2, 0,
                      (int)(0x40000000u | (st.text & 0xFFFFFFu)));
    int dx = x + w / 2;
    gmb_gfx_fill_rounded_rect(dx, cy + 2, 22, 22, 6, d->duplex ? st.accent : st.fill_hover);
    if (!d->duplex) gmb_gfx_stroke_rounded_rect(dx, cy + 2, 22, 22, 6, st.border);
    gmb_gfx_draw_text("Fronte-retro", dx + 32, cy + (28 - th) / 2, 0,
                      (int)(0x40000000u | (st.text & 0xFFFFFFu)));

    /* OK / Annulla. */
    int by = y + h - 46;
    gmb_gfx_fill_rounded_rect(x + w - 230, by, 100, 30, 15, (st.accent & 0xFFFFFFu) | 0xFF000000u);
    gmb_gfx_draw_text(d->ok_label, x + w - 230 + 50 - gmb_gfx_text_width_logical(d->ok_label) / 2,
                      by + (30 - th) / 2, 0, (int)(0x40000000u | 0xFFFFFFu));
    gmb_gfx_fill_rounded_rect(x + w - 118, by, 100, 30, 15, st.fill_hover);
    gmb_gfx_stroke_rounded_rect(x + w - 118, by, 100, 30, 15, st.border);
    gmb_gfx_draw_text(d->cancel_label, x + w - 118 + 50 - gmb_gfx_text_width_logical(d->cancel_label) / 2,
                      by + (30 - th) / 2, 0, (int)(0x40000000u | (st.text & 0xFFFFFFu)));
}

static bool pd_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_printer_dialog_t* d = (gmb_printer_dialog_t*)u;
    if (!c->enabled) return false;
    int bx, by, w, h; pd_px_layout(c, &bx, &by, &w, &h);
    if (x < bx || x >= bx + w || y < by || y >= by + h) return false;
    c->focused = true;

    /* Pulsanti (stesse zone del render). */
    int fby = by + h - 46;
    if (y >= fby && y < fby + 30)
    {
        if (x >= bx + w - 230 && x < bx + w - 130)
        { d->confirmed = true; if (gmb_form_modal_active()) gmb_form_modal_close(); return true; }
        if (x >= bx + w - 118 && x < bx + w - 18)
        { d->confirmed = false; d->btn_focus = 2; if (gmb_form_modal_active()) gmb_form_modal_close(); return true; }
    }
    /* Copie -/+ e duplex. */
    int cy = by + h - 88;
    if (y >= cy && y < cy + 28)
    {
        int cbx = bx + PD_PX_PAD + 60;
        if (x >= cbx && x < cbx + 30) { d->copies = (unsigned short)(d->copies > 1 ? d->copies - 1 : 1); return true; }
        if (x >= cbx + 54 && x < cbx + 84) { d->copies = (unsigned short)(d->copies < 999 ? d->copies + 1 : 999); return true; }
        int dx = bx + w / 2;
        if (x >= dx && x < dx + 22) { d->duplex = !d->duplex; return true; }
    }
    /* Elenco stampanti. */
    int ly = by + PD_PX_TB + 10;
    int lh = h - PD_PX_TB - 10 - 96;
    if (y >= ly && y < ly + lh)
    {
        int r = (y - ly) / PD_PX_ROW;
        int rows = lh / PD_PX_ROW;
        if (r >= 0 && r < rows && r < d->printers_count) { d->selected_index = r; return true; }
    }
    return true;
}

void gmb_printer_dialog_init(gmb_printer_dialog_t* d)
{
    memset(d, 0, sizeof(*d));
    /* Dialogo CENTRATO sulla pagina logica, con dimensioni proprie (pixel). */
    int sw = gmb_gfx_screen_width(), sh = gmb_gfx_screen_height();
    int w = PD_PX_W, h = PD_PX_H;
    int x = sw > 0 ? (sw - w) / 2 : 40;
    int y = sh > 0 ? (sh - h) / 2 : 40;
    gmb_control_init(&d->ctrl, "", x, y, w, h, pd_draw, pd_key);
    d->ctrl.coord_auto_w = 1; d->ctrl.coord_auto_h = 1;
    d->ctrl.draw_px_fn = pd_draw_px;      /* grafica: renderer pixel, niente celle */
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
