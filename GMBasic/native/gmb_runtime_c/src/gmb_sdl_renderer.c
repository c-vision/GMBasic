/* gmb_sdl_renderer.c — port FEDELE di CommonGraphics/Fonts/Atlas.cs (FontAtlas) +
 * CommonGraphics/Consoles/SdlRenderer.cs. UNA texture-atlante costruita con SDL_ttf
 * (TTF_RenderGlyph_Blended + SDL_BlitSurface); DrawChar = RenderCopy(glyphRect → destRect).
 * Guardato GMB_GFX_SDL_PRESENT; headless = no-op.
 */
#include "gmb_runtime.h"
#include "gmb_tui.h"
#include <string.h>
#include <stdlib.h>

#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#include <SDL_ttf.h>
#endif

#define GMB_ATLAS_GLYPHS 0x2600   /* firstChar ' ' .. lastChar 0x25FF (Atlas.cs / SdlConsole) */

typedef struct gmb_glyph_rect { int x, y, w, h; } gmb_glyph_rect_t;

typedef struct gmb_font_atlas {
    int _placeholder;
#ifdef GMB_GFX_SDL_PRESENT
    SDL_Texture* atlas;            /* _atlasTexture */
    gmb_glyph_rect_t g[GMB_ATLAS_GLYPHS];   /* _glyphRects */
    int char_w, char_h;            /* metriche cella (SdlConsole.UpdateFontAndRecalibrate) */
    int loaded;
#endif
} gmb_font_atlas_t;

gmb_font_atlas_t* gmb_font_atlas_new(void)
{
    return (gmb_font_atlas_t*)calloc(1, sizeof(gmb_font_atlas_t));
}

int gmb_font_atlas_load(gmb_font_atlas_t* a, void* sdl_renderer, const char* path, int px)
{
    if (!a) return 0;
    memset(a, 0, sizeof(*a));
#ifdef GMB_GFX_SDL_PRESENT
    if (!path || !sdl_renderer || px <= 0) return 0;
    SDL_Renderer* r = (SDL_Renderer*)sdl_renderer;
    TTF_Init();
    TTF_Font* font = TTF_OpenFont(path, px);
    if (!font) return 0;
    TTF_SetFontHinting(font, TTF_HINTING_NORMAL);

    /* maxGlyphWidth/Height su [first,last] forniti (Atlas.cs:124-142). */
    int maxW = 0, maxH = 0;
    for (int cp = 32; cp < GMB_ATLAS_GLYPHS; cp++) {
        if (TTF_GlyphIsProvided(font, cp) <= 0) continue;
        int w = 0, h = 0;
        char str[8]; int n = 0;
        if (cp < 0x80) { str[n++] = (char)cp; }
        else { str[n++] = (char)(0xC0 | (cp >> 6)); str[n++] = (char)(0x80 | (cp & 0x3F)); }
        str[n] = 0;
        if (TTF_SizeUTF8(font, str, &w, &h) == 0) { if (w > maxW) maxW = w; if (h > maxH) maxH = h; }
    }
    if (maxW == 0 || maxH == 0) { TTF_CloseFont(font); return 0; }

    int count = GMB_ATLAS_GLYPHS - 32;
    int atlasW = count * maxW;   /* ceil(sqrt(count))*maxW (Atlas.cs:147) */
    { int s = 1; while (s * s < count) s++; atlasW = s * maxW; }
    int rows = (atlasW / maxW) > 0 ? (count + (atlasW / maxW) - 1) / (atlasW / maxW) : count;
    int atlasH = rows * maxH;

    SDL_Surface* atlas = SDL_CreateRGBSurfaceWithFormat(0, atlasW, atlasH, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!atlas) { TTF_CloseFont(font); return 0; }
    SDL_SetSurfaceBlendMode(atlas, SDL_BLENDMODE_NONE);
    SDL_Color white = { 255, 255, 255, 255 };
    int cx = 0, cy = 0;
    for (int cp = 32; cp < GMB_ATLAS_GLYPHS; cp++) {
        SDL_Surface* g = TTF_RenderGlyph_Blended(font, cp, white);
        if (g) {
            SDL_Rect dst = { cx, cy, 0, 0 };
            SDL_BlitSurface(g, NULL, atlas, &dst);
            a->g[cp].x = cx; a->g[cp].y = cy; a->g[cp].w = g->w; a->g[cp].h = g->h;
            SDL_FreeSurface(g);
        }
        cx += maxW;
        if (cx + maxW > atlasW) { cx = 0; cy += maxH; }
    }
    a->atlas = SDL_CreateTextureFromSurface(r, atlas);
    SDL_FreeSurface(atlas);
    TTF_CloseFont(font);
    if (!a->atlas) return 0;
    SDL_SetTextureBlendMode(a->atlas, SDL_BLENDMODE_BLEND);

    /* metriche cella (SdlConsole.UpdateFontAndRecalibrate:294-295). */
    TTF_Font* f2 = TTF_OpenFont(path, px);
    if (f2) {
        int mw = 0, mh = 0; TTF_SizeUTF8(f2, "M", &mw, &mh);
        a->char_w = mw > 0 ? mw : 1;
        int fh = TTF_FontHeight(f2); a->char_h = fh > 0 ? fh : 1;
        TTF_CloseFont(f2);
    } else { a->char_w = maxW; a->char_h = maxH; }
    a->loaded = 1;
    return 1;
#else
    (void)sdl_renderer; (void)path; (void)px; return 0;
#endif
}

void gmb_font_atlas_free(gmb_font_atlas_t* a)
{
    if (!a) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (a->atlas) SDL_DestroyTexture(a->atlas);
#endif
    memset(a, 0, sizeof(*a));
}

int gmb_font_atlas_metrics(gmb_font_atlas_t* a, int* char_w, int* char_h)
{
    if (!a) return 0;
#ifdef GMB_GFX_SDL_PRESENT
    if (!a->loaded) return 0;
    if (char_w) *char_w = a->char_w;
    if (char_h) *char_h = a->char_h;
    return 1;
#else
    (void)char_w; (void)char_h; return 0;
#endif
}

/* DrawChar (Atlas.cs:235-244): color mod + RenderCopy(glyphRect → destRect). */
int gmb_font_atlas_draw_glyph(gmb_font_atlas_t* a, void* sdl_renderer, int cp,
                              int x, int y, int cw, int ch, int r, int g, int b)
{
    if (!a) return 0;
#ifdef GMB_GFX_SDL_PRESENT
    SDL_Renderer* rr = (SDL_Renderer*)sdl_renderer;
    if (!rr || !a->loaded || cp < 0 || cp >= GMB_ATLAS_GLYPHS) return 0;
    gmb_glyph_rect_t gr = a->g[cp];
    if (gr.w <= 0 || gr.h <= 0) return 0;
    SDL_SetTextureColorMod(a->atlas, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_Rect src = { gr.x, gr.y, gr.w, gr.h };
    SDL_Rect dst = { x, y, cw, ch };
    SDL_RenderCopy(rr, a->atlas, &src, &dst);
    return 1;
#else
    (void)a; (void)sdl_renderer; (void)cp; (void)x; (void)y;
    (void)cw; (void)ch; (void)r; (void)g; (void)b;
    return 0;
#endif
}

/* SdlRenderer.Render(buffer): clear bg + glyph per cella (usata dal bulk render). */
int gmb_sdl_renderer_render(void* sdl_renderer, gmb_screen_buffer_t* buffer,
                            int cell_w, int cell_h, const unsigned char default_bg[3],
                            gmb_font_atlas_t* atlas)
{
    if (!buffer) return 0;
#ifdef GMB_GFX_SDL_PRESENT
    SDL_Renderer* r = (SDL_Renderer*)sdl_renderer;
    if (!r) return 0;
    int w = gmb_screen_width(buffer), h = gmb_screen_height(buffer);
    if (w <= 0 || h <= 0) return 0;
    SDL_SetRenderDrawColor(r, default_bg ? default_bg[0] : 0, default_bg ? default_bg[1] : 0,
                           default_bg ? default_bg[2] : 0, 255);
    SDL_RenderClear(r);
    gmb_screen_cell_t* cells = gmb_screen_cells(buffer);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            gmb_screen_cell_t cell = cells[(size_t)y * w + x];
            if (cell.character <= 0x20u) continue;
            gmb_font_atlas_draw_glyph(atlas, r, (int)cell.character,
                                      x * cell_w, y * cell_h, cell_w, cell_h, 255, 255, 255);
        }
    return 1;
#else
    (void)sdl_renderer; (void)cell_w; (void)cell_h; (void)default_bg; (void)atlas;
    return 0;
#endif
}

/* --- primitive render per SdlConsole.RenderBuffer (target texture + fill + present) --- */
void* gmb_sdl_target_create(void* sdl_renderer, int w, int h)
{
#ifdef GMB_GFX_SDL_PRESENT
    SDL_Renderer* r = (SDL_Renderer*)sdl_renderer;
    if (!r || w <= 0 || h <= 0) return NULL;
    return SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
#else
    (void)sdl_renderer; (void)w; (void)h; return NULL;
#endif
}

void gmb_sdl_target_destroy(void* target)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (target) SDL_DestroyTexture((SDL_Texture*)target);
#else
    (void)target;
#endif
}

void gmb_sdl_render_begin(void* sdl_renderer, void* target)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (sdl_renderer) SDL_SetRenderTarget((SDL_Renderer*)sdl_renderer, (SDL_Texture*)target);
#else
    (void)sdl_renderer; (void)target;
#endif
}

void gmb_sdl_render_clear(void* sdl_renderer, int r, int g, int b)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (!sdl_renderer) return;
    SDL_SetRenderDrawColor((SDL_Renderer*)sdl_renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderClear((SDL_Renderer*)sdl_renderer);
#else
    (void)sdl_renderer; (void)r; (void)g; (void)b;
#endif
}

void gmb_sdl_render_fill(void* sdl_renderer, int x, int y, int w, int h, int r, int g, int b)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (!sdl_renderer) return;
    SDL_SetRenderDrawColor((SDL_Renderer*)sdl_renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_Rect rc = { x, y, w, h };
    SDL_RenderFillRect((SDL_Renderer*)sdl_renderer, &rc);
#else
    (void)sdl_renderer; (void)x; (void)y; (void)w; (void)h; (void)r; (void)g; (void)b;
#endif
}

void gmb_sdl_render_present(void* sdl_renderer, void* target)
{
#ifdef GMB_GFX_SDL_PRESENT
    if (!sdl_renderer) return;
    SDL_Renderer* r = (SDL_Renderer*)sdl_renderer;
    SDL_SetRenderTarget(r, NULL);
    if (target) { SDL_RenderCopy(r, (SDL_Texture*)target, NULL, NULL); }
    SDL_RenderPresent(r);
#else
    (void)sdl_renderer; (void)target;
#endif
}
