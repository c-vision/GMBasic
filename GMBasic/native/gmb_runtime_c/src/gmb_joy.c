/* gmb_joy.c — parità CommonGraphics/Consoles/JoyManager.cs:103 (SDL joystick, guardato)
 * CPU/headless: nessun joystick (count 0, is_present false, state 0, name "").
 * SDL presente (GMB_GFX_SDL_PRESENT): SDL_NumJoysticks/Open/Axis/Button/Hat reali.
 * Bitmask state (identica a JoyManager.cs:62-88): 1=UP 2=DOWN 4=LEFT 8=RIGHT 16=FIRE 32=BUTTON2
 */
#include "gmb_runtime.h"
#include <string.h>

#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#endif

#define GMB_JOY_MAX 16

typedef struct {
    int init;
    int count;
    int last_refresh;
    void* joysticks[GMB_JOY_MAX]; /* SDL_Joystick* oppure NULL */
} gmb_joy_t;

static gmb_joy_t g_joy;

static void joy_clear(void)
{
#ifdef GMB_GFX_SDL_PRESENT
    for (int i = 0; i < g_joy.count; i++)
        if (g_joy.joysticks[i]) SDL_JoystickClose((SDL_Joystick*)g_joy.joysticks[i]);
#endif
    memset(g_joy.joysticks, 0, sizeof(g_joy.joysticks));
    g_joy.count = 0;
    g_joy.last_refresh = 0;
}

void gmb_joy_refresh(void)
{
    if (!g_joy.init) { gmb_joy_init(); return; }
#ifdef GMB_GFX_SDL_PRESENT
    if (!SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) { joy_clear(); return; }
    int n = SDL_NumJoysticks();
    if (n == g_joy.last_refresh) return;
    g_joy.last_refresh = n;
    joy_clear();
    if (n > GMB_JOY_MAX) n = GMB_JOY_MAX;
    for (int i = 0; i < n; i++)
        g_joy.joysticks[i] = SDL_JoystickOpen(i);
    g_joy.count = n;
#else
    (void)0;
#endif
}

void gmb_joy_init(void)
{
    if (g_joy.init) return;
    memset(&g_joy, 0, sizeof(g_joy));
    g_joy.init = 1;
    gmb_joy_refresh();
}

int gmb_joy_count(void)
{
    if (!g_joy.init) gmb_joy_init();
    gmb_joy_refresh();
    return g_joy.count;
}

int gmb_joy_is_present(int id)
{
    if (id < 0 || id >= GMB_JOY_MAX) return 0;
    if (!g_joy.init) gmb_joy_init();
    gmb_joy_refresh();
    return id < g_joy.count && g_joy.joysticks[id] != NULL;
}

long gmb_joy_get_state(int id)
{
    if (!gmb_joy_is_present(id)) return 0L;
#ifdef GMB_GFX_SDL_PRESENT
    SDL_Joystick* joy = (SDL_Joystick*)g_joy.joysticks[id];
    long result = 0;
    if (SDL_JoystickGetAxis(joy, 0) < -16384) result |= 4;  /* LEFT */
    if (SDL_JoystickGetAxis(joy, 0) > 16384)  result |= 8;  /* RIGHT */
    if (SDL_JoystickGetAxis(joy, 1) < -16384) result |= 1;  /* UP */
    if (SDL_JoystickGetAxis(joy, 1) > 16384)  result |= 2;  /* DOWN */
    if (SDL_JoystickGetButton(joy, 0) != 0)   result |= 16; /* FIRE */
    if (SDL_JoystickGetButton(joy, 1) != 0)   result |= 32; /* BUTTON2 */
    if (SDL_JoystickNumHats(joy) > 0) {
        unsigned char hat = SDL_JoystickGetHat(joy, 0);
        if ((hat & SDL_HAT_UP) != 0)    result |= 1;
        if ((hat & SDL_HAT_DOWN) != 0)  result |= 2;
        if ((hat & SDL_HAT_LEFT) != 0)  result |= 4;
        if ((hat & SDL_HAT_RIGHT) != 0) result |= 8;
    }
    return result;
#else
    return 0L;
#endif
}

const char* gmb_joy_get_name(int id)
{
    if (!gmb_joy_is_present(id)) return "";
#ifdef GMB_GFX_SDL_PRESENT
    const char* n = SDL_JoystickName((SDL_Joystick*)g_joy.joysticks[id]);
    return n ? n : "";
#else
    return "";
#endif
}

void gmb_joy_dispose(void)
{
    joy_clear();
    g_joy.init = 0;
}
