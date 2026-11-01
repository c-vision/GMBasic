/* gmb_iconsole.c — implementazione C di IConsole (IConsole.cs) + factory all'avvio.
 * SystemConsole / StandardConsole / HeadlessConsole / SdlConsole con dispatch a vtable.
 * System/Standard/Headless sono reali e headless-safe; Sdl delega a gmb_sdl_console_*.
 */
#include "gmb_iconsole.h"
#include "gmb_runtime.h"
#include "gmb_sdl_console.h"
#include "gmb_sdl_context.h"
#include "gmb_sdl_renderer.h"
#include "gmb_graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#endif

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static gmb_screen_config_t iconsole_cfg(const gmb_screen_config_t* c)
{
    return c ? *c : gmb_screen_config_get_default(GMB_SM_TEXT);
}

static const char* cursor_style_name(gmb_cursor_t s)
{
    switch (s) {
        case GMB_CURSOR_BLINKING_BLOCK:     return "blinking_block";
        case GMB_CURSOR_STEADY_BLOCK:       return "steady_block";
        case GMB_CURSOR_BLINKING_UNDERLINE: return "blinking_underline";
        case GMB_CURSOR_STEADY_UNDERLINE:   return "steady_underline";
        case GMB_CURSOR_BLINKING_BAR:       return "blinking_bar";
        case GMB_CURSOR_STEADY_BAR:         return "steady_bar";
        default:                            return "steady_block";
    }
}

/* ------------------------------------------------------------------ */
/* Implementazione condivisa                                          */
/* KIND: 0 = System, 1 = Standard, 2 = Headless, 3 = Sdl              */
/* ------------------------------------------------------------------ */

typedef struct iconsole_impl {
    int kind;
    gmb_sdl_console_t* model;   /* Standard/Headless/Sdl: buffer+escape; System: NULL */
    char title[256];
    int scroll_top, scroll_bottom;
    /* stream (IConsole.cs:95-107) */
    gmb_text_reader_t in_stream;
    gmb_text_writer_t out_stream, err_stream;
    int has_in, has_out, has_err;
    /* palette attiva (IConsole.cs:220-226) */
    unsigned char active_palette[GMB_SCFG_MAX_PALETTE][3];
    int active_palette_n;
    char active_palette_name[64];
    /* CancelKeyPress (IConsole.cs:65) */
    void* cancel_ctx;
    int (*cancel_handler)(void*, void*);
    /* SDL */
    gmb_sdl_context_t ctx;
    gmb_font_atlas_t* atlas;
    void* texture;              /* SDL_Texture* (screen target) */
    int char_w, char_h;
    int opened, visible;
    char font_path[1024];
    int font_size;
} iconsole_impl_t;

static iconsole_impl_t* impl_of(gmb_iconsole_t* c) { return c ? (iconsole_impl_t*)c->impl : NULL; }

/* --- Sistema (SystemConsole.cs) / modello (Standard/Headless/Sdl) --- */

static const char* io_get_title(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return i ? i->title : ""; }
static void io_set_title(gmb_iconsole_t* c, const char* t)
{
    iconsole_impl_t* i = impl_of(c); if (!i) return;
    snprintf(i->title, sizeof(i->title), "%s", t ? t : "");
    gmb_console_set_title(i->title);
}

static int  io_get_bg(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_sdl_console_current_bg(i->model) : c->configuration.default_bg_color; }
static void io_set_bg(gmb_iconsole_t* c, int v) { iconsole_impl_t* i = impl_of(c); if (i && i->model) gmb_sdl_console_set_current_bg(i->model, v); }
static int  io_get_fg(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_sdl_console_current_fg(i->model) : c->configuration.default_fg_color; }
static void io_set_fg(gmb_iconsole_t* c, int v) { iconsole_impl_t* i = impl_of(c); if (i && i->model) gmb_sdl_console_set_current_fg(i->model, v); }

static int io_buf_w(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_screen_width(gmb_sdl_console_screen(i->model)) : gmb_console_buffer_width(); }
static int io_buf_h(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_screen_height(gmb_sdl_console_screen(i->model)) : gmb_console_buffer_height(); }
static void io_set_buf_size(gmb_iconsole_t* c, int w, int h)
{
    iconsole_impl_t* i = impl_of(c); if (!i) return;
    if (i->model) { gmb_sdl_console_free(i->model); i->model = gmb_sdl_console_new(&c->configuration, w, h); }
    else gmb_console_set_buffer_size(w, h);
}

static int io_win_w(gmb_iconsole_t* c) { (void)c; return gmb_console_buffer_width(); }
static int io_win_h(gmb_iconsole_t* c) { (void)c; return gmb_console_buffer_height(); }
static void io_set_win_size(gmb_iconsole_t* c, int w, int h) { (void)c; gmb_console_set_window_size(w, h); }
static int io_win_left(gmb_iconsole_t* c) { (void)c; return gmb_console_window_left(); }
static int io_win_top(gmb_iconsole_t* c) { (void)c; return gmb_console_window_top(); }
static void io_set_win_pos(gmb_iconsole_t* c, int left, int top) { (void)c; gmb_console_set_window_position(left, top); }

static int io_cursor_left(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_sdl_console_cursor_col(i->model) : 0; }
static int io_cursor_top(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_sdl_console_cursor_row(i->model) : 0; }
static void io_set_cursor_pos(gmb_iconsole_t* c, int left, int top)
{
    iconsole_impl_t* i = impl_of(c); if (!i) return;
    if (i->model) gmb_sdl_console_set_cursor_position(i->model, top + 1, left + 1); /* ITerminalBuffer 1-based */
    else gmb_console_set_cursor_position(left + 1, top + 1);                        /* ANSI 1-based */
}
static int io_cursor_visible(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? gmb_sdl_console_cursor_visible(i->model) : 1; }
static void io_set_cursor_visible(gmb_iconsole_t* c, int v) { iconsole_impl_t* i = impl_of(c); if (i && i->model) gmb_sdl_console_set_cursor_visible(i->model, v); }
static void io_set_cursor_style(gmb_iconsole_t* c, gmb_cursor_t s) { (void)c; gmb_console_cursor_shape(cursor_style_name(s)); }

static void io_beep(gmb_iconsole_t* c) { (void)c; gmb_console_beep(); }
static void io_clear(gmb_iconsole_t* c) { (void)c; gmb_cls(); }
static void io_reset_color(gmb_iconsole_t* c) { (void)c; gmb_console_reset_color(); }
static void io_set_scroll_region(gmb_iconsole_t* c, int top, int bottom) { iconsole_impl_t* i = impl_of(c); if (i) { i->scroll_top = top; i->scroll_bottom = bottom; } gmb_console_set_scroll_region(top, bottom); }

static int io_is_dirty(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->model) ? i->model->dirty : gmb_console_is_dirty(); }

static int  io_process_events(gmb_iconsole_t* c) { (void)c; return 0; }
static void io_show(gmb_iconsole_t* c) { (void)c; }
static void io_present_system(gmb_iconsole_t* c) { (void)c; }
static void io_set_visibility(gmb_iconsole_t* c, int v) { (void)c; (void)v; }
static void io_set_font(gmb_iconsole_t* c, const char* p, int s)
{
    iconsole_impl_t* i = impl_of(c); if (!i) return;
    snprintf(i->font_path, sizeof(i->font_path), "%s", p ? p : "");
    i->font_size = s;
}

static void io_copy_clip(gmb_iconsole_t* c) { (void)c; }
static void io_paste_clip(gmb_iconsole_t* c) { (void)c; }
static void io_set_palette(gmb_iconsole_t* c, int idx, int r, int g, int b) { (void)c; gmb_gfx_palette(idx, r, g, b); }

/* Out/Error: System → stdout diretto; Standard → buffer + present ANSI; Headless → stdout
 * lineare (come Headless.cs); Sdl → buffer + present SDL. */
static void io_write(gmb_iconsole_t* c, const char* text)
{
    iconsole_impl_t* i = impl_of(c); if (!text) return;
    if (!i || i->kind == 0) { gmb_console_out_write(text); return; }
    if (i->kind == 2) { gmb_console_out_write(text); if (i->model) gmb_sdl_console_write(i->model, text); return; }
    if (i->kind == 1 && i->model && i->model->native_scroll_mode) {
        /* StandardConsole.WriteInternalNative: in native-scroll il testo va DIRETTO al
         * terminale con i colori correnti (la palette della console) — niente repaint
         * dello schermo, scrollback conservato, come la REPL C# in `-c`. */
        int fr = 255, fg = 255, fb = 255, br = 0, bg = 0, bb = 0;
        int fi = i->model->current_fg, bi = i->model->current_bg;
        if (fi >= 0 && fi < i->model->palette_size) { fr = i->model->palette[fi][0]; fg = i->model->palette[fi][1]; fb = i->model->palette[fi][2]; }
        if (bi >= 0 && bi < i->model->palette_size) { br = i->model->palette[bi][0]; bg = i->model->palette[bi][1]; bb = i->model->palette[bi][2]; }
        printf("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm%s", fr, fg, fb, br, bg, bb, text);
        fflush(stdout);
    }
    if (i->model) gmb_sdl_console_write(i->model, text);
}
static int io_read_char(gmb_iconsole_t* c) { (void)c; return gmb_console_in_read_char(); }

static void io_get_cell(gmb_iconsole_t* c, int x, int y, gmb_screen_cell_t* out)
{
    iconsole_impl_t* i = impl_of(c);
    if (out) *out = GMB_CELL_EMPTY;
    if (!i || !i->model || !out) return;
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int w = gmb_screen_width(b), h = gmb_screen_height(b);
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    *out = gmb_screen_cells(b)[y * w + x];
}
static void io_put_cell(gmb_iconsole_t* c, int x, int y, gmb_screen_cell_t cell)
{
    iconsole_impl_t* i = impl_of(c);
    if (!i || !i->model) return;
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int w = gmb_screen_width(b), h = gmb_screen_height(b);
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    gmb_screen_cells(b)[y * w + x] = cell;
    i->model->dirty = 1;
}
static void io_clear_rect(gmb_iconsole_t* c, gmb_screen_rect_t rect, const gmb_screen_cell_t* fill)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model) return;
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int w = gmb_screen_width(b), h = gmb_screen_height(b);
    gmb_screen_cell_t cell = fill ? *fill : GMB_CELL_EMPTY;
    for (int y = rect.y; y < rect.y + rect.h; y++)
        for (int x = rect.x; x < rect.x + rect.w; x++)
            if (x >= 0 && y >= 0 && x < w && y < h) gmb_screen_cells(b)[y * w + x] = cell;
    i->model->dirty = 1;
}

static void io_dispose(gmb_iconsole_t* c)
{
    if (!c) return;
    iconsole_impl_t* i = impl_of(c);
    if (i) { if (i->model) gmb_sdl_console_free(i->model); free(i); }
    free(c);
}

/* Present ANSI del modello (StandardConsole.RenderBuffer, via gmb_render_screen). */
static void io_present_ansi(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model) return;
    /* StandardConsole.RenderBuffer: in native-scroll le scritture sono già andate al
     * terminale → non c'è nulla da ridisegnare (niente repaint full-screen). */
    if (i->model->native_scroll_mode) { i->model->dirty = 0; return; }
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int w = gmb_screen_width(b), h = gmb_screen_height(b);
    size_t cap = (size_t)w * h * 32 + 4096;
    char* buf = (char*)malloc(cap);
    if (!buf) return;
    /* gli indici di palette delle celle vanno risolti in RGB: render_screen legge
     * SOLO i campi RGB (senza, StandardConsole usciva nero su nero). */
    gmb_sdl_console_resolve_colors(i->model);
    size_t n = gmb_render_screen(b, buf, cap);
    /* Synchronized update (DEC 2026 ?2026h/l): il terminale applica TUTTO il frame in una
     * volta -> niente flicker e niente shift dell'ultima riga (status bar) durante il
     * drag: prima il frame era una sola write, ma il terminale lo applicava a celle.
     * No-op sui terminali che non lo supportano; sync su kitty/iTerm2/Terminal.app recente. */
    if (n) { fputs("\x1b[?2026h", stdout); fwrite(buf, 1, n, stdout); fputs("\x1b[?2026l", stdout); }
    /* RenderBuffer C#: dopo il frame il cursore REALE va sulla posizione logica e
     * torna visibile (è il cursore lampeggiante del terminale). */
    { int cr = gmb_sdl_console_cursor_row(i->model), cc = gmb_sdl_console_cursor_col(i->model);
      if (cr < 0) cr = 0; if (cc < 0) cc = 0;
      fprintf(stdout, "\x1b[%d;%dH", cr + 1, cc + 1);
      fputs(gmb_sdl_console_cursor_visible(i->model) ? "\x1b[?25h" : "\x1b[?25l", stdout); }
    fflush(stdout);
    free(buf);
    i->model->dirty = 0;
}

/* --- stream (IConsole.cs:95-107) --- */
static void io_set_in(gmb_iconsole_t* c, gmb_text_reader_t r) { iconsole_impl_t* i = impl_of(c); if (!i) return; i->in_stream = r; i->has_in = 1; gmb_console_set_in(r); }
static void io_set_out(gmb_iconsole_t* c, gmb_text_writer_t w) { iconsole_impl_t* i = impl_of(c); if (!i) return; i->out_stream = w; i->has_out = 1; gmb_console_set_out(w); }
static void io_set_error(gmb_iconsole_t* c, gmb_text_writer_t w) { iconsole_impl_t* i = impl_of(c); if (!i) return; i->err_stream = w; i->has_err = 1; gmb_console_set_error(w); }

/* --- AutoFlush (IConsole.cs:59) --- */
static int  io_get_auto_flush(gmb_iconsole_t* c) { return c ? c->auto_flush : 0; }
static void io_set_auto_flush(gmb_iconsole_t* c, int v) { if (c) c->auto_flush = v; }

/* --- palette attiva (IConsole.cs:220-226) --- */
static void io_set_active_palette(gmb_iconsole_t* c, const unsigned char* palette, int n, const char* name)
{
    iconsole_impl_t* i = impl_of(c); if (!i) return;
    if (n > GMB_SCFG_MAX_PALETTE) n = GMB_SCFG_MAX_PALETTE;
    if (palette && n > 0) { memcpy(i->active_palette, palette, (size_t)n * 3); i->active_palette_n = n; }
    snprintf(i->active_palette_name, sizeof(i->active_palette_name), "%s", name ? name : "");
    /* applica alla console reale: copia nella palette del modello + redraw (SdlConsole.IConsole.cs:387) */
    if (i->model && n > 0) { memcpy(i->model->palette, i->active_palette, (size_t)n * 3); i->model->palette_size = n; i->model->dirty = 1; }
}
static const char* io_get_active_palette_name(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); return (i && i->active_palette_name[0]) ? i->active_palette_name : NULL; }

/* --- blocco rettangolare (IConsole.cs:241-263) --- */
static void io_get_rect(gmb_iconsole_t* c, gmb_screen_rect_t r, gmb_screen_cell_t* out)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model || !out) return;
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int W = gmb_screen_width(b), H = gmb_screen_height(b);
    gmb_screen_cell_t* cells = gmb_screen_cells(b);
    for (int y = 0; y < r.h; y++) for (int x = 0; x < r.w; x++) {
        int sx = r.x + x, sy = r.y + y;
        out[y * r.w + x] = (sx >= 0 && sy >= 0 && sx < W && sy < H) ? cells[sy * W + sx] : GMB_CELL_EMPTY;
    }
}
static void io_put_rect(gmb_iconsole_t* c, int x, int y, int w, int h, const gmb_screen_cell_t* cells)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model || !cells) return;
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int W = gmb_screen_width(b), H = gmb_screen_height(b);
    gmb_screen_cell_t* dst = gmb_screen_cells(b);
    for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) {
        int dx = x + xx, dy = y + yy;
        if (dx >= 0 && dy >= 0 && dx < W && dy < H) dst[dy * W + dx] = cells[yy * w + xx];
    }
    i->model->dirty = 1;
}

/* --- window left/top singoli (IConsole.cs:118-119) --- */
static void io_set_window_left(gmb_iconsole_t* c, int v) { (void)c; gmb_console_set_window_position(v, gmb_console_window_top()); }
static void io_set_window_top(gmb_iconsole_t* c, int v) { (void)c; gmb_console_set_window_position(gmb_console_window_left(), v); }

/* --- lettura (In.Read/ReadLine/ReadKey) --- */
static int io_read(gmb_iconsole_t* c) { (void)c; return gmb_console_in_read_char(); }
static int io_read_line(gmb_iconsole_t* c, char* out, int cap)
{
    (void)c; if (!out || cap <= 0) return 0;
    if (!fgets(out, cap, stdin)) { out[0] = 0; return 0; }
    size_t n = strlen(out); while (n && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = 0;
    return (int)n;
}
static int io_read_key(gmb_iconsole_t* c, gmb_terminal_key_info_t* o, int intercept)
{
    (void)c; (void)intercept; if (!o) return 0;
    int ch = getchar(); if (ch == EOF) return 0;
    o->key_char = (char)ch; o->key = ch; o->shift = o->alt = o->control = 0; return 1;
}

/* --- CancelKeyPress (IConsole.cs:65) --- */
static void io_set_cancel_handler(gmb_iconsole_t* c, void* ctx, int (*h)(void*, void*)) { iconsole_impl_t* i = impl_of(c); if (!i) return; i->cancel_ctx = ctx; i->cancel_handler = h; }

/* --- Headless/Standard/Sdl extras (base) --- */
static char* io_get_all_screen_text(gmb_iconsole_t* c) { (void)c; return NULL; }
static void  io_poll_for_resize(gmb_iconsole_t* c) { (void)c; }
static void  io_reset_to_initial_state(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); if (i && i->model) { gmb_sdl_console_clear_screen(i->model); i->model->cursor_row = 0; i->model->cursor_col = 0; i->model->dirty = 1; } }

/* --- Headless (Headless.cs:135 GetAllScreenText): testo del buffer. --- */
static char* io_headless_get_all_screen_text(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model) return NULL;
    gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
    int W = gmb_screen_width(b), H = gmb_screen_height(b);
    gmb_screen_cell_t* cells = gmb_screen_cells(b);
    char* out = (char*)malloc((size_t)W * H * 4 + H + 1); if (!out) return NULL;
    size_t o = 0;
    for (int y = 0; y < H; y++) {
        int last = -1;
        for (int x = 0; x < W; x++) { uint32_t ch = cells[y*W+x].character; if (ch && ch != ' ') last = x; }
        for (int x = 0; x <= last; x++) { uint32_t ch = cells[y*W+x].character; out[o++] = (ch && ch < 128) ? (char)ch : ' '; }
        out[o++] = '\n';
    }
    out[o] = 0; return out;
}

/* --- StandardConsole.cs:702 PollForResize: se il terminale cambia dimensione, ricalibra. --- */
static void io_standard_poll_for_resize(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model) return;
#ifdef TIOCGWINSZ
    struct winsize ws; if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        /* Ricalibra il BUFFER DEL MODELLO (StandardConsole.RecalculateBuffer): prima
         * ridimensionava solo il gmb_console globale e lo schermo restava 100x40,
         * andando a capo/stirando su terminali di dimensione diversa. */
        gmb_screen_buffer_t* b = gmb_sdl_console_screen(i->model);
        int bw = gmb_screen_width(b), bh = gmb_screen_height(b);
        if (ws.ws_col != bw || ws.ws_row != bh) {
            gmb_screen_resize(b, ws.ws_col, ws.ws_row);
            i->model->view_width = ws.ws_col; i->model->view_height = ws.ws_row;
            i->model->dirty = 1;
        }
    }
#endif
}

/* --- SdlConsole.Internals.cs:235 ResetToInitialState: reset + ri-centra finestra. --- */
static void io_sdl_reset_to_initial_state(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c); if (!i || !i->model) return;
    gmb_sdl_console_clear_screen(i->model);
    i->model->cursor_row = 0; i->model->cursor_col = 0; i->model->dirty = 1;
#ifdef GMB_GFX_SDL_PRESENT
    { gmb_sdl_context_t* ctx = (gmb_sdl_context_t*)i->model->context;
      if (ctx && ctx->window_handle) { SDL_Window* w = (SDL_Window*)ctx->window_handle;
          SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED); } }
#endif
}

/* --- Vtable: adapter di base (comune) --- */
static void iconsole_init_common(gmb_iconsole_t* c, iconsole_impl_t* i, int kind,
                                 const gmb_screen_config_t* cfg)
{
    c->configuration = iconsole_cfg(cfg);
    c->impl = i;
    i->kind = kind;
    c->auto_flush = 1;
    c->get_title = io_get_title;         c->set_title = io_set_title;
    c->get_bg = io_get_bg;               c->set_bg = io_set_bg;
    c->get_fg = io_get_fg;               c->set_fg = io_set_fg;
    c->get_buffer_width = io_buf_w;      c->get_buffer_height = io_buf_h;
    c->set_buffer_size = io_set_buf_size;
    c->get_window_width = io_win_w;      c->get_window_height = io_win_h;
    c->set_window_size = io_set_win_size;
    c->get_window_left = io_win_left;    c->get_window_top = io_win_top;
    c->set_window_position = io_set_win_pos;
    c->get_cursor_left = io_cursor_left; c->get_cursor_top = io_cursor_top;
    c->set_cursor_position = io_set_cursor_pos;
    c->get_cursor_visible = io_cursor_visible; c->set_cursor_visible = io_set_cursor_visible;
    c->set_cursor_style = io_set_cursor_style;
    c->beep = io_beep;                   c->clear = io_clear;
    c->reset_color = io_reset_color;     c->set_scroll_region = io_set_scroll_region;
    c->is_dirty = io_is_dirty;
    c->process_events = io_process_events;
    c->show = io_show;                   c->present = io_present_system;
    c->set_visibility = io_set_visibility; c->set_font = io_set_font;
    c->copy_buffer_to_clipboard = io_copy_clip; c->paste_from_clipboard = io_paste_clip;
    c->set_palette_color = io_set_palette;
    c->write = io_write;                 c->read_char = io_read_char;
    c->get_cell = io_get_cell;           c->put_cell = io_put_cell;
    c->clear_rect = io_clear_rect;
    /* IConsole.cs metodi 1:1 (stream, AutoFlush, palette attiva, buffer rettangolare,
     * window left/top, lettura, CancelKeyPress, extra Headless/Standard/Sdl). */
    c->set_in = io_set_in;               c->set_out = io_set_out;   c->set_error = io_set_error;
    c->get_auto_flush = io_get_auto_flush; c->set_auto_flush = io_set_auto_flush;
    c->set_active_palette = io_set_active_palette; c->get_active_palette_name = io_get_active_palette_name;
    c->get_rect = io_get_rect;           c->put_rect = io_put_rect;
    c->set_window_left = io_set_window_left; c->set_window_top = io_set_window_top;
    c->read = io_read;                   c->read_line = io_read_line; c->read_key = io_read_key;
    c->set_cancel_handler = io_set_cancel_handler;
    c->get_all_screen_text = io_get_all_screen_text;
    c->poll_for_resize = io_poll_for_resize;
    c->reset_to_initial_state = io_reset_to_initial_state;
    c->dispose = io_dispose;
}

/* --- SystemConsole (SystemConsole.cs:20) --- */
gmb_iconsole_t* gmb_iconsole_system_new(const gmb_screen_config_t* cfg)
{
    gmb_iconsole_t* c = (gmb_iconsole_t*)calloc(1, sizeof(*c));
    iconsole_impl_t* i = (iconsole_impl_t*)calloc(1, sizeof(*i));
    if (!c || !i) { free(c); free(i); return NULL; }
    iconsole_init_common(c, i, 0, cfg);
    snprintf(i->title, sizeof(i->title), "GMBASIC - REPL");
    return c;
}

/* --- StandardConsole (StandardConsole.cs:16) — buffer + escape, present ANSI --- */
gmb_iconsole_t* gmb_iconsole_standard_new(const gmb_screen_config_t* cfg)
{
    gmb_iconsole_t* c = (gmb_iconsole_t*)calloc(1, sizeof(*c));
    iconsole_impl_t* i = (iconsole_impl_t*)calloc(1, sizeof(*i));
    if (!c || !i) { free(c); free(i); return NULL; }
    iconsole_init_common(c, i, 1, cfg);
    i->model = gmb_sdl_console_new(&c->configuration, 0, 0);
    /* StandardConsole: DEFAULT = BUFFER a celle (rendering per celle, scroll region,
     * VIEW PRINT, SAVE/RESTORE SCREEN). Il modo "nativo" (scritture dirette al
     * terminale) si attiva solo con CONSOLE.SCROLLMODE "NATIVE". */
    gmb_sdl_console_set_scroll_mode(i->model, 0);
    c->present = io_present_ansi;
    c->poll_for_resize = io_standard_poll_for_resize;
    snprintf(i->title, sizeof(i->title), "GMBASIC - REPL");
    return c;
}

/* --- HeadlessConsole (Headless.cs:20) — buffer + log lineare, nessun render --- */
gmb_iconsole_t* gmb_iconsole_headless_new(const gmb_screen_config_t* cfg)
{
    gmb_iconsole_t* c = (gmb_iconsole_t*)calloc(1, sizeof(*c));
    iconsole_impl_t* i = (iconsole_impl_t*)calloc(1, sizeof(*i));
    if (!c || !i) { free(c); free(i); return NULL; }
    iconsole_init_common(c, i, 2, cfg);
    i->model = gmb_sdl_console_new(&c->configuration, 80, 25);
    c->present = io_present_system;                 /* Headless: Present() no-op */
    c->get_all_screen_text = io_headless_get_all_screen_text;
    snprintf(i->title, sizeof(i->title), "GMBASIC - REPL");
    return c;
}

/* --- SdlConsole (SdlConsole.cs:22) — il MODELLO possiede contesto+font+texture+finestra --- */
static void io_sdl_open(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c); if (!i || i->opened) return;
    i->opened = 1;
    /* UNA sola finestra: la crea il modello (gmb_sdl_console_open). NON creare un secondo
     * gmb_sdl_context qui — apriva una SECONDA finestra (bug "due finestre"). Il vecchio
     * GMBasic aveva un solo SdlConsole.Show(). */
    gmb_sdl_console_open(i->model);
}
static void io_sdl_present(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); if (i && i->model) gmb_sdl_console_render(i->model); }
static void io_sdl_show(gmb_iconsole_t* c) { iconsole_impl_t* i = impl_of(c); if (i) { io_sdl_open(c); i->visible = 1; gmb_sdl_console_show(i->model); } }
static int  io_sdl_process_events(gmb_iconsole_t* c) { (void)c; return gmb_sdl_manager_poll_events(); }
static void io_sdl_set_visibility(gmb_iconsole_t* c, int v) { iconsole_impl_t* i = impl_of(c); if (i) { i->visible = v; gmb_sdl_console_set_visibility(i->model, v); } }
static void io_sdl_set_font(gmb_iconsole_t* c, const char* p, int s)
{
    iconsole_impl_t* i = impl_of(c); if (!i) return;
    io_set_font(c, p, s);
    if (i->model) gmb_sdl_console_set_font(i->model, p, s);
}

gmb_iconsole_t* gmb_iconsole_sdl_new(const gmb_screen_config_t* cfg)
{
    gmb_iconsole_t* c = (gmb_iconsole_t*)calloc(1, sizeof(*c));
    iconsole_impl_t* i = (iconsole_impl_t*)calloc(1, sizeof(*i));
    if (!c || !i) { free(c); free(i); return NULL; }
    iconsole_init_common(c, i, 3, cfg);
    i->model = gmb_sdl_console_new(&c->configuration, 0, 0);
    c->show = io_sdl_show;
    c->present = io_sdl_present;
    c->process_events = io_sdl_process_events;
    c->set_visibility = io_sdl_set_visibility;
    c->set_font = io_sdl_set_font;
    c->reset_to_initial_state = io_sdl_reset_to_initial_state;
    snprintf(i->title, sizeof(i->title), "GMBASIC - REPL");
    return c;
}

/* ------------------------------------------------------------------ */
/* Factory + selezione all'avvio                                     */
/* ------------------------------------------------------------------ */

gmb_iconsole_t* gmb_iconsole_create(int mode, const gmb_screen_config_t* config)
{
    const char* hl = getenv("GMBASIC_HEADLESS");
    if (hl && *hl && strcmp(hl, "0") != 0) return gmb_iconsole_headless_new(config);
    switch (mode) {
        case 3:  return gmb_iconsole_system_new(config);    /* SYSTEM */
        case 2:  return gmb_iconsole_standard_new(config);  /* ANSI */
        case 1:  /* C64: SDL con palette/font C64 (applicati da gmb_console_select) */
        case 0:  /* %SDL */
        default: return gmb_iconsole_sdl_new(config);
    }
}

/* ScreenConfig d'avvio (Executor.Startup.cs:294-318):
 *  - C64:  GetDefault(C64) + grid override;
 *  - Text: GetDefault(Text) + grid override + DefaultFgColor=15, DefaultBgColor=1. */
static gmb_screen_config_t iconsole_build_config(int mode)
{
    extern int gmb_settings_console_int(int index, const char* key, int* out);
    gmb_screen_config_t cfg = (mode == 1)
        ? gmb_screen_config_get_default(GMB_SM_C64)
        : gmb_screen_config_get_default(GMB_SM_TEXT);
    /* Testo (non C64): palette VGA16 + fg15/bg1 (sfondo BLU), come il vecchio
     * GMBasic Executor.Startup.cs (PaletteData=GetVGA16Palette, DefaultFg=15, DefaultBg=1). */
    if (mode != 1) {
        gmb_screen_palette_vga16((void*)cfg.resolved_palette);
        cfg.palette_size = 16;
        cfg.palette_name = "VGA16";
        cfg.default_fg_color = 15;
        cfg.default_bg_color = 1;
    }
    int gw = 0, gh = 0;
    if (gmb_settings_console_int(mode, "charGridWidth", &gw) && gw > 0) cfg.char_grid_width = gw;
    if (gmb_settings_console_int(mode, "charGridHeight", &gh) && gh > 0) cfg.char_grid_height = gh;
    /* -size/-font (env) sovrascrivono i settings (Arguments.cs). */
    const char* sz = getenv("GMB_CONSOLE_SIZE");
    if (sz && *sz) { int w = 0, h = 0; if (sscanf(sz, "%d,%d", &w, &h) == 2) { if (w > 0) cfg.char_grid_width = w; if (h > 0) cfg.char_grid_height = h; } }
    const char* fn = getenv("GMB_CONSOLE_FONT");
    if (fn && *fn) { char nm[128] = {0}; int fs = 0;
        if (sscanf(fn, "%127[^,],%d", nm, &fs) >= 1) { if (nm[0]) cfg.font_path = strdup(nm); if (fs > 0) cfg.font_size = fs; } }
    return cfg;
}

static gmb_iconsole_t* g_active = NULL;

gmb_iconsole_t* gmb_iconsole_boot(void)
{
    if (g_active) return g_active;
    extern int gmb_console_mode_get(void);
    const char* hl = getenv("GMBASIC_HEADLESS");
    int headless = (hl && *hl && strcmp(hl, "0") != 0);
    int mode = gmb_console_mode_get();               /* 0=%SDL 1=C64 2=ANSI 3=SYSTEM */
    gmb_screen_config_t cfg = headless
        ? gmb_screen_config_get_default(GMB_SM_TEXT)  /* HeadlessConsole.cs:79 */
        : iconsole_build_config(mode);
    g_active = gmb_iconsole_create(mode, &cfg);
    /* NB: NIENTE show() qui. La finestra SDL la apre il CHIAMANTE (REPL: rpl_loop_sdl).
     * Aprire al boot faceva drives_output()=1 e i MiniTest (stdout) perdevano l'output. */
    return g_active;
}

gmb_iconsole_t* gmb_iconsole_active(void) { return g_active; }
void gmb_iconsole_set_active(gmb_iconsole_t* c)
{
    if (g_active && g_active != c) g_active->dispose(g_active);
    g_active = c;
}

/* La console guida l'output quando e' una console REALE: Standard/System scelti
 * esplicitamente (GMB_CONSOLE 2/3), oppure SDL con finestra effettivamente aperta.
 * Default SDL su build/CI headless (ctx.ok==0) → 0, cosi' PRINT resta su stdout come prima. */
int gmb_iconsole_drives_output(void)
{
    if (!g_active) return 0;
    iconsole_impl_t* i = (iconsole_impl_t*)g_active->impl;
    if (!i) return 0;
    if (i->kind == 0 || i->kind == 1) return 1;   /* System (stdout) / Standard (buffer) */
    if (i->kind == 3) return (i->opened && i->model && i->model->opened) ? 1 : 0;  /* SDL solo con finestra */
    return 1;   /* Headless (2): la write e stdout + buffer (log lineare) */
}

int gmb_iconsole_try_write(const char* text)
{
    if (!gmb_iconsole_drives_output() || !g_active->write) return 0;
    g_active->write(g_active, text ? text : "");
    return 1;
}

int gmb_iconsole_sdl_active(void)
{
    if (!g_active) return 0;
    iconsole_impl_t* i = (iconsole_impl_t*)g_active->impl;
    return (i && i->kind == 3 && i->opened && i->model && i->model->opened) ? 1 : 0;
}

/* Modello SdlConsole della console attiva (solo SDL/SDL-C64), altrimenti NULL.
 * Serve alla REPL per pompare eventi/rendere/leggere tasti DENTRO la finestra. */
struct gmb_sdl_console* gmb_iconsole_sdl_model(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c);
    return (i && i->kind == 3) ? i->model : NULL;
}

/* Modello a celle per QUALSIASI tipo che ne abbia uno (Standard=1, Headless=2, Sdl=3):
 * la REPL scrive banner/prompt nel BUFFER e chiama Present() (StandardConsole a celle). */
struct gmb_sdl_console* gmb_iconsole_model(gmb_iconsole_t* c)
{
    iconsole_impl_t* i = impl_of(c);
    return i ? i->model : NULL;
}

int gmb_iconsole_present_if_active(void)
{
    if (!gmb_iconsole_drives_output() || !g_active->present) return 0;
    if (g_active->is_dirty && !g_active->is_dirty(g_active)) return 0;
    g_active->present(g_active);
    return 1;
}
