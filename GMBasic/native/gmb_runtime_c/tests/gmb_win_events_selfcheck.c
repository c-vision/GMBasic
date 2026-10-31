/* gmb_win_events_selfcheck.c — eventi finestra (CLOSE/MINIMIZE/...) con callback per handle.
 * Prima erano registrati per NOME ma MAI fired (nessun event loop). Qui si verifica che
 * gmb_winstmt_action(...) spari l'evento al gestore registrato per l'handle della finestra. */
#include "gmb_tui_windowstmt.h"
#include "gmb_runtime.h"
#include <stdio.h>

static int g_closed = 0, g_min = 0, g_act = 0;
static double cb_close(void) { g_closed++; return 0; }
static double cb_min(void) { g_min++; return 0; }
static double cb_act(void) { g_act++; return 0; }

int main(void)
{
    gmb_winstmt_reset();
    gmb_window_reset_events();

    int id = gmb_winstmt_define("w1", "Titolo", 1, 1, 10, 40, 0);
    if (id < 0) { printf("FAIL define\n"); return 1; }
    gmb_window_register_event_of_cb(id, "CLOSE", cb_close);
    gmb_window_register_event_of_cb(id, "MINIMIZE", cb_min);
    gmb_window_register_event_of_cb(id, "ACTIVATE", cb_act);

    gmb_winstmt_action("w1", "Minimize");
    if (g_min != 1) { printf("FAIL minimize=%d\n", g_min); return 1; }
    gmb_winstmt_action("w1", "Activate");
    if (g_act != 1) { printf("FAIL activate=%d\n", g_act); return 1; }
    gmb_winstmt_action("w1", "Close");
    if (g_closed != 1) { printf("FAIL close=%d\n", g_closed); return 1; }

    /* Rimozione (STOP): cb=NULL -> non scatta piu'. */
    gmb_winstmt_reset();
    gmb_window_reset_events();
    int id2 = gmb_winstmt_define("w2", "T", 1, 1, 5, 20, 0);
    gmb_window_register_event_of_cb(id2, "CLOSE", cb_close);
    gmb_window_register_event_of_cb(id2, "CLOSE", NULL);
    g_closed = 0;
    gmb_winstmt_action("w2", "Close");
    if (g_closed != 0) { printf("FAIL stop close=%d\n", g_closed); return 1; }

    printf("WIN-EVENTS-OK closed=%d min=%d act=%d\n", g_closed, g_min, g_act);
    return 0;
}
