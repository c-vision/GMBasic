/* gmb_form_selfcheck.c — verifica deterministica del form GPM-safe (headless).
 * Prova: registrazione bottone, hit-test + fire callback, click fuori = no-fire,
 * chiusura. Non richiede SDL ne' un task GPM (gmb_form_poll/dispatch sono inline). */
#include "gmb_form.h"
#include "gmb_gfx_input.h"
#include <stdio.h>

static int g_hits = 0;
static gmb_double_t on_click(void) { g_hits++; return 0.0; }

int main(void)
{
    gmb_form_reset();
    if (gmb_form_add_button(10, 10, 100, 30, "ok", on_click) != 0) { printf("FAIL add\n"); return 1; }
    if (gmb_form_button_count() != 1) { printf("FAIL count\n"); return 1; }
    if (gmb_form_closed() != 0) { printf("FAIL initial closed\n"); return 1; }

    /* click DENTRO il bottone (coordinate native; render_scale headless = 1) */
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, 20, 20, 1, 0);
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_UP, 20, 20, 0, 0);
    gmb_form_poll();
    if (g_hits != 1) { printf("FAIL hit=%d (atteso 1)\n", g_hits); return 1; }

    /* click FUORI: nessun fire */
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, 500, 500, 1, 0);
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_UP, 500, 500, 0, 0);
    gmb_form_poll();
    if (g_hits != 1) { printf("FAIL out-of-bounds hit=%d (atteso 1)\n", g_hits); return 1; }

    /* chiusura */
    gmb_form_request_close();
    if (gmb_form_closed() != 1) { printf("FAIL closed\n"); return 1; }

    printf("FORM-SELFCHECK-OK hits=%d\n", g_hits);
    return 0;
}
