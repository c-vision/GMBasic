/* gmb_sdl_manager.c — parità CommonGraphics/Consoles/SdlManager.cs:131 (SDL_Init + PollEvents + Quit)
 * Singleton SDL del processo: init (VIDEO|EVENTS|JOYSTICK + TTF), pump eventi con dispatch
 * a callback, quit. Guardato GMB_GFX_SDL_PRESENT: headless = no-op.
 */
#include "gmb_runtime.h"
#include <string.h>

#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#include <SDL_ttf.h>
#endif

#define GMB_SDLMGR_CALLBACKS 8

typedef struct gmb_sdl_event_cb_entry {
    void (*cb)(int event_type, void* userdata);
    void* userdata;
    int in_use;
} gmb_sdl_event_cb_entry_t;

typedef struct gmb_sdl_manager {
    int initialized;
    gmb_sdl_event_cb_entry_t quit_cb[GMB_SDLMGR_CALLBACKS];
    gmb_sdl_event_cb_entry_t keyboard_cb[GMB_SDLMGR_CALLBACKS];
    gmb_sdl_event_cb_entry_t window_cb[GMB_SDLMGR_CALLBACKS];
    gmb_sdl_event_cb_entry_t mouse_button_cb[GMB_SDLMGR_CALLBACKS];
    gmb_sdl_event_cb_entry_t mouse_motion_cb[GMB_SDLMGR_CALLBACKS];
    gmb_sdl_event_cb_entry_t text_input_cb[GMB_SDLMGR_CALLBACKS];
    gmb_sdl_event_cb_entry_t other_cb[GMB_SDLMGR_CALLBACKS];
} gmb_sdl_manager_t;

static gmb_sdl_manager_t g_sdlmgr;

int gmb_sdl_manager_is_initialized(void) { return g_sdlmgr.initialized; }

void gmb_sdl_manager_init(void)
{
    if (g_sdlmgr.initialized) return;
#ifdef GMB_GFX_SDL_PRESENT
    uint32_t flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0) flags |= SDL_INIT_JOYSTICK;
    if (SDL_Init(flags) < 0) return;
    if (TTF_Init() < 0) { SDL_Quit(); return; }
#endif
    g_sdlmgr.initialized = 1;
}

static void fire(gmb_sdl_event_cb_entry_t* arr, int event_type)
{
    for (int i = 0; i < GMB_SDLMGR_CALLBACKS; i++)
        if (arr[i].in_use && arr[i].cb) arr[i].cb(event_type, arr[i].userdata);
}

static void sdlmgr_add(gmb_sdl_event_cb_entry_t* arr, void (*cb)(int, void*), void* ud)
{
    for (int i = 0; i < GMB_SDLMGR_CALLBACKS; i++)
        if (!arr[i].in_use) { arr[i].cb = cb; arr[i].userdata = ud; arr[i].in_use = 1; return; }
}
static void sdlmgr_remove(gmb_sdl_event_cb_entry_t* arr, void (*cb)(int, void*))
{
    for (int i = 0; i < GMB_SDLMGR_CALLBACKS; i++)
        if (arr[i].in_use && arr[i].cb == cb) { arr[i].in_use = 0; return; }
}

void gmb_sdl_manager_on_quit(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.quit_cb, cb, ud); }
void gmb_sdl_manager_on_keyboard(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.keyboard_cb, cb, ud); }
void gmb_sdl_manager_on_window(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.window_cb, cb, ud); }
void gmb_sdl_manager_on_mouse_button(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.mouse_button_cb, cb, ud); }
void gmb_sdl_manager_on_mouse_motion(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.mouse_motion_cb, cb, ud); }
void gmb_sdl_manager_on_text_input(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.text_input_cb, cb, ud); }
void gmb_sdl_manager_on_other(void (*cb)(int, void*), void* ud) { sdlmgr_add(g_sdlmgr.other_cb, cb, ud); }

void gmb_sdl_manager_remove_quit(void (*cb)(int, void*)) { sdlmgr_remove(g_sdlmgr.quit_cb, cb); }
void gmb_sdl_manager_remove_keyboard(void (*cb)(int, void*)) { sdlmgr_remove(g_sdlmgr.keyboard_cb, cb); }

/* Pump eventi: ritorna 1 se è stato richiesto quit. Dispatch per tipo. */
int gmb_sdl_manager_poll_events(void)
{
    if (!g_sdlmgr.initialized) return 0;
    int quit = 0;
#ifdef GMB_GFX_SDL_PRESENT
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
            case SDL_QUIT: quit = 1; fire(g_sdlmgr.quit_cb, 0); break;
            case SDL_KEYDOWN: case SDL_KEYUP:
                fire(g_sdlmgr.keyboard_cb, e.type == SDL_KEYDOWN ? 1 : 2); break;
            case SDL_TEXTINPUT: fire(g_sdlmgr.text_input_cb, 3); break;
            case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
                fire(g_sdlmgr.mouse_button_cb, e.type == SDL_MOUSEBUTTONDOWN ? 4 : 5); break;
            case SDL_MOUSEMOTION: fire(g_sdlmgr.mouse_motion_cb, 6); break;
            case SDL_WINDOWEVENT: fire(g_sdlmgr.window_cb, 7); break;
            default: fire(g_sdlmgr.other_cb, e.type); break;
        }
    }
    SDL_Delay(1);
#else
    (void)0;
#endif
    return quit;
}

void gmb_sdl_manager_quit(void)
{
    if (!g_sdlmgr.initialized) return;
#ifdef GMB_GFX_SDL_PRESENT
    TTF_Quit();
    SDL_Quit();
#endif
    memset(&g_sdlmgr, 0, sizeof(g_sdlmgr));
}
