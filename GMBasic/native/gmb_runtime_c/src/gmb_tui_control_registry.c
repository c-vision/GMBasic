/* gmb_tui_control_registry.c — registro dei controlli (core).
 * Estratto da gmb_tui_app.c (UI): il core crea/registra i controlli; l'app (SDL/TUI
 * loop) usa gli stessi globals via extern. */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <string.h>
#define GMB_TUI_CTRL_MAX 256
#include "gmb_tui_app.h"

gmb_control_t* g_reg_ctrl[GMB_TUI_CTRL_MAX];
int g_reg_n = 0;
int g_reg_focus = -1;

void gmb_tui_controls_reset(void) { g_reg_n = 0; g_reg_focus = -1; }
int  gmb_tui_control_add(gmb_control_t* c)
{
    if (!c) return -1;
    if (g_reg_n >= GMB_TUI_CTRL_MAX)
    {
        /* MAI silenzioso: oltre il limite il controllo non viene registrato né
         * disegnato (era la causa di "controlli che non si vedono"). */
        static int warned = 0;
        if (!warned) { warned = 1; fprintf(stderr, "[gmb_tui] registro PIENO: GMB_TUI_CTRL_MAX=%d (controllo ignorato)\n", GMB_TUI_CTRL_MAX); }
        return -1;
    }
    g_reg_ctrl[g_reg_n] = c;
    c->handle = g_reg_n;
    if (g_reg_focus < 0) g_reg_focus = g_reg_n;
    /* "Ultimo controllo creato" = handle, in QUALSIASI registry: le clausole della
     * dichiarazione (NAME/STYLE/…/metric) si applicano per handle, non per indice. */
    { extern void gmb_form_note_handle(int h); gmb_form_note_handle(c->handle); }
    return g_reg_n++;
}

/* Rimuove l'ultimo controllo registrato: il chiamante ne resta proprietario. */
void gmb_tui_control_remove_last(void)
{
    if (g_reg_n <= 0) return;
    g_reg_n--;
    g_reg_ctrl[g_reg_n] = NULL;
    if (g_reg_focus >= g_reg_n) g_reg_focus = g_reg_n - 1;
}
