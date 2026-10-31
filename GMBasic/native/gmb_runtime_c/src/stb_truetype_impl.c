/* Unico punto di implementazione di stb_truetype (usato da gmb_sdl_renderer.c
 * per l'atlas console TUI). Il modulo testo grafico (gmb_graphics_text.c) usa
 * SDL_ttf/FreeType per parita' col motore C#. */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
