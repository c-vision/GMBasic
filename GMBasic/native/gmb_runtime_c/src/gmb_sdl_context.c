/* gmb_sdl_context.c — parità CommonGraphics/Consoles/SdlContext.cs:98 (lifecycle SDL Window+Renderer) */
#include "gmb_sdl_context.h"
#include "gmb_runtime.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#endif

int gmb_sdl_context_create(gmb_sdl_context_t* c, const char* title, int x, int y, int w, int h, uint32_t flags)
{
    if (!c) return 0;
    memset(c, 0, sizeof(*c));
#ifdef GMB_GFX_SDL_PRESENT
    gmb_sdl_manager_init();
    /* SdlContext.cs:123: sempre RESIZABLE|ALLOW_HIGHDPI (renderScale = output/points). */
    SDL_Window* win = SDL_CreateWindow(title, x, y, w, h,
        flags | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) return 0;
    SDL_RaiseWindow(win);
    SDL_SetWindowInputFocus(win);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
#ifdef __APPLE__
    {
        const char* drv = getenv("SDL_VIDEODRIVER");
        if (!drv || strcmp(drv, "dummy") != 0) {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
            SDL_SetHint("SDL_RENDER_BATCHING", "1");
        }
    }
#endif
    SDL_Renderer* r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!r)
        r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!r) {
        SDL_DestroyWindow(win);
        return 0;
    }
    c->window_handle = win;
    c->renderer_handle = r;
    c->ok = 1;
    return 1;
#else
    (void)title; (void)x; (void)y; (void)w; (void)h; (void)flags;
    return 0;
#endif
}

void gmb_sdl_context_destroy(gmb_sdl_context_t* c)
{
    if (!c) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (c->renderer_handle) SDL_DestroyRenderer((SDL_Renderer*)c->renderer_handle);
    if (c->window_handle) SDL_DestroyWindow((SDL_Window*)c->window_handle);
#endif
    c->renderer_handle = NULL;
    c->window_handle = NULL;
    c->ok = 0;
}

void gmb_sdl_context_set_window_size(gmb_sdl_context_t* c, int w, int h)
{
    if (!c || !c->ok) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (c->window_handle && w > 0 && h > 0) SDL_SetWindowSize((SDL_Window*)c->window_handle, w, h);
#else
    (void)w; (void)h;
#endif
}

void gmb_sdl_context_set_window_position(gmb_sdl_context_t* c, int x, int y)
{
    if (!c || !c->ok) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (c->window_handle) SDL_SetWindowPosition((SDL_Window*)c->window_handle, x, y);
#else
    (void)x; (void)y;
#endif
}

int gmb_sdl_context_render_scale(gmb_sdl_context_t* c)
{
    if (!c || !c->ok) return 0;
#ifdef GMB_GFX_SDL_PRESENT
    int ow = 0, oh = 0, ww = 0, wh = 0;
    SDL_GetRendererOutputSize((SDL_Renderer*)c->renderer_handle, &ow, &oh);
    SDL_GetWindowSize((SDL_Window*)c->window_handle, &ww, &wh);
    if (ow > 0 && ww > 0) { int s = ow / ww; return s > 0 ? s : 1; }
    return 1;
#else
    return 0;
#endif
}

void gmb_sdl_context_set_visible(gmb_sdl_context_t* c, int visible)
{
    if (!c || !c->ok) return;
#ifdef GMB_GFX_SDL_PRESENT
    if (!c->window_handle) return;
    if (visible) SDL_ShowWindow((SDL_Window*)c->window_handle);
    else SDL_HideWindow((SDL_Window*)c->window_handle);
#else
    (void)visible;
#endif
}
