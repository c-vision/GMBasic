/* gmb_sdl_renderer.h — parità CommonGraphics/Consoles/SdlRenderer.cs + FontAtlas (glyph cache)
 * SDL-guarded; headless = no-op. Include <SDL.h> solo quando GMB_GFX_SDL_PRESENT.
 */
#ifndef GMB_SDL_RENDERER_H
#define GMB_SDL_RENDERER_H

#include "gmb_tui.h"

typedef struct gmb_font_atlas gmb_font_atlas_t;

gmb_font_atlas_t* gmb_font_atlas_new(void);

/* Carica un TTF nel FontAtlas (SdlRenderer.cs:22 SdlRenderer(renderer, fontAtlas,...)).
 * Ritorna 1 se ok (SDL presente), 0 headless/errore. */
int gmb_font_atlas_load(gmb_font_atlas_t* a, void* sdl_renderer, const char* path, int px);
void gmb_font_atlas_free(gmb_font_atlas_t* a);

/* Render di un buffer celle sul renderer SDL (SdlRenderer.cs:34 Render): clear bg + glyph. */
int gmb_sdl_renderer_render(void* sdl_renderer, gmb_screen_buffer_t* buffer,
                            int cell_w, int cell_h, const unsigned char default_bg[3],
                            gmb_font_atlas_t* atlas);

/* Metriche cella dal font (SdlConsole.Internals UpdateFontAndRecalibrate: advance 'M' e
 * TTF_FontHeight). Ritorna 1 se disponibili. */
int gmb_font_atlas_metrics(gmb_font_atlas_t* a, int* char_w, int* char_h);
/* Disegna un glyph scalato nella cella (x,y,cw,ch) col colore RGB dato (DrawCell).
 * Ritorna 1 se disegnato. */
int gmb_font_atlas_draw_glyph(gmb_font_atlas_t* a, void* sdl_renderer, int cp,
                              int x, int y, int cw, int ch, int r, int g, int b);

/* --- primitive render per SdlConsole.RenderBuffer (target texture + fill + present) --- */
void* gmb_sdl_target_create(void* sdl_renderer, int w, int h);   /* SdlConsole screen texture */
void  gmb_sdl_target_destroy(void* target);
void  gmb_sdl_render_begin(void* sdl_renderer, void* target);    /* SDL_SetRenderTarget */
void  gmb_sdl_render_clear(void* sdl_renderer, int r, int g, int b);
void  gmb_sdl_render_fill(void* sdl_renderer, int x, int y, int w, int h, int r, int g, int b);
void  gmb_sdl_render_present(void* sdl_renderer, void* target);  /* RenderCopy(target)+RenderPresent */

#endif
