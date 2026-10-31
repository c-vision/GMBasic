/* gmb_sdl_context.h — parità CommonGraphics/Consoles/SdlContext.cs:98 (lifecycle SDL Window+Renderer)
 * Crea/distingue coppia window+renderer, RaiseWindow+input focus, hint scale quality "1" e
 * driver Metal su macOS (non-dummy), fallback software, Dispose (renderer poi window).
 * SDL-guarded; headless: handles 0.
 */
#ifndef GMB_SDL_CONTEXT_H
#define GMB_SDL_CONTEXT_H

#include <stdint.h>

typedef struct gmb_sdl_context {
    void* window_handle;    /* SDL_Window* */
    void* renderer_handle;  /* SDL_Renderer* */
    int ok;
} gmb_sdl_context_t;

/* Crea window+renderer (SdlContext.cs:30-77). flags = SDL_WindowFlags (0 = default). */
int gmb_sdl_context_create(gmb_sdl_context_t* c, const char* title, int x, int y, int w, int h, uint32_t flags);
/* Distrugge renderer poi window (SdlContext.cs:79-97). */
void gmb_sdl_context_destroy(gmb_sdl_context_t* c);

/* --- primitive finestra (usate da SdlConsole.UpdateFontAndRecalibrate/Show/SetVisibility) --- */
void gmb_sdl_context_set_window_size(gmb_sdl_context_t* c, int w, int h);
void gmb_sdl_context_set_window_position(gmb_sdl_context_t* c, int x, int y);
int  gmb_sdl_context_render_scale(gmb_sdl_context_t* c);   /* output px / window points (HiDPI) */
void gmb_sdl_context_set_visible(gmb_sdl_context_t* c, int visible);

#endif
