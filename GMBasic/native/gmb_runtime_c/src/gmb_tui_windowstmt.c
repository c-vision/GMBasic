/* ============================================================================
 * gmb_tui_windowstmt.c — modello finestre per gli statement WINDOW.
 * Vedi include/gmb_tui_windowstmt.h. Nessun no-op silenzioso: ogni operazione
 * aggiorna lo stato (registry + WindowManager statico) e ridisegna il buffer.
 * ============================================================================ */
#include "gmb_tui_windowstmt.h"
#include "gmb_graphics.h"
#include "gmb_runtime.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_clipper.h"
#include "gmb_gfx_input.h"
#include "gmb_ui_gfx.h"
#ifdef GMB_GFX_SDL_PRESENT
#include "gmb_gfx_present.h"
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define GMB_WINSTMT_MAX        16
#define GMB_WINSTMT_NAME_MAX   64
#define GMB_WINSTMT_TITLE_MAX  128
#define GMB_WINSTMT_LINES      64
#define GMB_WINSTMT_LINE_MAX   256

typedef struct gmb_winstmt {
    bool used;
    char name[GMB_WINSTMT_NAME_MAX];
    char title[GMB_WINSTMT_TITLE_MAX];
    int  id;
    int  frame;                 /* GMB_WM_BORDER_* */
    bool shadow;
    bool pixel;
    bool nobuttons;
    bool edit;
    bool closed;
    bool minimized;
    bool maximized;
    int  desktop;
    char lines[GMB_WINSTMT_LINES][GMB_WINSTMT_LINE_MAX];
    int  line_count;
    int  scroll_top;
} gmb_winstmt_t;

static gmb_winstmt_t g_ws[GMB_WINSTMT_MAX];
static int g_ws_theme = 0;
static int g_ws_desktop = 0;

/* Stato transiente dell'interazione mouse (drag/resize/zoom). Vive qui, prima di ogni
 * uso, perché sia gmb_winstmt_action sia gmb_winstmt_handle_mouse_px devono poterlo
 * azzerare tramite ws_forget_window quando una finestra viene rimossa. */
static int s_drag_id = -1, s_drag_dx = 0, s_drag_dy = 0;
static int s_min_seq = 0;   /* contatore ordine minimizzazione */
static int s_resize_id = -1, s_rsx = 0, s_rsy = 0, s_rsw = 0, s_rsh = 0;
static char s_edge[3] = {0,0,0};
static int s_max_id = -1, s_max_rx = 0, s_max_ry = 0, s_max_rw = 0, s_max_rh = 0;

/* Punto UNICO di pulizia: quando una finestra sparisce (close/release/rimozione) va
 * azzerato ogni stato transiente che la referenzia, altrimenti drag/resize restano
 * "agganciati" a una finestra inesistente e assorbono i movimenti successivi. */
static void ws_forget_window(int id)
{
    if (id < 0) return;
    if (s_drag_id == id) s_drag_id = -1;
    if (s_resize_id == id) { s_resize_id = -1; s_edge[0] = 0; }
    if (s_max_id == id) s_max_id = -1;
}

static gmb_winstmt_t* ws_find(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < GMB_WINSTMT_MAX; i++)
        if (g_ws[i].used && strcmp(g_ws[i].name, name) == 0) return &g_ws[i];
    return NULL;
}

static gmb_winstmt_t* ws_find_by_id(int id)
{
    if (id < 0) return NULL;
    for (int i = 0; i < GMB_WINSTMT_MAX; i++)
        if (g_ws[i].used && g_ws[i].id == id) return &g_ws[i];
    return NULL;
}

static gmb_winstmt_t* ws_slot(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    if (w) return w;
    for (int i = 0; i < GMB_WINSTMT_MAX; i++) {
        if (!g_ws[i].used) {
            memset(&g_ws[i], 0, sizeof(g_ws[i]));
            g_ws[i].used = true;
            snprintf(g_ws[i].name, sizeof(g_ws[i].name), "%s", name ? name : "");
            g_ws[i].id = -1;
            g_ws[i].frame = GMB_WM_BORDER_SINGLE;
            g_ws[i].desktop = g_ws_desktop;
            return &g_ws[i];
        }
    }
    return NULL;
}

static void ws_put_line(gmb_winstmt_t* w, const char* text)
{
    if (!w || !text) return;
    if (w->line_count < GMB_WINSTMT_LINES) {
        snprintf(w->lines[w->line_count], GMB_WINSTMT_LINE_MAX, "%s", text);
        w->line_count++;
    } else {
        /* scroll di una riga: mantieni le ultime N */
        memmove(w->lines[0], w->lines[1], (GMB_WINSTMT_LINES - 1) * GMB_WINSTMT_LINE_MAX);
        snprintf(w->lines[GMB_WINSTMT_LINES - 1], GMB_WINSTMT_LINE_MAX, "%s", text);
    }
}

/* ---- render sul buffer TUI globale (se presente) ---- */
/* Tabella BIOS: serve per scurire le celle sotto l'ombra (TV usa l'attributo, non un blocco). */
static const unsigned char WS_BIOS_RGB[16][3] = {
    {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
};
static void ws_shadow_cell(gmb_screen_buffer_t* buf, int x, int y)
{
    int bw = gmb_screen_width(buf), bh = gmb_screen_height(buf);
    if (x < 0 || y < 0 || x >= bw || y >= bh) return;
    gmb_screen_cell_t* c = &gmb_screen_cells(buf)[y * bw + x];
    unsigned fr, fg, fb, br, bg, bb;
    if (c->fg_palette >= 0 && c->fg_palette < 16) { fr = WS_BIOS_RGB[c->fg_palette][0]; fg = WS_BIOS_RGB[c->fg_palette][1]; fb = WS_BIOS_RGB[c->fg_palette][2]; }
    else { fr = c->fg_rgb.r; fg = c->fg_rgb.g; fb = c->fg_rgb.b; }
    if (c->bg_palette >= 0 && c->bg_palette < 16) { br = WS_BIOS_RGB[c->bg_palette][0]; bg = WS_BIOS_RGB[c->bg_palette][1]; bb = WS_BIOS_RGB[c->bg_palette][2]; }
    else { br = c->bg_rgb.r; bg = c->bg_rgb.g; bb = c->bg_rgb.b; }
    c->fg_rgb = (gmb_cell_color_t){ fr * 55 / 100, fg * 55 / 100, fb * 55 / 100, 0 };
    c->bg_rgb = (gmb_cell_color_t){ br * 55 / 100, bg * 55 / 100, bb * 55 / 100, 0 };
    c->fg_palette = -1;
    c->bg_palette = -1;
}

static void ws_draw_core(gmb_winstmt_t* w)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (!st || !st->buffer) return;
    int bw = gmb_screen_width(st->buffer);
    int bh = gmb_screen_height(st->buffer);
    if (bw <= 0 || bh <= 0) return;
    gmb_window_manager_t* m = gmb_window_manager_static();
    gmb_managed_window_t* mw = (w->id >= 0) ? gmb_window_manager_get(m, w->id) : NULL;
    if (!mw) return;
    if (mw->minimized) return;   /* minimizzata: non disegnare (va nel chip della status bar) */
    int top = mw->rect.y, left = mw->rect.x;
    int bottom = top + mw->rect.h - 1, right = left + mw->rect.w - 1;
    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= bh) bottom = bh - 1;
    if (right >= bw) right = bw - 1;
    if (bottom <= top || right <= left) return;
    (void)st;
    /* Tema unico del runtime: finestre TUI e controlli condividono gli stessi colori. */
    gmb_theme_t th = gmb_ui_theme();
    int focused_ = (gmb_window_manager_focused(m) == w->id);
    int wcols = right - left + 1;
    /* Turbo Vision 2.0 (TFrame::draw): cornice DOPPIA solo quando la finestra è attiva,
     * SINGOLA quando non lo è; titolo ` Title ` centrato sulla linea (niente ╡ ╞);
     * icone `[■]` a x=2 e zoom `[↑]` (o `[↕]` se massimizzata) a width-5, solo se attiva;
     * angoli inferiori `└─` / `─┘` (drag icons) quando attiva. */
    int rounded = (w->frame == GMB_WM_BORDER_ROUNDED);
    int dbl = (!rounded && w->frame == GMB_WM_BORDER_DOUBLE && focused_);
    gmb_cell_color_t frame_fg = focused_ ? th.border : th.dim;
    gmb_cell_color_t body_fg = th.text;
    gmb_cell_color_t body_bg = th.window_bg;
    gmb_cell_color_t title_bg = focused_ ? th.title_focus : th.title_blur;
    uint32_t Btl, Bh, Bv, Btr, Bbl, Bbr;
    if (rounded) { Btl = 0x256D; Bh = 0x2500; Bv = 0x2502; Btr = 0x256E; Bbl = 0x2570; Bbr = 0x256F; }
    else if (dbl) { Btl = 0x2554; Bh = 0x2550; Bv = 0x2551; Btr = 0x2557; Bbl = 0x255A; Bbr = 0x255D; }
    else { Btl = 0x250C; Bh = 0x2500; Bv = 0x2502; Btr = 0x2510; Bbl = 0x2514; Bbr = 0x2518; }
    /* corpo */
    for (int yy = top + 1; yy < bottom; yy++)
        for (int xx = left + 1; xx < right; xx++)
            gmb_screen_setchar(st->buffer, xx, yy, ' ', body_fg, body_bg);
    /* cornice */
    gmb_screen_setchar(st->buffer, left, top, Btl, frame_fg, body_bg);
    for (int x = left + 1; x < right; x++) gmb_screen_setchar(st->buffer, x, top, Bh, frame_fg, body_bg);
    gmb_screen_setchar(st->buffer, right, top, Btr, frame_fg, body_bg);
    for (int y = top + 1; y < bottom; y++) {
        gmb_screen_setchar(st->buffer, left, y, Bv, frame_fg, body_bg);
        gmb_screen_setchar(st->buffer, right, y, Bv, frame_fg, body_bg);
    }
    for (int x = left + 1; x < right; x++) gmb_screen_setchar(st->buffer, x, bottom, Bh, frame_fg, body_bg);
    gmb_screen_setchar(st->buffer, left, bottom, Bbl, frame_fg, body_bg);
    gmb_screen_setchar(st->buffer, right, bottom, Bbr, frame_fg, body_bg);
    /* titolo centrato sulla larghezza (TV: cTitle), poi le icone lo possono coprire */
    {
        int tlen = (int)strlen(mw->title);
        int maxl = wcols - 10;
        if (tlen > maxl) tlen = maxl;
        if (tlen > 0 && maxl > 0) {
            int start = left + (wcols - tlen) / 2;
            gmb_screen_setchar(st->buffer, start - 1, top, ' ', th.title_fg, title_bg);
            gmb_screen_setstring_n(st->buffer, start, top, mw->title, tlen, th.title_fg, title_bg);
            gmb_screen_setchar(st->buffer, start + tlen, top, ' ', th.title_fg, title_bg);
        }
    }
    /* icone TV: solo finestra ATTIVA */
    if (focused_ && !w->nobuttons && wcols >= 10) {
        gmb_screen_setchar(st->buffer, left + 2, top, '[', frame_fg, body_bg);
        gmb_screen_setchar(st->buffer, left + 3, top, 0x25A0, frame_fg, body_bg);       /* ■ */
        gmb_screen_setchar(st->buffer, left + 4, top, ']', frame_fg, body_bg);
        gmb_screen_setchar(st->buffer, right - 4, top, '[', frame_fg, body_bg);
        gmb_screen_setchar(st->buffer, right - 3, top, w->maximized ? 0x2195 : 0x2191, frame_fg, body_bg); /* ↕ / ↑ */
        gmb_screen_setchar(st->buffer, right - 2, top, ']', frame_fg, body_bg);
    }
    /* NIENTE drag icons `└─` / `─┘` sulla cornice inferiore.
     * TFrame::draw (tvision) li sovrascrive sulla riga `╚══╝` quando la finestra è growable,
     * ma il risultato mescola linee SINGOLE (└ ─ ┘) con le DOPPIE (═ ║): l'angolo resta
     * aperto e la verticale non si unisce alla cornice. Qui la cornice resta COERENTE:
     * attiva ╔═╗ / ║ / ╚═╝, inattiva ┌─┐ / │ / └─┘, rounded ╭─╮ / │ / ╰─╯. */


    /* ombra TV: 2 colonne a destra + 1 riga sotto, come attributo SCURITO del contenuto
     * (shadowAttr), non come blocco nero. */
    if (w->shadow) {
        for (int y = top + 1; y <= bottom + 1; y++) {
            ws_shadow_cell(st->buffer, right + 1, y);
            ws_shadow_cell(st->buffer, right + 2, y);
        }
        for (int x = left + 2; x <= right + 2; x++) ws_shadow_cell(st->buffer, x, bottom + 1);
    }
    /* contenuto (rispetta scroll_top) */
    int inner_h = bottom - top - 1;
    for (int i = 0; i < inner_h; i++) {
        int li = w->scroll_top + i;
        if (li >= w->line_count) break;
        gmb_screen_setstring_n(st->buffer, left + 1, top + 1 + i, w->lines[li],
                               right - left - 1, th.text, th.window_bg);
    }
}

/* Wrapper: disegna e rende lo screen in ANSI (parità IL: la finestra appare sul terminale). */
static void ws_draw(gmb_winstmt_t* w)
{
    ws_draw_core(w);
    gmb_tui_screen_flush();
    /* Finestra PIXEL + screen grafico attivo: render nativo sul page gfx. */
    if (w && w->pixel && GMB_GFX_SCREEN_ON())
    {
        gmb_clipper_state_t* st = gmb_clipper_global_state();
        int bw = st && st->buffer ? gmb_screen_width(st->buffer) : 80;
        int bh = st && st->buffer ? gmb_screen_height(st->buffer) : 25;
        int gw = GMB_GFX_SCREEN_W();
        int gh = GMB_GFX_SCREEN_H();
        int cw = (bw > 0 && gw > 0) ? gw / bw : 8; if (cw < 1) cw = 8;
        int ch = (bh > 0 && gh > 0) ? gh / bh : 16; if (ch < 1) ch = 16;
        gmb_winstmt_render_pixel(w->name, 0, 0, cw, ch);
    }
}

/* Ripristina lo sfondo desktop (blu ░) su tutto il buffer TUI. Serve a far sparire
 * una finestra chiusa/minimizzata senza lasciare pixel residui (parità IL
 * RenderTuiWindows, che ridisegna su un buffer pulito ad ogni azione). */
static void ws_repaint_desktop(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (!st || !st->buffer) return;
    /* Colore e retino del desktop = tema attivo (implementazione unica nel tema). */
    gmb_theme_paint_desktop(st->buffer);
}

/* La UI registra qui il proprio refresh: quando il tema cambia (WINDOW THEME o
 * SET THEME, stessa palette e stesso percorso) si ridisegnano desktop e finestre. */
static void ws_on_theme_changed(const char* name)
{
    (void)name;
    gmb_theme_t th = gmb_ui_theme();
    { extern void gmb_gfx_page_bg(unsigned); gmb_gfx_page_bg(((unsigned)th.desktop_bg.r << 16) | ((unsigned)th.desktop_bg.g << 8) | th.desktop_bg.b); }
    ws_repaint_desktop();
    gmb_winstmt_render_all();
}
static void ws_ensure_theme_hook(void)
{
    static int done = 0;
    if (!done) { done = 1; gmb_ui_theme_on_change(ws_on_theme_changed); }
}
/* Registrazione al CARICAMENTO del layer UI: anche un SET THEME eseguito prima del primo
 * WINDOW deve ridisegnare desktop/finestre con il tema nuovo (stesso percorso di
 * WINDOW THEME). Se il layer UI non è linkato, nessuna registrazione (hook NULL). */
__attribute__((constructor)) static void ws_theme_hook_init(void) { gmb_ui_theme_on_change(ws_on_theme_changed); }

/* ---- operazioni ---- */
int gmb_winstmt_define(const char* name, const char* title,
                       int top, int left, int bottom, int right, int flags)
{
    if (right < left) { int t = left; left = right; right = t; }
    if (bottom < top) { int t = top; top = bottom; bottom = t; }
    gmb_winstmt_t* w = ws_slot(name);
    if (!w) return -1;
    int created = (w->id < 0);   /* finestra creata adesso (non un re-DEFINE) */
    w->frame = (flags & GMB_WINSTMT_DOUBLE) ? GMB_WM_BORDER_DOUBLE
             : (flags & GMB_WINSTMT_ROUNDED) ? GMB_WM_BORDER_ROUNDED
             : GMB_WM_BORDER_SINGLE;
    w->shadow = (flags & GMB_WINSTMT_SHADOW) != 0;
    w->pixel = (flags & GMB_WINSTMT_PIXEL) != 0;
    w->nobuttons = (flags & GMB_WINSTMT_NOBUTTONS) != 0;
    /* Da qui il programma lavora in TUI: PRINT/LOCATE targettano lo screen (parità IL). */
    gmb_tui_screen_enable();
    snprintf(w->title, sizeof(w->title), "%s", (title && title[0]) ? title : (name ? name : ""));
    /* I pulsanti ci sono SEMPRE: la finestra si allarga per ospitarli
     * (1 bordo + [■] 4 + zoom [_][▲] 6 + 1 bordo = 12; + titolo se presente). */
    int _tlen = (int)strlen(w->title);
    int _minw = w->nobuttons ? 4 : (12 + (_tlen > 0 ? _tlen + 4 : 0));
    int _rw = right - left + 1;
    if (_rw < _minw) _rw = _minw;
    gmb_rect_t rect = gmb_rect_new(left, top, _rw, bottom - top + 1);
    /* Finestre PIXEL: FROM/TO sono estremi in PIXEL logici (larghezza = right-left, come
     * GfxWindowManager/IL). Le TUI restano in celle (inclusive: right-left+1). */
    if (w->pixel) { rect.w = right - left; rect.h = bottom - top; if (rect.w < 16) rect.w = 16; if (rect.h < 24) rect.h = 24; }
    gmb_window_manager_t* m = gmb_window_manager_static();
    if (w->id >= 0) {
        gmb_managed_window_t* mw = gmb_window_manager_get(m, w->id);
        if (mw) { mw->rect = rect; snprintf(mw->title, sizeof(mw->title), "%s", w->title); mw->pixel = w->pixel; }
    } else {
        w->id = gmb_window_manager_add(m, w->title, rect);
        created = 1;
        gmb_managed_window_t* mw = gmb_window_manager_get(m, w->id);
        if (mw) { snprintf(mw->name, sizeof(mw->name), "%s", name ? name : ""); mw->pixel = w->pixel; }
    }
    ws_draw(w);
    /* OPEN: la finestra è appena stata creata → evento all'handle (parità IL/PCode, che lo
     * innalzano su AddWindow). Solo alla creazione, non a ogni DEFINE della stessa finestra. */
    if (created) gmb_window_fire(w->id, "OPEN");
    return w->id;
}

void gmb_winstmt_select(const char* name)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    /* SELECT 0 / SELECT "" → screen base: nessuna finestra attiva.
     * Parità IlRuntime.WindowSelect: Select(null) sul WindowManager. */
    if (!name || !name[0] || strcmp(name, "0") == 0) {
        for (int i = 0; i < GMB_WINSTMT_MAX; i++) {
            if (!g_ws[i].used || g_ws[i].id < 0) continue;
            gmb_managed_window_t* mw = gmb_window_manager_get(m, g_ws[i].id);
            if (mw && mw->focused) gmb_window_fire(g_ws[i].id, "DEACTIVATE");
            if (mw) mw->focused = false;
        }
        m->focus = -1;
        gmb_winstmt_render_all();
        return;
    }
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return;
    /* FOCUS sul nuovo attivo (WindowEventKind.Focus), poi la selezione vera. */
    gmb_window_fire(w->id, "FOCUS");
    gmb_window_manager_select(m, w->id);
    gmb_window_manager_raise(m, w->id);
    gmb_winstmt_render_all();
}

void gmb_winstmt_set_title(const char* name, const char* title)
{
    /* Titolo per nome, oppure della finestra FOCALIZZATA se il nome è vuoto.
     * Parità IlRuntime.WindowSetTitle → WindowManager.SetFocusedTitle. */
    gmb_winstmt_t* w = (name && name[0]) ? ws_find(name)
                                         : ws_find_by_id(gmb_window_manager_focused(gmb_window_manager_static()));
    if (!w || w->id < 0) return;
    snprintf(w->title, sizeof(w->title), "%s", title ? title : "");
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (mw) snprintf(mw->title, sizeof(mw->title), "%s", w->title);
    ws_draw(w);
}

void gmb_winstmt_append(const char* name, const char* text)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w) return;
    ws_put_line(w, text);
    ws_draw(w);
}

void gmb_winstmt_content(const char* name, const char* text)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w) return;
    w->line_count = 0;
    w->scroll_top = 0;
    if (text && text[0]) ws_put_line(w, text);
    ws_draw(w);
}

void gmb_winstmt_clear(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w) return;
    w->line_count = 0;
    w->scroll_top = 0;
    ws_draw(w);
}

void gmb_winstmt_scroll(const char* name, int lines)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w) return;
    w->scroll_top += lines;
    if (w->scroll_top < 0) w->scroll_top = 0;
    if (w->scroll_top > w->line_count) w->scroll_top = w->line_count;
    ws_draw(w);
}

void gmb_winstmt_edit(const char* name, int on)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w) return;
    w->edit = on != 0;
}

void gmb_winstmt_move_desktop(const char* name, int desktop)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return;
    w->desktop = desktop;
    gmb_window_manager_move_to_desktop(gmb_window_manager_static(), w->id, desktop);
    ws_draw(w);
}

void gmb_winstmt_frame(const char* name, int border)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return;
    w->frame = border;
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (mw) mw->border = border;
    ws_draw(w);
}

void gmb_winstmt_shadow(const char* name, int on)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w) return;
    w->shadow = on != 0;
    ws_draw(w);
}

extern void gmb_window_fire(int win, const char* ev);
void gmb_winstmt_action(const char* name, const char* action)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0 || !action) return;
    int _win = w->id;
    gmb_window_manager_t* m = gmb_window_manager_static();
    if (strcmp(action, "Activate") == 0) {
        /* FOCUS è il nome canonico (WindowEventKind.Focus = 4, Parser: `ON WINDOW FOCUS`);
         * ACTIVATE resta per compatibilità delle registrazioni storiche. */
        gmb_window_fire(_win, "FOCUS");
        gmb_window_fire(_win, "ACTIVATE");
        gmb_window_manager_select(m, w->id);
        gmb_window_manager_raise(m, w->id);
    } else if (strcmp(action, "Deactivate") == 0) {
        gmb_window_fire(_win, "DEACTIVATE");
        gmb_window_manager_deactivate(m, w->id);
    } else if (strcmp(action, "Release") == 0) {
        /* Parità IL (IlRuntime: case "RELEASE" → wm.RemoveWindow): RELEASE RIMUOVE la
         * finestra, non la sola disattivazione. Prima restava visibile a video. */
        gmb_window_fire(_win, "DEACTIVATE");
        gmb_window_manager_remove(m, w->id);
        w->used = false;
        w->id = -1;
        ws_forget_window(_win);
    } else if (strcmp(action, "Close") == 0) {
        gmb_window_fire(_win, "CLOSE");
        gmb_window_manager_remove(m, w->id);
        w->used = false;
        w->id = -1;
        ws_forget_window(_win);
    } else if (strcmp(action, "Minimize") == 0) {
        gmb_window_fire(_win, "MINIMIZE");
        gmb_window_manager_minimize(m, w->id);
    } else if (strcmp(action, "Restore") == 0) {
        /* RESTORE su una finestra non minimizzata è un no-op (parità IL/PCode: nessun
         * evento). Prima innalzava RESTORE comunque → conteggio eventi divergente. */
        gmb_managed_window_t* _mw = gmb_window_manager_get(m, w->id);
        if (_mw && _mw->minimized) {
            gmb_window_fire(_win, "RESTORE");
            gmb_window_manager_restore(m, w->id);
        }
    }
    /* Ridisegna su desktop pulito (parità IL RenderTuiWindows): la finestra
     * chiusa/minimizzata sparisce invece di lasciare pixel residui. */
    ws_repaint_desktop();
    gmb_winstmt_render_all();
}

void gmb_winstmt_show(int on) {
#ifdef GMB_GFX_SDL_PRESENT
    GMB_GFX_WINDOW_SET_VISIBLE(on);
#else
    (void)on;   /* senza SDL non c'e' finestra da mostrare/nascondere (gmb_gfx_present_sdl.c non compilato) */
#endif
}
void gmb_winstmt_theme(int theme)      { g_ws_theme = theme; ws_ensure_theme_hook(); ws_repaint_desktop(); gmb_winstmt_render_all(); }
void gmb_winstmt_theme_name(const char* name)
{
    /* Tema per nome (Turbo/Nord/…): id numerico stabile = hash semplice del nome;
     * il tema VERO (palette) vive nel registro unico gmb_ui_*, che notifica la UI
     * (hook) e fa ridisegnare desktop e finestre. */
    unsigned h = 0;
    if (name) for (const char* p = name; *p; ++p) h = h * 131u + (unsigned char)*p;
    g_ws_theme = (int)(h & 0x7fffffff);
    ws_ensure_theme_hook();
    gmb_ui_set_theme_name(name);   /* palette reale: finestre, controlli, widget pixel */
}
void gmb_winstmt_desktop(int desktop)  { g_ws_desktop = desktop;
                                         gmb_window_manager_switch_desktop(gmb_window_manager_static(), desktop);
                                         gmb_winstmt_render_all(); }

void gmb_winstmt_frame_all(int border)
{
    for (int i = 0; i < GMB_WINSTMT_MAX; i++)
        if (g_ws[i].used) gmb_winstmt_frame(g_ws[i].name, border);
}

void gmb_winstmt_shadow_all(int on)
{
    for (int i = 0; i < GMB_WINSTMT_MAX; i++)
        if (g_ws[i].used) gmb_winstmt_shadow(g_ws[i].name, on);
}

/* ---- introspezione ---- */
int gmb_winstmt_count(void)
{
    int n = 0;
    for (int i = 0; i < GMB_WINSTMT_MAX; i++) if (g_ws[i].used) n++;
    return n;
}

int gmb_winstmt_id(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    return w ? w->id : -1;
}

/* Origine (celle) dell'area contenuto della window `name` (bordo +1). Per l'hosting dei
 * controlli figli: le coordinate del figlio sono relative al contenuto. Ritorna 0 se la
 * window non esiste. */
int gmb_winstmt_content_origin(const char* name, int* out_x, int* out_y)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return 0;
    gmb_window_manager_t* m = gmb_window_manager_static();
    gmb_managed_window_t* mw = gmb_window_manager_get(m, w->id);
    if (!mw) return 0;
    if (out_x) *out_x = mw->rect.x + 1;
    if (out_y) *out_y = mw->rect.y + 1;
    return 1;
}

/* Nome della window attiva (focused) — per lo scoping focus/tastiera dei controlli
 * ospitati (un controllo con container != attivo non riceve tastiera). "" se nessuna. */
const char* gmb_winstmt_active(void)
{
    gmb_winstmt_t* w = ws_find_by_id(gmb_window_manager_focused(gmb_window_manager_static()));
    return w ? w->name : "";
}

int gmb_winstmt_line_count(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    return w ? w->line_count : -1;
}

const char* gmb_winstmt_line(const char* name, int i)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || i < 0 || i >= w->line_count) return "";
    return w->lines[i];
}

int gmb_winstmt_theme_get(void)   { return g_ws_theme; }
int gmb_winstmt_desktop_get(void) { return g_ws_desktop; }

int gmb_winstmt_edit_get(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    return w ? (w->edit ? 1 : 0) : -1;
}

int gmb_winstmt_shadow_get(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    return w ? (w->shadow ? 1 : 0) : -1;
}

int gmb_winstmt_frame_get(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    return w ? w->frame : -1;
}

void gmb_winstmt_render(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    if (w) ws_draw(w);
}

void gmb_winstmt_render_all(void)
{
    ws_ensure_theme_hook();
    /* Composizione in ordine di z crescente (stile WindowManager): la finestra
     * focalizzata/elevata (z piu' alto) viene disegnata per ultima, quindi SOPRA.
     * Senza questo, l'ordine era quello di creazione e SELECT non riportava in
     * primo piano la finestra attiva. */
    gmb_window_manager_t* m = gmb_window_manager_static();
    int idx[GMB_WINSTMT_MAX], n = 0;
    for (int i = 0; i < GMB_WINSTMT_MAX; i++) if (g_ws[i].used) idx[n++] = i;
    for (int a = 1; a < n; a++) {
        int k = idx[a];
        gmb_managed_window_t* wa = (m && g_ws[k].id >= 0) ? gmb_window_manager_get(m, g_ws[k].id) : NULL;
        int za = wa ? wa->z : -0x7fffffff;
        int b = a - 1;
        while (b >= 0) {
            int j = idx[b];
            gmb_managed_window_t* wj = (m && g_ws[j].id >= 0) ? gmb_window_manager_get(m, g_ws[j].id) : NULL;
            int zj = wj ? wj->z : -0x7fffffff;
            if (zj > za) { idx[b + 1] = idx[b]; b--; } else break;
        }
        idx[b + 1] = k;
    }
    for (int k = 0; k < n; k++) ws_draw(&g_ws[idx[k]]);
}

void gmb_winstmt_reset(void)
{
    memset(g_ws, 0, sizeof(g_ws));
    g_ws_theme = 0;
    g_ws_desktop = 0;
}

/* Render nativo PIXEL di una finestra (piano TUI-GUI): frame rounded sul page gfx.
 * Il rect e' in celle (dal WindowManager); origin/cell in pixel. */
void gmb_winstmt_render_pixel(const char* name, int origin_x, int origin_y, int cell_w, int cell_h)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return;
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (!mw) return;
    int px = origin_x + mw->rect.x * cell_w;
    int py = origin_y + mw->rect.y * cell_h;
    int pw = mw->rect.w * cell_w;
    int ph = mw->rect.h * cell_h;
    unsigned int bg = 0xFF000000u | (16u << 16) | (16u << 8) | 16u;
    unsigned int cy = 0xFF000000u | (0u << 16) | (255u << 8) | 255u;
    unsigned int ye = 0xFF000000u | (255u << 16) | (255u << 8) | 0u;
    unsigned int wh = 0xFFFFFFFFu;
    gmb_gfx_fill_rounded_rect(px, py, pw, ph, 6, bg);
    gmb_gfx_stroke_rounded_rect(px, py, pw, ph, 6, cy);
    if (w->title[0]) gmb_gfx_draw_text(w->title, px + cell_w, py + cell_h / 4, 0, (int)ye);
    for (int i = 0; i < w->line_count; i++)
        gmb_gfx_draw_text(w->lines[i], px + cell_w, py + (1 + i) * cell_h + cell_h / 4, 0, (int)wh);
}

/* ===== Compositor pixel (porting di GfxWindowManager.cs / GfxWindowCanvas.cs) =====
 * Usato quando lo screen grafico è attivo: le finestre (anche non-PIXEL) rendono
 * per-pixel con chrome nativo, invece del blit a celle. */
#define GMB_WIN_TITLE_H 24
#define GMB_WIN_RADIUS  6
#define GMB_WIN_PAD     6
#define GMB_COL_TITLE_TOP   0xFFAAAAAAu
#define GMB_COL_TITLE_BOT   0xFFAAAAAAu
#define GMB_COL_TITLE_TOPF  0xFFAAAAAAu
#define GMB_COL_TITLE_BOTF  0xFFAAAAAAu
#define GMB_COL_BORDER      0xFFAAAAAAu
#define GMB_COL_BORDER_DIM  0xFF555555u
#define GMB_COL_RING        0x40AAAAAAu
#define GMB_COL_BODY        0xFFAAAAAAu
#define GMB_COL_TITLE       0xFF000000u
#define GMB_COL_TEXT        0xFF000000u
#define GMB_COL_CLOSE       0xFF00AA00u
#define GMB_COL_MIN         0xFF00AA00u
#define GMB_COL_MAX         0xFF008800u
#define GMB_COL_SHADOW      0x8A000000u
#define GMB_COL_DESKTOP     0xFF0000AAu
#define GMB_COL_TASKBAR_BG  0xFFAAAAAAu
#define GMB_COL_CHIP        0xFFAAAAAAu
#define GMB_COL_CHIP_FOCUS  0xFF00AAAAu
#define GMB_COL_CHIP_TEXT   0xFF000000u
#define GMB_TASKBAR_H       28
#define GMB_CHIP_W          130
#define GMB_CHIP_GAP        6

static unsigned int wm_alpha(unsigned int rgba, double f)
{
    int a = (int)(((rgba >> 24) & 0xFF) * f + 0.5);
    if (a < 0) a = 0; if (a > 255) a = 255;
    return ((unsigned int)a << 24) | (rgba & 0xFFFFFFu);
}
static void wm_text(const char* s, int x, int y, unsigned int argb)
{
    gmb_gfx_draw_text(s, x, y, 0, (int)(0xFF000000u | (argb & 0xFFFFFFu)));
}

static void wm_title_buttons(int x, int y, int w, int focus, int titleh)
{
    int cy = y + titleh / 2;
    int closeX = x + w - 24;
    int maxX = closeX - 24;
    int minX = maxX - 24;
    if (minX < x + 4) return;
    double af = focus ? 1.0 : 0.35;
    unsigned int icon = wm_alpha(0xFF000000u, af * 0.5);
    gmb_gfx_fill_circle(minX + 7, cy, 12, wm_alpha(GMB_COL_MIN, af));
    gmb_gfx_fill_circle(maxX + 7, cy, 12, wm_alpha(GMB_COL_MAX, af));
    gmb_gfx_fill_circle(closeX + 7, cy, 12, wm_alpha(GMB_COL_CLOSE, af));
    gmb_gfx_fill_rounded_rect(minX + 4, cy - 1, 6, 2, 1, icon);       /* − */
    gmb_gfx_fill_rounded_rect(maxX + 4, cy - 1, 6, 2, 1, icon);       /* + orizz */
    gmb_gfx_fill_rounded_rect(maxX + 6, cy - 3, 2, 6, 1, icon);       /* + vert */
    gmb_gfx_line(closeX + 4, cy - 3, closeX + 10, cy + 3, (int)icon, "");  /* × */
    gmb_gfx_line(closeX + 10, cy - 3, closeX + 4, cy + 3, (int)icon, "");
}

void gmb_winstmt_compose_all_pixel(void)
{
    if (!GMB_GFX_SCREEN_ON()) return;
    float sc = gmb_gfx_render_scale(); if (sc < 1.0f) sc = 1.0f;
    int W = (int)(gmb_gfx_width() * sc + 0.5f), H = (int)(gmb_gfx_height() * sc + 0.5f);
    if (W <= 0 || H <= 0) return;
    int cw = gmb_gfx_text_width("M"); if (cw <= 0) cw = (int)(8 * sc);
    int ch = gmb_gfx_text_height();    if (ch <= 0) ch = (int)(16 * sc);
    if (cw * 4 < W / 60) cw = (int)(8 * sc);   /* sanity */
    if (ch * 4 < H / 40) ch = (int)(16 * sc);
    gmb_gfx_clear_rect(0, 0, W - 1, H - 1, (int)(0x40000000u | (GMB_COL_DESKTOP & 0xFFFFFFu)));
    gmb_window_manager_t* m = gmb_window_manager_static();
    int order[GMB_WM_MAX_WINDOWS], n = 0;
    for (int i = 0; i < m->count; i++) order[n++] = i;
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
        if (m->windows[order[b]].z < m->windows[order[a]].z) { int t = order[a]; order[a] = order[b]; order[b] = t; }
    for (int k = 0; k < n; k++)
    {
        gmb_managed_window_t* mw = &m->windows[order[k]];
        gmb_winstmt_t* w = NULL;
        for (int i = 0; i < GMB_WINSTMT_MAX; i++)
            if (g_ws[i].used && g_ws[i].id == mw->id) { w = &g_ws[i]; break; }
        if (!w || w->closed || mw->minimized) continue;
        if (w->desktop != g_ws_desktop) continue;
        /* Finestre PIXEL: rect già in PIXEL LOGICI (WINDOW DEFINE … PIXEL, parità C#).
         * Finestre TUI mostrate su schermo grafico: rect in celle → conversione. */
        int x, y, ww, hh;
        if (mw->pixel) { x = mw->rect.x; y = mw->rect.y; ww = mw->rect.w; hh = mw->rect.h; }
        else { x = mw->rect.x * cw; y = mw->rect.y * ch; ww = mw->rect.w * cw; hh = mw->rect.h * ch; }
        if (ww < 16) ww = 16;
        if (hh < 24) hh = 24;
        int isFocus = mw->focused;
        int titleh = GMB_WIN_TITLE_H; if (titleh > hh / 2) titleh = hh / 2;
        /* Colori per-finestra (parità PixelWindow.BodyColor/FgColor/TitleColor): 0 = tema. */
        unsigned int col_body = mw->body_color ? mw->body_color : GMB_COL_BODY;
        unsigned int col_title = mw->title_color ? mw->title_color
                                 : (isFocus ? GMB_COL_TITLE_TOPF : GMB_COL_TITLE_TOP);
        unsigned int col_title2 = mw->title_color ? mw->title_color
                                  : (isFocus ? GMB_COL_TITLE_BOTF : GMB_COL_TITLE_BOT);
        if (s_drag_id < 0 && s_resize_id < 0) gmb_gfx_drop_shadow(x, y, ww, hh, 3, GMB_COL_SHADOW, 10);
        gmb_gfx_fill_rounded_rect(x, y, ww, hh, GMB_WIN_RADIUS, col_body);
        gmb_gfx_gradient_rect(x, y, ww, titleh, col_title, col_title2, 1);
        gmb_gfx_stroke_rounded_rect(x, y, ww, hh, GMB_WIN_RADIUS,
                                    isFocus ? GMB_COL_BORDER : GMB_COL_BORDER_DIM);
        if (isFocus)
            gmb_gfx_stroke_rounded_rect(x - 2, y - 2, ww + 4, hh + 4, GMB_WIN_RADIUS + 2, GMB_COL_RING);
        if (w->title[0])
        {
            int tw = (int)(gmb_gfx_text_width(w->title) / sc + 0.5f);
            int tx = x + (ww - tw) / 2; if (tx < x + 4) tx = x + 4;
            wm_text(w->title, tx, y + titleh / 2 - ch / 2, GMB_COL_TITLE);
        }
        wm_title_buttons(x, y, ww, isFocus, titleh);
        int crx = x + GMB_WIN_PAD, cry = y + titleh;
        int crw = ww - 2 * GMB_WIN_PAD, crh = hh - titleh - GMB_WIN_PAD;
        if (crw > 0 && crh > 0)
        {
            /* Contenuto della finestra = regione di st->buffer (celle) → render cella->pixel.
             * Cosi' testo (WINDOW PRINT) E CONTROLLI (disegnati nel buffer globale) appaiono
             * DENTRO la finestra. Fallback alle righe w->lines solo se il buffer non c'e'. */
            gmb_clipper_state_t* stc = gmb_clipper_global_state();
            int rendered = 0;
            if (stc && stc->buffer)
            {
                int bw = gmb_screen_width(stc->buffer), bh = gmb_screen_height(stc->buffer);
                gmb_screen_cell_t* cells = gmb_screen_cells(stc->buffer);
                int cols = crw / cw, rws = crh / ch;
                int cx0 = mw->rect.x + 1, cy0 = mw->rect.y + 1;   /* origine content in st->buffer */
                for (int r = 0; r < rws; r++)
                    for (int c2 = 0; c2 < cols; c2++)
                    {
                        int bx = cx0 + c2, by = cy0 + w->scroll_top + r;
                        if (bx < 0 || by < 0 || bx >= bw || by >= bh) continue;
                        gmb_screen_cell_t* cell = &cells[by * bw + bx];
                        uint32_t fgc = 0x40000000u | ((uint32_t)cell->fg_rgb.r << 16)
                                     | ((uint32_t)cell->fg_rgb.g << 8) | cell->fg_rgb.b;
                        uint32_t bgc = 0x40000000u | ((uint32_t)cell->bg_rgb.r << 16)
                                     | ((uint32_t)cell->bg_rgb.g << 8) | cell->bg_rgb.b;
                        /* draw_cell NON applica la render-scale: passiamo coordinate native. */
                        int ncx = crx + c2 * cw, ncy = cry + r * ch;
                        int ncw = cw, nch = ch;
                        gmb_gfx_draw_cell(ncx, ncy, ncw, nch,
                                          (uint32_t)cell->character, fgc, bgc);
                    }
                rendered = 1;
            }
            if (!rendered)
            {
                int rows = crh / ch;
                for (int r = 0; r < rows; r++)
                {
                    int idx = w->scroll_top + r;
                    if (idx >= w->line_count) break;
                    wm_text(w->lines[idx], crx, cry + r * ch,
                            mw->fg_color ? mw->fg_color : GMB_COL_TEXT);
                }
            }
        }
    }

    /* Taskbar: chip per ogni finestra minimizzata (click = ripristina). */
    {
        int barY = H - GMB_TASKBAR_H;
        gmb_gfx_fill_rounded_rect(0, barY, W, GMB_TASKBAR_H, 0, GMB_COL_TASKBAR_BG);
        int cx = GMB_CHIP_GAP;
        for (int k = 0; k < n; k++)
        {
            gmb_managed_window_t* mw = &m->windows[order[k]];
            if (!mw->minimized) continue;
            gmb_winstmt_t* w = NULL;
            for (int i2 = 0; i2 < GMB_WINSTMT_MAX; i2++) if (g_ws[i2].used && g_ws[i2].id == mw->id) { w = &g_ws[i2]; break; }
            if (!w) continue;
            int ch2 = GMB_TASKBAR_H - 6;
            gmb_gfx_fill_rounded_rect(cx, barY + 3, GMB_CHIP_W, ch2, 4, mw->focused ? GMB_COL_CHIP_FOCUS : GMB_COL_CHIP);
            char ttl[160];
            snprintf(ttl, sizeof(ttl), "%s", w->title[0] ? w->title : w->name);
            wm_text(ttl, cx + 8, barY + 3 + (ch2 - ch) / 2, GMB_COL_CHIP_TEXT);
            cx += GMB_CHIP_W + GMB_CHIP_GAP;
        }
        /* STATUSBAR "testo" (parità PixelWindow/taskbar C#: testo dopo i chip). Prima il
         * runtime C disegnava solo i chip → il testo impostato da STATUSBAR con schermo
         * grafico non appariva (capacità presente solo in C#). */
        if (m->status_text[0])
            wm_text(m->status_text, cx + 6, barY + 3 + (GMB_TASKBAR_H - 6 - ch) / 2, GMB_COL_CHIP_TEXT);
    }
}

/* ===== API pixel per il client C# (stato nel runtime C) =====
 * Geometria delle finistre PIXEL: coordinate in PIXEL LOGICI; layout identico al compositor
 * (title 24 px, padding client 6 px, pulsanti a destra, bordo resize 4 px). */

void gmb_winstmt_pixel_client_rect(const char* name, int* out4)
{
    if (!out4) return;
    out4[0] = out4[1] = out4[2] = out4[3] = 0;
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return;
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (!mw) return;
    int titleh = GMB_WIN_TITLE_H; if (titleh > mw->rect.h / 2) titleh = mw->rect.h / 2;
    out4[0] = mw->rect.x + GMB_WIN_PAD;
    out4[1] = mw->rect.y + titleh;
    out4[2] = mw->rect.w - 2 * GMB_WIN_PAD;
    out4[3] = mw->rect.h - titleh - GMB_WIN_PAD;
}

/* 1 = close, 2 = minimize, 3 = maximize (0 = nessuno). */
int gmb_winstmt_pixel_control_at(const char* name, int x, int y)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return 0;
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (!mw || mw->minimized) return 0;
    if (y < mw->rect.y || y >= mw->rect.y + GMB_WIN_TITLE_H || mw->rect.w < 40) return 0;
    int closeX = mw->rect.x + mw->rect.w - 24;
    int maxX = closeX - 24;
    int minX = maxX - 24;
    if (x >= closeX && x < closeX + 20) return 1;
    if (x >= maxX && x < maxX + 20) return 3;
    if (x >= minX && x < minX + 20) return 2;
    return 0;
}

/* Bitmask bordi (N=1 S=2 E=4 W=8) per il resize in pixel; 0 se non sul bordo. */
int gmb_winstmt_pixel_resize_edge_at(const char* name, int x, int y)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return 0;
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (!mw || mw->minimized) return 0;
    const int edge = 4;
    int onE = (x >= mw->rect.x + mw->rect.w - edge && x <= mw->rect.x + mw->rect.w + edge);
    int onS = (y >= mw->rect.y + mw->rect.h - edge && y <= mw->rect.y + mw->rect.h + edge);
    int onW = (x >= mw->rect.x - edge && x <= mw->rect.x + edge);
    int onN = (y >= mw->rect.y - edge && y <= mw->rect.y + edge) && y < mw->rect.y + GMB_WIN_TITLE_H;
    return (onN ? 1 : 0) | (onS ? 2 : 0) | (onE ? 4 : 0) | (onW ? 8 : 0);
}

/* Chip taskbar pixel in (x,y): layout del compositor (chip da sinistra, minimizzate per z). */
int gmb_winstmt_pixel_taskbar_chip_at(int x, int y)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    int H = (int)(gmb_gfx_height() * (gmb_gfx_render_scale() < 1.0f ? 1.0f : gmb_gfx_render_scale()) + 0.5f);
    int barY = H - GMB_TASKBAR_H;
    if (y < barY || y >= H) return -1;
    int cx = GMB_CHIP_GAP;
    for (int z = 0; z < m->count; z++) {
        /* minimizzate in ordine di z crescente (come il render) */
        gmb_managed_window_t* best = NULL;
        for (int i = 0; i < m->count; i++) {
            gmb_managed_window_t* c = &m->windows[i];
            if (!c->minimized || !c->pixel) continue;
            int rank = 0;
            for (int j = 0; j < m->count; j++)
                if (m->windows[j].minimized && m->windows[j].pixel && m->windows[j].z < c->z) rank++;
            if (rank == z) { best = c; break; }
        }
        if (!best) break;
        if (x >= cx && x < cx + GMB_CHIP_W) return best->id;
        cx += GMB_CHIP_W + GMB_CHIP_GAP;
    }
    return -1;
}

/* Imposta i colori di una finestra pixel (0 = tema). usato da COLOR/API. */
void gmb_winstmt_set_colors(const char* name, unsigned int body, unsigned int fg, unsigned int title)
{
    gmb_winstmt_t* w = ws_find(name);
    if (!w || w->id < 0) return;
    gmb_managed_window_t* mw = gmb_window_manager_get(gmb_window_manager_static(), w->id);
    if (!mw) return;
    mw->body_color = body; mw->fg_color = fg; mw->title_color = title;
}

/* Mouse sulle finestre PIXEL (coordinate LOGICHE). kind: 0=down, 1=release, 2=move.
 * Porting 1:1 di GfxWindowManager.HandleMouse: chip taskbar → restore, topmost → raise,
 * pulsanti titlebar → azione, titlebar → drag, bordi → resize. Ritorna 1 se consumato. */
int gmb_winstmt_pixel_handle_mouse(int kind, int x, int y)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    if (!m) return 0;
    if (kind == 0)
    {
        int chip = gmb_winstmt_pixel_taskbar_chip_at(x, y);
        if (chip >= 0)
        {
            gmb_managed_window_t* cw = gmb_window_manager_get(m, chip);
            if (cw) { cw->minimized = false; cw->state = GMB_WSTATE_FLOATING; }
            gmb_window_manager_raise(m, chip);
            return 1;
        }
        int top = -1, bestz = -1;
        for (int i = 0; i < m->count; i++)
        {
            gmb_managed_window_t* w = &m->windows[i];
            if (!w->pixel || w->minimized) continue;
            if (w->desktop != m->current_desktop) continue;
            if (x >= w->rect.x && x < w->rect.x + w->rect.w &&
                y >= w->rect.y && y < w->rect.y + w->rect.h && w->z > bestz)
            { top = w->id; bestz = w->z; }
        }
        if (top < 0) { gmb_window_manager_clear_focus(m); return 0; }
        gmb_window_manager_raise(m, top);
        gmb_managed_window_t* w = gmb_window_manager_get(m, top);
        if (!w) return 0;
        int ctrl = gmb_winstmt_pixel_control_at(w->name, x, y);
        if (ctrl == 1) { gmb_window_manager_remove(m, top); return 1; }
        if (ctrl == 3) { gmb_window_manager_maximize_toggle(m, top); return 1; }
        if (ctrl == 2) { gmb_window_manager_minimize(m, top); return 1; }
        if (y < w->rect.y + GMB_WIN_TITLE_H && w->draggable && w->state == GMB_WSTATE_FLOATING)
        { gmb_window_manager_start_drag(m, top, x, y); return 1; }
        int e = gmb_winstmt_pixel_resize_edge_at(w->name, x, y);
        if (e)
        {
            char ea[3] = { 0, 0, 0 }; int n = 0;
            if (e & 1) { ea[n++] = 'n'; }
            if (e & 2) { ea[n++] = 's'; }
            if (e & 4) { ea[n++] = 'e'; }
            if (e & 8) { ea[n++] = 'w'; }
            gmb_window_manager_start_resize(m, top, x, y, ea);
            return 1;
        }
        return 1;
    }
    if (kind == 1)
    {
        gmb_window_manager_end_drag(m);
        gmb_window_manager_end_resize(m);
        return 0;
    }
    if (kind == 2)
    {
        if (gmb_window_manager_is_dragging(m)) { gmb_window_manager_update_drag(m, x, y); return 1; }
        if (gmb_window_manager_is_resizing(m)) { gmb_window_manager_update_resize(m, x, y); return 1; }
        return 0;
    }
    return 0;
}


/* ===== Mouse sulle finestre (main thread, chiamato dal present) ===== */

/* Handler mouse in coordinate PIXEL-logiche (una evento). Usato dal path gfx
 * (con metriche gfx) e dalla SdlConsole (con le proprie metriche cw/ch). */
void gmb_winstmt_handle_mouse_px(int lx, int ly, int type, int button, int cw, int ch, int workW, int workH)
{
    if (cw <= 0) cw = 8; if (ch <= 0) ch = 16;
    gmb_window_manager_t* m = gmb_window_manager_static();
    (void)button;
        /* Stato orfano: una finestra in drag/resize non piu' esistente (chiusa durante il
         * trascinamento, oppure UP perso col cursore fuori finestra) mandava il ramo MOVE a
         * ritornare SEMPRE, assorbendo per sempre i movimenti: l'altra finestra non riceveva
         * piu' eventi. Qui si azzera lo stato orfano prima di instradare l'evento. */
        if (s_drag_id >= 0 && !gmb_window_manager_get(m, s_drag_id)) s_drag_id = -1;
        if (s_resize_id >= 0 && !gmb_window_manager_get(m, s_resize_id)) { s_resize_id = -1; s_edge[0] = 0; }
        if (type == GMB_GFX_MOUSE_UP) { s_drag_id = -1; s_resize_id = -1; s_edge[0] = 0; return; }
        if (type == GMB_GFX_MOUSE_MOVE)
        {
            if (s_drag_id >= 0)
            {
                gmb_managed_window_t* mw = gmb_window_manager_get(m, s_drag_id);
                if (mw) { mw->rect.x = (lx - s_drag_dx) / cw; mw->rect.y = (ly - s_drag_dy) / ch;
                          if (mw->rect.x < 0) mw->rect.x = 0; if (mw->rect.y < 0) mw->rect.y = 0; }
            }
            else if (s_resize_id >= 0)
            {
                gmb_managed_window_t* mw = gmb_window_manager_get(m, s_resize_id);
                if (mw)
                {
                    int dx = lx - s_rsx, dy = ly - s_rsy, nw = s_rsw, nh = s_rsh;
                    int nxp = mw->rect.x * cw, nyp = mw->rect.y * ch;
                    if (strchr(s_edge, 'e')) nw = s_rsw + dx;
                    if (strchr(s_edge, 's')) nh = s_rsh + dy;
                    if (strchr(s_edge, 'w')) { nw = s_rsw - dx; nxp = mw->rect.x * cw + (s_rsw - nw); }
                    if (strchr(s_edge, 'n')) { nh = s_rsh - dy; nyp = mw->rect.y * ch + (s_rsh - nh); }
                    if (nw < 16) nw = 16; if (nh < 24) nh = 24;
                    mw->rect.x = nxp / cw; mw->rect.y = nyp / ch;
                    mw->rect.w = nw / cw; if (mw->rect.w < 2) mw->rect.w = 2;
                    mw->rect.h = nh / ch; if (mw->rect.h < 2) mw->rect.h = 2;
                }
            }
            return;
        }
        if (type != GMB_GFX_MOUSE_DOWN) return;
        s_drag_id = -1; s_resize_id = -1;   /* nuovo click: azzera drag/resize pendenti */
        /* Taskbar: chip delle finestre minimizzate (click = ripristina). Il click si CONSUMA
         * solo se colpisce un chip; altrimenti prosegue verso l'hit-test delle finestre.
         * Prima OGNI click dell'ultima riga veniva inghiottito: se una finestra copriva la
         * riga (massimizzata/drag), quella perdeva tutti gli eventi. */
        if (ly >= workH - ch)
        {
            int ids[GMB_WM_MAX_WINDOWS], nn = 0;
            for (int i = 0; i < m->count; i++) if (m->windows[i].minimized) ids[nn++] = i;
            for (int a = 0; a < nn; a++) for (int b2 = a + 1; b2 < nn; b2++)
                if (m->windows[ids[b2]].z < m->windows[ids[a]].z) { int t = ids[a]; ids[a] = ids[b2]; ids[b2] = t; }
            int cx = GMB_CHIP_GAP;
            for (int a = 0; a < nn; a++)
            {
                if (lx >= cx && lx < cx + GMB_CHIP_W) {
                    m->windows[ids[a]].minimized = false;
                    for (int i2 = 0; i2 < GMB_WINSTMT_MAX; i2++)
                        if (g_ws[i2].used && g_ws[i2].id == m->windows[ids[a]].id) g_ws[i2].minimized = false;
                    gmb_window_manager_raise(m, m->windows[ids[a]].id);
                    return;   /* chip colpito: evento consumato */
                }
                cx += GMB_CHIP_W + GMB_CHIP_GAP;
            }
            /* nessun chip colpito: NON consumare, prosegui con l'hit-test finestre */
        }
        /* TopmostAt (max z). */
        gmb_managed_window_t* top = NULL;
        for (int i = 0; i < m->count; i++)
        {
            gmb_managed_window_t* w = &m->windows[i];
            if (w->minimized) continue;
            int wx = w->rect.x * cw, wy = w->rect.y * ch, ww = w->rect.w * cw, wh = w->rect.h * ch;
            if (lx >= wx && lx < wx + ww && ly >= wy && ly < wy + wh)
                if (!top || w->z > top->z) top = w;
        }
        if (!top) return;
        gmb_window_fire(top->id, "FOCUS");
        gmb_window_fire(top->id, "ACTIVATE");
        gmb_window_manager_raise(m, top->id);
        gmb_winstmt_t* gw = NULL;
        for (int i = 0; i < GMB_WINSTMT_MAX; i++) if (g_ws[i].used && g_ws[i].id == top->id) { gw = &g_ws[i]; break; }
        int wx = top->rect.x * cw, wy = top->rect.y * ch, ww = top->rect.w * cw, wh = top->rect.h * ch;
        int relx = lx - wx, rely = ly - wy;
        /* Barra titolo = 1 cella. Layout TV (come ws_draw_core/ClipperLib):
         * close [■] a SINISTRA (celle 2..4), zoom [▼][▲] a DESTRA (ultime celle). */
        if (rely >= 0 && rely < ch)
        {
            int close0 = wx + 2 * cw, close1 = wx + 5 * cw;
            int max0 = wx + ww - 3 * cw, max1 = wx + ww;
            int min0 = wx + ww - 6 * cw, min1 = wx + ww - 3 * cw;
            if (lx >= close0 && lx < close1) { int _tid = top->id; gmb_window_fire(_tid, "CLOSE"); gmb_window_manager_remove(m, _tid); if (gw) { gw->closed = true; gw->used = false; gw->id = -1; } ws_forget_window(_tid);
                /* Ridisegna SUBITO: se era l'ultima finestra, il pump (gate count>0) non
                 * ridisegnerebbe e resterebbe a schermo la finestra "fantasma" (chiusa). */
                ws_repaint_desktop(); gmb_winstmt_render_all(); }
            else if (lx >= max0 && lx < max1)
            {
                gmb_window_fire(top->id, "RESIZE");
                if (gw && gw->maximized) { top->rect = top->restore_rect; gw->maximized = false; }
                else { top->restore_rect = top->rect; top->rect.x = 0; top->rect.y = 0;
                       top->rect.w = workW / cw; top->rect.h = workH / ch; if (gw) gw->maximized = true; }
            }
            else if (lx >= min0 && lx < min1) { gmb_window_fire(top->id, "MINIMIZE"); top->minimized = true; top->min_order = ++s_min_seq; if (gw) gw->minimized = true; }
            else { s_drag_id = top->id; s_drag_dx = relx; s_drag_dy = rely; }
            return;
        }
        /* Resize dai bordi (C# ResizeEdgeAt). */
        {
            const int edge = cw;   /* tolleranza = una cella (il resize e' raggiungibile dalla cella di bordo) */
            int onE = (lx >= wx + ww - edge) && (lx <= wx + ww + edge);
            int onS = (ly >= wy + wh - edge) && (ly <= wy + wh + edge);
            int onW = (lx >= wx - edge) && (lx <= wx + edge);
            int onN = (ly >= wy - edge) && (ly <= wy + edge) && ly < wy + ch;
            s_edge[0] = 0; s_edge[1] = 0; s_edge[2] = 0;
            if (onN && onE) strcat(s_edge, "ne"); else if (onN && onW) strcat(s_edge, "nw");
            else if (onS && onE) strcat(s_edge, "se"); else if (onS && onW) strcat(s_edge, "sw");
            else if (onS) strcat(s_edge, "s"); else if (onE) strcat(s_edge, "e"); else if (onW) strcat(s_edge, "w");
            else if (onN) strcat(s_edge, "n");
            if (s_edge[0]) { s_resize_id = top->id; s_rsx = lx; s_rsy = ly; s_rsw = ww; s_rsh = wh; }
        }
}

/* Wrapper gfx: svuota la FIFO input e converte le coord native in logiche. */
void gmb_winstmt_handle_mouse_events(void)
{
#ifdef GMB_GFX_SDL_PRESENT
    float sc = gmb_gfx_render_scale(); if (sc < 1.0f) sc = 1.0f;
    int cw = (int)(gmb_gfx_text_width("M") / sc + 0.5f); if (cw <= 0) cw = 8;
    int ch = (int)(gmb_gfx_text_height() / sc + 0.5f);   if (ch <= 0) ch = 16;
    int workW = gmb_gfx_width(), workH = gmb_gfx_height();
    int type, mx, my, button, wheel;
    while (gmb_gfx_input_poll_mouse(&type, &mx, &my, &button, &wheel))
        gmb_winstmt_handle_mouse_px((int)(mx / sc), (int)(my / sc), type, button, cw, ch, workW, workH);
#endif
}



/* Iniezione eventi mouse da env GMB_MOUSE_SCRIPT: "d,X,Y;m,X,Y;u,X,Y" (coord LOGICHE),
 * un evento per frame. Serve per test autonomi (nessun click manuale). */
void gmb_winstmt_mouse_script_pump(void)
{
#ifdef GMB_GFX_SDL_PRESENT
    static int init = 0; static char buf[1024]; static char* p = NULL;
    if (!init) { init = 1; const char* s = getenv("GMB_MOUSE_SCRIPT"); if (s && *s) { snprintf(buf, sizeof(buf), "%s", s); p = buf; } }
    if (!p || !*p) return;
    char* semi = strchr(p, ';');
    if (semi) *semi = '\0';
    char t = p[0]; int x = 0, y = 0, b = 1;
    if (sscanf(p + 1, ",%d,%d,%d", &x, &y, &b) < 2) sscanf(p + 1, " %d %d %d", &x, &y, &b);
    p = semi ? semi + 1 : p + strlen(p);
    int type = (t == 'd') ? GMB_GFX_MOUSE_DOWN : (t == 'u') ? GMB_GFX_MOUSE_UP : GMB_GFX_MOUSE_MOVE;
    gmb_gfx_input_push_mouse(type, x, y, b, 0);
#endif
}

void gmb_winstmt_restore_by_id(int id)
{
    gmb_window_manager_t* m = gmb_window_manager_static();
    gmb_managed_window_t* mw = gmb_window_manager_get(m, id);
    if (mw) { mw->minimized = false; mw->min_order = 0; gmb_window_manager_raise(m, id); }
    for (int i = 0; i < GMB_WINSTMT_MAX; i++)
        if (g_ws[i].used && g_ws[i].id == id) { g_ws[i].minimized = false; break; }
}

int gmb_winstmt_is_minimized(const char* name)
{
    gmb_winstmt_t* w = ws_find(name);
    return w ? (w->minimized ? 1 : 0) : -1;
}
