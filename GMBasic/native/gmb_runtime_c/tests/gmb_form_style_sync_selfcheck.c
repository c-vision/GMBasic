/* ============================================================================
 * gmb_form_style_sync_selfcheck.c — verifica che una spec di STILE applicata per
 * HANDLE arrivi anche al control TUI OMONIMO, non solo al widget PIXEL del
 * registry form.
 *
 * Perché esiste: gli handle dei due registri (g_wid/g_btns e g_reg_ctrl) si
 * sovrappongono — entrambi partono da 0 — quindi una ricerca per handle nel
 * registry form può trovare un ALTRO controllo e fermarsi lì. Il reflow TUI
 * (`gmb_tui_layout_reflow`) però muove solo il control di `g_reg_ctrl`: senza
 * sincronizzazione `minw=/maxw=/grow=` restano scritti sul pixel e ignorati.
 *
 * Il test crea apposta il caso peggiore: stesso handle (0) nei due registri e
 * stesso nome. Esce 1 se la spec non arriva al control TUI.
 * ============================================================================ */
#include "gmb_form.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

int main(void)
{
    double v_form = 0.0, v_tui = 0.0;

    /* Due registri puliti: il form widget prende handle 0, il control TUI idem. */
    gmb_tui_controls_reset();

    int h_form = gmb_form_add_checkbox(10, 10, "Top", &v_form);
    int h_tui  = gmb_control_add_checkbox(10, 10, 200, "Top", &v_tui);

    check(h_form == 0 && h_tui == 0, "precondizione: handle sovrapposti (0 in entrambi i registri)");

    /* Nome del control TUI: è la chiave comune fra i due registri. */
    gmb_control_apply_style_handle(h_tui, "name=Top");

    gmb_control_t* t = gmb_tui_control_find_by_name("Top");
    check(t != NULL, "il control TUI si trova per nome");
    if (!t) { printf("%d verifiche fallite\n", g_fails); return 1; }

    /* Prima della spec: layout non attivo. */
    check(t->layout_on == 0, "precondizione: control TUI senza layout");

    /* La spec va applicata per HANDLE: il form widget ha lo stesso handle 0. */
    gmb_form_apply_style_handle(h_form, "name=Top minw=40 maxw=120 grow=50 anchor=left");
    check(t->layout_on == 1, "layout attivo sul control TUI omonimo");
    check(t->layout_min_w == 40,   "minw sul control TUI");
    check(t->layout_max_w == 120,  "maxw sul control TUI");
    check(t->layout_grow  == 50,   "grow sul control TUI");
    check(t->layout_anchor == GMB_LAYOUT_LEFT, "anchor sul control TUI");
    check(t->layout_is_tui == 1,   "unità del layout = celle");

    /* Il limite dichiarato vale SUBITO anche sulla dimensione corrente (200 > 120). */
    check(t->width == 120, "maxw applicato alla larghezza corrente");

    /* Idempotenza: riapplicare la stessa spec non cambia la base del delta. */
    int bx = t->layout_base_x, bw = t->layout_base_w;
    gmb_form_apply_style_handle(h_form, "maxw=120 grow=50");
    check(t->layout_base_x == bx && t->layout_base_w == bw, "la base del delta non si sposta");

    /* Nessun omonimo: nessun crash e nessun controllo inventato. */
    gmb_form_apply_style_handle(h_form, "name=Altro maxw=300");
    check(gmb_tui_control_find_by_name("Altro") == NULL, "nessun controllo creato dal nulla");

    if (g_fails) { printf("%d verifiche fallite\n", g_fails); return 1; }
    printf("gmb_form_style_sync_selfcheck: OK\n");
    return 0;
}
