/* ui_tui_parity.c — PROVA VISIVA: i controlli creati dalle factory TUI devono avere
 * lo STESSO aspetto grafico dei controlli GUI (parità totale), perché in grafica
 * usano il renderer dei widget FORM. Salva un PNG con tutti e dieci affiancati.
 * Build/run: tools/sh/ui_tui_parity.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gmb_runtime.h"
#include "gmb_graphics.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_form.h"

int main(void)
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    gmb_gfx_screen(12, 900, 560, 32);
    gmb_ui_set_theme_name("glass");

    /* i dieci controlli "TUI" creati in GRAFICA */
    gmb_tui_add_toggle(40, 40, 240, "live updates");
    gmb_tui_add_tab(40, 90, 240, "Overview|Metrics|Logs");
    gmb_tui_add_statusbar(40, 140, 240, "ready");
    gmb_tui_add_slider(40, 180, 240, 0, 100, 64);
    gmb_tui_add_radiogroup(40, 220, 240, 80, "Fast|Balanced|Safe");
    { static gmb_panel_t pn; gmb_panel_init(&pn, 40, 320, 240, 90, "PANEL"); gmb_tui_control_register(&pn.ctrl); }
    gmb_tui_add_listbox(40, 430, 240, 90, "uno|due|tre");
    gmb_tui_add_grid(320, 40, 240, 120, 2, "a|1;b|2;c|3");
    gmb_tui_add_combobox(320, 180, 240, "alpha|beta|gamma");
    gmb_tui_add_colorpicker(320, 230, 240, 48);

    { extern void gmb_form_render_if_dirty(void); extern void gmb_tui_draw_px_all(void);
      gmb_form_render_if_dirty(); gmb_tui_draw_px_all(); }
    gmb_gfx_save_screen_png("tools/reports/ui_tui_parity.png");
    printf("TUI-PARITY-OK\n");
    return 0;
}
