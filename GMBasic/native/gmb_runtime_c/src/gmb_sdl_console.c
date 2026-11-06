/* gmb_sdl_console.c — parità CommonGraphics/Consoles/SdlConsole*.cs (console config-accepting)
 * Emulatore di terminale C: ITerminalBuffer + EscapeSequences + Writer + streams.
 * Port completo, nessun bridge C↔C#.
 */
#include "gmb_sdl_console.h"
#include "gmb_screen_config.h"
#include "gmb_runtime.h"
#include "gmb_sdl_context.h"
#include "gmb_sdl_renderer.h"
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_clipper.h"
#include "gmb_tui_widgets.h"
#include "gmb_gfx_input.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#endif

#define SC_MAX(a,b) ((a)>(b)?(a):(b))

gmb_sdl_console_t* gmb_sdl_console_new(const gmb_screen_config_t* config, int width, int height)
{
    gmb_sdl_console_t* c = (gmb_sdl_console_t*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    if (config) c->config = *config; else c->config = gmb_screen_config_get_default(GMB_SM_TEXT);
    if (width <= 0) width = c->config.char_grid_width > 0 ? c->config.char_grid_width : 80;
    if (height <= 0) height = c->config.char_grid_height > 0 ? c->config.char_grid_height : 50;
    c->buffer = gmb_screen_new(width, height);
    if (!c->buffer) { free(c); return NULL; }
    c->cursor_visible = 1;
    c->cursor_style = GMB_CS_BLOCK;
    c->blink_on = 1;
    c->blink_at = 0;
    c->scroll_top = 0;
    c->scroll_bottom = -1;
    c->view_top = 0; c->view_left = 0; c->view_width = width; c->view_height = height;
    c->current_fg = c->config.default_fg_color >= 0 ? c->config.default_fg_color : 15;
    c->current_bg = c->config.default_bg_color >= 0 ? c->config.default_bg_color : 0;
    c->palette_size = c->config.palette_size;
    if (c->palette_size > 0)
        memcpy(c->palette, c->config.resolved_palette, (size_t)c->palette_size * 3);
    else {
        gmb_screen_palette_vga16(c->palette);
        c->palette_size = 16;
    }
    /* Pulizia iniziale coi colori di default (SdlConsole ctor -> Clear()): senza, le celle
     * vuote restano bg=0 (nero) e coprono lo sfondo configurato (bg=1 = blu). */
    c->cursor_row = 0; c->cursor_col = 0;
    gmb_sdl_console_clear_screen(c);
    c->dirty = 1;
    return c;
}

void gmb_sdl_console_free(gmb_sdl_console_t* c)
{
    if (!c) return;
    if (c->font_atlas) gmb_font_atlas_free((gmb_font_atlas_t*)c->font_atlas);
    if (c->texture) gmb_sdl_target_destroy(c->texture);
    if (c->context) { gmb_sdl_context_destroy((gmb_sdl_context_t*)c->context); free(c->context); }
    gmb_screen_free(c->buffer);
    free(c);
}

gmb_screen_buffer_t* gmb_sdl_console_screen(const gmb_sdl_console_t* c)
{ return c ? c->buffer : NULL; }

int gmb_sdl_console_current_fg(const gmb_sdl_console_t* c) { return c ? c->current_fg : 0; }
int gmb_sdl_console_current_bg(const gmb_sdl_console_t* c) { return c ? c->current_bg : 0; }
void gmb_sdl_console_set_current_fg(gmb_sdl_console_t* c, int fg) { if (c) { c->current_fg = fg; c->dirty = 1; } }
void gmb_sdl_console_set_current_bg(gmb_sdl_console_t* c, int bg) { if (c) { c->current_bg = bg; c->dirty = 1; } }
int gmb_sdl_console_cursor_row(const gmb_sdl_console_t* c) { return c ? c->cursor_row : 0; }
int gmb_sdl_console_cursor_col(const gmb_sdl_console_t* c) { return c ? c->cursor_col : 0; }
int gmb_sdl_console_cursor_visible(const gmb_sdl_console_t* c) { return c ? c->cursor_visible : 0; }
void gmb_sdl_console_set_cursor_visible(gmb_sdl_console_t* c, int vis) { if (c) { c->cursor_visible = vis ? 1 : 0; c->dirty = 1; } }
gmb_cursor_style_t gmb_sdl_console_cursor_style(const gmb_sdl_console_t* c) { return c ? c->cursor_style : GMB_CS_BLOCK; }
void gmb_sdl_console_set_cursor_style(gmb_sdl_console_t* c, gmb_cursor_style_t st) { if (c) { c->cursor_style = st; } }

static int sc_w(const gmb_sdl_console_t* c) { return gmb_screen_width(c->buffer); }
static int sc_h(const gmb_sdl_console_t* c) { return gmb_screen_height(c->buffer); }

static void sc_scroll_up(gmb_sdl_console_t* c, int lines)
{
    int top = c->scroll_top;
    int bottom = c->scroll_bottom < 0 ? sc_h(c) - 1 : (c->scroll_bottom < sc_h(c) ? c->scroll_bottom : sc_h(c) - 1);
    int w = sc_w(c);
    if (lines > bottom - top + 1) lines = bottom - top + 1;
    for (int r = top; r + lines <= bottom; r++)
        memcpy(&gmb_screen_cells(c->buffer)[r * w], &gmb_screen_cells(c->buffer)[(r + lines) * w],
               (size_t)w * sizeof(gmb_screen_cell_t));
    gmb_screen_cell_t blank;
    memset(&blank, 0, sizeof(blank));
    blank.fg_palette = c->current_fg;
    blank.bg_palette = c->current_bg;
    for (int r = bottom - lines + 1; r <= bottom; r++)
        for (int x = 0; x < w; x++) gmb_screen_cells(c->buffer)[r * w + x] = blank;
    c->dirty = 1;
}

void gmb_sdl_console_print(gmb_sdl_console_t* c, char character)
{
    if (!c || !c->buffer) return;
    int w = sc_w(c), h = sc_h(c);
    switch (character) {
        case '\n':
            c->cursor_col = 0; /* C# ITerminalBuffer.Print('\n'): col=0 + AdvanceLine */
            c->cursor_row++;
            if (c->cursor_row > (c->scroll_bottom < 0 ? h - 1 : SC_MAX(0, c->scroll_bottom))) {
                c->cursor_row = (c->scroll_bottom < 0 ? h - 1 : SC_MAX(0, c->scroll_bottom));
                sc_scroll_up(c, 1);
            }
            break;
        case '\r':
            c->cursor_col = 0;
            break;
        case '\b':
            if (c->cursor_col > 0) c->cursor_col--;
            break;
        case '\t':
            /* C#: il TAB si ferma al bordo, NON va a capo */
            c->cursor_col = ((c->cursor_col / 8 + 1) * 8 < w) ? (c->cursor_col / 8 + 1) * 8 : w - 1;
            break;
        case '\a':
            gmb_console_beep();
            break;
        default: {
            if (c->cursor_col >= w) { /* wrap PRIMA di scrivere (C#:111-115) */
                c->cursor_col = 0;
                gmb_sdl_console_print(c, '\n');
            }
            if (c->cursor_row < 0 || c->cursor_row >= h) break;
            if (c->cursor_col < 0 || c->cursor_col >= w) break;
            gmb_screen_cell_t cell;
            memset(&cell, 0, sizeof(cell));
            cell.character = (unsigned char)character;
            cell.fg_palette = c->current_fg;
            cell.bg_palette = c->current_bg;
            cell.attrs = (uint8_t)c->attributes;
            gmb_screen_cells(c->buffer)[c->cursor_row * w + c->cursor_col] = cell;
            c->cursor_col++;
            if (c->cursor_col >= w) { c->cursor_col = 0; gmb_sdl_console_print(c, '\n'); }
            break;
        }
    }
    c->dirty = 1;
}

void gmb_sdl_console_clear_screen(gmb_sdl_console_t* c)
{
    if (!c || !c->buffer) return;
    gmb_screen_cell_t blank;
    memset(&blank, 0, sizeof(blank));
    blank.fg_palette = c->current_fg;
    blank.bg_palette = c->current_bg;
    int n = sc_w(c) * sc_h(c);
    for (int i = 0; i < n; i++) gmb_screen_cells(c->buffer)[i] = blank;
    c->cursor_row = 0; c->cursor_col = 0;
    c->dirty = 1;
}

void gmb_sdl_console_set_cursor_position(gmb_sdl_console_t* c, int row, int col) /* 1-based */
{
    if (!c) return;
    c->cursor_row = (row > 0 ? row - 1 : 0);
    c->cursor_col = (col > 0 ? col - 1 : 0);
    int h = sc_h(c), w = sc_w(c);
    if (c->cursor_row >= h) c->cursor_row = h - 1;
    if (c->cursor_col >= w) c->cursor_col = w - 1;
    c->dirty = 1;
}

void gmb_sdl_console_erase_to_eol(gmb_sdl_console_t* c)
{
    if (!c || !c->buffer) return;
    int w = sc_w(c);
    gmb_screen_cell_t blank;
    memset(&blank, 0, sizeof(blank));
    blank.fg_palette = c->current_fg;
    blank.bg_palette = c->current_bg;
    for (int x = c->cursor_col; x < w; x++)
        gmb_screen_cells(c->buffer)[c->cursor_row * w + x] = blank;
    c->dirty = 1;
}

/* --- EscapeSequences (EscapeSequences.cs) — CSI subset --- */

static int sc_param(gmb_sdl_console_t* c, int idx, int def)
{
    /* parametro idx-esimo (1-based) separato da ';' */
    int v = def;
    int cur = 1;
    int i = 0;
    while (i < c->esc_param_len && cur < idx) { if (c->esc_param[i] == ';') cur++; i++; }
    if (i < c->esc_param_len) v = atoi(c->esc_param + i);
    return v;
}

void gmb_sdl_console_process_char(gmb_sdl_console_t* c, char ch)
{
    if (!c) return;
    if (c->esc_state == 0) {
        if (ch == 0x1B) { c->esc_state = 1; return; }
        gmb_sdl_console_print(c, ch);
        return;
    }
    if (c->esc_state == 1) {
        if (ch == '[') { c->esc_state = 3; c->esc_param_len = 0; c->esc_param[0] = 0; return; }
        if (ch == ']') { c->esc_state = 2; return; }
        if (ch == '(' || ch == ')') { c->esc_state = 4; return; }
        c->esc_state = 0;
        return;
    }
    if (c->esc_state == 2) { /* OSC: consuma fino a BEL o ST */
        if (ch == 0x07) c->esc_state = 0;
        return;
    }
    if (c->esc_state == 4) { c->esc_state = 0; return; } /* charset select: ignora */
    /* state 3: parametri + finale */
    if ((ch >= '0' && ch <= '9') || ch == ';' || ch == '?' || ch == '>' || ch == '<' || ch == '=') {
        if (c->esc_param_len < (int)sizeof(c->esc_param) - 1) c->esc_param[c->esc_param_len++] = ch;
        c->esc_param[c->esc_param_len] = 0;
        return;
    }
    int n = c->esc_param_len > 0 ? atoi(c->esc_param) : 0;
    switch (ch) {
        case 'A': c->cursor_row -= (n > 0 ? n : 1); if (c->cursor_row < 0) c->cursor_row = 0; break;
        case 'B': c->cursor_row += (n > 0 ? n : 1); if (c->cursor_row >= sc_h(c)) c->cursor_row = sc_h(c) - 1; break;
        case 'C': c->cursor_col += (n > 0 ? n : 1); if (c->cursor_col >= sc_w(c)) c->cursor_col = sc_w(c) - 1; break;
        case 'D': c->cursor_col -= (n > 0 ? n : 1); if (c->cursor_col < 0) c->cursor_col = 0; break;
        case 'H': case 'f': {
            int r = sc_param(c, 1, 1), co = sc_param(c, 2, 1);
            gmb_sdl_console_set_cursor_position(c, r, co);
            break;
        }
        case 'J': {
            int mode = n;
            if (mode == 2 || mode == 3) gmb_sdl_console_clear_screen(c);
            break;
        }
        case 'K': gmb_sdl_console_erase_to_eol(c); break;
        case 'm': {
            /* SGR: 0 reset, 30-37/90-97 fg, 40-47/100-107 bg */
            if (n == 0) { c->current_fg = c->config.default_fg_color >= 0 ? c->config.default_fg_color : 15; c->current_bg = c->config.default_bg_color >= 0 ? c->config.default_bg_color : 0; c->attributes = 0; }
            else if (n >= 30 && n <= 37) c->current_fg = n - 30;
            else if (n >= 90 && n <= 97) c->current_fg = n - 90 + 8;
            else if (n >= 40 && n <= 47) c->current_bg = n - 40;
            else if (n >= 100 && n <= 107) c->current_bg = n - 100 + 8;
            else if (n == 1) c->attributes |= 1;
            break;
        }
        case 'l': case 'h': {
            if (strstr(c->esc_param, "?25")) c->cursor_visible = (ch == 'h') ? 1 : 0;
            break;
        }
        case 'r': {
            int t = sc_param(c, 1, 1), b = sc_param(c, 2, 0);
            c->scroll_top = (t > 0 ? t - 1 : 0);
            c->scroll_bottom = (b > 0 ? b - 1 : -1);
            break;
        }
        default: break;
    }
    c->dirty = 1;
    c->esc_state = 0;
    c->esc_param_len = 0;
    c->esc_param[0] = 0;
}

void gmb_sdl_console_write(gmb_sdl_console_t* c, const char* s)
{
    if (!c || !s) return;
    if (c->native_scroll_mode) { /* StandardConsole nativo: va al terminale reale */
        gmb_console_out_write(s);
        return;
    }
    for (const char* p = s; *p; p++) gmb_sdl_console_process_char(c, *p);
}
void gmb_sdl_console_write_line(gmb_sdl_console_t* c, const char* s)
{
    gmb_sdl_console_write(c, s);
    gmb_sdl_console_write(c, "\n");
}

/* --- StandardConsole scroll mode (StandardConsole.cs:84-87,409-432) --- */
int gmb_sdl_console_is_native_scroll_mode(const gmb_sdl_console_t* c) { return c ? c->native_scroll_mode : 0; }

void gmb_sdl_console_set_scroll_mode(gmb_sdl_console_t* c, int native)
{
    if (!c) return;
    if (c->native_scroll_mode == native) return;
    c->native_scroll_mode = native ? 1 : 0;
    if (native) gmb_console_clear_scrollback();
}

void gmb_console_clear_scrollback(void)
{
    /* ANSI/xterm: svuota lo scrollback del terminale reale (StandardConsole.cs:432). */
    printf("\033[3J");
    fflush(stdout);
}

/* ============================================================================
 * Rendering proprio di SdlConsole (SdlConsole.cs ctor + Internals.cs)
 * UpdateFontAndRecalibrate + RenderBuffer/DrawCell. Le primitive grafiche sono
 * usate SOLO per disegnare; finestra/font/griglia sono di proprieta' della console.
 * ========================================================================== */

#ifdef GMB_GFX_SDL_PRESENT
static int sdlcon_file_exists(const char* p);

/* Asset della `Resources/` CONDIVISA (una sola, alla radice del monorepo). */
static int sdlcon_shared_asset(const char* name, char* out, int cap)
{
    char dir[2048];
    if (!gmb_paths_resources_dir(dir, sizeof dir) || !dir[0]) return 0;
    snprintf(out, (size_t)cap, "%s/%s", dir, name);
    return sdlcon_file_exists(out);
}

static int sdlcon_file_exists(const char* p)
{
    if (!p || !*p) return 0;
    FILE* f = fopen(p, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* FontManager.FindFontEntry/SelectMonospaceFont (Manager.cs:286): risolve il nome font
 * della config a un file, altrimenti fallback monospace macOS
 * DejaVuSansMono, Monaco, Andale Mono, Menlo. */
static int sdlcon_resolve_font(const char* name, char* out, int cap)
{
    if (!name || !*name) return 0;
    if (strchr(name, '/') || strstr(name, ".ttf") || strstr(name, ".otf") || strstr(name, ".ttc")) {
        if (sdlcon_file_exists(name)) { snprintf(out, cap, "%s", name); return 1; }
        char alt[1200]; snprintf(alt, sizeof(alt), "Resources/%s", name);
        if (sdlcon_file_exists(alt)) { snprintf(out, cap, "%s", alt); return 1; }
        return 0;
    }
    if (strcasestr(name, "c64") || strcasestr(name, "sixtyfour"))
    {
        /* C64 Pro Mono rimosso (licenza): la modalita' C64 usa Sixtyfour (OFL 1.1). */
        if (sdlcon_shared_asset("Sixtyfour.ttf", out, cap)) return 1;
        snprintf(out, cap, "Resources/Sixtyfour.ttf"); return sdlcon_file_exists(out);
    }
    if (strcasestr(name, "fira")) { const char* p[] = { "Resources/FiraCode-Regular.ttf", "Resources/FiraCode.ttf", "/Library/Fonts/FiraCode-Regular.ttf", NULL };
        for (int i = 0; p[i]; i++) if (sdlcon_file_exists(p[i])) { snprintf(out, cap, "%s", p[i]); return 1; } return 0; }
    if (strcasestr(name, "menlo"))  { snprintf(out, cap, "/System/Library/Fonts/Menlo.ttc"); return sdlcon_file_exists(out); }
    if (strcasestr(name, "monaco")) { snprintf(out, cap, "/System/Library/Fonts/Monaco.ttf"); return sdlcon_file_exists(out); }
    /* SF Mono (Apple) NON e' ridistribuibile: rimosso dal repo. L'alias resta e punta a
     * Sixtyfour (SIL OFL 1.1, "fonts based on the Commodore 64"), cosi' chi aveva 'sfmono'
     * nella configurazione continua a funzionare senza il font Apple. */
    if (strcasestr(name, "sfmono") || strcasestr(name, "sf mono") || strcasestr(name, "sixtyfour"))
    {
        if (sdlcon_shared_asset("Sixtyfour.ttf", out, cap)) return 1;
        snprintf(out, cap, "Resources/Sixtyfour.ttf"); return sdlcon_file_exists(out);
    }
    if (strcasestr(name, "andale")) { snprintf(out, cap, "/System/Library/Fonts/Supplemental/Andale Mono.ttf"); return sdlcon_file_exists(out); }
    if (strcasestr(name, "dejavu")) { snprintf(out, cap, "/Library/Fonts/DejaVuSansMono.ttf"); return sdlcon_file_exists(out); }
    if (strcasestr(name, "courier")) { snprintf(out, cap, "/System/Library/Fonts/Supplemental/Courier New.ttf"); return sdlcon_file_exists(out); }
    if (strcasestr(name, "arial")) { snprintf(out, cap, "/System/Library/Fonts/Supplemental/Arial.ttf"); return sdlcon_file_exists(out); }
    return 0;
}

static void sdlcon_select_monospace(gmb_sdl_console_t* c)
{
    if (c->font_path[0]) return;
    if (sdlcon_resolve_font(c->config.font_path, c->font_path, sizeof(c->font_path))) return;
    /* Stesso ordine della REPL C# (FontManager.SelectMonospaceFont): prima il font del
     * progetto (Resources/font.ttf = Cascadia, OFL), poi i monospace con path assoluto.
     * Serve a rendere identico l'aspetto fra SDLConsole C e C#.
     * IMPORTANTE: Menlo PRIMA di Monaco — Monaco non ha i glifi box-drawing (┌─┐║ ecc.)
     * e le cornici TUI sparivano (tofu/vuoto). */
    { extern int access(const char*, int);
      const char* direct[] = { "Resources/font.ttf", "/System/Library/Fonts/Menlo.ttc",
                               "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
                               "/System/Library/Fonts/Monaco.ttf", NULL };
      for (int i = 0; direct[i]; i++)
          if (access(direct[i], 4) == 0) { snprintf(c->font_path, sizeof(c->font_path), "%s", direct[i]); return; } }
    /* Preferisci un font con i glifi box-drawing (Menlo); Monaco non li ha (tofu). */
    const char* fb[] = { "Menlo", "DejaVuSansMono", "Monaco", "Andale Mono", NULL };
    for (int i = 0; fb[i]; i++)
        if (sdlcon_resolve_font(fb[i], c->font_path, sizeof(c->font_path))) return;
    snprintf(c->font_path, sizeof(c->font_path), "%s", "/System/Library/Fonts/Monaco.ttf");
}

static void sdlcon_palette_rgb(gmb_sdl_console_t* c, int idx, const gmb_cell_color_t* rgb,
                               int* r, int* g, int* b)
{
    if (idx < 0 && rgb) { *r = rgb->r; *g = rgb->g; *b = rgb->b; return; }
    if (idx >= 0 && idx < c->palette_size) { *r = c->palette[idx][0]; *g = c->palette[idx][1]; *b = c->palette[idx][2]; return; }
    { int v = (idx <= 0) ? 0 : 170; *r = *g = *b = v; }
}
#endif /* GMB_GFX_SDL_PRESENT */

/* Risolve fg/bg_palette → fg_rgb/bg_rgb su tutte le celle.
 * Serve al render ANSI (StandardConsole/Present): gmb_render_screen legge SOLO i campi
 * RGB risolti, mentre la print() del modello memorizza solo gli INDICI di palette.
 * Senza questa risoluzione lo schermo ANSI usciva tutto nero (0,0,0). */
void gmb_sdl_console_resolve_colors(gmb_sdl_console_t* c)
{
    if (!c || !c->buffer) return;
    int w = gmb_screen_width(c->buffer), h = gmb_screen_height(c->buffer);
    gmb_screen_cell_t* cells = gmb_screen_cells(c->buffer);
    for (int i = 0; i < w * h; i++)
    {
        gmb_screen_cell_t* cell = &cells[i];
        int fr, fg, fb, br, bg, bb;
        int fi = cell->fg_palette, bi = cell->bg_palette;
        if (fi >= 0 && fi < c->palette_size) { fr = c->palette[fi][0]; fg = c->palette[fi][1]; fb = c->palette[fi][2]; }
        else { int v = fi <= 0 ? 0 : 170; fr = fg = fb = v; }
        if (bi >= 0 && bi < c->palette_size) { br = c->palette[bi][0]; bg = c->palette[bi][1]; bb = c->palette[bi][2]; }
        else { int v = bi <= 0 ? 0 : 170; br = bg = bb = v; }
        cell->fg_rgb.r = (uint8_t)fr; cell->fg_rgb.g = (uint8_t)fg; cell->fg_rgb.b = (uint8_t)fb;
        cell->fg_rgb.is_default = false;
        cell->bg_rgb.r = (uint8_t)br; cell->bg_rgb.g = (uint8_t)bg; cell->bg_rgb.b = (uint8_t)bb;
        cell->bg_rgb.is_default = false;
    }
}

void gmb_sdl_console_update_font(gmb_sdl_console_t* c)
{
    if (!c) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (!c->context) return;
    gmb_sdl_context_t* ctx = (gmb_sdl_context_t*)c->context;
    int sc = gmb_sdl_context_render_scale(ctx); if (sc < 1) sc = 1;
    c->render_scale = sc;
    int base = c->font_size > 0 ? c->font_size : (c->config.font_size > 0 ? c->config.font_size : 16);
    int px = base * sc;                       /* scaledFontSize = fontSize * renderScale */
    sdlcon_select_monospace(c);
    if (c->font_atlas) { gmb_font_atlas_free((gmb_font_atlas_t*)c->font_atlas); c->font_atlas = NULL; }
    c->font_atlas = gmb_font_atlas_new();
    if (!gmb_font_atlas_load((gmb_font_atlas_t*)c->font_atlas, ctx->renderer_handle, c->font_path, px))
        gmb_font_atlas_load((gmb_font_atlas_t*)c->font_atlas, ctx->renderer_handle,
                            "/System/Library/Fonts/Monaco.ttf", px);
    c->char_width = 0; c->char_height = 0;
    gmb_font_atlas_metrics((gmb_font_atlas_t*)c->font_atlas, &c->char_width, &c->char_height);
    if (c->char_width <= 0) c->char_width = 8;
    if (c->char_height <= 0) c->char_height = px;
    int gw = gmb_screen_width(c->buffer), gh = gmb_screen_height(c->buffer);
    int winW = (gw * c->char_width) / sc; if (winW < 1) winW = 1;
    int winH = (gh * c->char_height) / sc; if (winH < 1) winH = 1;
    gmb_sdl_context_set_window_size(ctx, winW, winH);
    /* SdlConsole.Internals.cs:249: dopo il resize la finestra va RI-CENTRATA (nasce 1x1
     * centrata e crescendo finiva in basso a destra) + portata in primo piano/focus. */
    { SDL_Window* w = (SDL_Window*)ctx->window_handle;
      if (w) { SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
               SDL_RaiseWindow(w); SDL_SetWindowInputFocus(w); } }
    if (c->texture) { gmb_sdl_target_destroy(c->texture); c->texture = NULL; }
    c->texture = gmb_sdl_target_create(ctx->renderer_handle, gw * c->char_width, gh * c->char_height);
    c->dirty = 1;
#else
    (void)c;
#endif
}

int gmb_sdl_console_open(gmb_sdl_console_t* c)
{
    if (!c) return 0;
    if (c->opened) return 1;
    c->opened = 1;
#ifdef GMB_GFX_SDL_PRESENT
    gmb_sdl_manager_init();                    /* SdlManager.Instance.Initialize() */
    gmb_sdl_context_t* ctx = (gmb_sdl_context_t*)calloc(1, sizeof(gmb_sdl_context_t));
    /* SdlConsole.cs:122: finestra creata con SDL_WINDOWPOS_CENTERED (x e y). */
    if (ctx) { gmb_sdl_context_create(ctx, "GMBASIC - REPL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1, 1, 0); c->context = ctx; }
    gmb_sdl_console_update_font(c);
#endif
    c->cursor_visible = 0;   /* console TUI: nessun cursore ("set cursor off") */
    return 1;
}

#ifdef GMB_GFX_SDL_PRESENT
extern int gmb_gfx_box_mask(uint32_t cp, int* dbl);                            /* gmb_gfx_cellfont.c */
extern int gmb_gfx_box_segments(uint32_t cp, int cw, int ch, int seg[8][4]);  /* gmb_gfx_cellfont.c */
/* Box-drawing PROCEDURALE anche nella SDLConsole: STESSA geometria del renderer a pixel
 * (gmb_gfx_box_segments, unica fonte). I glifi dei font hanno side-bearing/metriche proprie
 * e le cornici risultavano spezzate ("═" che non si unisce a "╔"/"╗"); qui i segmenti si
 * disegnano dalla geometria della cella, con gli angoli annidati come nei glifi CP437. */
static void sdlcon_box_cell(void* rh, int x0, int y0, int cw, int ch, uint32_t cp,
                            int r, int g, int b)
{
    int seg[8][4];
    int n = gmb_gfx_box_segments(cp, cw, ch, seg);
    for (int i = 0; i < n; i++)
        gmb_sdl_render_fill(rh, x0 + seg[i][0], y0 + seg[i][1], seg[i][2], seg[i][3], r, g, b);
}

/* Block elements procedurati (come cf_box): i glifi ▀▄█▌▐ del font hanno side-bearing e a
 * queste dimensioni lasciavano righe "tratteggiate" (bottoni TV e progress bar). */
static int sdlcon_block_cell(void* rh, int x0, int y0, int cw, int ch, uint32_t cp,
                             int r, int g, int b)
{
    int hw = cw / 2, hh = ch / 2;
    switch (cp)
    {
    case 0x2588: gmb_sdl_render_fill(rh, x0, y0, cw, ch, r, g, b); return 1;      /* █ */
    case 0x2580: gmb_sdl_render_fill(rh, x0, y0, cw, hh, r, g, b); return 1;      /* ▀ */
    case 0x2584: gmb_sdl_render_fill(rh, x0, y0 + hh, cw, ch - hh, r, g, b); return 1; /* ▄ */
    case 0x258C: gmb_sdl_render_fill(rh, x0, y0, hw, ch, r, g, b); return 1;      /* ▌ */
    case 0x2590: gmb_sdl_render_fill(rh, x0 + hw, y0, cw - hw, ch, r, g, b); return 1; /* ▐ */
    case 0x2581: gmb_sdl_render_fill(rh, x0, y0 + ch - ch / 8, cw, ch / 8, r, g, b); return 1;
    case 0x2582: gmb_sdl_render_fill(rh, x0, y0 + ch - ch / 4, cw, ch / 4, r, g, b); return 1;
    case 0x2583: gmb_sdl_render_fill(rh, x0, y0 + ch - ch / 3, cw, ch / 3, r, g, b); return 1;
    case 0x2585: gmb_sdl_render_fill(rh, x0, y0 + ch - (ch * 5) / 8, cw, (ch * 5) / 8, r, g, b); return 1;
    case 0x2586: gmb_sdl_render_fill(rh, x0, y0 + ch - (ch * 3) / 4, cw, (ch * 3) / 4, r, g, b); return 1;
    case 0x2587: gmb_sdl_render_fill(rh, x0, y0 + ch - (ch * 7) / 8, cw, (ch * 7) / 8, r, g, b); return 1;
    default: return 0;
    }
}

/* Frecce e triangoli procedurali (non alterano il buffer: cambia solo il disegno della
 * stessa cella). Servono a scrollbar ▲▼, combo ▾, spin ▴▾, menu ▸, calendario ◄►. */
static int sdlcon_symbol_cell(void* rh, int x0, int y0, int cw, int ch, uint32_t cp,
                              int r, int g, int b)
{
    int cx = x0 + cw / 2, cy = y0 + ch / 2;
    int hh = (ch * 3) / 4; if (hh < 3) hh = ch;
    int ww = (cw * 3) / 4; if (ww < 3) ww = cw;
    switch (cp)
    {
    case 0x25B2: /* ▲ */
        for (int i = 0; i < hh; i++) { int w = (i + 1) * cw / hh; if (w < 1) w = 1;
            gmb_sdl_render_fill(rh, cx - w / 2, y0 + (ch - hh) / 2 + i, w, 1, r, g, b); }
        return 1;
    case 0x25BC: /* ▼ */
        for (int i = 0; i < hh; i++) { int w = (hh - i) * cw / hh; if (w < 1) w = 1;
            gmb_sdl_render_fill(rh, cx - w / 2, y0 + (ch - hh) / 2 + i, w, 1, r, g, b); }
        return 1;
    case 0x25C0: /* ◄ */
        for (int i = 0; i < ww; i++) { int h = (i + 1) * ch / ww; if (h < 1) h = 1;
            gmb_sdl_render_fill(rh, x0 + (cw - ww) / 2 + i, cy - h / 2, 1, h, r, g, b); }
        return 1;
    case 0x25B6: /* ► */
        for (int i = 0; i < ww; i++) { int h = (ww - i) * ch / ww; if (h < 1) h = 1;
            gmb_sdl_render_fill(rh, x0 + (cw - ww) / 2 + i, cy - h / 2, 1, h, r, g, b); }
        return 1;
    case 0x2191: case 0x2193: /* ↑ ↓ */
    {
        int sw = cw / 8; if (sw < 1) sw = 1;
        int head = hh / 3; if (head < 2) head = 2;
        int top = y0 + (ch - hh) / 2;
        int up = (cp == 0x2191);
        gmb_sdl_render_fill(rh, cx - sw / 2, up ? top + head : top, sw, hh - head, r, g, b);
        for (int i = 0; i < head; i++) { int w = (i + 1) * cw / head; if (w < 1) w = 1;
            gmb_sdl_render_fill(rh, cx - w / 2, up ? top + i : top + hh - 1 - i, w, 1, r, g, b); }
        return 1;
    }
    case 0x2190: case 0x2192: /* ← → */
    {
        int sh = ch / 8; if (sh < 1) sh = 1;
        int head = ww / 3; if (head < 2) head = 2;
        int left = x0 + (cw - ww) / 2;
        int right = (cp == 0x2192);
        gmb_sdl_render_fill(rh, right ? left : left + head, cy - sh / 2, ww - head, sh, r, g, b);
        for (int i = 0; i < head; i++) { int h2 = (i + 1) * ch / head; if (h2 < 1) h2 = 1;
            gmb_sdl_render_fill(rh, right ? left + head - 1 - i : left + i, cy - h2 / 2, 1, h2, r, g, b); }
        return 1;
    }
    case 0x2195: /* ↕ */
    {
        int sw = cw / 8; if (sw < 1) sw = 1;
        int head = ch / 5; if (head < 2) head = 2;
        gmb_sdl_render_fill(rh, cx - sw / 2, y0 + head, sw, ch - 2 * head, r, g, b);
        for (int i = 0; i < head; i++) { int w = (i + 1) * cw / head; if (w < 1) w = 1;
            gmb_sdl_render_fill(rh, cx - w / 2, y0 + i, w, 1, r, g, b);
            gmb_sdl_render_fill(rh, cx - w / 2, y0 + ch - 1 - i, w, 1, r, g, b); }
        return 1;
    }
    default: return 0;
    }
}
#endif /* GMB_GFX_SDL_PRESENT */

void gmb_sdl_console_render(gmb_sdl_console_t* c){
    if (!c) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (!c->opened) gmb_sdl_console_open(c);
    gmb_sdl_context_t* ctx = (gmb_sdl_context_t*)c->context;
    if (!ctx || !ctx->ok || !c->texture || !c->font_atlas) return;
    /* Lampeggio del cursore: BLINK_MS (400) come SdlConsole.cs. Il render viene
     * richiamato dal loop della console (~60 Hz), quindi il toggle e' a tempo. */
    long long now_ms = (long long)gmb_ticks_ms();
    if (now_ms - c->blink_at >= 400) {
        c->blink_on = !c->blink_on;
        c->blink_at = now_ms;
    }
    int gw = gmb_screen_width(c->buffer), gh = gmb_screen_height(c->buffer);
    int cw = c->char_width, ch = c->char_height;
    int dbg = c->config.default_bg_color >= 0 ? c->config.default_bg_color : 0;
    int dr = 0, dg = 0, db = 0; sdlcon_palette_rgb(c, dbg, NULL, &dr, &dg, &db);
    gmb_sdl_render_begin(ctx->renderer_handle, c->texture);
    gmb_sdl_render_clear(ctx->renderer_handle, dr, dg, db);
    gmb_screen_cell_t* cells = gmb_screen_cells(c->buffer);
    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) {
            gmb_screen_cell_t cell = cells[y * gw + x];
            int is_cursor = c->cursor_visible && c->blink_on &&
                            y == c->cursor_row && x == c->cursor_col;
            int inv = is_cursor || ((cell.attrs & GMB_ATTR_REVERSE) != 0);
            int fr, fg, fb, br, bg, bb;
            sdlcon_palette_rgb(c, cell.fg_palette, &cell.fg_rgb, &fr, &fg, &fb);
            sdlcon_palette_rgb(c, cell.bg_palette, &cell.bg_rgb, &br, &bg, &bb);
            if (inv) { int t; t = fr; fr = br; br = t; t = fg; fg = bg; bg = t; t = fb; fb = bb; bb = t; }
            int x0 = x * cw, y0 = y * ch;
            gmb_sdl_render_fill(ctx->renderer_handle, x0, y0, cw, ch, br, bg, bb);
            if (cell.character > 0x20u)
            {
                int bdbl = 0;
                if (sdlcon_symbol_cell(ctx->renderer_handle, x0, y0, cw, ch, cell.character, fr, fg, fb)) { }
                else if (sdlcon_block_cell(ctx->renderer_handle, x0, y0, cw, ch, cell.character, fr, fg, fb)) { }
                else if (gmb_gfx_box_mask(cell.character, &bdbl))
                    sdlcon_box_cell(ctx->renderer_handle, x0, y0, cw, ch, cell.character, fr, fg, fb);
                else
                    gmb_font_atlas_draw_glyph((gmb_font_atlas_t*)c->font_atlas, ctx->renderer_handle,
                                              (int)cell.character, x0, y0, cw, ch, fr, fg, fb);
            }
        }
    }
    gmb_sdl_render_present(ctx->renderer_handle, c->texture);
    c->dirty = 0;
#else
    (void)c;
#endif
}

void gmb_sdl_console_show(gmb_sdl_console_t* c)
{
    if (!c) return;
    gmb_sdl_console_open(c);
#ifdef GMB_GFX_SDL_PRESENT
    if (c->context) gmb_sdl_context_set_visible((gmb_sdl_context_t*)c->context, 1);
#endif
    c->visible = 1;
    gmb_sdl_console_render(c);
}

void gmb_sdl_console_set_visibility(gmb_sdl_console_t* c, int visible)
{
    if (!c) return;
    c->visible = visible ? 1 : 0;
#ifdef GMB_GFX_SDL_PRESENT
    if (c->context) gmb_sdl_context_set_visible((gmb_sdl_context_t*)c->context, c->visible);
#endif
}

void gmb_sdl_console_set_font(gmb_sdl_console_t* c, const char* path, int size)
{
    if (!c) return;
    if (path && *path) snprintf(c->font_path, sizeof(c->font_path), "%s", path);
    if (size > 0) c->font_size = size;
    gmb_sdl_console_update_font(c);
}

/* ============================================================================
 * Pump eventi della SECONDA finestra (SdlConsole): tastiera + mouse.
 * Rende le finestre/controlli TUI reattivi. Port di SdlManager.PollEvents +
 * dispatch al WindowManager (come il path gfx).
 * ========================================================================== */
#ifdef GMB_GFX_SDL_PRESENT
static int sdlcon_map_key(int sym, char* out)
{
    switch (sym) {
        case SDLK_RETURN: case SDLK_KP_ENTER: out[0] = '\r'; out[1] = 0; return 1;
        case SDLK_BACKSPACE: out[0] = 127; out[1] = 0; return 1;   /* GMB_K_BS (127), come il terminale */
        case SDLK_TAB: out[0] = '\t'; out[1] = 0; return 1;
        case SDLK_ESCAPE: out[0] = 0x1b; out[1] = 0; return 1;
        /* Frecce/home/end/pg: direttamente le COSTANTI GMB_K_* (byte), che e' cio' che i
         * controlli TUI (es. codearea_key) si aspettano (gmb_clipper_input_inkey non decodifica CSI). */
        case SDLK_LEFT:  out[0] = (char)19; out[1] = 0; return 1;   /* GMB_K_LEFT */
        case SDLK_RIGHT: out[0] = (char)4;  out[1] = 0; return 1;   /* GMB_K_RIGHT */
        case SDLK_UP:    out[0] = (char)5;  out[1] = 0; return 1;   /* GMB_K_UP */
        case SDLK_DOWN:  out[0] = (char)24; out[1] = 0; return 1;   /* GMB_K_DOWN */
        case SDLK_HOME:  out[0] = (char)1;  out[1] = 0; return 1;   /* GMB_K_HOME */
        case SDLK_END:   out[0] = (char)6;  out[1] = 0; return 1;   /* GMB_K_END */
        case SDLK_PAGEUP:   out[0] = (char)18; out[1] = 0; return 1; /* GMB_K_PGUP */
        case SDLK_PAGEDOWN: out[0] = (char)3;  out[1] = 0; return 1; /* GMB_K_PGDN */
        case SDLK_DELETE:   out[0] = (char)7;  out[1] = 0; return 1; /* GMB_K_DEL */
        case SDLK_INSERT:   out[0] = (char)22; out[1] = 0; return 1; /* GMB_K_INS */
        default: break;
    }
    if (sym >= SDLK_a && sym <= SDLK_z) { out[0] = (char)(sym - 32); out[1] = 0; return 1; }  /* sempre MAIUSCOLO (GWBASIC/C64): la console non conosce il minuscolo */
    if (sym >= SDLK_0 && sym <= SDLK_9) { out[0] = (char)sym; out[1] = 0; return 1; }
    if (sym == SDLK_SPACE) { out[0] = ' '; out[1] = 0; return 1; }
    return 0;
}
#endif

/* --- Status bar console: testo + chip minimizzate. Se i titoli NON stanno dopo il
 * testo, si raccolgono in UNA voce "▣ Windows" (subito dopo il testo) che apre un menu
 * allineato lì. Riusabile da SdlConsole e da console ANSI/terminale. --- */
static int s_tb_id[16], s_tb_x0[16], s_tb_x1[16], s_tb_n = 0;
static int s_grp_open = 0, s_grp_x0 = -1, s_grp_x1 = -1, s_grp_n = 0;
static int s_grp_id[16], s_grp_row[16], s_grp_mx0 = 1, s_grp_mx1 = 1;

void gmb_console_statusbar_draw(gmb_sdl_console_t* c)
{
    int bw = gmb_screen_width(c->buffer), bh = gmb_screen_height(c->buffer), y = bh - 1;
    gmb_window_manager_t* m = gmb_window_manager_static();
    gmb_cell_color_t black = { 0, 0, 0, 0 }, grey = { 170, 170, 170, 0 }, cyan = { 0, 170, 170, 0 };
    for (int x = 0; x < bw; x++) gmb_screen_setchar(c->buffer, x, y, ' ', black, grey);
    int tx = 1;
    if (m->status_text[0]) {
        int l = (int)strlen(m->status_text);
        for (int k = 0; k < l && tx < bw; k++, tx++) gmb_screen_setchar(c->buffer, tx, y, (unsigned char)m->status_text[k], black, grey);
        tx++;
    }
    int ids[16], n = 0;
    for (int i = 0; i < m->count && n < 16; i++) if (m->windows[i].minimized) ids[n++] = i;
    /* ordine di MINIMIZZAZIONE (chip accodati: i nuovi in coda) */
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
        if (m->windows[ids[b]].min_order < m->windows[ids[a]].min_order) { int t = ids[a]; ids[a] = ids[b]; ids[b] = t; }
    s_tb_n = 0; s_grp_x0 = -1; s_grp_n = 0;
    if (n == 0) { s_grp_open = 0; return; }
    int need = 0;
    for (int a = 0; a < n; a++) { const char* t = m->windows[ids[a]].title; int l = t[0] ? (int)strlen(t) : 6; need += l + 3; }
    if (tx + need <= bw) {
        int x = tx;
        for (int a = 0; a < n; a++) {
            gmb_managed_window_t* w = &m->windows[ids[a]];
            const char* t = w->title[0] ? w->title : "window";
            char chip[128]; int cl = snprintf(chip, sizeof(chip), " %s ", t);
            if (x + cl >= bw) break;
            for (int k = 0; k < cl; k++) gmb_screen_setchar(c->buffer, x + k, y, (unsigned char)chip[k], black, grey);
            s_tb_id[s_tb_n] = w->id; s_tb_x0[s_tb_n] = x; s_tb_x1[s_tb_n] = x + cl; s_tb_n++;
            x += cl + 1;
        }
        s_grp_open = 0;
    } else {
        int x = tx;
        gmb_screen_setchar(c->buffer, x++, y, 0x25A3, black, grey);   /* ▣ (una cella) */
        const char* word = " Windows ";
        for (int k = 0; word[k] && x < bw; k++) gmb_screen_setchar(c->buffer, x++, y, (unsigned char)word[k], black, grey);
        s_grp_x0 = tx; s_grp_x1 = x; s_grp_n = n;
        for (int a = 0; a < n; a++) s_grp_id[a] = m->windows[ids[a]].id;
        if (s_grp_open) {
            int mw = 8;
            for (int a = 0; a < n; a++) { const char* t = m->windows[ids[a]].title; int l = t[0] ? (int)strlen(t) : 6; if (l + 4 > mw) mw = l + 4; }
            int mx0 = tx; if (mx0 + mw > bw) mx0 = bw - mw; if (mx0 < 1) mx0 = 1;
            s_grp_mx0 = mx0; s_grp_mx1 = mx0 + mw;
            int my0 = y - n; if (my0 < 0) my0 = 0;
            for (int r2 = my0; r2 <= y - 1; r2++) for (int x2 = mx0; x2 < mx0 + mw && x2 < bw; x2++)
                gmb_screen_setchar(c->buffer, x2, r2, ' ', black, grey);
            for (int a = 0; a < n; a++) {
                int yy = my0 + a; s_grp_row[a] = yy;
                const char* t = m->windows[ids[a]].title; if (!t[0]) t = "window";
                char item[160]; int il = snprintf(item, sizeof(item), " %s", t);
                for (int k = 0; k < il && mx0 + k < bw; k++)
                    gmb_screen_setchar(c->buffer, mx0 + k, yy, (unsigned char)item[k], black,
                                       (m->windows[ids[a]].id == m->focus) ? cyan : grey);
            }
        }
    }
}

/* Click sulla status bar: chip singolo -> ripristina; voce "▣ Windows" -> apre/chiude
 * il menu; voce di menu -> ripristina+focus. Ritorna 1 se gestito. */
int gmb_console_statusbar_click(gmb_sdl_console_t* c, int cx, int cy)
{
    int bh = gmb_screen_height(c->buffer), y = bh - 1;
    if (s_grp_x0 >= 0 && cy == y && cx >= s_grp_x0 && cx < s_grp_x1) { s_grp_open = !s_grp_open; return 1; }
    if (s_grp_open) {
        for (int a = 0; a < s_grp_n; a++)
            if (cy == s_grp_row[a] && cx >= s_grp_mx0 && cx < s_grp_mx1) {
                gmb_winstmt_restore_by_id(s_grp_id[a]);
                gmb_window_manager_select(gmb_window_manager_static(), s_grp_id[a]);
                s_grp_open = 0; return 1;
            }
        s_grp_open = 0; return 1;   /* click fuori dal menu: chiudi */
    }
    if (cy != y) return 0;
    for (int i = 0; i < s_tb_n; i++)
        if (cx >= s_tb_x0[i] && cx < s_tb_x1[i]) {
            gmb_winstmt_restore_by_id(s_tb_id[i]);
            gmb_window_manager_select(gmb_window_manager_static(), s_tb_id[i]);
            return 1;
        }
    return 0;
}

int gmb_sdl_console_pump(gmb_sdl_console_t* c)
{
    if (!c) return 0;
#ifdef GMB_GFX_SDL_PRESENT
    if (!c->opened) gmb_sdl_console_open(c);
    gmb_sdl_context_t* ctx = (gmb_sdl_context_t*)c->context;
    if (!ctx || !ctx->ok) return gmb_sdl_manager_poll_events();
    /* Adotta il buffer TUI condiviso come screen della console: finestre e controlli
     * disegnano QUI (gmb_clipper_global_state()->buffer) e la console le rende. */
    { gmb_clipper_state_t* st = gmb_clipper_global_state();
      if (st && st->buffer != c->buffer) st->buffer = c->buffer; }
    /* Niente SDL_StartTextInput: l'input viene da SDL_KEYDOWN (parità KeyboardServiceSdl). */
    SDL_Window* win = (SDL_Window*)ctx->window_handle;
    Uint32 wid = SDL_GetWindowID(win);
    /* CONVENZIONE UNICA: gli eventi mouse SDL sono in PUNTI-finestra (logici); la cella
     * del font è in PIXEL nativi => larghezza cella in punti = char_width / render_scale.
     * Verificato: warp(100,100) -> motion(100,100) con output 800x400 su finestra 400x200
     * (scale 2). Prima il path WINDOW divideva per i PIXEL e quello dei controlli
     * moltiplicava per lo scale: su Retina i click cadevano a metà cella (mouse "morto"). */
    int sc = c->render_scale > 0 ? c->render_scale : 1;
    int cwp = c->char_width / sc;  if (cwp < 1) cwp = 1;
    int chp = c->char_height / sc; if (chp < 1) chp = 1;
    int gw = gmb_screen_width(c->buffer) * cwp;      /* griglia in PUNTI */
    int gh = gmb_screen_height(c->buffer) * chp;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(win, &winW, &winH);
    int quit = 0; SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { quit = 1; continue; }
        if (e.type == SDL_WINDOWEVENT && e.window.windowID == wid) {
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) quit = 1;
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) SDL_GetWindowSize(win, &winW, &winH);
            continue;
        }
        if (e.type == SDL_KEYDOWN) {
            /* SOLO KEYDOWN (parità KeyboardServiceSdl: char dal keysym, shift-aware; repeat scartato).
             * NIENTE SDL_TEXTINPUT: gestire entrambi raddoppiava ogni tasto ("abc" → "aabbcc"). */
            if (e.key.windowID == wid) {   /* include i repeat (auto-ripetizione tenendo premuto) */
                int sym = (int)e.key.keysym.sym;
                if (sym >= SDLK_F1 && sym <= SDLK_F12) {
                    /* F1-F12 = hotkey dell'host: vanno nella coda tasti host (int), non alla typeahead. */
                    extern void gmb_tui_host_key_push(int);
                    gmb_tui_host_key_push(GMB_K_F1 + (sym - SDLK_F1));
                } else {
                    char k[8]; if (sdlcon_map_key(sym, k)) gmb_gfx_input_push_key(k);
                }
            }
            continue;
        }
        if (e.type == SDL_TEXTINPUT) continue;   /* ignorato: i char arrivano da KEYDOWN */
        if (c->char_width <= 0 || c->char_height <= 0) continue;
        int lx = (e.type == SDL_MOUSEMOTION) ? e.motion.x : e.button.x;   /* PUNTI */
        int ly = (e.type == SDL_MOUSEMOTION) ? e.motion.y : e.button.y;
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.windowID == wid) {
            if (gmb_console_statusbar_click(c, lx / cwp, ly / chp)) continue;
        }
        if (e.type == SDL_MOUSEMOTION && e.motion.windowID == wid) {
            /* Nessun bottone premuto -> nessun drag/resize pendente: azzera eventuali stati
             * rimasti agganciati (UP perso fuori finestra), altrimenti i movimenti restano
             * assorbiti e le altre finestre non ricevono piu' eventi. */
            if (!(e.motion.state & SDL_BUTTON_LMASK))
                gmb_winstmt_handle_mouse_px(lx, ly, GMB_GFX_MOUSE_UP, 0, cwp, chp, gw, gh);
            gmb_winstmt_handle_mouse_px(lx, ly, GMB_GFX_MOUSE_MOVE, 0, cwp, chp, gw, gh);
        } else if ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.windowID == wid) {
            int down = (e.type == SDL_MOUSEBUTTONDOWN);
            SDL_CaptureMouse(down ? SDL_TRUE : SDL_FALSE);   /* l'UP arriva anche col cursore fuori finestra */
            gmb_winstmt_handle_mouse_px(lx, ly, down ? GMB_GFX_MOUSE_DOWN : GMB_GFX_MOUSE_UP, e.button.button, cwp, chp, gw, gh);
        }
        /* Oltre al sistema WINDOW, porta ai controlli TUI a celle SOLO i click (down/up):
         * il motion continuo riempirebbe la coda eventi e terrebbe l'app in un drain perenne. */
        { extern void gmb_tui_handle_mouse_cell(int, int, int);
          extern int gmb_tui_menu_any_open(void);
          int tcx = lx / cwp;
          int tcy = ly / chp;
          if (e.type == SDL_MOUSEBUTTONDOWN && e.button.windowID == wid) gmb_tui_handle_mouse_cell(tcx, tcy, 1);
          else if (e.type == SDL_MOUSEBUTTONUP && e.button.windowID == wid) gmb_tui_handle_mouse_cell(tcx, tcy, 2);
          else if (e.type == SDL_MOUSEMOTION && e.motion.windowID == wid && gmb_tui_menu_any_open()) gmb_tui_handle_mouse_cell(tcx, tcy, 0); }
    }
    if (gmb_winstmt_count() > 0) {
        /* ripulisci il desktop prima di ridisegnare (niente scie nel drag/move).
         * UNICA implementazione (gmb_theme_paint_desktop): desktop + retino dal TEMA;
         * prima erano fissi blu/▒, quindi il tema non arrivava alla SDLConsole. */
        gmb_theme_paint_desktop(c->buffer);
        gmb_winstmt_render_all();
        gmb_console_statusbar_draw(c);
    }
    return quit;
#else
    (void)c; return 0;
#endif
}

/* Screenshot PNG della finestra console: renderizza lo stato corrente e legge i pixel
 * dal renderer SDL (indipendente dallo z-order: funziona anche se la finestra è coperta).
 * Usato dagli harness/demo per la verifica visiva; -1 se non disponibile. */
int gmb_sdl_console_save_png(gmb_sdl_console_t* c, const char* path)
{
#ifdef GMB_GFX_SDL_PRESENT
    extern int save_png_rgb24(const char* path, const uint32_t* px, int w, int h);
    if (!c || !path) return -1;
    if (!c->opened) gmb_sdl_console_open(c);
    gmb_sdl_context_t* ctx = (gmb_sdl_context_t*)c->context;
    if (!ctx || !ctx->ok || !ctx->renderer_handle) return -1;
    gmb_sdl_console_render(c);
    SDL_Renderer* r = (SDL_Renderer*)ctx->renderer_handle;
    if (!c->texture) return -1;
    /* Legge dalla TEXTURE della console (dopo il present il backbuffer è indefinito:
     * leggere da lì dava un PNG completamente nero). */
    if (SDL_SetRenderTarget(r, (SDL_Texture*)c->texture) != 0) return -1;
    int w = 0, h = 0;
    if (SDL_GetRendererOutputSize(r, &w, &h) != 0 || w <= 0 || h <= 0) { SDL_SetRenderTarget(r, NULL); return -1; }
    unsigned char* rgba = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    if (!rgba) { SDL_SetRenderTarget(r, NULL); return -1; }
    if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, rgba, w * 4) != 0)
    { free(rgba); SDL_SetRenderTarget(r, NULL); return -1; }
    SDL_SetRenderTarget(r, NULL);
    uint32_t* px = (uint32_t*)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!px) { free(rgba); return -1; }
    for (int i = 0; i < w * h; i++)
    {
        unsigned char b = rgba[(size_t)i * 4 + 0];
        unsigned char g = rgba[(size_t)i * 4 + 1];
        unsigned char rr = rgba[(size_t)i * 4 + 2];
        px[i] = ((uint32_t)rr << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
    int rc = save_png_rgb24(path, px, w, h);
    free(px); free(rgba);
    return rc;
#else
    (void)c; (void)path; return -1;
#endif
}
