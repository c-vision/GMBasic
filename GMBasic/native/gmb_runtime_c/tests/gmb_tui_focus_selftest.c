/* gmb_tui_focus_selftest.c — tastiera → finestra FOCALIZZATA.
 *
 * Verifica headless di `tui_key_focus_sync()` (gmb_tui_app.c): se il controllo che ha il
 * focus appartiene a un `container` diverso dalla finestra attiva, il focus si sposta sul
 * primo controllo eleggibile di quella finestra PRIMA di applicare il tasto. Senza il fix i
 * tasti continuavano ad andare al controllo della finestra di sfondo.
 *
 * Setup: due finestre (wA, wB) e due checkbox, una per finestra (container = nome finestra).
 */
#include "gmb_tui_app.h"
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_widgets.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <string.h>

extern gmb_control_t* g_reg_ctrl[];
extern int g_reg_n;

static int index_of(gmb_control_t* c)
{
    for (int i = 0; i < g_reg_n; i++) if (g_reg_ctrl[i] == c) return i;
    return -1;
}

int main(void)
{
    gmb_winstmt_reset();
    gmb_window_reset_events();

    int wa = gmb_winstmt_define("wA", "A", 0, 0, 10, 30, 0);
    int wb = gmb_winstmt_define("wB", "B", 12, 0, 22, 30, 0);
    if (wa < 0 || wb < 0) { printf("FAIL define wa=%d wb=%d\n", wa, wb); return 1; }

    gmb_control_t* ca = gmb_tui_add_checkbox(1, 1, 20, "A");
    gmb_control_t* cb = gmb_tui_add_checkbox(1, 2, 20, "B");
    if (!ca || !cb) { printf("FAIL add checkbox\n"); return 1; }
    snprintf(ca->container, sizeof(ca->container), "wA");
    snprintf(cb->container, sizeof(cb->container), "wB");

    int ia = index_of(ca), ib = index_of(cb);
    if (ia < 0 || ib < 0 || ia == ib) { printf("FAIL registry ia=%d ib=%d\n", ia, ib); return 1; }

    /* ── 1. finestra attiva wA, focus sul controllo di wB: il tasto deve andare a wA ── */
    gmb_winstmt_select("wA");
    gmb_tui_control_focus(ib);
    gmb_tui_push_gfx_key(' ');
    gmb_tui_process_pending_keys();

    int a_checked = ((gmb_checkbox_t*)ca->user)->checked ? 1 : 0;
    int b_checked = ((gmb_checkbox_t*)cb->user)->checked ? 1 : 0;
    long long focused = gmb_tui_control_focused();
    if (focused != ia) { printf("FAIL focus sync: focused=%lld atteso=%d\n", focused, ia); return 1; }
    if (!a_checked)    { printf("FAIL il tasto non e' arrivato al controllo di wA\n"); return 1; }
    if (b_checked)     { printf("FAIL il tasto e' arrivato al controllo di wB (finestra di sfondo)\n"); return 1; }

    /* ── 2. simmetrico: finestra attiva wB, focus sul controllo di wA → il tasto va a wB ── */
    gmb_winstmt_select("wB");
    gmb_tui_control_focus(ia);
    gmb_tui_push_gfx_key(' ');
    gmb_tui_process_pending_keys();

    int a2 = ((gmb_checkbox_t*)ca->user)->checked ? 1 : 0;
    int b2 = ((gmb_checkbox_t*)cb->user)->checked ? 1 : 0;
    long long focused2 = gmb_tui_control_focused();
    if (focused2 != ib) { printf("FAIL focus sync 2: focused=%lld atteso=%d\n", focused2, ib); return 1; }
    if (!b2) { printf("FAIL il tasto non e' arrivato al controllo di wB\n"); return 1; }
    if (a2 != 1) { printf("FAIL il controllo di wA e' cambiato per un evento di wB\n"); return 1; }

    /* ── 3. `WINDOW SELECT 0` (nessuna finestra attiva): i controlli top-level restano
     *     eleggibili; qui i due sono in finestra, quindi il focus NON viene perso. ── */
    gmb_winstmt_select("0");
    gmb_tui_control_focus(ia);
    gmb_tui_push_gfx_key(' ');
    gmb_tui_process_pending_keys();
    if (gmb_tui_control_focused() != ia) { printf("FAIL focus perso con SELECT 0\n"); return 1; }

    printf("TUI-FOCUS-OK a=%d b=%d focus=%lld/%lld\n", a2, b2, focused, focused2);
    return 0;
}
