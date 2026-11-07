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
    /* Renderer PIXEL guidato dal tema. Chip COMPATTI (larghezza dal testo) come una
     * barra di tasti vera; prima ogni chip prendeva w/n e l'altezza veniva forzata a 66,
     * disegnando fuori dal rect del controllo. */
    gmb_fkeybar_ctrl_t* f = (gmb_fkeybar_ctrl_t*)u;
    gmb_widget_style_t s = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (h < 20) h = 20;
    int rounded = (s.shape == 1);
    int rad = s.radius > 0 ? s.radius : 6;

    if (rounded) gmb_gfx_fill_rounded_rect(x, y, w, h, rad, s.fill);
    else gmb_gfx_fill_rounded_rect(x, y, w, h, 0, s.fill);
    if (s.border_w > 0 || s.shape == 0) gmb_gfx_stroke_rounded_rect(x, y, w, h, rounded ? rad : 0, s.border);

    int n = 0; for (int i = 0; i < 10; i++) if (f->bar.keys[i][0]) n++;
    if (n <= 0) return;

    const int pad = 6, gap = 4;
    int avail = w - 2 * pad;
    int cwl[10]; int total = 0;
    for (int i = 0, k = 0; i < 10; i++)
    {
        const char* key = f->bar.keys[i];
        if (!key || !*key) continue;
        int tw = (int)gmb_gfx_text_width(key);
        int cw = tw + 18; if (cw < 30) cw = 30;
        cwl[k++] = cw; total += cw;
    }
    total += gap * (n - 1);
    if (total > avail && total > 0)
    {
        double sc = (double)avail / (double)total;
        for (int k = 0; k < n; k++) { cwl[k] = (int)(cwl[k] * sc); if (cwl[k] < 18) cwl[k] = 18; }
    }

    int ch = h - 6; if (ch < 16) ch = 16;
    int cx = x + pad, cy = y + (h - ch) / 2;
    for (int i = 0, k = 0; i < 10; i++)
    {
        const char* key = f->bar.keys[i];
        if (!key || !*key) continue;
        int hot = (i == f->bar.last_key);
        int hov = (i == f->hover_key);
        extern void gmb_form_draw_chip(int, int, int, int, const char*, int);
        gmb_form_draw_chip(cx, cy, cwl[k], ch, key, hot ? 2 : (hov ? 1 : 0));
        cx += cwl[k] + gap;
        k++;
        if (cx >= x + w - pad) break;
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
