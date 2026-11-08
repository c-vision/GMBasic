/* ============================================================================
 * gmb_graphics.c — Sottosistema grafico software (porting CommonGraphics).
 *
 * Rendering software su framebuffer: pagine multiple, palette (VGA16/VGA256),
 * colori, primitive (PSET/LINE/CIRCLE/PAINT/DRAW/POINT/POLYGON), testo (font
 * TrueType se presente, altrimenti nessun output), texture/sprite da PNG
 * (decodifica PNG via zlib, senza dipendenze esterne).
 *
 * Semantica di riferimento: GraphicsManager/SdlGraphicsService di CommonGraphics.
 * Uso: sia il dispatch VM/C sia l'emitter C99 chiamano queste funzioni.
 * ============================================================================ */
#include "gmb_graphics.h"
#include "gmb_gfx_present.h"
#include "gmb_gfx_input.h"
#include "gmb_tui.h"
#include "gmb_ui_gfx.h"
#include "gmb_tui_windowstmt.h"
#include "gmb_px_renderer.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "stb_truetype.h"

#ifdef __has_include
#if __has_include(<zlib.h>)
#define GMB_GFX_HAVE_ZLIB 1
#include <zlib.h>
#endif
#else
#include <zlib.h>
#define GMB_GFX_HAVE_ZLIB 1
#endif

#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#endif

/* ================================ Modello ================================ */

gmb_gfx_ctx_t g_gfx;
stbtt_fontinfo g_font_info;
int g_font_loaded = 0;

/* ================================ Palette ================================ */

static const unsigned char k_vga16[16][3] = {
    {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
};

static void build_vga256(unsigned char pal[256][3])
{
    memcpy(pal, k_vga16, 16 * 3);
    int idx = 16;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
            { pal[idx][0] = (unsigned char)(r * 51); pal[idx][1] = (unsigned char)(g * 51); pal[idx][2] = (unsigned char)(b * 51); idx++; }
    for (int i = 0; i < 24; i++)
    { unsigned char s = (unsigned char)(8 + (int)(i * 10.3)); pal[232 + i][0] = s; pal[232 + i][1] = s; pal[232 + i][2] = s; }
}

/* Indice palette per un pixel ARGB (parità CommonGraphics FindColorIndex):
 * check_alpha=1 (sprite da PNG): alpha<128 → -1 (trasparente);
 * altrimenti indice esatto (hash + fallback) o il più vicino per distanza RGB².
 * MAI packed RGB: i sample copiano in array SHORT/BYTE (overflow, 2026-10-03). */
extern uint16_t g_rev_pal[4096];

int gmb_gfx_find_color_index(uint32_t argb, int check_alpha)
{
    if (check_alpha && (((argb >> 24) & 0xFF) < 128)) return -1;
    uint32_t rgb = argb & 0xFFFFFFu;
    int h = (int)((rgb ^ (rgb >> 10)) & 4095);
    if (g_rev_pal[h] > 0)
    {
        int cand = (int)(g_rev_pal[h] - 1);
        if (cand < g_gfx.palette_size && palette_rgb(cand) == rgb) return cand;
    }
    for (int i = 0; i < g_gfx.palette_size; i++)
        if (palette_rgb(i) == rgb) return i;
    int r = (int)((rgb >> 16) & 0xFF), g = (int)((rgb >> 8) & 0xFF), b = (int)(rgb & 0xFF);
    int best = 0, bd = 0x7FFFFFFF;
    for (int i = 0; i < g_gfx.palette_size; i++)
    {
        uint32_t c = palette_rgb(i);
        int dr = r - (int)((c >> 16) & 0xFF), dg = g - (int)((c >> 8) & 0xFF), db = b - (int)(c & 0xFF);
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; if (d == 0) break; }
    }
    return best;
}

uint32_t palette_rgb(int idx)
{
    if (idx < 0 || idx >= g_gfx.palette_size) idx = 0;
    return ((uint32_t)g_gfx.palette[idx][0] << 16) |
           ((uint32_t)g_gfx.palette[idx][1] << 8) |
           (uint32_t)g_gfx.palette[idx][2];
}

/* Colore BASIC → 0xRRGGBB: flag 0x40000000 = RGB diretto, altrimenti indice. */
uint32_t resolve_color(int color)
{
    if (color < 0) return palette_rgb(g_gfx.fg);
    if (color >= GMB_GFX_RGB_FLAG) return (uint32_t)(color & 0xFFFFFF);
    return palette_rgb(color);
}

/* ============================ Framebuffer ============================ */

static void free_pages(void)
{
    for (int i = 0; i < GMB_GFX_MAX_PAGES; i++)
    {
        free(g_gfx.pages[i].pixels);
        g_gfx.pages[i].pixels = NULL;
        g_gfx.pages[i].in_use = 0;
    }
    g_gfx.active_page = 0;
    g_gfx.visible_page = 0;
}

/* Canvas offscreen: liberati a shutdown (SCREEN non li tocca: sono indipendenti). */
static void free_canvases(void)
{
    for (int i = 0; i < GMB_GFX_MAX_CANVASES; i++)
    {
        free(g_gfx.canvases[i].pixels);
        g_gfx.canvases[i].pixels = NULL;
        g_gfx.canvases[i].w = g_gfx.canvases[i].h = 0;
        g_gfx.canvases[i].in_use = 0;
    }
    g_gfx.canvas_active = 0;
}

gmb_gfx_page_t* active_page(void)
{
    /* Canvas attivo: vale anche SENZA SCREEN (disegno offscreen headless). */
    if (g_gfx.canvas_active > 0)
    {
        int i = g_gfx.canvas_active - 1;
        if (i < GMB_GFX_MAX_CANVASES && g_gfx.canvases[i].in_use && g_gfx.canvases[i].pixels)
            return &g_gfx.canvases[i];
        g_gfx.canvas_active = 0;
    }
    if (!g_gfx.screen_on) return NULL;
    if (g_gfx.active_page < 0 || g_gfx.active_page >= GMB_GFX_MAX_PAGES) return NULL;
    return &g_gfx.pages[g_gfx.active_page];
}

static void free_textures(void)
{
    for (int i = 0; i < GMB_GFX_MAX_TEXTURES; i++)
    {
        free(g_gfx.textures[i].pixels);
        g_gfx.textures[i].pixels = NULL;
        g_gfx.textures[i].in_use = 0;
    }
    g_gfx.next_tex_id = 1;
}

static void free_sprites(void)
{
    memset(g_gfx.sprites, 0, sizeof(g_gfx.sprites));
    g_gfx.next_sprite_id = 1;
}

static void alloc_page(int idx, int w, int h)
{
    gmb_gfx_page_t* p = &g_gfx.pages[idx];
    free(p->pixels);
    p->pixels = (uint32_t*)calloc((size_t)w * h, sizeof(uint32_t));
    p->w = w;
    p->h = h;
    p->in_use = 1;
    /* sfondo nero (indice 0) */
    if (g_gfx.palette_size > 0)
    {
        uint32_t bg = ((uint32_t)g_gfx.palette[0][0] << 16) |
                      ((uint32_t)g_gfx.palette[0][1] << 8) |
                      (uint32_t)g_gfx.palette[0][2];
        for (int i = 0; i < w * h; i++) p->pixels[i] = bg;
    }
}

/* PNG decode spostato in gmb_graphics_texture.c per evitare monolite */

unsigned char* read_whole_file(const char* path, size_t* len)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) { free(buf); return NULL; }
    *len = n;
    return buf;
}

/* ============================== Public API ============================== */

/* Spazio coordinate DICHIARATO dai controlli (SCREEN ... USE CELLS|PIXELS).
 * -1 = AUTO: nessuna dichiarazione → lo spazio nativo del backend (grafica=pixel,
 * TUI=celle). 0 = PIXELS, 1 = CELLS. Non e' una conversione implicita: e' cio' che
 * l'utente ha DICHIARATO, e il renderer la applica per-frame. */
static int g_coord_mode = -1;
void gmb_gfx_set_coord_mode(int mode) { g_coord_mode = (mode == 0 || mode == 1) ? mode : -1; }
int  gmb_gfx_coord_mode(void) { return g_coord_mode; }
int  gmb_gfx_coords_cells(void)
{
    if (g_coord_mode == 0) return 0;              /* dichiarato PIXELS */
    if (g_coord_mode == 1) return 1;              /* dichiarato CELLS  */
    return gmb_gfx_screen_on() ? 0 : 1;           /* AUTO: spazio nativo */
}

extern int gmb_gfx_present_video_is_real(void);
extern void gmb_px_browse_draw(void* browse, void* buffer);
static const gmb_ui_gfx_iface_t gmb_gfx_iface = {
    .screen_on = gmb_gfx_screen_on, .print = gmb_gfx_print, .cls = gmb_gfx_cls,
    .locate = gmb_gfx_locate, .color = gmb_gfx_color,
    .present_video_is_real = gmb_gfx_present_video_is_real,
    .present_tick = gmb_gfx_present_tick, .present_should_quit = gmb_gfx_present_should_quit,
    .input_poll_key = gmb_gfx_input_poll_key,
    .screen_width = gmb_gfx_screen_width, .screen_height = gmb_gfx_screen_height,
    .draw_text = gmb_gfx_draw_text, .text_height = gmb_gfx_text_height, .text_width = gmb_gfx_text_width,
    .fill_rounded_rect = gmb_gfx_fill_rounded_rect, .stroke_rounded_rect = gmb_gfx_stroke_rounded_rect,
    .gradient_rect = gmb_gfx_gradient_rect, .fill_rounded_gradient = gmb_gfx_fill_rounded_gradient,
    .drop_shadow = gmb_gfx_drop_shadow, .fill_circle = gmb_gfx_fill_circle, .line = gmb_gfx_line,
    .input_poll_mouse = gmb_gfx_input_poll_mouse,
    .surface_autoattach = (void(*)(void*))gmb_surface_autoattach, .winstmt_render_all = gmb_winstmt_render_all,
    .font_path = gmb_gfx_font_path, .font_size = gmb_gfx_font_size, .render_scale = gmb_gfx_render_scale,
    .screen = gmb_gfx_screen, .text_end_x = gmb_gfx_text_end_x,
    .render_cells_at = gmb_gfx_render_cells_at,
    .browse_draw_px = gmb_px_browse_draw,
    .save_screen_png = gmb_gfx_save_screen_png, .restore_screen_png = gmb_gfx_restore_screen_png,
    .init = gmb_gfx_init, .palette = gmb_gfx_palette, .font = gmb_gfx_font,
};
__attribute__((constructor)) static void gmb_ui_gfx_register(void) { gmb_ui_gfx_install(&gmb_gfx_iface); }

void gmb_gfx_init(void)
{
    gmb_px_renderers_install();
    gmb_ui_gfx.screen_width = gmb_gfx_screen_width;
    gmb_ui_gfx.screen_height = gmb_gfx_screen_height;
    gmb_ui_gfx.browse_draw_px = gmb_px_browse_draw;
    if (g_gfx.init) return;
    memset(&g_gfx, 0, sizeof(g_gfx));
    g_gfx.init = 1;
    g_gfx.next_tex_id = 1;
    g_gfx.next_sprite_id = 1;
    g_gfx.fg = 15;
    g_gfx.bg = 0;
}

void gmb_gfx_shutdown(void)
{
    free_pages();
    free_canvases();
    free_textures();
    free_sprites();
    free(g_gfx.font_data);
    g_gfx.font_data = NULL;
    g_gfx.font_len = 0;
    g_font_loaded = 0;
    g_gfx.init = 0;
    g_gfx.screen_on = 0;
}

/* Reverse-palette hash (RGB 24bit → indice palette): velocizza gmb_gfx_image_get da
   O(pixels × palette) a O(pixels). Ricostruita quando la palette cambia (screen mode). */
uint16_t g_rev_pal[4096];
static void gmb_gfx_rebuild_rev_pal(void)
{
    for (int i = 0; i < 4096; i++) g_rev_pal[i] = 0;
    for (int i = 0; i < g_gfx.palette_size; i++)
    {
        uint32_t rgb = palette_rgb(i);
        int h = (int)((rgb ^ (rgb >> 10)) & 4095);
        g_rev_pal[h] = (uint16_t)(i + 1);
    }
}

static void apply_screen_mode(int mode)
{
    switch (mode)
    {
        case 1:  g_gfx.width = 320; g_gfx.height = 200; g_gfx.palette_size = 16;  memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16)); break;
        case 9:  g_gfx.width = 640; g_gfx.height = 350; g_gfx.palette_size = 16;  memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16)); break;
        case 12: g_gfx.width = 640; g_gfx.height = 480; g_gfx.palette_size = 16;  memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16)); break;
        case 13: g_gfx.width = 320; g_gfx.height = 200; g_gfx.palette_size = 256; build_vga256(g_gfx.palette); break;
        case 16: g_gfx.width = 640; g_gfx.height = 480; g_gfx.palette_size = 256; build_vga256(g_gfx.palette); break;
        case 64: g_gfx.width = 640; g_gfx.height = 200; g_gfx.palette_size = 16;  memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16)); break;
        case 65: g_gfx.width = 320; g_gfx.height = 200; g_gfx.palette_size = 16;  memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16)); break;
        default: g_gfx.width = 640; g_gfx.height = 480; g_gfx.palette_size = 16;  memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16)); break;
    }
    /* Default del viewport = schermo intero: WINDOW (senza VIEW) mappa su questo
     * rettangolo. Senza, vx2/vy2 restavano 0 (init azzera) → ogni coordinata
     * mappata a 0 (LINE/PSET sparivano: GraphicsWindows fase 2). */
    g_gfx.vx1 = 0; g_gfx.vy1 = 0;
    g_gfx.vx2 = g_gfx.width - 1; g_gfx.vy2 = g_gfx.height - 1;
    gmb_gfx_rebuild_rev_pal();
}

/* 2.6(r): "null mode" — parità NullGraphicsService (common/BRT/Services/
 * NullGraphicsService.cs): con GMB_GFX_NULL=1 le primitive grafiche completano
 * come nel driver headless GPM (POINT=0, LOAD TEXTURE/SPRITE LOAD=1, GETPIXELS
 * dummy 1x1 valido). Una sola implementazione condivisa da VM/C, C99 e nativo. */
int gmb_gfx_null_mode(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char* e = gmb_opt_env("GMB_GFX_NULL");
        cached = (e && e[0] && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return cached;
}

static void sync_base_window(int graphics)
{
    extern void gmb_base_window_set_kind(int kind);
    extern void gmb_tui_refresh_window(void);
    gmb_base_window_set_kind(graphics ? 3 : 1);   /* 3=GRAPHICS, 1=STANDARD */
    gmb_tui_refresh_window();
}

void gmb_gfx_screen(int mode, int width, int height, int bpp)
{
    (void)bpp;
    { extern void gmb_chart_screen_reset(void); gmb_chart_screen_reset(); }
    gmb_gfx_init();
    /* Un nuovo SCREEN riporta il target allo schermo (i canvas restano allocati). */
    g_gfx.canvas_active = 0;
    {   /* Override scala per i test headless (es. GMB_GFX_SCALE=2 per confrontare col
         * riferimento nativo del motore C#): altrimenti resta la scala del display. */
        const char* ov = gmb_opt_env("GMB_GFX_SCALE");
        if (ov && *ov) { float sc = (float)atof(ov); if (sc >= 1.0f) gmb_gfx_set_render_scale(sc); }
    }
    if (gmb_gfx_null_mode()) { g_gfx.mode = mode; g_gfx.screen_on = 0; sync_base_window(0); return; }
    g_gfx.mode = mode;
    if (mode == 0)
    {
        /* SCREEN 0: modalita' testo + CHIUDE la finestra grafica (se aperta). */
        g_gfx.screen_on = 0;
        free_pages();
#ifdef GMB_GFX_SDL_PRESENT
        { extern void gmb_gfx_present_close_window(void); gmb_gfx_present_close_window(); }
#endif
        sync_base_window(0);
        return;
    }
    if (mode == -1)
    {
        /* SCREEN .., , , DESKTOP: risoluzione del display corrente. Headless:
         * usa la risoluzione richiesta (width/height), altrimenti il desktop SDL
         * (GMB_GFX_SDL_PRESENT) o 640x480 di default. */
#ifdef GMB_GFX_SDL_PRESENT
        if (SDL_WasInit(SDL_INIT_VIDEO) == 0) SDL_InitSubSystem(SDL_INIT_VIDEO);
        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) { width = dm.w; height = dm.h; }
#endif
        g_gfx.mode = 13;
        apply_screen_mode(13);
        if (width <= 8 || height <= 8) { width = 640; height = 480; }
        g_gfx.width = width;
        g_gfx.height = height;
        g_gfx.screen_on = 1;
        free_pages();
        { float _s = g_gfx_scale > 0 ? g_gfx_scale : 1.0f; alloc_page(0, (int)lround(g_gfx.width * _s), (int)lround(g_gfx.height * _s)); }
        g_gfx.active_page = 0;
        sync_base_window(1);
#ifdef GMB_GFX_SDL_PRESENT
        { extern void gmb_gfx_present_start(void); gmb_gfx_present_start(); }
#endif
        g_gfx.visible_page = 0;
        g_gfx.cursor_x = 0;
        g_gfx.cursor_y = 0;
#ifdef GMB_GFX_SDL_PRESENT
        { extern void gmb_gfx_present_ensure(void); gmb_gfx_present_ensure(); }
#endif
        return;
    }
    g_gfx.screen_on = 1;
    sync_base_window(1);
#ifdef GMB_GFX_SDL_PRESENT
    { extern void gmb_gfx_present_start(void); gmb_gfx_present_start(); }  /* apre la finestra SDL */
#endif
    apply_screen_mode(mode);
    if (width > 8 && height > 8) { g_gfx.width = width; g_gfx.height = height; }
    free_pages();
    { float _s = g_gfx_scale > 0 ? g_gfx_scale : 1.0f; alloc_page(0, (int)lround(g_gfx.width * _s), (int)lround(g_gfx.height * _s)); }
    g_gfx.active_page = 0;
    g_gfx.visible_page = 0;
    g_gfx.cursor_x = 0;
    g_gfx.cursor_y = 0;
#ifdef GMB_GFX_SDL_PRESENT
    /* Finestra reale + renderScale nativo SUBITO: la pagina appena allocata viene
     * ricreata alla risoluzione giusta (vuota) prima del primo disegno. */
    { extern void gmb_gfx_present_ensure(void); gmb_gfx_present_ensure(); }
#endif
}

void gmb_gfx_screenset(int visible_page, int active_page)
{
    if (active_page >= 0 && active_page < GMB_GFX_MAX_PAGES)
    {
        if (!g_gfx.pages[active_page].in_use && g_gfx.screen_on)
        {
            /* Pagine NATIVE (logico × renderScale), come la pagina 0 di SCREEN.
             * Prima erano allocate a dimensione LOGICA: su Retina metà risoluzione,
             * poi ingrandite dal present (sprite grandi/sfocati, bug SCREENSET). */
            float _s = g_gfx_scale > 0 ? g_gfx_scale : 1.0f;
            alloc_page(active_page, (int)lround(g_gfx.width * _s), (int)lround(g_gfx.height * _s));
        }
        g_gfx.active_page = active_page;
    }
    if (visible_page >= 0 && visible_page < GMB_GFX_MAX_PAGES)
        g_gfx.visible_page = visible_page;
}

void gmb_gfx_pcopy(int src, int dst)
{
    gmb_gfx_page_t* s = (src >= 0 && src < GMB_GFX_MAX_PAGES) ? &g_gfx.pages[src] : NULL;
    gmb_gfx_page_t* d = (dst >= 0 && dst < GMB_GFX_MAX_PAGES) ? &g_gfx.pages[dst] : NULL;
    if (!s || !d || !s->in_use) return;
    if (!d->in_use)
    {
        float _s = g_gfx_scale > 0 ? g_gfx_scale : 1.0f;
        alloc_page(dst, (int)lround(g_gfx.width * _s), (int)lround(g_gfx.height * _s));
    }
    size_t n = (size_t)(d->w < s->w ? d->w : s->w) * (size_t)(d->h < s->h ? d->h : s->h);
    memcpy(d->pixels, s->pixels, n * sizeof(uint32_t));
}

/* Scala HiDPI (renderScale C#): pagine a risoluzione NATIVA (logica*scala); le
 * primitive ricevono coordinate LOGICHE e le scalano internamente (come GfxWindowCanvas).
 * Present 1:1 -> testo/sprite nitidi. */
float g_gfx_scale = 1.0f;
float gmb_gfx_render_scale(void) { return g_gfx_scale; }
static void resize_pages(int nw, int nh)
{
    if (nw <= 0 || nh <= 0) return;
    for (int i = 0; i < GMB_GFX_MAX_PAGES; i++)
    {
        gmb_gfx_page_t* p = &g_gfx.pages[i];
        if (!p->in_use || !p->pixels) continue;
        if (p->w == nw && p->h == nh) continue;
        uint32_t* np = (uint32_t*)calloc((size_t)nw * nh, sizeof(uint32_t));
        if (!np) continue;
        /* STRETCH (nearest-neighbor), non copia in alto a sinistra: quando il renderScale
         * viene determinato al primo present (dopo il disegno a scala 1) o la finestra viene
         * ridimensionata, il contenuto esistente deve riempire TUTTA la nuova pagina.
         * Prima si copiava solo l'angolo → l'immagine occupava un quarto della finestra
         * (bug Mandelbrot/grafica: pagina 640x480 dentro drawable 1280x960). */
        int ow = p->w, oh = p->h;
        for (int y = 0; y < nh; y++)
        {
            int sy = (oh > 0) ? (int)((long long)y * oh / nh) : 0;
            if (sy >= oh) sy = oh - 1;
            const uint32_t* srow = p->pixels + (size_t)sy * (size_t)ow;
            uint32_t* drow = np + (size_t)y * (size_t)nw;
            for (int x = 0; x < nw; x++)
            {
                int sx = (ow > 0) ? (int)((long long)x * ow / nw) : 0;
                if (sx >= ow) sx = ow - 1;
                drow[x] = srow[sx];
            }
        }
        free(p->pixels); p->pixels = np; p->w = nw; p->h = nh;
    }
}
void gmb_gfx_set_render_scale(float sc)
{
    if (sc < 1.0f) sc = 1.0f;
    if (sc == g_gfx_scale) return;
    g_gfx_scale = sc;
    if (!g_gfx.screen_on) return;
    resize_pages((int)lround(g_gfx.width * sc), (int)lround(g_gfx.height * sc));
}
/* Resize dello schermo logico (desktop): nuovo size in pixel logici (es. window resize). */
void gmb_gfx_resize_screen(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    g_gfx.width = w; g_gfx.height = h;
    if (!g_gfx.screen_on) return;
    resize_pages((int)lround(w * g_gfx_scale), (int)lround(h * g_gfx_scale));
}
int gmb_gfx_screen_on(void) { return g_gfx.screen_on; }
int gmb_gfx_current_mode(void) { return g_gfx.screen_on ? g_gfx.mode : 0; }
int gmb_gfx_screen_width(void) { return g_gfx.width; }
int gmb_gfx_screen_height(void) { return g_gfx.height; }
int gmb_gfx_active_page(void) { return g_gfx.active_page; }
int gmb_gfx_visible_page(void) { return g_gfx.visible_page; }
int gmb_gfx_width(void) { return g_gfx.width; }
int gmb_gfx_height(void) { return g_gfx.height; }

void gmb_gfx_color(int fg, int bg)
{
    { extern unsigned g_page_bg_rgb; g_page_bg_rgb = 0; }
    if (fg >= 0) g_gfx.fg = fg;
    if (bg >= 0) g_gfx.bg = bg;
}

void gmb_gfx_palette(int index, int r, int g, int b)
{
    if (index < 0 || index >= GMB_GFX_MAX_PALETTE) return;
    g_gfx.palette[index][0] = (unsigned char)(r & 0xFF);
    g_gfx.palette[index][1] = (unsigned char)(g & 0xFF);
    g_gfx.palette[index][2] = (unsigned char)(b & 0xFF);
    if (index >= g_gfx.palette_size) g_gfx.palette_size = index + 1;
}

unsigned g_page_bg_rgb = 0;
void gmb_gfx_page_bg(unsigned rgb) { g_page_bg_rgb = rgb; }
unsigned gmb_gfx_page_bg_get(void) { return g_page_bg_rgb; }

void gmb_gfx_cls(int color)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    /* CLS da BASIC: la pagina e. ripulita, quindi il form deve ridisegnare TUTTO
     * (il repaint incrementale ripulirebbe solo le zone sporche). */
    { extern int gmb_form_active(void); extern int gmb_form_in_form_draw(void);
      extern void gmb_form_invalidate_all(void);
      if (gmb_form_active() && !gmb_form_in_form_draw()) gmb_form_invalidate_all(); }
    /* CLS senza colore → sfondo (bg), parità ClearInternal(null) del motore C#;
     * resolve_color(-1) userebbe il FOREGROUND (white dopo COLOR 15,0) → pagina bianca. */
    uint32_t c = (color < 0) ? (g_page_bg_rgb ? (g_page_bg_rgb & 0xFFFFFFu) : palette_rgb(g_gfx.bg)) : resolve_color(color);
    /* Con VIEW attivo CLS pulisce SOLO il viewport (parità ClearInternal C# con clip
     * rect); prima riempiva l'intera pagina cancellando i pannelli esterni. */
    if (g_gfx.view_set)
    {
        float s = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
        int x1 = (int)lround((double)g_gfx.vx1 * s);
        int y1 = (int)lround((double)g_gfx.vy1 * s);
        int x2 = (int)lround((double)(g_gfx.vx2 + 1) * s) - 1;
        int y2 = (int)lround((double)(g_gfx.vy2 + 1) * s) - 1;
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 >= p->w) x2 = p->w - 1;
        if (y2 >= p->h) y2 = p->h - 1;
        for (int y = y1; y <= y2; y++)
            for (int x = x1; x <= x2; x++)
                p->pixels[(size_t)y * p->w + x] = c;
        return;
    }
    for (int i = 0; i < p->w * p->h; i++) p->pixels[i] = c;
}

int gmb_gfx_fg(void) { return g_gfx.fg; }
int gmb_gfx_bg(void) { return g_gfx.bg; }
/* RGB dello sfondo di pagina (stesso colore che usa CLS senza argomento): serve al
 * repaint incrementale per ripulire solo le zone sporche con il colore giusto. */
int gmb_gfx_page_bg_color(void)
{
    return (int)(g_page_bg_rgb ? (g_page_bg_rgb & 0xFFFFFFu) : palette_rgb(g_gfx.bg));
}

void put_pixel(gmb_gfx_page_t* p, int x, int y, uint32_t c)
{
    if (!p || x < 0 || y < 0 || x >= p->w || y >= p->h) return;
    if (g_gfx.view_set && !g_gfx.canvas_active)
    {
        /* VIEW è in coordinate LOGICHE, put_pixel riceve NATIVE (×renderScale):
         * confrontare x nativo con vx logico dimezzava il clip su Retina. */
        float s = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
        int x1 = (int)lround((double)g_gfx.vx1 * s);
        int y1 = (int)lround((double)g_gfx.vy1 * s);
        int x2 = (int)lround((double)(g_gfx.vx2 + 1) * s) - 1;
        int y2 = (int)lround((double)(g_gfx.vy2 + 1) * s) - 1;
        if (x < x1 || x > x2 || y < y1 || y > y2) return;
    }
    p->pixels[(size_t)y * p->w + x] = c;
}

/* Coordinata LOGICA → NATIVA (renderScale): le pagine sono allocate a risoluzione
 * nativa (logico*scale), quindi ogni primitiva deve scalare. Prima PSET/PRESET/CIRCLE/
 * PAINT non scalavano: su Retina (scale 2) il disegno finiva nel quarto in alto a
 * sinistra (bug Mandelbrot). Non-static: usata anche dalle primitive in
 * gmb_graphics_primitive.c. */
/* Scala da applicare a un'operazione di disegno: su un canvas è sempre 1 (pixel 1:1),
 * altrimenti la render-scale del display. Usata da primitivi/testo/immagini: la sola
 * gmb_gfx_scale_pt non basta perché alcuni scalano il raggio o il font direttamente. */
float gmb_gfx_scale_eff(void)
{
    if (g_gfx.canvas_active) return 1.0f;
    return g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
}

void gmb_gfx_scale_pt(int* x, int* y)
{
    /* Su canvas le coordinate sono 1:1 (niente render-scale del display): il contenuto
     * dipende solo dai pixel richiesti, quindi è riproducibile e confrontabile fra backend. */
    if (g_gfx.canvas_active) return;
    float s = g_gfx_scale;
    if (s > 0.0f && s != 1.0f) { *x = (int)lround((double)*x * s); *y = (int)lround((double)*y * s); }
}

/* Mappa coordinate mondo (WINDOW) → pixel del viewport. */
void gmb_gfx_map(int* x, int* y)
{
    /* Canvas: coordinate pixel pure (VIEW/WINDOW sono dello schermo). */
    if (g_gfx.canvas_active) return;
    if (!g_gfx.window_set)
    {
        /* VIEW senza WINDOW: le coordinate sono RELATIVE al viewport (QB: (0,0)
         * = angolo del VIEW). Prima non si aggiungeva l'origine → dentro i
         * pannelli si disegnava in coordinate assolute (clippate/visibili solo
         * per caso: GraphicsWindows). */
        if (g_gfx.view_set) { *x += g_gfx.vx1; *y += g_gfx.vy1; }
        return;
    }
    int vx1 = g_gfx.vx1, vy1 = g_gfx.vy1, vx2 = g_gfx.vx2, vy2 = g_gfx.vy2;
    if (g_gfx.wx2 != g_gfx.wx1)
    {
        double nx = (double)(*x - g_gfx.wx1) / (double)(g_gfx.wx2 - g_gfx.wx1) * (double)(vx2 - vx1) + vx1;
        *x = (int)(nx + 0.5);
    }
    if (g_gfx.wy2 != g_gfx.wy1)
    {
        double ny = (double)(*y - g_gfx.wy1) / (double)(g_gfx.wy2 - g_gfx.wy1) * (double)(vy2 - vy1) + vy1;
        *y = (int)(ny + 0.5);
    }
}

void gmb_gfx_pset(int x, int y, int color)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    gmb_gfx_map(&x, &y);
    g_gfx.cursor_x = x;   /* cursore in unità LOGICHE (testo/righe lo riscalano) */
    g_gfx.cursor_y = y;
    gmb_gfx_scale_pt(&x, &y);
    /* Un pixel LOGICO copre un blocco scale×scale nativo: senza, a scala 2 si
     * dipingevano punti isolati su una griglia (frattale scuro/retinato). */
    int bs = (int)lround((double)(g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f));
    if (bs < 1) bs = 1;
    uint32_t c = resolve_color(color < 0 ? g_gfx.fg : color);
    for (int dy = 0; dy < bs; dy++)
        for (int dx = 0; dx < bs; dx++)
            put_pixel(p, x + dx, y + dy, c);
}

/* Mappatura WORLD→VIEWPORT in double (coordinate frazionarie con WINDOW). */
void gmb_gfx_map_d(double* x, double* y)
{
    if (g_gfx.canvas_active) return;
    if (!g_gfx.window_set)
    {
        if (g_gfx.view_set) { *x += g_gfx.vx1; *y += g_gfx.vy1; }
        return;
    }
    int vx1 = g_gfx.vx1, vy1 = g_gfx.vy1, vx2 = g_gfx.vx2, vy2 = g_gfx.vy2;
    if (g_gfx.wx2 != g_gfx.wx1)
        *x = (*x - g_gfx.wx1) / (double)(g_gfx.wx2 - g_gfx.wx1) * (double)(vx2 - vx1) + vx1;
    if (g_gfx.wy2 != g_gfx.wy1)
        *y = (*y - g_gfx.wy1) / (double)(g_gfx.wy2 - g_gfx.wy1) * (double)(vy2 - vy1) + vy1;
}

/* PSET con coordinate FRAZIONARIE (WINDOW): mappa in double e arrotonda. */
void gmb_gfx_pset_f(double x, double y, int color)
{
    int ws, vs;
    if (!active_page()) return;
    gmb_gfx_map_d(&x, &y);
    ws = g_gfx.window_set; vs = g_gfx.view_set;
    g_gfx.window_set = 0; g_gfx.view_set = 0;
    gmb_gfx_pset((int)lround(x), (int)lround(y), color);
    g_gfx.window_set = ws; g_gfx.view_set = vs;
}

void gmb_gfx_preset(int x, int y)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    gmb_gfx_map(&x, &y);
    g_gfx.cursor_x = x;
    g_gfx.cursor_y = y;
    gmb_gfx_scale_pt(&x, &y);
    int bs = (int)lround((double)(g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f));
    if (bs < 1) bs = 1;
    uint32_t c = resolve_color(g_gfx.bg);
    for (int dy = 0; dy < bs; dy++)
        for (int dx = 0; dx < bs; dx++)
            put_pixel(p, x + dx, y + dy, c);
}

int gmb_gfx_point(int x, int y)
{
    if (gmb_gfx_null_mode()) return 0;
    gmb_gfx_page_t* p = active_page();
    if (!p) return -1;
    gmb_gfx_map(&x, &y);
    gmb_gfx_scale_pt(&x, &y);
    if (x < 0 || y < 0 || x >= p->w || y >= p->h) return -1;
    uint32_t c = p->pixels[(size_t)y * p->w + x];
    for (int i = 0; i < g_gfx.palette_size; i++)
        if (palette_rgb(i) == c) return i;
    return (int)c;
}

/* ============================ Canvas offscreen ============================
 * Una pagina indipendente dallo schermo. Handle = indice+1 (1..GMB_GFX_MAX_CANVASES),
 * 0 = schermo. Con canvas attivo active_page() restituisce il canvas: TUTTE le primitive
 * esistenti (PSET/LINE/CIRCLE/PAINT/DRAW/…) ci disegnano sopra senza modifiche, anche
 * senza SCREEN (headless). Coordinate 1:1, niente VIEW/WINDOW/render-scale. */

static gmb_gfx_page_t* canvas_page(int h)
{
    int i = h - 1;
    if (i < 0 || i >= GMB_GFX_MAX_CANVASES) return NULL;
    if (!g_gfx.canvases[i].in_use || !g_gfx.canvases[i].pixels) return NULL;
    return &g_gfx.canvases[i];
}

/* h == 0 → pagina visibile dello schermo (per SAVE/BLIT/W/H/GET). */
static gmb_gfx_page_t* canvas_target(int h)
{
    if (h != 0) return canvas_page(h);
    int vi = g_gfx.visible_page;
    if (vi < 0 || vi >= GMB_GFX_MAX_PAGES) vi = g_gfx.active_page;
    gmb_gfx_page_t* p = &g_gfx.pages[vi];
    return (p->in_use && p->pixels) ? p : NULL;
}

int gmb_gfx_canvas_new(int w, int h)
{
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return 0;
    if ((long long)w * h > 268435456LL) return 0;   /* ~1 GiB di pixel: limite di sicurezza */
    gmb_gfx_init();
    for (int i = 0; i < GMB_GFX_MAX_CANVASES; i++)
    {
        if (g_gfx.canvases[i].in_use) continue;
        gmb_gfx_page_t* p = &g_gfx.canvases[i];
        p->pixels = (uint32_t*)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
        if (!p->pixels) return 0;
        p->w = w;
        p->h = h;
        p->in_use = 1;
        /* Palette di default se nessuno SCREEN è stato aperto: senza, gli indici 0..15
         * sarebbero tutti neri e il disegno headless sarebbe inutilizzabile. */
        if (g_gfx.palette_size == 0)
        {
            memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16));
            g_gfx.palette_size = 16;
        }
        return i + 1;
    }
    return 0;
}

void gmb_gfx_canvas_free(int h)
{
    gmb_gfx_page_t* p = canvas_page(h);
    if (!p) return;
    free(p->pixels);
    p->pixels = NULL;
    p->w = p->h = 0;
    p->in_use = 0;
    if (g_gfx.canvas_active == h) g_gfx.canvas_active = 0;
}

int gmb_gfx_canvas_use(int h)
{
    if (h == 0) { g_gfx.canvas_active = 0; return 0; }
    if (!canvas_page(h)) return -1;
    g_gfx.canvas_active = h;
    return 0;
}

int gmb_gfx_canvas_active(void) { return g_gfx.canvas_active; }

int gmb_gfx_canvas_w(int h)
{
    gmb_gfx_page_t* p = canvas_target(h);
    return p ? p->w : -1;
}

int gmb_gfx_canvas_h(int h)
{
    gmb_gfx_page_t* p = canvas_target(h);
    return p ? p->h : -1;
}

int gmb_gfx_canvas_clear(int h, int color)
{
    gmb_gfx_page_t* p = canvas_page(h);
    if (!p) return -1;
    uint32_t c = (color < 0) ? 0u : resolve_color(color);
    c &= 0xFFFFFFu;
    for (size_t i = 0; i < (size_t)p->w * (size_t)p->h; i++) p->pixels[i] = c;
    return 0;
}

int gmb_gfx_canvas_set(int h, int x, int y, int color)
{
    gmb_gfx_page_t* p = canvas_page(h);
    if (!p || x < 0 || y < 0 || x >= p->w || y >= p->h) return -1;
    p->pixels[(size_t)y * p->w + x] = resolve_color(color) & 0xFFFFFFu;
    return 0;
}

int gmb_gfx_canvas_get(int h, int x, int y)
{
    gmb_gfx_page_t* p = canvas_target(h);
    if (!p || x < 0 || y < 0 || x >= p->w || y >= p->h) return -1;
    /* Colore impacchettato con RGB flag: riutilizzabile direttamente come argomento
     * colore (es. CANVAS SET c, x, y, <risultato di CANVAS GET>). */
    return (int)((uint32_t)GMB_GFX_RGB_FLAG | (p->pixels[(size_t)y * p->w + x] & 0xFFFFFFu));
}

int gmb_gfx_canvas_save(int h, const char* path)
{
    gmb_gfx_page_t* p = canvas_target(h);
    if (!p || !path || !*path) return -1;
    return save_png_rgb24(path, p->pixels, p->w, p->h);
}

int gmb_gfx_canvas_load(const char* path)
{
    size_t len = 0;
    if (!path || !*path) return -1;
    unsigned char* data = read_whole_file(path, &len);
    if (!data) return -1;
    uint32_t* px = NULL;
    int w = 0, h = 0;
    int rc = png_decode(data, len, &px, &w, &h);
    free(data);
    if (rc != 0 || !px || w <= 0 || h <= 0) { free(px); return -1; }
    gmb_gfx_init();
    for (int i = 0; i < GMB_GFX_MAX_CANVASES; i++)
    {
        if (g_gfx.canvases[i].in_use) continue;
        gmb_gfx_page_t* p = &g_gfx.canvases[i];
        p->pixels = px;   /* proprietà trasferita */
        p->w = w;
        p->h = h;
        p->in_use = 1;
        if (g_gfx.palette_size == 0)
        {
            memcpy(g_gfx.palette, k_vga16, sizeof(k_vga16));
            g_gfx.palette_size = 16;
        }
        return i + 1;
    }
    free(px);
    return -1;
}

int gmb_gfx_canvas_blit(int src, int dx, int dy)
{
    gmb_gfx_page_t* s = canvas_target(src);
    gmb_gfx_page_t* d = active_page();
    if (!s || !d) return -1;
    /* offset negativi: rifiutati (nessun accesso fuori buffer) */
    if (dx < 0 || dy < 0) return -1;
    int w = s->w < d->w - dx ? s->w : d->w - dx;
    int h = s->h < d->h - dy ? s->h : d->h - dy;
    if (w <= 0 || h <= 0) return -1;
    if (s == d)
    {
        for (int y = 0; y < h; y++) memmove(d->pixels + (size_t)(dy + y) * d->w + dx,
                                           s->pixels + (size_t)y * s->w, (size_t)w * sizeof(uint32_t));
    }
    else
    {
        for (int y = 0; y < h; y++) memcpy(d->pixels + (size_t)(dy + y) * d->w + dx,
                                          s->pixels + (size_t)y * s->w, (size_t)w * sizeof(uint32_t));
    }
    return 0;
}

static int gmb_streq_ci(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b)
    {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

double gmb_gfx_canvas_op(const char* op, const double* a, int nargs, const char* path)
{
    if (!op) return -1;
    if (gmb_streq_ci(op, "NEW")) return (nargs >= 2) ? gmb_gfx_canvas_new((int)a[0], (int)a[1]) : 0;
    if (gmb_streq_ci(op, "USE")) return (nargs >= 1) ? gmb_gfx_canvas_use((int)a[0]) : -1;
    if (gmb_streq_ci(op, "END")) { g_gfx.canvas_active = 0; return 0; }
    if (gmb_streq_ci(op, "CLEAR")) return (nargs >= 1) ? gmb_gfx_canvas_clear((int)a[0], nargs >= 2 ? (int)a[1] : -1) : -1;
    if (gmb_streq_ci(op, "SET")) return (nargs >= 4) ? gmb_gfx_canvas_set((int)a[0], (int)a[1], (int)a[2], (int)a[3]) : -1;
    if (gmb_streq_ci(op, "GET")) return (nargs >= 3) ? gmb_gfx_canvas_get((int)a[0], (int)a[1], (int)a[2]) : -1;
    if (gmb_streq_ci(op, "BLIT")) return gmb_gfx_canvas_blit(nargs >= 1 ? (int)a[0] : 0, nargs >= 2 ? (int)a[1] : 0, nargs >= 3 ? (int)a[2] : 0);
    if (gmb_streq_ci(op, "SAVE")) return (nargs >= 1) ? gmb_gfx_canvas_save((int)a[0], path) : -1;
    if (gmb_streq_ci(op, "LOAD")) return gmb_gfx_canvas_load(path);
    if (gmb_streq_ci(op, "FREE")) { if (nargs >= 1) gmb_gfx_canvas_free((int)a[0]); return 0; }
    if (gmb_streq_ci(op, "WIDTH")) return (nargs >= 1) ? gmb_gfx_canvas_w((int)a[0]) : -1;
    if (gmb_streq_ci(op, "HEIGHT")) return (nargs >= 1) ? gmb_gfx_canvas_h((int)a[0]) : -1;
    return -1;
}


/* Primitive spostate in gmb_graphics_primitive.c per evitare monolite */

/* Image/bload spostato in gmb_graphics_image.c per evitare monolite */

/* ================================ Sync / input ================================ */

const uint32_t* gmb_gfx_visible_pixels(int* out_w, int* out_h)
{
    if (!g_gfx.screen_on) return NULL;
    if (g_gfx.visible_page < 0 || g_gfx.visible_page >= GMB_GFX_MAX_PAGES) return NULL;
    gmb_gfx_page_t* p = &g_gfx.pages[g_gfx.visible_page];
    if (!p->in_use || !p->pixels) return NULL;
    if (out_w) *out_w = p->w;
    if (out_h) *out_h = p->h;
    return p->pixels;
}

static gmb_gfx_collision_fn g_collision_cb = NULL;
static gmb_gfx_collisions_fn g_collisions_cb = NULL;
void gmb_gfx_on_sprite_collision(gmb_gfx_collision_fn cb) { g_collision_cb = cb; }
void gmb_gfx_on_sprite_collisions(gmb_gfx_collisions_fn cb) { g_collisions_cb = cb; }

/* Collisioni AABB fra sprite visibili (rettangoli = dimensioni della texture, come gmb_gfx_collide).
 * Semantica identica su tutti i backend: il SYNC N rileva TUTTE le coppie, il SYNC N+1 le consegna agli handler
 * (singolare: una chiamata con la PRIMA coppia, (x, y) = centro del rettangolo dello sprite con id minore;
 * plurale: una chiamata con gli id coinvolti, unici, nell'ordine id1, id2 di ogni coppia). */
#define GFX_COL_MAX 256
static int    g_col_n = 0;
static int    g_col_a[GFX_COL_MAX], g_col_b[GFX_COL_MAX];
static float  g_col_x[GFX_COL_MAX], g_col_y[GFX_COL_MAX];

static void gfx_detect_collisions(void)
{
    g_col_n = 0;
    for (int i = 1; i < g_gfx.next_sprite_id && i < GMB_GFX_MAX_SPRITES; i++)
    {
        gmb_gfx_sprite_t* a = &g_gfx.sprites[i];
        if (!a->in_use || !a->visible || a->tex_id <= 0 || a->tex_id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[a->tex_id].in_use) continue;
        for (int j = i + 1; j < g_gfx.next_sprite_id && j < GMB_GFX_MAX_SPRITES; j++)
        {
            gmb_gfx_sprite_t* b = &g_gfx.sprites[j];
            if (!b->in_use || !b->visible || b->tex_id <= 0 || b->tex_id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[b->tex_id].in_use) continue;
            if (!gmb_gfx_collide(i, j)) continue;
            if (g_col_n >= GFX_COL_MAX) return;
            g_col_a[g_col_n] = i; g_col_b[g_col_n] = j;
            g_col_x[g_col_n] = a->x + g_gfx.textures[a->tex_id].w / 2.0f;
            g_col_y[g_col_n] = a->y + g_gfx.textures[a->tex_id].h / 2.0f;
            g_col_n++;
        }
    }
}

static void gfx_dispatch_collisions(void)
{
    if ((!g_collision_cb && !g_collisions_cb) || g_col_n == 0) return;   /* senza handler C le coppie restano in attesa per gmb_gfx_take_collisions */
    int n = g_col_n; g_col_n = 0;
    /* copia locale: l'handler puo' invocare SYNC/altro che rimpiazza le coppie */
    int ca[GFX_COL_MAX], cb[GFX_COL_MAX]; float cx = g_col_x[0], cy = g_col_y[0];
    memcpy(ca, g_col_a, sizeof(int) * (size_t)n); memcpy(cb, g_col_b, sizeof(int) * (size_t)n);
    if (g_collision_cb) g_collision_cb((double)cx, (double)cy);
    if (g_collisions_cb)
    {
        double ids[2 * GFX_COL_MAX]; int m = 0;
        for (int k = 0; k < n; k++)
        {
            int cand[2] = { ca[k], cb[k] };
            for (int q = 0; q < 2; q++)
            {
                int dup = 0;
                for (int z = 0; z < m; z++) if ((int)ids[z] == cand[q]) { dup = 1; break; }
                if (!dup) ids[m++] = (double)cand[q];
            }
        }
        g_collisions_cb(ids, m);
    }
}

/* Preleva le coppie rilevate dall'ultimo SYNC (per i backend che dispacciano gli handler da se': C#/VM-C).
 * Riempie fino a max coppie (id1, id2, x, y), ritorna il numero e svuota l'attesa. */
int gmb_gfx_take_collisions(int* id1, int* id2, float* x, float* y, int max)
{
    int n = g_col_n < max ? g_col_n : max;
    for (int k = 0; k < n; k++)
    {
        if (id1) id1[k] = g_col_a[k];
        if (id2) id2[k] = g_col_b[k];
        if (x) x[k] = g_col_x[k];
        if (y) y[k] = g_col_y[k];
    }
    g_col_n = 0;
    return n;
}

void gmb_gfx_sync(void)
{
    gfx_dispatch_collisions();   /* consegna le collisioni rilevate dal SYNC precedente */
    gmb_gfx_page_t* p = active_page();
    if (!p) { gfx_detect_collisions(); return; }
    /* Present: disegna gli sprite visibili (in ordine di priorita') sul frame.
     * Usa gmb_gfx_draw_texture così rotazione/scala per-sprite sono rispettate. */
    for (int prio = 0; prio < 256; prio++)
        for (int i = 0; i < GMB_GFX_MAX_SPRITES; i++)
        {
            gmb_gfx_sprite_t* s = &g_gfx.sprites[i];
            if (!s->in_use || !s->visible || s->priority != prio) continue;
            if (s->tex_id <= 0 || s->tex_id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[s->tex_id].in_use) continue;
            gmb_gfx_draw_texture(s->tex_id, (int)s->x, (int)s->y, s->rotation, s->scale_x, s->scale_y);
        }
    gfx_detect_collisions();
}

void gmb_gfx_waitkey(void)
{
    /* headless: nessuna finestra → ritorna subito. */
}

/* Screenshot spostato in gmb_graphics_screenshot.c per evitare monolite */

/* ── conversioni coordinate/pixel e info schermo (funzioni pure, §0bis) ──────
 * Fonte unica per VM/C, C99Native. Semantica = GraphicsManager
 * (IL/PCode): C2PX(col,size)=col*size, P2CX(x,size)=x/size (size 0 → x). */
double gmb_gfx_c2px(double col, double size) { return col * size; }
double gmb_gfx_c2py(double row, double size) { return row * size; }
double gmb_gfx_p2cx(double x, double size)   { return size != 0.0 ? x / size : x; }
double gmb_gfx_p2cy(double y, double size)   { return size != 0.0 ? y / size : y; }
int    gmb_gfx_screens(void)                 { return 1; }

/* TEXTMODE n: ponte esportato in libgmb_gfx per il runtime C#. Seleziona la modalita'
 * console (0=%SDL,1=%C64,2=%ANSI,3=%SYSTEM) applicando consoleConfiguration[n] e
 * ridisegnando la TUI (implementazione in gmb_console.c). */
void gmb_gfx_console_select(int mode)
{
    extern void gmb_console_select(int n);
    gmb_console_select(mode);
}
