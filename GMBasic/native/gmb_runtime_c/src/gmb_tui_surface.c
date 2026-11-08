/* ============================================================================
 * gmb_tui_surface.c — Surface sink pixel (GfxSurface del piano TUI-GUI).
 * Rende le celle di una gmb_screen_buffer_t sulla pagina gfx attiva, così i
 * widget (che usano gmb_screen_*) disegnano in grafica senza modifiche.
 * ============================================================================ */
#include "gmb_runtime.h"
#include "gmb_tui.h"
#include "gmb_graphics.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gfx_sink {
    gmb_screen_sink_t sink;   /* primo campo: user = questo */
    int ox, oy, cw, ch;       /* origine pixel + dimensione cella */
} gfx_sink_t;

static uint32_t color_to_gfx(gmb_cell_color_t c)
{
    return 0x40000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* Codifica un codepoint in UTF-8 (max 4 byte). */
static int utf8_encode(uint32_t u, char* out)
{
    int n = 0;
    if (u < 0x80) out[n++] = (char)u;
    else if (u < 0x800) { out[n++] = (char)(0xC0 | (u >> 6)); out[n++] = (char)(0x80 | (u & 0x3F)); }
    else if (u < 0x10000) { out[n++] = (char)(0xE0 | (u >> 12)); out[n++] = (char)(0x80 | ((u >> 6) & 0x3F)); out[n++] = (char)(0x80 | (u & 0x3F)); }
    else { out[n++] = (char)(0xF0 | (u >> 18)); out[n++] = (char)(0x80 | ((u >> 12) & 0x3F)); out[n++] = (char)(0x80 | ((u >> 6) & 0x3F)); out[n++] = (char)(0x80 | (u & 0x3F)); }
    out[n] = '\0';
    return n;
}

static void gs_put(void* user, int x, int y, uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg)
{
    gfx_sink_t* s = (gfx_sink_t*)user;
    int px = s->ox + x * s->cw;
    int py = s->oy + y * s->ch;
    gmb_gfx_clear_rect(px, py, px + s->cw - 1, py + s->ch - 1, (int)color_to_gfx(bg));
    if (ch != 0 && ch != ' ')
    {
        char buf[8];
        utf8_encode(ch, buf);
        gmb_gfx_draw_text(buf, px, py, 0, (int)color_to_gfx(fg));
    }
}

static void gs_fill(void* user, int x, int y, int w, int h, gmb_cell_color_t bg)
{
    gfx_sink_t* s = (gfx_sink_t*)user;
    if (w <= 0 || h <= 0) return;
    int x1 = s->ox + x * s->cw;
    int y1 = s->oy + y * s->ch;
    int x2 = s->ox + (x + w) * s->cw - 1;
    int y2 = s->oy + (y + h) * s->ch - 1;
    gmb_gfx_clear_rect(x1, y1, x2, y2, (int)color_to_gfx(bg));
}

static void gs_destroy(void* user)
{
    free(user);
}

void gmb_screen_attach_gfx(gmb_screen_buffer_t* b, int origin_x, int origin_y, int cell_w, int cell_h)
{
    if (!b) return;
    gfx_sink_t* s = (gfx_sink_t*)calloc(1, sizeof(gfx_sink_t));
    if (!s) return;
    s->sink.user = s;
    s->sink.put_cell = gs_put;
    s->sink.fill = gs_fill;
    s->sink.destroy = gs_destroy;
    s->ox = origin_x;
    s->oy = origin_y;
    s->cw = cell_w > 0 ? cell_w : 8;
    s->ch = cell_h > 0 ? cell_h : 16;
    s->sink.is_gfx = 1;
    s->sink.cell_w = s->cw;
    s->sink.cell_h = s->ch;
    s->sink.origin_x = s->ox;
    s->sink.origin_y = s->oy;
    gmb_screen_set_sink(b, &s->sink);
}

/* Primitivo alto livello: rettangolo arrotondato. Con sink gfx usa il chrome
 * nativo (gmb_gfx_fill/stroke_rounded_rect); altrimenti riempie le celle di bg. */


/* ---- Auto-attach della surface (target grafico) ---- */
static int g_surface_autoattach = 1;

void gmb_surface_set_autoattach(int on) { g_surface_autoattach = on ? 1 : 0; }

void gmb_surface_autoattach(gmb_screen_buffer_t* b)
{
    if (!g_surface_autoattach) return;
    if (!b || gmb_screen_has_sink(b)) return;
    if (!gmb_gfx_screen_on()) return;
    int gw = gmb_gfx_screen_width();
    int gh = gmb_gfx_screen_height();
    int bw = gmb_screen_width(b);
    int bh = gmb_screen_height(b);
    if (gw <= 0 || gh <= 0 || bw <= 0 || bh <= 0) return;
    int cw = gw / bw; if (cw < 1) cw = 8;
    int ch = gh / bh; if (ch < 1) ch = 16;
    gmb_screen_attach_gfx(b, 0, 0, cw, ch);
}

/* ---- Present del buffer TUI globale sulla finestra grafica ---- */
#include "gmb_tui_widgets.h"
#include "gmb_tui_windowstmt.h"
#ifdef GMB_GFX_SDL_PRESENT
#include "gmb_gfx_present.h"
#endif

/* Screenshot di debug una-tantum: se GMB_GFX_SHOT=<path> e' impostata, salva la pagina
 * corrente al primo present (utile per verificare il rendering senza display interattivo). */
static void maybe_shot(void)
{
    static int done = 0; static int frames = 0;
    const char* p = gmb_opt_env("GMB_GFX_SHOT");
    if (!p || !*p || done) return;
    { extern int gmb_winstmt_count(void);
      frames++;
      if (frames < 30) return;                 /* lascia definire tutte le finestre */
      if (gmb_winstmt_count() <= 0) return; }
    done = 1;
    gmb_gfx_save_screen_png(p);
    fprintf(stderr, "[GMB_GFX_SHOT] %s\n", p);
}

void gmb_tui_present_gfx(void)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (!gmb_gfx_screen_on()) return;
    if (!gmb_gfx_present_on_main_thread()) return;
#ifdef GMB_GFX_SDL_PRESENT
    gmb_gfx_present_start();   /* idempotente, main thread */
#endif
    /* Finestre WINDOW (anche tutte chiuse): compositor pixel nativo del desktop,
     * MAI fallback a celle (che mostrerebbe una "gui testuale"). Il WM conserva le
     * finestre anche dopo Close, quindi resta la modalita' compositor. */
    {
        const char* sdlcon = gmb_opt_env("GMB_SDLCONSOLE");
        int force_tui = (sdlcon && sdlcon[0] && strcmp(sdlcon, "0") != 0);
        gmb_window_manager_t* wm = gmb_window_manager_static();
        if (!force_tui && wm && wm->count > 0)
        {
            gmb_winstmt_mouse_script_pump();
            gmb_winstmt_handle_mouse_events();
            gmb_winstmt_compose_all_pixel();
            maybe_shot();
            gmb_gfx_present_tick();
            return;
        }
    }
    /* Form (BUTTON/FORM_*): render sul main thread, solo se dirty. Il goroutine
     * eventi (gmb_form_poll) non disegna; qui si porta in pagina lo stato. */
    { extern void gmb_form_render_if_dirty(void); gmb_form_render_if_dirty(); }
    /* GUI a FORM (BUTTON/listbox/statusbar): il form È il contenuto. NON blittare
     * anche le celle del buffer clipper (era la "console TUI" che si sovrapponeva
     * al form in basso a destra). */
    { extern int gmb_form_active(void);
      if (gmb_form_active()) {
          /* OVERLAY (lista aperta di dropdown/date) SEMPRE per ULTIMO, appena prima del
           * present: cosi. nessun passaggio successivo (TUI/says) puo. ricoprirlo. */
          { extern void gmb_form_draw_overlays(void); gmb_form_draw_overlays(); }
          maybe_shot(); gmb_gfx_present_tick(); return; } }
    /* GUI grafica (non SDLConsole): NON blittare il buffer clipper (era la "console
     * TUI" che si sovrapponeva al form). Solo se GMB_SDLCONSOLE si usa il blit a celle. */
    { const char* sdlc = gmb_opt_env("GMB_SDLCONSOLE");
      if (!(sdlc && sdlc[0] && strcmp(sdlc, "0") != 0)) { maybe_shot(); gmb_gfx_present_tick(); return; } }
    gmb_clipper_state_t* st = gmb_clipper_peek_state();
    if (st && st->buffer)
    {
        int w = gmb_screen_width(st->buffer), h = gmb_screen_height(st->buffer);
        int pw = (int)(gmb_gfx_width() * gmb_gfx_render_scale() + 0.5f);
        int ph = (int)(gmb_gfx_height() * gmb_gfx_render_scale() + 0.5f);
        int cwx = (pw > 0 && w > 0) ? pw / w : gmb_gfx_text_width("M"); if (cwx < 1) cwx = 8;
        int chx = (ph > 0 && h > 0) ? ph / h : gmb_gfx_text_height();   if (chx < 1) chx = 16;
        gmb_screen_cell_t* cells = gmb_screen_cells(st->buffer);
        static int* chars = NULL; static unsigned int* fg = NULL; static unsigned int* bg = NULL; static int cap = 0;
        int n = w * h;
        if (n > cap)
        {
            free(chars); free(fg); free(bg);
            chars = (int*)malloc(sizeof(int) * (size_t)n);
            fg = (unsigned int*)malloc(sizeof(unsigned int) * (size_t)n);
            bg = (unsigned int*)malloc(sizeof(unsigned int) * (size_t)n);
            cap = n;
        }
        if (chars && fg && bg)
        {
            for (int i = 0; i < n; i++)
            {
                chars[i] = (int)cells[i].character;
                fg[i] = 0x40000000u | ((unsigned int)cells[i].fg_rgb.r << 16) | ((unsigned int)cells[i].fg_rgb.g << 8) | cells[i].fg_rgb.b;
                bg[i] = 0x40000000u | ((unsigned int)cells[i].bg_rgb.r << 16) | ((unsigned int)cells[i].bg_rgb.g << 8) | cells[i].bg_rgb.b;
            }
            gmb_gfx_render_cells_ex(chars, fg, bg, w, h, cwx, chx);
            maybe_shot();
        }
    }
    gmb_gfx_present_tick();
#endif
}

/* ---- Thread UI (main): SDL/Cocoa va toccato SOLO qui. Registrato all'avvio del
 * programma emesso; i worker GPM risultano non-main e non presentano. ---- */
static pthread_t g_ui_thread;
static int g_ui_set = 0;
void gmb_gfx_present_init_main_thread(void)
{
#ifndef GMB_GFX_SDL_PRESENT
    (void)0;
#endif
    if (!g_ui_set) { g_ui_thread = pthread_self(); g_ui_set = 1; }
}
int gmb_gfx_present_on_main_thread(void)
{
    if (!g_ui_set) return 0;   /* finché non registrato: mai main (niente SDL) */
    return pthread_equal(pthread_self(), g_ui_thread) ? 1 : 0;
}

#ifndef GMB_GFX_SDL_PRESENT
int  gmb_gfx_present_should_quit(void) { return 0; }
void gmb_gfx_present_request_quit(void) {}
void gmb_gfx_present_start(void) {}
void gmb_gfx_present_stop(void) {}
/* gmb_console.c (INKEY/READKEY) referenzia gmb_gfx_present_tick() anche senza SDL:
 * in build headless/senza GMB_GFX_SDL_PRESENT serve comunque il simbolo (no-op, come
 * gli altri stub sopra) o il link fallisce (gate C99). Vedi gmb_gfx_present_sdl.c. */
void gmb_gfx_present_tick(void) {}
#endif
