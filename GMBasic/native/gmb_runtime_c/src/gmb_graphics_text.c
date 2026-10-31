/* gmb_graphics_text.c — testo con SDL_ttf/FreeType (parita' col motore C#).
 *
 * Il motore C# (CommonGraphics/Internals.cs) usa SDL_ttf con hinting MONO e rende
 * il testo a risoluzione NATIVA (size x renderScale), poi blitta 1:1 nel buffer
 * della pagina. Questo modulo replica ESATTAMENTE quel percorso: stesso rasterizer
 * (FreeType) -> stessa resa e parita' pixel.
 *
 * Convenzioni:
 *   - le funzioni gmb_gfx_text_width/height ritornano valori NATIVI (pixel pagina);
 *     le varianti *_logical ritornano il valore in unita' LOGICHE (native/scale),
 *     semantica di TEXTWIDTH/TEXTHEIGHT per il BASIC (come il motore C#).
 *   - il cursore (g_gfx.cursor_x/y) e' in unita' LOGICHE (come il C#).
 */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include "gmb_runtime.h"     /* gmb_paths_resources_dir: Resources CONDIVISA dei due mondi */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>
#include <unistd.h>

extern gmb_gfx_ctx_t g_gfx;
extern float g_gfx_scale;
extern int g_font_loaded;
static char g_font_path[512] = {0};
const char* gmb_gfx_font_path(void) { return g_font_path; }
int gmb_gfx_font_size(void) { return g_gfx.font_size; }

static TTF_Font* g_ttf = NULL;
static int g_font_logical_size = 12;
int gmb_gfx_font_logical_size(void) { return g_font_logical_size; }
static int g_ttf_inited = 0;
static float gfx_sc(void) { return g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f; }
static int gfx_resolve_font_path(const char* name, char* out, int cap);
int gmb_gfx_text_height_logical(void);
int gmb_gfx_text_width_logical(const char* text);

static void gmb_ttf_ensure_init(void)
{
    if (!g_ttf_inited) { g_ttf_inited = 1; if (TTF_WasInit() == 0) TTF_Init(); }
}

/* Blit di una surface SDL_ttf nella pagina: blend del colore fg con l'alpha della
 * surface sul pixel di sfondo della pagina (stesso blend del motore C#). */
static void blit_alpha_surface(SDL_Surface* s, int dx, int dy, uint32_t fg)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || !s) return;
    if (SDL_LockSurface(s) != 0) return;
    int bpp = s->format->BytesPerPixel;
    Uint8 fr = (fg >> 16) & 0xFF, fgc = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    /* ALPHA estratta dai maschere del formato: SDL_GetRGBA per pixel chiamava
     * SDL_GetPixelFormatDetails a ogni pixel (path SDL3) ed era il collo di bottiglia. */
    Uint32 amask = s->format->Amask;
    int ashift = 0, amax = 0;
    if (amask) { Uint32 m = amask; while (m && !(m & 1u)) { m >>= 1; ashift++; } amax = (int)m; }
    for (int y = 0; y < s->h; y++) {
        int py = dy + y;
        if (py < 0 || py >= p->h) continue;
        Uint8* row = (Uint8*)s->pixels + (size_t)y * s->pitch;
        for (int x = 0; x < s->w; x++) {
            Uint32 px = 0;
            memcpy(&px, row + (size_t)x * bpp, (size_t)bpp);
            Uint32 a = amask ? ((px & amask) >> ashift) : 255u;
            if (amask && amax && amax != 255) a = a * 255u / (Uint32)amax;
            if (a == 0) continue;
            int px_ = dx + x;
            if (px_ < 0 || px_ >= p->w) continue;
            if (g_gfx.view_set && (px_ < g_gfx.vx1 || px_ > g_gfx.vx2 || py < g_gfx.vy1 || py > g_gfx.vy2)) continue;
            if (a >= 255) { p->pixels[(size_t)py * p->w + px_] = ((uint32_t)fr << 16) | ((uint32_t)fgc << 8) | fb; continue; }
            uint32_t dst = p->pixels[(size_t)py * p->w + px_];
            uint32_t inv = 255 - a;
            uint32_t r = ((((dst >> 16) & 0xFF) * inv + fr * a) / 255) & 0xFF;
            uint32_t gg = ((((dst >> 8) & 0xFF) * inv + fgc * a) / 255) & 0xFF;
            uint32_t b = (((dst & 0xFF) * inv + fb * a) / 255) & 0xFF;
            p->pixels[(size_t)py * p->w + px_] = (r << 16) | (gg << 8) | b;
        }
    }
    SDL_UnlockSurface(s);
}

/* Rende una riga (senza '\n') alla posizione LOGICA (lx, ly); ritorna la larghezza
 * NATIVA della surface. Usato da print/draw_text. */
static int render_line(const char* line, int lx, int ly, uint32_t color)
{
    if (!g_ttf || !line || !*line) return 0;
    SDL_Color c = { (Uint8)((color >> 16) & 0xFF), (Uint8)((color >> 8) & 0xFF), (Uint8)(color & 0xFF), 255 };
    SDL_Surface* s = TTF_RenderUTF8_Blended(g_ttf, line, c);
    if (!s) return 0;
    float sc = gfx_sc();
    int cx = (int)lround(lx * sc), cy = (int)lround(ly * sc);
    int pad = s->h - TTF_FontHeight(g_ttf);
    blit_alpha_surface(s, cx, cy - pad, color);
    int w = s->w;
    SDL_FreeSurface(s);
    return w;
}

void gmb_gfx_font(const char* path, int size)
{
    gmb_gfx_init();
    gmb_ttf_ensure_init();
    if (!path)   /* scarica esplicitamente il font (UnloadFontC) */
    {
        if (g_ttf) { TTF_CloseFont(g_ttf); g_ttf = NULL; }
        g_font_loaded = 0; g_gfx.font_size = size > 0 ? size : 12;
        return;
    }
    /* Prova ad aprire il NUOVO font senza distruggere il precedente: se il path e'
     * errato il testo corrente non sparisce. */
    float sc = gfx_sc();
    int lsize = size > 0 ? size : 12;
    int ssize = (int)lround(lsize * sc); if (ssize < 1) ssize = 1;   /* buffer FISICO: 2x su Retina */
    /* Risolvi il path: i sample usano "font.ttf" (relativo) ma il file sta in
     * Resources/ alla ROOT del repo — non nella CWD del sample. Cerca in: CWD,
     * Resources/<name>, <repo-root>/Resources/<name>. */
    char resolved[1400]; const char* use = path;
    /* Font legacy "font.ttf" (sample storici) -> font di SISTEMA (aspetto nativo),
     * PRIMA della risoluzione relativa (che altrimenti rimappa sul TTF del repo). */
    int sys_font = 0;
    { extern int access(const char*, int);
      static const char* sysfonts[] = {
          "/System/Library/Fonts/Supplemental/Arial.ttf",
          "/System/Library/Fonts/Supplemental/Verdana.ttf",
          "/System/Library/Fonts/SFNS.ttf", 0 };
      if (strstr(path, "font.ttf"))
          for (int i = 0; sysfonts[i]; i++)
              if (access(sysfonts[i], 4) == 0) { use = sysfonts[i]; sys_font = 1; break; } }
    if (!sys_font && gfx_resolve_font_path(path, resolved, (int)sizeof(resolved))) use = resolved;
    /* CACHE del font per (path,size): BIGTEXT e i widget form cambiano/ripristinano la
     * dimensione a ogni frame; ricaricare il TTF ogni volta rendeva il frame lentissimo. */
    static struct { char path[1400]; int size; TTF_Font* f; } g_font_cache[8];
    static int g_font_cache_n = 0;
    for (int ci = 0; ci < g_font_cache_n; ci++)
        if (g_font_cache[ci].size == ssize && strcmp(g_font_cache[ci].path, use) == 0)
        {
            if (g_ttf != g_font_cache[ci].f) { g_ttf = g_font_cache[ci].f; }
            snprintf(g_font_path, sizeof(g_font_path), "%s", use);
            g_font_loaded = 1; g_font_logical_size = lsize; g_gfx.font_size = ssize;
            return;
        }
    TTF_Font* nf = TTF_OpenFont(use, ssize);
    if (!nf) return;   /* mantieni font+size precedenti */
    TTF_SetFontHinting(nf, TTF_HINTING_MONO);   /* come il motore C# */
    if (g_font_cache_n < 8) { g_font_cache[g_font_cache_n].f = nf; g_font_cache[g_font_cache_n].size = ssize; snprintf(g_font_cache[g_font_cache_n].path, sizeof(g_font_cache[0].path), "%s", use); g_font_cache_n++; }
    g_ttf = nf;
    snprintf(g_font_path, sizeof(g_font_path), "%s", use);
    g_font_loaded = 1;
    g_font_logical_size = lsize;
    g_gfx.font_size = ssize;
}

void gmb_gfx_locate(int x, int y) { g_gfx.cursor_x = x; g_gfx.cursor_y = y; }

void gmb_gfx_print(const char* text)
{
    if (!text || !g_gfx.screen_on) return;
    if (!g_font_loaded || !g_ttf) { g_gfx.cursor_x += (int)lround(g_font_logical_size * 0.6f); return; }
    extern uint32_t resolve_color(int color);
    uint32_t col = resolve_color(g_gfx.fg);
    float sc = gfx_sc();
    const char* s = text;
    char buf[8192];
    while (*s) {
        const char* nl = strchr(s, '\n');
        size_t n = nl ? (size_t)(nl - s) : strlen(s);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, s, n); buf[n] = 0;
        if (n > 0) {
            /* TAB in grafica: espansione a SPAZI fino alla zona successiva di 14
             * caratteri (parità Internals.PrintInternal C#). In console il \t lo
             * espande la console; qui SDL_ttf lo renderebbe nullo. */
            char ebuf[16384];
            int k = 0;
            int charW = gmb_gfx_text_width_logical(" ");
            if (charW <= 0) charW = (int)lround((double)g_font_logical_size * 0.6);
            for (size_t i = 0; i < n && k < (int)sizeof(ebuf) - 2; i++) {
                if (buf[i] == '\t') {
                    int col = (g_gfx.cursor_x + k * charW) / charW;
                    int next = (col / 14 + 1) * 14;
                    int sp = next - col;
                    while (sp-- > 0 && k < (int)sizeof(ebuf) - 2) ebuf[k++] = ' ';
                } else ebuf[k++] = buf[i];
            }
            ebuf[k] = 0;
            int w = render_line(ebuf, g_gfx.cursor_x, g_gfx.cursor_y, col);
            g_gfx.cursor_x += (int)lround(w / sc);   /* avanzamento LOGICO */
        }
        if (!nl) break;
        g_gfx.cursor_x = 0;
        g_gfx.cursor_y += gmb_gfx_text_height_logical();
        s = nl + 1;
    }
}

int gmb_gfx_text_width(const char* text)   /* NATIVO */
{
    if (!text) return 0;
    /* Font non ancora caricato (es. host IL/PCode che misura senza present): caricane uno
     * di default, altrimenti la formula di fallback (25/19) diverge dalla VM/C (21/16). */
    if (!g_ttf) { extern int gmb_gfx_font_default(int); gmb_gfx_font_default(0); }
    if (!g_ttf) return (int)strlen(text) * (g_gfx.font_size > 0 ? g_gfx.font_size : 12) * 6 / 10;
    int w = 0, h = 0;
    TTF_SizeUTF8(g_ttf, text, &w, &h);
    return w;
}
int gmb_gfx_text_width_logical(const char* text)   /* LOGICO (TEXTWIDTH BASIC) */
{
    int w = gmb_gfx_text_width(text);
    return (int)lround(w / gfx_sc());
}
int gmb_gfx_text_height(void)   /* NATIVO */
{
    if (!g_ttf) { extern int gmb_gfx_font_default(int); gmb_gfx_font_default(0); }
    if (!g_ttf) return g_gfx.font_size > 0 ? g_gfx.font_size : 16;
    return TTF_FontHeight(g_ttf);
}
int gmb_gfx_text_height_logical(void)   /* LOGICO (TEXTHEIGHT BASIC) */
{
    int h = gmb_gfx_text_height();
    return (int)lround(h / gfx_sc());
}

int gmb_gfx_text_end_x(const char* text, int x, int y)
{
    (void)y;
    if (!text || !g_gfx.screen_on || !g_ttf) return x;
    int w = 0, h = 0;
    TTF_SizeUTF8(g_ttf, text, &w, &h);
    return x + (int)lround(w / gfx_sc());
}

void gmb_gfx_draw_text(const char* text, int x, int y, int align, int color)
{
    if (!text || !g_gfx.screen_on || !g_ttf) return;
    extern uint32_t resolve_color(int color);
    uint32_t col = resolve_color(color < 0 ? g_gfx.fg : color);
    float sc = gfx_sc();
    int w = 0, h = 0; TTF_SizeUTF8(g_ttf, text, &w, &h);
    int lx = x;
    if (align == 1) lx = x - (int)lround(w / sc) / 2;
    else if (align == 2) lx = x - (int)lround(w / sc);
    int ly = y;
    const char* s = text; char buf[8192];
    while (*s) {
        const char* nl = strchr(s, '\n');
        size_t n = nl ? (size_t)(nl - s) : strlen(s);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, s, n); buf[n] = 0;
        if (n > 0) render_line(buf, lx, ly, col);
        if (!nl) break;
        ly += gmb_gfx_text_height_logical();
        s = nl + 1;
    }
}

/* Glifo singolo (celle TUI): rasterizza il codepoint con SDL_ttf e lo blitta
 * col top alla baseline del cursore (y + FontAscent). */
static void gmb_gfx_render_glyph(int* cx, int* cy, int cp, uint32_t color)
{
    if (!g_ttf) return;
    SDL_Color c = { (Uint8)((color >> 16) & 0xFF), (Uint8)((color >> 8) & 0xFF), (Uint8)(color & 0xFF), 255 };
    SDL_Surface* s = TTF_RenderGlyph_Blended(g_ttf, (Uint16)cp, c);
    if (!s) return;
    int minx = 0, adv = 0;
    TTF_GlyphMetrics(g_ttf, (Uint16)cp, &minx, NULL, NULL, NULL, &adv);
    int baseline = TTF_FontAscent(g_ttf);
    blit_alpha_surface(s, *cx + minx, *cy + baseline - s->h, color);
    SDL_FreeSurface(s);
    *cx += adv;
}

/* ============================ Estensioni bridge ============================ */
void gmb_gfx_set_cursor_state(int visible) { g_gfx.cursor_visible = visible ? 1 : 0; }
void gmb_gfx_set_cursor_shape(int shape) { g_gfx.cursor_shape = shape; }
void gmb_gfx_set_attribute(int attr) { g_gfx.attribute = attr; }
int gmb_gfx_get_cursor_x(void) { return g_gfx.cursor_x; }
int gmb_gfx_get_cursor_y(void) { return g_gfx.cursor_y; }

void gmb_gfx_render_cells(const int* chars, const unsigned int* fg, const unsigned int* bg, int w, int h)
{
    if (!chars || !fg || !bg || w <= 0 || h <= 0) return;
    int cw = gmb_gfx_text_width("M"); if (cw <= 0) cw = 8;
    int ch = gmb_gfx_text_height();    if (ch <= 0) ch = 16;
    /* Console a CELLE (come SdlConsole C#): lo schermo = griglia x cella (metriche naturali). */
    { float sc = gmb_gfx_render_scale(); if (sc < 1.0f) sc = 1.0f;
      int want_w = (int)((w * cw) / sc + 0.5f), want_h = (int)((h * ch) / sc + 0.5f);
      if (gmb_gfx_screen_on() && want_w > 0 && want_h > 0 && (gmb_gfx_width() != want_w || gmb_gfx_height() != want_h))
          gmb_gfx_resize_screen(want_w, want_h);
    }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { size_t i = (size_t)y * w + x; gmb_gfx_cell_bg(x * cw, y * ch, cw, ch, bg[i]); }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { size_t i = (size_t)y * w + x; gmb_gfx_cell_glyph(x * cw, y * ch, cw, ch, (uint32_t)chars[i], fg[i]); }
}

void gmb_gfx_draw_glyph(uint32_t cp, int x, int y, uint32_t color)
{
    if (!g_font_loaded) return;
    int cx = x, cy = y;
    gmb_gfx_render_glyph(&cx, &cy, (int)cp, color);
}
void gmb_gfx_draw_glyph_cell(uint32_t cp, int x, int y, int cw, int ch, uint32_t color)
{
    (void)cw; (void)ch;
    gmb_gfx_draw_glyph(cp, x, y, color);
}

/* C64 vertical-fill: glifo singolo (approssimazione con SDL_ttf). */
void gmb_gfx_draw_glyph_vfill(int cp, int x, int y, int cw, int ch, uint32_t color)
{
    (void)cw; (void)ch;
    if (!g_font_loaded) return;
    int cx = x, cy = y;
    gmb_gfx_render_glyph(&cx, &cy, cp, color);
}

void gmb_gfx_render_cells_at(const int* chars, const unsigned int* fg, const unsigned int* bg,
                             int w, int h, int cw, int ch, int ox, int oy)
{
    if (!chars || !fg || !bg || w <= 0 || h <= 0 || cw <= 0 || ch <= 0) return;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { size_t i = (size_t)y * w + x; gmb_gfx_cell_bg(ox + x * cw, oy + y * ch, cw, ch, bg[i]); }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { size_t i = (size_t)y * w + x; gmb_gfx_cell_glyph(ox + x * cw, oy + y * ch, cw, ch, (uint32_t)chars[i], fg[i]); }
}
void gmb_gfx_render_cells_ex(const int* chars, const unsigned int* fg, const unsigned int* bg,
                             int w, int h, int cw, int ch)
{
    gmb_gfx_render_cells_at(chars, fg, bg, w, h, cw, ch, 0, 0);
}

/* Fallback (non sostituzione): la cartella `Resources/` CONDIVISA dei due mondi, risolta dal
 * runtime (`gmb_paths_resources_dir`: CWD → su → <exe> → su → repo → monorepo). Prima si risaliva
 * la CWD cercando il marker `GMBASIC.sln`, che non esiste più, quindi il fallback era morto. */
static int gfx_repo_resources_path(const char* rel, char* out, int cap)
{
    char dir[2048];
    if (!gmb_paths_resources_dir(dir, sizeof dir) || !dir[0]) return 0;
    snprintf(out, (size_t)cap, "%s/%s", dir, rel);
    FILE* f = fopen(out, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}
static int gfx_resolve_font_path(const char* name, char* out, int cap)
{
    if (!name || !*name) return 0;
    if (strchr(name, '/') || strstr(name, ".ttf") || strstr(name, ".otf")) {
        /* 1) percorso dato / relativo alla CWD del programma BASIC (PRIORITARIO). */
        FILE* f = fopen(name, "rb");
        if (f) { fclose(f); snprintf(out, cap, "%s", name); return 1; }
        /* 2) Resources/<nome> (relativo alla CWD). */
        char alt[1200]; snprintf(alt, sizeof(alt), "Resources/%s", name);
        f = fopen(alt, "rb"); if (f) { fclose(f); snprintf(out, cap, "%s", alt); return 1; }
        /* 3) AGGIUNTA: Resources della root del repo (fallback, non sostituzione). */
        char rr[1400]; if (gfx_repo_resources_path(name, rr, sizeof(rr))) { snprintf(out, cap, "%s", rr); return 1; }
    }
    if (strcasestr(name, "c64") || strcasestr(name, "sixtyfour"))
    {
        if (gfx_repo_resources_path("Sixtyfour.ttf", out, cap)) return 1;
        snprintf(out, cap, "Resources/Sixtyfour.ttf"); return 1;
    }
    /* SF Mono (Apple) rimosso (licenza: non ridistribuibile) -> Sixtyfour (OFL 1.1). */
    if (strcasestr(name, "sfmono") || strcasestr(name, "sf mono") || strcasestr(name, "sixtyfour"))
    {
        if (gfx_repo_resources_path("Sixtyfour.ttf", out, cap)) return 1;
        snprintf(out, cap, "Resources/Sixtyfour.ttf"); return 1;
    }
    return 0;
}

int gmb_gfx_font_default(int size)
{
    if (g_font_loaded) return 1;
    const char* env = getenv("GMB_FONT_PATH");
    char fpath[1200] = {0};
    int fsize = size > 0 ? size : 16;
    if (env && *env) snprintf(fpath, sizeof(fpath), "%s", env);
    else {
        extern int gmb_settings_console_str(int index, const char* key, char* out, int cap);
        extern int gmb_settings_console_int(int index, const char* key, int* out);
        int idx = 0; const char* e2 = getenv("GMB_CONSOLE"); if (e2 && *e2) idx = atoi(e2);
        char nm[256]; int sz;
        if (gmb_settings_console_str(idx, "fontPath", nm, sizeof(nm))) gfx_resolve_font_path(nm, fpath, sizeof(fpath));
        if (gmb_settings_console_int(idx, "fontSize", &sz) && sz > 0) fsize = sz;
    }
    if (fpath[0]) { gmb_gfx_font(fpath, fsize); if (g_font_loaded) return 1; }
    const char* cands[] = { "/System/Library/Fonts/SFNS.ttf",
                            "/System/Library/Fonts/Helvetica.ttc",
                            "/System/Library/Fonts/Supplemental/Arial.ttf",
                            "/System/Library/Fonts/Supplemental/Helvetica.ttf",
                            "/System/Library/Fonts/Geneva.ttf",
                            "/System/Library/Fonts/Monaco.ttf", "/System/Library/Fonts/SFNSMono.ttf",
                            "Resources/Sixtyfour.ttf", "Resources/font.ttf", NULL };
    for (int i = 0; cands[i]; i++) { gmb_gfx_font(cands[i], fsize); if (g_font_loaded) return 1; }
    return 0;
}
