/* gmb_gfx_present_input_selfcheck.c — integrazione SDL → gmb_gfx_input (headless).
 * Usa il driver video "dummy": nessuna finestra OS. Gli eventi SDL vengono
 * processati con gmb_gfx_present_process_event (la stessa traduzione usata da
 * gmb_gfx_present_tick sul loop SDL_PollEvent; la coda eventi SDL sintetica e'
 * inaffidabile con sdl2-compat/dummy).
 * Copre: tasti di controllo, TEXTINPUT, Ctrl+C, mouse (move/down/wheel),
 * stato fisico tasti, reset a present_stop. */
#include "gmb_graphics.h"
#include "gmb_gfx_present.h"
#include "gmb_gfx_input.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static void push_key(SDL_Keycode sym, SDL_Scancode sc, SDL_Keymod mod)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_KEYDOWN;
    e.key.repeat = 0;
    e.key.keysym.sym = sym;
    e.key.keysym.scancode = sc;
    e.key.keysym.mod = mod;
    gmb_gfx_present_process_event(&e);
}

static void push_keyup(SDL_Scancode sc)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_KEYUP;
    e.key.keysym.scancode = sc;
    gmb_gfx_present_process_event(&e);
}

static void push_text(const char* text)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_TEXTINPUT;
    strncpy(e.text.text, text, sizeof(e.text.text) - 1);
    gmb_gfx_present_process_event(&e);
}

static void push_motion(int x, int y)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEMOTION;
    e.motion.x = x; e.motion.y = y;
    gmb_gfx_present_process_event(&e);
}

static void push_button(int button, int down, int x, int y)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.button = (Uint8)button;
    e.button.x = x; e.button.y = y;
    gmb_gfx_present_process_event(&e);
}

static void push_wheel(int y)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEWHEEL;
    e.wheel.y = y;
    gmb_gfx_present_process_event(&e);
}

int main(void)
{
    char key[64];
    int type = 0, x = 0, y = 0, button = 0, wheel = 0;

    setenv("SDL_VIDEODRIVER", "dummy", 1);
    gmb_gfx_init();
    gmb_gfx_screen(13, 320, 200, 8);
    gmb_gfx_present_start();
    gmb_gfx_present_tick(); /* crea la finestra (dummy) + avvia il text input */

    /* Tasto di controllo: ESC → "\x1b". */
    push_key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, KMOD_NONE);
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 1 && strcmp(key, "\x1b") == 0, "ESC → 0x1b");

    /* Stampabile: con TEXTINPUT attivo arriva da SDL_TEXTINPUT (niente doppioni). */
    push_key(SDLK_a, SDL_SCANCODE_A, KMOD_NONE);
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_key_down(SDL_SCANCODE_A) == 1, "key down fisico");
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 0, "nessun doppione da KEYDOWN");
    push_text("a");
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 1 && strcmp(key, "a") == 0, "TEXTINPUT → a");
    push_keyup(SDL_SCANCODE_A);
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_key_down(SDL_SCANCODE_A) == 0, "key up fisico");

    /* Ctrl+C: flag consumabile, nessun tasto accodato. */
    push_key(SDLK_c, SDL_SCANCODE_C, KMOD_LCTRL);
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_take_ctrl_c() == 1, "ctrl-c rilevato");
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 0, "ctrl-c non accoda tasti");

    /* Mouse: move → down → wheel (la wheel usa l'ultima posizione). */
    push_motion(40, 30);
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_MOVE && x == 40 && y == 30, "mouse move");
    push_button(1, 1, 40, 30);
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_DOWN && button == 1, "mouse down sinistro");
    push_wheel(1);
    gmb_gfx_present_tick();
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_WHEEL && wheel == 1 && x == 40 && y == 30, "mouse wheel");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 0, "coda mouse vuota");

    /* Stop: reset completo dell'input. */
    push_text("z");
    gmb_gfx_present_tick();
    gmb_gfx_present_stop();
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 0, "reset a present_stop");

    if (fails == 0) { printf("gmb_gfx_present_input_selfcheck: OK\n"); return 0; }
    printf("gmb_gfx_present_input_selfcheck: %d FAIL\n", fails);
    return 1;
}
