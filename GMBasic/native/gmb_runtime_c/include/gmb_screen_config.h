/* gmb_screen_config.h — parità CommonLib/Screens/ScreenConfig.cs:509 (record immutabile + palette + GetDefault)
 * Configurazione di una console/schermo accettata dalla console C (gmb_sdl_console).
 * Nessun bridge C↔C#: tutto portato.
 */
#ifndef GMB_SCREEN_CONFIG_H
#define GMB_SCREEN_CONFIG_H

#include <stdbool.h>

typedef enum gmb_screen_window_mode {
    GMB_WM_WINDOWED = 0, GMB_WM_FULLSCREEN, GMB_WM_FULLSCREEN_DESKTOP, GMB_WM_BORDERLESS
} gmb_screen_window_mode_t;

typedef enum gmb_font_hinting { GMB_FH_NORMAL = 0, GMB_FH_LIGHT, GMB_FH_MONOSPACE, GMB_FH_NONE } gmb_font_hinting_t;

typedef enum gmb_screen_mode {
    GMB_SM_TEXT = 0, GMB_SM_C64, GMB_SM_CGA, GMB_SM_EGA, GMB_SM_VGA, GMB_SM_SVGA, GMB_SM_XGA, GMB_SM_CRT
} gmb_screen_mode_t;

#define GMB_SCFG_MAX_PALETTE 256

typedef struct gmb_screen_config {
    gmb_screen_mode_t mode;
    int logical_width, logical_height;       /* grafica (pixel); 0 = n/a */
    int char_grid_width, char_grid_height;   /* console a caratteri; 0 = n/a */
    unsigned char resolved_palette[GMB_SCFG_MAX_PALETTE][3];
    int palette_size;
    const char* palette_name;
    int default_fg_color, default_bg_color;
    int window_width, window_height, window_x, window_y; /* 0 = default */
    const char* font_path;
    int font_size;
    gmb_font_hinting_t font_hinting;
    bool use_logical_size_scaling;
    bool use_native_resolution_rendering;
    gmb_screen_window_mode_t window_mode;
} gmb_screen_config_t;

/* GetDefault(ScreenMode) — ScreenConfig.cs:374-455 */
gmb_screen_config_t gmb_screen_config_get_default(gmb_screen_mode_t mode);

/* Palette (ScreenConfig.cs:313-365 + 21 named) */
void gmb_screen_palette_vga16(unsigned char p[16][3]);
void gmb_screen_palette_vga256(unsigned char p[256][3]);
void gmb_screen_palette_c64(unsigned char p[16][3]);
void gmb_screen_palette_c64_crt(unsigned char p[16][3]);
void gmb_screen_palette_c64_classic(unsigned char p[16][3]);
/* Named palette per nome ("Console","Solarized","Dracula",...): ritorna 1 se trovata, copia in out[16][3] */
int gmb_screen_palette_named(const char* name, unsigned char out[16][3]);
/* Elenca i nomi delle 21 palette named (per CONSOLE.PALETTE.USE) */
int gmb_screen_palette_named_list(char out[][32], int cap);

/* Applica la config allo stato console+grafica corrente (dimensioni/palette/fg/bg/font). */
void gmb_screen_config_apply(const gmb_screen_config_t* cfg);

#endif
