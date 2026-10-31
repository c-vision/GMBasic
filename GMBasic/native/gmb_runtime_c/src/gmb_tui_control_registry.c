/* gmb_tui_control_registry.c — registro dei controlli (core).
 * Estratto da gmb_tui_app.c (UI): il core crea/registra i controlli; l'app (SDL/TUI
 * loop) usa gli stessi globals via extern. */
#include "gmb_tui_widgets.h"
#define GMB_TUI_CTRL_MAX 64
#include "gmb_tui_app.h"

gmb_control_t* g_reg_ctrl[GMB_TUI_CTRL_MAX];
int g_reg_n = 0;
int g_reg_focus = -1;

void gmb_tui_controls_reset(void) { g_reg_n = 0; g_reg_focus = -1; }
int  gmb_tui_control_add(gmb_control_t* c)
{
    if (!c || g_reg_n >= GMB_TUI_CTRL_MAX) return -1;
    g_reg_ctrl[g_reg_n] = c;
    c->handle = g_reg_n;
    if (g_reg_focus < 0) g_reg_focus = g_reg_n;
    return g_reg_n++;
}
