/* ============================================================================
 * gmb_tui_fkeybar.c — gmb_tui_fkeybar. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include "gmb_tui.h"
#include "gmb_tui_app.h"
#include "gmb_form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== FunctionKeyBar ===================== */

void gmb_function_key_bar_init(gmb_function_key_bar_t* b, int x, int y, int width)
{
    memset(b, 0, sizeof(*b));
    memset(b, 0, sizeof(*b));
    b->last_key = -1;
    b->x = x; b->y = y; b->width = width;
    static const char* def[10] = {"1Help","2Save","3Load","4Find","5Run","6Edit","7List","8Auto","9Del","10Quit"};
    for (int i = 0; i < 10; i++) snprintf(b->keys[i], 24, "%s", def[i]);
    b->bg = GMB_CELLCOLOR_GREY;
    b->fg = GMB_CELLCOLOR_BLACK;
    b->key_bg = GMB_CELLCOLOR_DARKBLUE;
    b->key_fg = GMB_CELLCOLOR_WHITE;
}
void gmb_function_key_bar_draw(gmb_function_key_bar_t* b, gmb_screen_buffer_t* buffer)
{
    for (int x = b->x; x < b->x + b->width && x < gmb_screen_width(buffer); x++)
        gmb_screen_setchar(buffer, x, b->y, ' ', b->fg, b->bg);
    int col = b->x + 1;
    for (int i = 0; i < 10; i++)
    {
        if (!b->keys[i][0]) break;   /* voci non fornite: la barra finisce qui */
        const char* label = b->keys[i];
        int num_end = 0;
        while (label[num_end] >= '0' && label[num_end] <= '9') num_end++;
        for (int k = 0; k < num_end; k++)
        {
            if (col >= b->x + b->width) break;
            gmb_screen_setchar(buffer, col++, b->y, (unsigned char)label[k], b->key_fg, b->key_bg);
        }
        for (int k = num_end; label[k]; k++)
        {
            if (col >= b->x + b->width) break;
            gmb_screen_setchar(buffer, col++, b->y, (unsigned char)label[k], b->fg, b->bg);
        }
        if (col < b->x + b->width)
        {
            gmb_screen_setchar(buffer, col++, b->y, ' ', b->fg, b->bg);
            if (col < b->x + b->width) gmb_screen_setchar(buffer, col++, b->y, ' ', b->fg, b->bg);
        }
    }
}
bool gmb_function_key_bar_handle_key(gmb_function_key_bar_t* b, int key)
{
    (void)b;
    int idx = key - GMB_K_F1;
    return idx >= 0 && idx < 10;
}
int gmb_function_key_bar_get_command(gmb_function_key_bar_t* b, int key)
{
    (void)b;
    return key - GMB_K_F1;
}

/* ===================== Control wrapper (famiglia unica gmb_control_t) =====================
 * FunctionKeyBar come verbo CONTROLS: rende UNA riga (TEXT, celle) e, in GRAPHICS, riusa
 * la barra di stato pixel (GMB_PX_STATUSBAR). Registrato nel registry TUI non-bloccante. */
typedef struct {
    gmb_control_t ctrl;
    gmb_function_key_bar_t bar;
    int hover_key;   /* tasto sotto il mouse (-1 nessuno) */
} gmb_fkeybar_ctrl_t;

static void fkeybar_ctrl_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)u;
    f->bar.x = c->x; f->bar.y = c->y; f->bar.width = c->width;
    gmb_function_key_bar_draw(&f->bar, buf);
}

static void fkeybar_ctrl_draw_px(void* u, gmb_control_t* c)
{
    /* Renderer PIXEL guidato dal TEMA (gmb_widget_style_current): stessa ricetta dei
     * widget form (shadow soft, radius/shape del tema, fill/hover/press, testo del tema).
     * Nessun colore hardcoded: cambiando tema cambia il look&feel. */
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)u;
    gmb_widget_style_t s = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (h < 66) h = 66;
    int rounded = (s.shape == 1);
    int rad = s.radius > 0 ? s.radius : 6;
    int crad = rad + 2;

    /* 1) ombra soft (come draw_button_body) */
    if (s.shadow_on)
    {
        for (int L = 3; L >= 1; L--)
        {
            unsigned int sh = s.shadow;
            unsigned a = ((sh >> 24) & 0xFF) / (unsigned)(L + 1);
            sh = (a << 24) | (sh & 0xFFFFFFu);
            int sx = x + s.shadow_dx + (L - 1), sy = y + s.shadow_dy + (L - 1);
            if (rounded) gmb_gfx_fill_rounded_rect(sx, sy, w, h, crad, sh);
            else gmb_gfx_fill_rounded_rect(sx, sy, w, h, 0, sh);
        }
    }
    /* 2) card */
    if (rounded) gmb_gfx_fill_rounded_rect(x, y, w, h, crad, s.fill);
    else gmb_gfx_fill_rounded_rect(x, y, w, h, 0, s.fill);
    if (s.border_w > 0 || s.shape == 0) gmb_gfx_stroke_rounded_rect(x, y, w, h, rounded ? crad : 0, s.border);

    int n = 0; for (int i = 0; i < 10; i++) if (f->bar.keys[i][0]) n++;
    if (n <= 0) return;
    int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;
    const int pad = 10, gap = 6, ktop = 8, foot = 26;
    int kh = h - ktop - foot; if (kh < 28) kh = 28;
    int x0 = x + pad, x1 = x + w - pad;
    int span = x1 - x0 - gap * (n - 1);
    if (span < n * 24) span = n * 24;
    int base = span / n, rem = span % n;
    int cx = x0;
    for (int i = 0; i < 10; i++)
    {
        const char* k = f->bar.keys[i];
        if (!k || !*k) continue;
        int cw = base + (rem > 0 ? 1 : 0); if (rem > 0) rem--;
        int hot = (i == f->bar.last_key);
        int hov = (i == f->hover_key);
        /* Chip con il renderer dei widget pixel (draw_button_body): centratura/testo/
         * shadow/radius dal tema, niente testo manuale. */
        { extern void gmb_form_draw_chip(int, int, int, int, const char*, int);
          gmb_form_draw_chip(cx, y + ktop, cw, kh, k, hot ? 2 : (hov ? 1 : 0)); }
        cx += cw + gap;
    }
    if (f->bar.last_key >= 0 && f->bar.last_key < 10 && f->bar.keys[f->bar.last_key][0])
    {
        char fb[64];
        snprintf(fb, sizeof(fb), "F%d  %s", f->bar.last_key + 1, f->bar.keys[f->bar.last_key]);
        gmb_gfx_draw_text(fb, x + pad, y + h - 22, 0, (int)(0x40000000u | (s.text & 0xFFFFFFu)));
    }
}
static bool fkeybar_ctrl_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    if (!a || !c) return false;
    /* Solo MOVE qui (hover): CLICK/RELEASE restano a handle_click_fn. */
    if (a->kind != GMB_MA_MOVE) return false;
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)u;
    int n = 0; for (int i = 0; i < 10; i++) if (f->bar.keys[i][0]) n++;
    if (n <= 0 || c->width <= 0) return false;
    int kw = c->width / n; if (kw < 1) kw = 1;
    int idx = (a->x - c->x) / kw;
    if (idx < 0) idx = 0;
    if (idx > 9) idx = 9;
    while (idx < 10 && !f->bar.keys[idx][0]) idx++;
    if (idx >= 10) idx = -1;
    if (f->hover_key != idx) { f->hover_key = idx; { extern void gmb_form_control_refresh(const char*); gmb_form_control_refresh(""); } }
    return true;
}

static bool fkeybar_ctrl_click(void* u, gmb_control_t* c, int x, int y)
{
    (void)y;
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)u;
    int n = 0; for (int i = 0; i < 10; i++) if (f->bar.keys[i][0]) n++;
    if (n <= 0 || c->width <= 0) return false;
    int kw = c->width / n; if (kw < 1) kw = 1;
    int idx = (x - c->x) / kw;
    if (idx < 0) idx = 0;
    if (idx >= 10) idx = 9;
    /* salta per gli slot vuoti (label corta) */
    while (idx < 10 && !f->bar.keys[idx][0]) idx++;
    if (idx >= 10) return false;
    f->bar.last_key = idx;
    { extern void gmb_form_control_refresh(const char*); gmb_form_control_refresh(""); }
    { extern void gmb_tui_fire(int, int); gmb_tui_fire(c->handle, 1); }
    return true;
}

static bool fkeybar_ctrl_key(void* u, gmb_control_t* c, int key)
{
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)u;
    if (key < GMB_K_F1 || key > GMB_K_F10) return false;
    f->bar.last_key = key - GMB_K_F1;
    { extern void gmb_form_control_refresh(const char*); gmb_form_control_refresh(""); }
    { extern void gmb_tui_fire(int, int); gmb_tui_fire(c->handle, 1); }
    return true;
}

gmb_control_t* gmb_tui_add_fkeybar(int x, int y, int w, const char* keys)
{
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)calloc(1, sizeof(*f));
    if (!f) return NULL;
    gmb_control_init(&f->ctrl, "", x, y, w, 1, fkeybar_ctrl_draw, NULL);
    f->ctrl.draw_px_fn = fkeybar_ctrl_draw_px;
    f->ctrl.handle_key_fn = fkeybar_ctrl_key;
    f->ctrl.handle_click_fn = fkeybar_ctrl_click;
    f->ctrl.handle_mouse_fn = fkeybar_ctrl_mouse;
    f->ctrl.ctrl_kind = GMB_CK_FKEYBAR;
    f->ctrl.user = f;
    gmb_function_key_bar_init(&f->bar, x, y, w);
    if (keys && *keys)
    {
        char tmp[10][24]; int n = 0; const char* p = keys;
        while (*p && n < 10)
        {
            int i = 0;
            while (*p && *p != '|' && i < 23) tmp[n][i++] = *p++;
            tmp[n][i] = 0; n++;
            if (*p == '|') p++;
        }
        if (n > 0)
            for (int i = 0; i < 10; i++) snprintf(f->bar.keys[i], 24, "%s", i < n ? tmp[i] : "");
    }
    gmb_tui_control_add(&f->ctrl);
    return &f->ctrl;
}
