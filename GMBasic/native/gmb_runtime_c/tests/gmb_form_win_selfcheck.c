/* gmb_form_win_selfcheck.c — WindowBase (base di tutte le finestre), headless.
 * Verifica il loop comune: fira Load/Activate/Paint/Unload e gestisce l'input. */
#include "gmb_form.h"
#include "gmb_gfx_input.h"
#include <stdio.h>

static int g_load = 0, g_act = 0, g_paint = 0, g_unload = 0, g_ev = 0;
static gmb_double_t on_load(void) { g_load++; return 0; }
static gmb_double_t on_act(void) { g_act++; return 0; }
static gmb_double_t on_paint(void) { g_paint++; return 0; }
static gmb_double_t on_unload(void) { g_unload++; return 0; }

static void wdraw(void* ctx) { (void)ctx; }
static int wev(gmb_form_win_t* w, int ev, int x, int y, void* ctx)
{
    (void)w; (void)x; (void)y; (void)ctx;
    if (ev == GMB_EV_MOUSEDOWN) { g_ev++; return 1; }   /* chiude al primo click */
    return 0;
}

int main(void)
{
    gmb_form_win_t w;
    gmb_form_win_init(&w);
    snprintf(w.caption, sizeof(w.caption), "Test Window");
    gmb_form_win_on(&w, GMB_EV_LOAD, on_load);
    gmb_form_win_on(&w, GMB_EV_ACTIVATE, on_act);
    gmb_form_win_on(&w, GMB_EV_PAINT, on_paint);
    gmb_form_win_on(&w, GMB_EV_UNLOAD, on_unload);

    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, 5, 5, 1, 0);
    gmb_form_win_run(&w, wdraw, wev, NULL, 0);   /* frame_ms=0: nessun park */

    if (g_load != 1 || g_act != 1 || g_paint < 1 || g_unload != 1 || g_ev != 1)
        { printf("FAIL load=%d act=%d paint=%d unload=%d ev=%d\n", g_load, g_act, g_paint, g_unload, g_ev); return 1; }
    printf("FORM-WIN-OK load=%d activate=%d paint=%d unload=%d ev=%d\n", g_load, g_act, g_paint, g_unload, g_ev);
    return 0;
}
