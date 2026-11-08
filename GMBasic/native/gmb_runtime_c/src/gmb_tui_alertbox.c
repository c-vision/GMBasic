/* ============================================================================
 * gmb_tui_alertbox.c — gmb_tui_alertbox. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include "gmb_ui_gfx.h"
#include "gmb_tui_app.h"
#include "gmb_px_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== AlertBox ===================== */

/* Disegno A CELLE (mirror di ClipperLib/Controls/AlertBox.cs:Draw): stesso controllo,
 * stesso contenuto, reso nella SDLConsole/TUI a celle. Il rendering a pixel resta in
 * alertbox_draw_px: UN controllo, due target (celle/pixel), nessun ramo nel chiamante. */
static void alertbox_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_alert_box_t* a = (gmb_alert_box_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    int X = c->x, Y = c->y, W = c->width, H = c->height;
    if (W < 4 || H < 3) return;
    if (a->has_fg) fg = a->fg;
    if (a->has_bg) bg = a->bg;
    unsigned tl = 0x250C, th = 0x2500, tr = 0x2510, vl = 0x2502, br = 0x2518, bh = 0x2500, bl = 0x2514, vr = 0x2502;
    if (a->border == GMB_PB_DOUBLE) { tl = 0x2554; th = 0x2550; tr = 0x2557; vl = 0x2551; br = 0x255D; bh = 0x2550; bl = 0x255A; vr = 0x2551; }
    else if (a->border == GMB_PB_ROUNDED) { tl = 0x256D; th = 0x2500; tr = 0x256E; vl = 0x2502; br = 0x256F; bh = 0x2500; bl = 0x2570; vr = 0x2502; }
    for (int y = Y + 1; y < Y + H - 1; y++)
        for (int x = X + 1; x < X + W - 1; x++) gmb_screen_setchar(buf, x, y, ' ', fg, bg);
    gmb_screen_setchar(buf, X, Y, tl, fg, bg);
    gmb_screen_setchar(buf, X + W - 1, Y, tr, fg, bg);
    gmb_screen_setchar(buf, X, Y + H - 1, bl, fg, bg);
    gmb_screen_setchar(buf, X + W - 1, Y + H - 1, br, fg, bg);
    for (int x = X + 1; x < X + W - 1; x++) { gmb_screen_setchar(buf, x, Y, th, fg, bg); gmb_screen_setchar(buf, x, Y + H - 1, bh, fg, bg); }
    for (int y = Y + 1; y < Y + H - 1; y++) { gmb_screen_setchar(buf, X, y, vl, fg, bg); gmb_screen_setchar(buf, X + W - 1, y, vr, fg, bg); }
    if (a->title[0]) { char t[160]; snprintf(t, sizeof(t), " %s ", a->title); if ((int)strlen(t) < W - 2) gmb_screen_setstring(buf, X + 1, Y, t, fg, bg); }
    { char lines[8][256]; int nl = gmb_alert_wrap_text(a->message, W - 3, lines, 8);
      if (nl < 1) { snprintf(lines[0], sizeof(lines[0]), "%s", a->message); nl = 1; }
      for (int i = 0; i < nl && Y + 2 + i < Y + H - 1; i++)
      {
          int ll = (int)strlen(lines[i]);
          int mx = X + 2;
          if (a->align == 1) mx = X + (W - ll) / 2;        /* AlertAlign.Center */
          else if (a->align == 2) mx = X + W - ll - 2;     /* AlertAlign.Right */
          if (mx < X + 2) mx = X + 2;
          gmb_screen_setstring_n(buf, mx, Y + 2 + i, lines[i], W - 4, fg, bg);
      } }
    { int nb = a->buttons_count > 0 ? a->buttons_count : 1;
      int zoneW = (W - 2) / (nb > 0 ? nb : 1);
      for (int i = 0; i < nb; i++)
      {
          const char* bl = a->buttons_count > 0 ? a->buttons[i] : (a->ok_label[0] ? a->ok_label : "OK");
          char lbl[64]; snprintf(lbl, sizeof(lbl), "[ %s ]", bl);
          int llen = (int)strlen(lbl);
          int bx = X + 1 + i * zoneW + (zoneW - llen) / 2;
          if (bx < X + 1) bx = X + 1;
          gmb_cell_color_t bfg = (i == a->focused_button) ? GMB_CELLCOLOR_YELLOW : fg;
          gmb_screen_setstring_n(buf, bx, Y + H - 2, lbl, W - 2, bfg, bg);
      } }
}

/* AlertBox.cs: HandleKey — TAB/←/→ spostano il focus; Enter/Space attivano; ESC chiude (0). */
static bool alertbox_key(void* u, gmb_control_t* c, int key)
{
    gmb_alert_box_t* a = (gmb_alert_box_t*)u;
    if (!c->enabled) return false;
    int n = a->buttons_count > 0 ? a->buttons_count : 1;
    switch (key)
    {
        case GMB_K_TAB: case GMB_K_RIGHT: a->focused_button = (a->focused_button + 1) % n; return true;
        case GMB_K_LEFT: a->focused_button = (a->focused_button + n - 1) % n; return true;
        case ' ': a->focused_button = (a->focused_button + 1) % n; a->result = a->focused_button + 1; return true;
        case GMB_K_ENTER: a->result = a->focused_button + 1; return true;
        case GMB_K_ESC:   a->result = 0; return true;
    }
    return false;
}
/* AlertBox.cs: HandleClick/HandleMouse — click su un pulsante lo attiva; drag sul titolo sposta. */
static bool alertbox_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* m)
{
    gmb_alert_box_t* a = (gmb_alert_box_t*)u;
    if (!c || !m) return false;
    if (m->kind == GMB_MA_RELEASE) { a->dragging = 0; return true; }

    /* GRAFICA: hit-test con la STESSA geometria del renderer pixel (px_alertbox).
     * Prima si testava la riga di celle `c->y + height - 2`: in pixel il pulsante
     * OK sta in basso a destra e quel confronto non era MAI vero — l'alert non
     * rispondeva ai click. */
    if (GMB_GFX_SCREEN_ON() && m->kind == GMB_MA_CLICK)
    {
        int x0 = (c->px_w > 0) ? c->px_x : c->x;
        int y0 = (c->px_h > 0) ? c->px_y : c->y;
        int w0 = (c->px_w > 0) ? c->px_w : c->width;
        int h0 = (c->px_h > 0) ? c->px_h : c->height;
        if (m->x < x0 || m->x >= x0 + w0 || m->y < y0 || m->y >= y0 + h0) return false;
        int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 14;
        int bh = th + 8;
        int bw = gmb_gfx_text_width_logical(a->ok_label[0] ? a->ok_label : "OK") + 24; if (bw < 60) bw = 60;
        int bx = x0 + w0 - bw - 10, by = y0 + h0 - bh - 8;
        if (m->x >= bx && m->x < bx + bw && m->y >= by && m->y < by + bh)
        {
            a->focused_button = 0;
            a->result = 1;
            gmb_tui_fire(c->handle, GMB_EV_CLICK);        /* ON CLICK sull'ALERTBOX */
            if (gmb_form_modal_active() && gmb_form_modal_control() == c) gmb_form_modal_close();
        }
        return true;
    }

    if (m->kind == GMB_MA_DRAG && a->dragging) { c->x = m->x - a->drag_off_x; c->y = m->y - a->drag_off_y; return true; }
    if (m->kind != GMB_MA_CLICK) return false;
    int n = a->buttons_count > 0 ? a->buttons_count : 1;
    if (m->y == c->y + c->height - 2 && m->x >= c->x + 1 && m->x < c->x + c->width - 1 && c->width > 2)
    { int zone = (m->x - (c->x + 1)) * n / (c->width - 2); a->focused_button = zone; a->result = zone + 1; return true; }
    if (a->draggable && m->y == c->y && m->x >= c->x && m->x < c->x + c->width)
    { a->dragging = 1; a->drag_off_x = m->x - c->x; a->drag_off_y = m->y - c->y; return true; }
    return false;
}
void gmb_alert_box_init(gmb_alert_box_t* a, int x, int y, int w, int h)
{
    memset(a, 0, sizeof(*a));
    gmb_control_init(&a->ctrl, "", x, y, w, h, alertbox_draw, alertbox_key);
    a->ctrl.handle_mouse_fn = alertbox_mouse;
    a->ctrl.draw_px_fn = gmb_px_renderer_get(GMB_CK_ALERTBOX);
    a->ctrl.user = a;
    a->border = GMB_PB_SINGLE;
    /* Parità AlertBox.cs: default AlertBox (Alignment=Left, Draggable=true, BoxWidth=null). */
    a->align = 0;              /* AlertAlign.Left */
    a->draggable = 1;
    a->box_width = 0;
    a->has_fg = 0; a->has_bg = 0;
    a->buttons_count = 0;      /* solo OKButton (ALERT) */
    a->focused_button = 0;
    a->dragging = 0;
    snprintf(a->ok_label, sizeof(a->ok_label), "OK");
}
/* ---- AlertBox.cs: With*, Centered, Show — via unica C (nessuna proprietà persa) ---- */
extern int gmb_tui_run_control_modal(gmb_control_t* ctrl);
void gmb_alert_box_with_title(gmb_alert_box_t* a, const char* t) { if (a) snprintf(a->title, sizeof(a->title), "%s", t ? t : ""); }
void gmb_alert_box_with_message(gmb_alert_box_t* a, const char* m) { if (a) snprintf(a->message, sizeof(a->message), "%s", m ? m : ""); }
void gmb_alert_box_with_border(gmb_alert_box_t* a, int b) { if (a) a->border = b; }
void gmb_alert_box_with_alignment(gmb_alert_box_t* a, int al) { if (a) a->align = al; }
void gmb_alert_box_with_width(gmb_alert_box_t* a, int w) { if (a) a->box_width = w; }
void gmb_alert_box_with_colors(gmb_alert_box_t* a, int has_fg, gmb_cell_color_t fg, int has_bg, gmb_cell_color_t bg)
{ if (!a) return; a->has_fg = has_fg; a->fg = fg; a->has_bg = has_bg; a->bg = bg; }
/* Variante P/Invoke-friendly (nessuno struct): colori RGB opzionali. */
void gmb_alert_box_with_colors_rgb(gmb_alert_box_t* a, int has_fg, int fr, int fg_, int fb,
                                   int has_bg, int br, int bg_, int bb)
{
    if (!a) return;
    a->has_fg = has_fg; a->fg.r = fr; a->fg.g = fg_; a->fg.b = fb;
    a->has_bg = has_bg; a->bg.r = br; a->bg.g = bg_; a->bg.b = bb;
}
void gmb_alert_box_with_buttons(gmb_alert_box_t* a, const char* b0, const char* b1, const char* b2)
{
    if (!a) return;
    a->buttons_count = 0;
    if (b0) snprintf(a->buttons[a->buttons_count++], sizeof(a->buttons[0]), "%s", b0);
    if (b1) snprintf(a->buttons[a->buttons_count++], sizeof(a->buttons[0]), "%s", b1);
    if (b2) snprintf(a->buttons[a->buttons_count++], sizeof(a->buttons[0]), "%s", b2);
}
void gmb_alert_box_centered(gmb_alert_box_t* a, int term_w, int term_h, const char* title, const char* message, int width, int align)
{
    char lines[8][256];
    int n = gmb_alert_wrap_text(message ? message : "", width > 0 ? width : (term_w - 12), lines, 8);
    if (n < 1) { snprintf(lines[0], sizeof(lines[0]), "%s", message ? message : ""); n = 1; }
    int maxLine = 0; for (int i = 0; i < n; i++) { int l = (int)strlen(lines[i]); if (l > maxLine) maxLine = l; }
    int tl = (int)strlen(title ? title : "");
    int w = width > 0 ? width : (tl + 4 > maxLine + 6 ? tl + 4 : maxLine + 6);
    if (w > term_w - 4) w = term_w - 4;
    if (w < 6) w = 6;
    int h = 6 + n;
    gmb_alert_box_init(a, (term_w - w) / 2, (term_h - h) / 2, w, h);
    gmb_alert_box_with_title(a, title);
    gmb_alert_box_with_message(a, message);
    gmb_alert_box_with_alignment(a, align);
    a->box_width = width;
}
int gmb_alert_box_show(gmb_alert_box_t* a)
{
    if (!a) return 0;
    a->result = 1;
    gmb_tui_run_control_modal(&a->ctrl);
    return a->result;
}
int gmb_alert_wrap_text(const char* text, int width, char out[][256], int cap)
{
    int n = 0;
    if (!text) return 0;
    char copy[4096];
    snprintf(copy, sizeof(copy), "%s", text);
    char* line = copy;
    while (line && n < cap)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        int ll = (int)strlen(line);
        if (ll <= width) { snprintf(out[n++], 256, "%s", line); }
        else
        {
            char wordbuf[4096];
            char* save = NULL;
            int pos = 0;
            char line2[4096];
            snprintf(line2, sizeof(line2), "%s", line);
            for (char* w = strtok_r(line2, " ", &save); w && n < cap; w = strtok_r(NULL, " ", &save))
            {
                int wl = (int)strlen(w);
                if (pos + wl + 1 <= width)
                {
                    if (pos > 0) { wordbuf[pos++] = ' '; }
                    memcpy(wordbuf + pos, w, (size_t)wl);
                    pos += wl;
                }
                else
                {
                    if (pos > 0) { wordbuf[pos] = '\0'; snprintf(out[n++], 256, "%s", wordbuf); pos = 0; }
                    memcpy(wordbuf + pos, w, (size_t)wl);
                    pos = wl;
                }
            }
            if (pos > 0 && n < cap) { wordbuf[pos] = '\0'; snprintf(out[n++], 256, "%s", wordbuf); }
        }
        if (!nl) break;
        line = nl + 1;
    }
    return n;
}
