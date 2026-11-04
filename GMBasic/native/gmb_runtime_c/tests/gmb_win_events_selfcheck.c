/* gmb_win_events_selfcheck.c — eventi finestra (CLOSE/MINIMIZE/...) con callback per handle.
 * Prima erano registrati per NOME ma MAI fired (nessun event loop). Qui si verifica che
 * gmb_winstmt_action(...) spari l'evento al gestore registrato per l'handle della finestra. */
#include "gmb_tui_windowstmt.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <string.h>

static int g_closed = 0, g_min = 0, g_act = 0;
static double cb_close(void) { g_closed++; return 0; }
static double cb_min(void) { g_min++; return 0; }
static double cb_act(void) { g_act++; return 0; }

static int g_min_a = 0, g_min_b = 0, g_focus = 0, g_any = 0;
static double cb_min_a(void) { g_min_a++; return 0; }
static double cb_min_b(void) { g_min_b++; return 0; }
static double cb_focus(void) { g_focus++; return 0; }
static double cb_any(void) { g_any++; return 0; }
static int g_last_win = -99;
static const char* g_last_ev = NULL;
static double cb_ctx(void) { gmb_window_firing(&g_last_win, &g_last_ev); return 0; }

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

    /* ON WINDOW <ev> OF <handle>: la registrazione vale SOLO per quella finestra.
     * (Fase 5 del piano eventi: senza handle l'evento arrivava a ogni finestra.) */
    gmb_winstmt_reset();
    gmb_window_reset_events();
    int a = gmb_winstmt_define("wa", "A", 1, 1, 8, 30, 0);
    int b = gmb_winstmt_define("wb", "B", 10, 1, 18, 30, 0);
    if (a < 0 || b < 0 || a == b) { printf("FAIL define a=%d b=%d\n", a, b); return 1; }
    gmb_window_register_event_of_cb(a, "MINIMIZE", cb_min_a);
    gmb_window_register_event_of_cb(b, "MINIMIZE", cb_min_b);
    gmb_winstmt_action("wb", "Minimize");
    if (g_min_a != 0 || g_min_b != 1) { printf("FAIL handle filter a=%d b=%d\n", g_min_a, g_min_b); return 1; }
    gmb_winstmt_action("wb", "Restore");
    gmb_winstmt_action("wa", "Minimize");
    if (g_min_a != 1 || g_min_b != 1) { printf("FAIL handle filter2 a=%d b=%d\n", g_min_a, g_min_b); return 1; }

    /* Registrazione SENZA OF (handle -1 = wildcard): scatta per qualunque finestra. */
    gmb_window_register_event_of_cb(-1, "RESTORE", cb_any);
    gmb_winstmt_action("wa", "Restore");
    gmb_winstmt_action("wb", "Restore");
    if (g_any != 2) { printf("FAIL wildcard restore=%d\n", g_any); return 1; }

    /* FOCUS: WINDOW … ACTIVATE innalza FOCUS (WindowEventKind.Focus) oltre al legacy ACTIVATE. */
    gmb_window_register_event_of_cb(b, "FOCUS", cb_focus);
    gmb_winstmt_action("wa", "Activate");
    if (g_focus != 0) { printf("FAIL focus leaked=%d\n", g_focus); return 1; }
    gmb_winstmt_action("wb", "Activate");
    if (g_focus != 1) { printf("FAIL focus=%d\n", g_focus); return 1; }

    /* Contesto del firing leggibile dalla callback: (finestra, evento) corretti. */
    gmb_window_register_event_of_cb(a, "CLOSE", cb_ctx);
    gmb_winstmt_action("wa", "Close");
    if (g_last_win != a || !g_last_ev || strcmp(g_last_ev, "CLOSE") != 0)
    { printf("FAIL firing ctx win=%d ev=%s\n", g_last_win, g_last_ev ? g_last_ev : "(null)"); return 1; }

    printf("WIN-EVENTS-HANDLE-OK a=%d b=%d any=%d focus=%d\n", g_min_a, g_min_b, g_any, g_focus);
    return 0;
}
