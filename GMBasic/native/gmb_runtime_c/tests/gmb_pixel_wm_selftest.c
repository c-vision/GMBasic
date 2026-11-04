/* gmb_pixel_wm_selftest.c — capacità delle finestre PIXEL nel runtime C.
 *
 * Verifica le funzioni che prima esistevano SOLO nella classe C# GfxWindowManager:
 *  - flag `pixel` e colori per-finestra (BodyColor/FgColor/TitleColor via campo generico)
 *  - client rect in pixel logici (title 24, padding 6)
 *  - hit-test dei pulsanti titlebar (close/min/max) in pixel
 *  - bordi di resize in pixel (N=1 S=2 E=4 W=8)
 *  - set_colors → campo body/fg/title della finestra C
 */
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_widgets.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <string.h>

/* stessi id del binding C# (gmb_tui_windowmanager.c) */
#define MWF_BODY_COLOR   24
#define MWF_FG_COLOR     25
#define MWF_TITLE_COLOR  26
#define MWF_PIXEL        27

int main(void)
{
    gmb_winstmt_reset();
    gmb_window_reset_events();

    /* DEFINE pixel: 40,40 → 440,160 (400x120 pixel logici) */
    int id = gmb_winstmt_define("pw", "Pixel", 40, 40, 160, 440,
                               GMB_WINSTMT_PIXEL | GMB_WINSTMT_SHADOW);
    if (id < 0) { printf("FAIL define id=%d\n", id); return 1; }

    gmb_managed_window_t* w = gmb_window_manager_get(gmb_window_manager_static(), id);
    if (!w) { printf("FAIL managed window\n"); return 1; }
    if (gmb_managed_window_get_int(w, MWF_PIXEL) != 1) { printf("FAIL flag pixel\n"); return 1; }
    if (w->rect.w != 400 || w->rect.h != 120) { printf("FAIL rect %dx%d\n", w->rect.w, w->rect.h); return 1; }

    /* Colori per-finestra (0 = tema) */
    if (gmb_managed_window_get_int(w, MWF_BODY_COLOR) != 0) { printf("FAIL colore default\n"); return 1; }
    gmb_winstmt_set_colors("pw", 0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u);
    if ((unsigned)gmb_managed_window_get_int(w, MWF_BODY_COLOR) != 0xFF0000FFu)
    { printf("FAIL body_color\n"); return 1; }
    if ((unsigned)gmb_managed_window_get_int(w, MWF_FG_COLOR) != 0xFF00FF00u)
    { printf("FAIL fg_color\n"); return 1; }
    if ((unsigned)gmb_managed_window_get_int(w, MWF_TITLE_COLOR) != 0xFFFF0000u)
    { printf("FAIL title_color\n"); return 1; }

    /* Client rect: x+6, y+24, w-12, h-24-6 */
    int cr[4] = {0};
    gmb_winstmt_pixel_client_rect("pw", cr);
    if (cr[0] != 46 || cr[1] != 64 || cr[2] != 388 || cr[3] != 90)
    { printf("FAIL client_rect %d,%d,%d,%d\n", cr[0], cr[1], cr[2], cr[3]); return 1; }

    /* Pulsanti titlebar: close a X+W-24, max a -48, min a -72 (larghezza 20) */
    int rx = 40 + 400;
    if (gmb_winstmt_pixel_control_at("pw", rx - 12, 50) != 1) { printf("FAIL control close\n"); return 1; }
    if (gmb_winstmt_pixel_control_at("pw", rx - 36, 50) != 3) { printf("FAIL control max\n"); return 1; }
    if (gmb_winstmt_pixel_control_at("pw", rx - 60, 50) != 2) { printf("FAIL control min\n"); return 1; }
    if (gmb_winstmt_pixel_control_at("pw", 45, 50) != 0) { printf("FAIL control centro\n"); return 1; }
    /* Fuori dalla titlebar (y >= 40+24) non è un pulsante */
    if (gmb_winstmt_pixel_control_at("pw", rx - 12, 70) != 0) { printf("FAIL control fuori titlebar\n"); return 1; }

    /* Bordi resize in pixel (edge 4) */
    if (gmb_winstmt_pixel_resize_edge_at("pw", 440, 100) != 4) { printf("FAIL edge E\n"); return 1; }
    if (gmb_winstmt_pixel_resize_edge_at("pw", 240, 159) != 2) { printf("FAIL edge S\n"); return 1; }
    if (gmb_winstmt_pixel_resize_edge_at("pw", 40, 50) != 8) { printf("FAIL edge W\n"); return 1; }
    if (gmb_winstmt_pixel_resize_edge_at("pw", 440, 159) != 6) { printf("FAIL edge SE\n"); return 1; }
    if (gmb_winstmt_pixel_resize_edge_at("pw", 240, 100) != 0) { printf("FAIL edge centro\n"); return 1; }

    printf("PIXEL-WM-OK id=%d client=%d,%d,%d,%d\n", id, cr[0], cr[1], cr[2], cr[3]);
    return 0;
}
