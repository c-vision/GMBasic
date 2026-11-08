/* ============================================================================
 * gmb_tui_confirmationbox.c — gmb_tui_confirmationbox. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ConfirmationBox ===================== */

/* Disegno A CELLE (specchio ConfirmationBox.cs): stesso controllo, reso nella SDLConsole/TUI.
 * Il rendering a pixel resta in confirmationbox_draw_px: un controllo, due target. */
static void confirmationbox_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_confirmation_box_t* cb = (gmb_confirmation_box_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    int X = c->x, Y = c->y, W = c->width, H = c->height;
    if (W < 6 || H < 4) return;
    unsigned tl = 0x2554, th = 0x2550, tr = 0x2557, vl = 0x2551, br = 0x255D, bh = 0x2550, bl = 0x255A, vr = 0x2551;
    if (cb->border == GMB_PB_SINGLE) { tl = 0x250C; th = 0x2500; tr = 0x2510; vl = 0x2502; br = 0x2518; bh = 0x2500; bl = 0x2514; vr = 0x2502; }
    else if (cb->border == GMB_PB_ROUNDED) { tl = 0x256D; th = 0x2500; tr = 0x256E; vl = 0x2502; br = 0x256F; bh = 0x2500; bl = 0x2570; vr = 0x2502; }
    for (int y = Y + 1; y < Y + H - 1; y++)
        for (int x = X + 1; x < X + W - 1; x++) gmb_screen_setchar(buf, x, y, ' ', fg, bg);
    gmb_screen_setchar(buf, X, Y, tl, fg, bg);
    gmb_screen_setchar(buf, X + W - 1, Y, tr, fg, bg);
    gmb_screen_setchar(buf, X, Y + H - 1, bl, fg, bg);
    gmb_screen_setchar(buf, X + W - 1, Y + H - 1, br, fg, bg);
    for (int x = X + 1; x < X + W - 1; x++) { gmb_screen_setchar(buf, x, Y, th, fg, bg); gmb_screen_setchar(buf, x, Y + H - 1, bh, fg, bg); }
    for (int y = Y + 1; y < Y + H - 1; y++) { gmb_screen_setchar(buf, X, y, vl, fg, bg); gmb_screen_setchar(buf, X + W - 1, y, vr, fg, bg); }
    { char t[160]; snprintf(t, sizeof(t), " %s ", cb->title[0] ? cb->title : "Conferma"); if ((int)strlen(t) < W - 2) gmb_screen_setstring(buf, X + 1, Y, t, fg, bg); }
    { char lines[8][256]; int nl = gmb_alert_wrap_text(cb->message, W - 3, lines, 8);
      if (nl < 1) { snprintf(lines[0], sizeof(lines[0]), "%s", cb->message); nl = 1; }
      for (int i = 0; i < nl && Y + 2 + i < Y + H - 2; i++)
          gmb_screen_setstring_n(buf, X + 2, Y + 2 + i, lines[i], W - 4, fg, bg); }
    { const char* bl2[3]; int nb = 0;
      bl2[nb++] = cb->confirm_label[0] ? cb->confirm_label : "OK";
      bl2[nb++] = cb->cancel_label[0] ? cb->cancel_label : "Cancel";
      if (cb->has_retry) bl2[nb++] = cb->retry_label[0] ? cb->retry_label : "Riprova";
      int by = Y + H - 2, bx = X + 2;
      for (int i = 0; i < nb; i++) { char lbl[64]; snprintf(lbl, sizeof(lbl), "[ %s ]", bl2[i]); gmb_screen_setstring_n(buf, bx, by, lbl, W - 3, fg, bg); bx += (int)strlen(lbl) + 2; } }
}

static void confirmationbox_draw_px(void* u, gmb_control_t* c)
{
    gmb_confirmation_box_t* cb = (gmb_confirmation_box_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    /* Metrica LOGICA: c->x/y/w/h sono in unita. logiche, mentre
     * gmb_gfx_text_height()/text_width() sono NATIVE (gia. moltiplicate per la
     * render-scale). Su Retina la barra del titolo e i pulsanti raddoppiavano e
     * finivano SOPRA gli altri controlli. */
    int tl = gmb_gfx_text_height_logical(); if (tl <= 0) tl = 14;
    gmb_gfx_fill_rounded_rect(x, y, w, h, 6, st.fill);
    int th = tl + 8;
    gmb_gfx_fill_rounded_rect(x, y, w, th, 6, st.title_bg);
    gmb_gfx_draw_text(cb->title[0] ? cb->title : "Conferma", x + 8, y + 4, 0, (int)st.title_fg);
    gmb_gfx_draw_text(cb->message, x + 10, y + th + 8, 0, (int)st.text);
    const char* btns[3]; int nb = 0;
    btns[nb++] = cb->confirm_label[0] ? cb->confirm_label : "OK";
    btns[nb++] = cb->cancel_label[0] ? cb->cancel_label : "Annulla";
    if (cb->has_retry) btns[nb++] = cb->retry_label[0] ? cb->retry_label : "Riprova";
    int bh = tl + 8, gap = 8, totw = gap * (nb - 1);
    for (int i = 0; i < nb; i++) { int bw = gmb_gfx_text_width_logical(btns[i]) + 24; if (bw < 60) bw = 60; totw += bw; }
    int bx = x + w - totw - 10, by = y + h - bh - 8;
    for (int i = 0; i < nb; i++) {
        int bw = gmb_gfx_text_width_logical(btns[i]) + 24; if (bw < 60) bw = 60;
        gmb_gfx_fill_rounded_rect(bx, by, bw, bh, 4, i == 0 ? st.fill_hover : st.fill);
        gmb_gfx_stroke_rounded_rect(bx, by, bw, bh, 4, st.border);
        gmb_gfx_draw_text(btns[i], bx + bw / 2, by + (bh - tl) / 2, 1, (int)st.text);
        bx += bw + gap;
    }
    gmb_gfx_stroke_rounded_rect(x, y, w, h, 6, st.border);
}
void gmb_confirmation_box_init(gmb_confirmation_box_t* c, int x, int y, int w, int h)
{
    memset(c, 0, sizeof(*c));
    gmb_control_init(&c->ctrl, "", x, y, w, h, confirmationbox_draw, NULL);
    c->ctrl.draw_px_fn = confirmationbox_draw_px;
    c->ctrl.user = c;
    c->border = GMB_PB_DOUBLE;
    c->focused_choice = GMB_CONF_CONFIRM;
    c->draggable = true;
    snprintf(c->confirm_label, sizeof(c->confirm_label), "OK");
    snprintf(c->cancel_label, sizeof(c->cancel_label), "Cancel");
}

/* ---- 100% parity ConfirmationBox.cs:223 — with_*, factory, keys, click ---- */
void gmb_confirmation_box_with_title(gmb_confirmation_box_t* c, const char* title)
{ if (c) snprintf(c->title, sizeof(c->title), "%s", title ? title : ""); }
void gmb_confirmation_box_with_message(gmb_confirmation_box_t* c, const char* message)
{ if (c) snprintf(c->message, sizeof(c->message), "%s", message ? message : ""); }
void gmb_confirmation_box_with_buttons(gmb_confirmation_box_t* c, const char* confirm, const char* cancel, const char* retry)
{
    if (!c) return;
    if (confirm) snprintf(c->confirm_label, sizeof(c->confirm_label), "%s", confirm);
    if (cancel) snprintf(c->cancel_label, sizeof(c->cancel_label), "%s", cancel);
    c->has_retry = (retry != NULL);
    if (retry) snprintf(c->retry_label, sizeof(c->retry_label), "%s", retry);
    else c->retry_label[0] = '\0';
}
void gmb_confirmation_box_confirm_cancel(gmb_confirmation_box_t* c, int x, int y, int w, int h, const char* title, const char* msg)
{
    gmb_confirmation_box_init(c, x, y, w, h);
    gmb_confirmation_box_with_title(c, title);
    gmb_confirmation_box_with_message(c, msg);
    gmb_confirmation_box_with_buttons(c, "OK", "Cancel", NULL);
}
void gmb_confirmation_box_centered(gmb_confirmation_box_t* c, int term_w, int term_h, const char* title, const char* msg)
{
    int w = term_w / 3 < 30 ? 30 : term_w / 3;
    int h = 7;
    int x = (term_w - w) / 2;
    int y = (term_h - h) / 2;
    gmb_confirmation_box_confirm_cancel(c, x, y, w, h, title, msg);
}
bool gmb_confirmation_box_handle_key(gmb_confirmation_box_t* c, int key)
{
    if (!c) return false;
    /* Tab/left/right spostano il focus; Enter conferma; Esc cancella */
    switch (key) {
        case 9: case 8: case 'L': case 'R': { /* Tab/Backspace/frecce: ciclo conferma↔cancella */
            int n = c->has_retry ? 3 : 2;
            c->focused_choice = (c->focused_choice + 1) % n;
            return true;
        }
        case 13: case 10: { /* Enter: attiva scelta corrente */
            c->result = c->focused_choice;
            return true;
        }
        case 27: { c->result = GMB_CONF_CANCEL; return true; } /* Esc → cancel */
        default: return false;
    }
}
bool gmb_confirmation_box_handle_click(gmb_confirmation_box_t* c, int x, int y)
{
    if (!c) return false;
    if (x < c->ctrl.x || x >= c->ctrl.x + c->ctrl.width ||
        y < c->ctrl.y || y >= c->ctrl.y + c->ctrl.height) return false;
    /* clic sui bottoni in basso: 3 zone (confirm/cancel/retry) */
    int buttons = c->has_retry ? 3 : 2;
    int bw = c->ctrl.width / buttons;
    int bx = x - c->ctrl.x;
    int idx = bx / bw;
    if (idx == 0) c->focused_choice = 0;       /* confirm */
    else if (idx == 1) c->focused_choice = 1;  /* cancel (o retry) */
    else if (idx == 2) c->focused_choice = 2;  /* retry */
    return true;
}
