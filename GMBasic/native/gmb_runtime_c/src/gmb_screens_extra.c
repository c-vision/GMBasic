/* gmb_screens_extra.c — parità CommonGraphics/Screens/Manager.cs:90 (ScreenManager.AvailableModes)
 * Headless: lista vuota (0). SDL presente (GMB_GFX_SDL_PRESENT): enumera SDL_GetNumVideoDisplays/
 * GetDisplayMode, de-duplica e ordina per width,height,refresh (Manager.cs:37-89).
 */
#include "gmb_runtime.h"
#include <string.h>

#ifdef GMB_GFX_SDL_PRESENT
#include <SDL.h>
#endif

int gmb_screens_available_modes(gmb_display_mode_info_t* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int n = 0;
#ifdef GMB_GFX_SDL_PRESENT
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0 && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        return 0;
    int numDisplays = SDL_GetNumVideoDisplays();
    if (numDisplays < 1) return 0;
    for (int i = 0; i < numDisplays && n < cap; i++) {
        int numModes = SDL_GetNumDisplayModes(i);
        if (numModes < 1) continue;
        for (int j = 0; j < numModes && n < cap; j++) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(i, j, &mode) != 0) continue;
            gmb_display_mode_info_t m;
            m.width = mode.w;
            m.height = mode.h;
            m.refresh_rate = mode.refresh_rate;
            m.display_index = i;
            snprintf(m.pixel_format, sizeof(m.pixel_format), "%s",
                     SDL_GetPixelFormatName(mode.format));
            /* de-dup come C# Distinct() */
            int dup = 0;
            for (int k = 0; k < n; k++)
                if (out[k].width == m.width && out[k].height == m.height &&
                    out[k].refresh_rate == m.refresh_rate &&
                    out[k].display_index == m.display_index &&
                    strcmp(out[k].pixel_format, m.pixel_format) == 0) { dup = 1; break; }
            if (!dup) out[n++] = m;
        }
    }
    /* sort: width asc, height asc, refresh desc (Manager.cs:85-87) */
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++) {
            int less = out[b].width < out[a].width ||
                       (out[b].width == out[a].width && out[b].height < out[a].height) ||
                       (out[b].width == out[a].width && out[b].height == out[a].height &&
                        out[b].refresh_rate > out[a].refresh_rate);
            if (less) { gmb_display_mode_info_t t = out[a]; out[a] = out[b]; out[b] = t; }
        }
#else
    (void)0;
#endif
    return n;
}
