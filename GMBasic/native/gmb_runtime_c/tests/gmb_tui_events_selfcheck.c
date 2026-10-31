/* gmb_tui_events_selfcheck.c — eventi dei controlli TUI (non-bloccante): registra un
 * gestore CHANGE, inietta un tasto nel typeahead, esegue un frame, verifica che scatti
 * e che il polling di stato (CHANGED) lo rifletta. */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_form.h"
#include <stdio.h>

static int g_change = 0;
static double cb_change(void) { g_change++; return 0.0; }

int main(void)
{
    gmb_tui_controls_reset();
    gmb_control_t* c = gmb_tui_add_checkbox(0, 0, 20, "X");
    if (!c) { printf("FAIL add\n"); return 1; }
    int h = c->handle;
    gmb_tui_set_event_id(h, 3, cb_change);   /* 3 = CHANGE */

    gmb_tui_inject_keys(" ");                /* spazio = toggle checkbox */
    gmb_tui_controls_frame(0);

    if (g_change != 1) { printf("FAIL change fired=%d\n", g_change); return 1; }
    if (gmb_tui_changed(h) != 1) { printf("FAIL changed poll=%g\n", gmb_tui_changed(h)); return 1; }
    if (gmb_form_changed(h) != 1) { printf("FAIL unified gmb_form_changed=%g\n", gmb_form_changed(h)); return 1; }

    /* STOP: rimuove il gestore */
    gmb_tui_set_event_id(h, 3, NULL);
    g_change = 0;
    gmb_tui_inject_keys(" ");
    gmb_tui_controls_frame(0);
    if (g_change != 0) { printf("FAIL stop change=%d\n", g_change); return 1; }

    printf("TUI-EVENTS-OK change=%d\n", g_change);
    return 0;
}
