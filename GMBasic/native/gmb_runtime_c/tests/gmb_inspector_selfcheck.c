/* ============================================================================
 * gmb_inspector_selfcheck.c — inspector della UI (parte headless).
 *
 * Il pannello è testo: si verifica la voce del registro che lo accende, il
 * contenuto delle righe (nome, tipo, rect, flag, colonne della griglia) e il
 * costo del frame. Il disegno su celle/pixel non si misura qui: è un overlay
 * senza stato, e il suo input è esattamente il testo verificato sotto.
 * ============================================================================ */
#include "gmb_ui_inspector.h"
#include "gmb_tui_widgets.h" /* GMB_CK_*: nomi dei tipi usati dal pannello */
#include "gmb_runtime.h"     /* gmb_opt_set: registro opzioni */

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

static int has(const char* hay, const char* needle)
{
    return strstr(hay, needle) != NULL;
}

int main(void)
{
    char text[8192];

    /* ---- interruttore dal registro opzioni --------------------------------- */
    check(gmb_inspector_enabled() == 0, "default: inspector spento");
    check(gmb_opt_set(GMB_INSPECTOR_OPT, "1") == 0, "opt set ui.inspector=1");
    check(gmb_inspector_enabled() == 1, "opt: inspector acceso");
    gmb_opt_set(GMB_INSPECTOR_OPT, "on");
    check(gmb_inspector_enabled() == 1, "opt: 'on' acceso");
    gmb_opt_set(GMB_INSPECTOR_OPT, "0");
    check(gmb_inspector_enabled() == 0, "opt: '0' spento");
    gmb_opt_set(GMB_INSPECTOR_OPT, "1");

    /* ---- nomi dei tipi ---------------------------------------------------- */
    check(!strcmp(gmb_inspector_kind_name(GMB_CK_BUTTON), "BUTTON"), "kind: BUTTON");
    check(!strcmp(gmb_inspector_kind_name(GMB_CK_LISTBOX), "LISTBOX"), "kind: LISTBOX");
    check(!strcmp(gmb_inspector_kind_name(GMB_CK_TBROWSE), "TBROWSE"), "kind: TBROWSE");
    check(!strcmp(gmb_inspector_kind_name(9999), "?"), "kind: sconosciuto");

    /* ---- contenuto del pannello ------------------------------------------- */
    gmb_inspector_reset();
    check(gmb_inspector_add("btnOK", "BUTTON", 4, 2, 12, 2,
                            GMB_INSPECT_VISIBLE | GMB_INSPECT_ENABLED | GMB_INSPECT_FOCUSED
                            | GMB_INSPECT_LAYOUT, 0) == 0, "add: primo indice 0");
    check(gmb_inspector_add("cards", "LISTBOX", 4, 10, 56, 16,
                            GMB_INSPECT_VISIBLE | GMB_INSPECT_ENABLED | GMB_INSPECT_FLOW, 3) == 1,
          "add: secondo indice 1");
    check(gmb_inspector_add("", "", 0, 0, 1, 1, 0, 0) == 2, "add: terzo indice 2");
    check(gmb_inspector_count() == 3, "add: tre voci");

    gmb_inspector_stats(NULL, NULL, NULL);
    int n = gmb_inspector_lines(text, (int)sizeof text);
    check(n > 0, "lines: testo prodotto");
    check(has(text, "INSPECTOR  ctrl=3"), "lines: intestazione con numero controlli");
    check(has(text, "name") && has(text, "kind") && has(text, "flags"), "lines: colonne");
    check(has(text, "btnOK") && has(text, "BUTTON"), "lines: nome e tipo della prima voce");
    check(has(text, "4,2") && has(text, "12x2"), "lines: rect della prima voce");
    check(has(text, "vefL"), "lines: flag v/e/f/L");
    check(has(text, "Fl/3"), "lines: flow grid con 3 colonne");
    check(has(text, "(anon)"), "lines: nome vuoto mostrato come (anon)");

    /* ---- costo del frame --------------------------------------------------- */
    gmb_inspector_note_frame(1537, 3);
    gmb_inspector_reset();
    gmb_inspector_add("w", "LABEL", 0, 0, 5, 1, GMB_INSPECT_VISIBLE, 0);
    gmb_inspector_lines(text, (int)sizeof text);
    check(has(text, "frame=1.537ms"), "stats: costo dell'ultimo frame in ms");
    check(has(text, "frames=1"), "stats: contatore frame");
    {
        int ms = -1, ctrl = -1, frames = -1;
        gmb_inspector_stats(&ms, &ctrl, &frames);
        check(ms == 1537 && ctrl == 3 && frames == 1, "stats: valori letti");
    }

    /* ---- split in righe (input del disegno) -------------------------------- */
    {
        char lines[GMB_INSPECTOR_MAXLINES][GMB_INSPECTOR_LINE_MAX];
        gmb_inspector_reset();
        gmb_inspector_add("a", "BUTTON", 1, 2, 3, 2, GMB_INSPECT_VISIBLE, 0);
        gmb_inspector_add("b", "LABEL", 5, 6, 7, 1, GMB_INSPECT_ENABLED, 0);
        int nl = gmb_inspector_split(lines, GMB_INSPECTOR_MAXLINES);
        check(nl == 4, "split: intestazione + colonne + 2 voci");
        check(has(lines[0], "INSPECTOR"), "split: prima riga = intestazione");
        check(has(lines[2], "a") && has(lines[3], "b"), "split: le voci sono righe");
    }

    /* ---- pannello pieno: nessuna scrittura oltre il massimo ---------------- */
    {
        gmb_inspector_reset();
        for (int i = 0; i < GMB_INSPECTOR_MAX + 8; i++)
            gmb_inspector_add("n", "LABEL", i, 0, 1, 1, 0, 0);
        check(gmb_inspector_count() == GMB_INSPECTOR_MAX, "limite: voci ferme al massimo");
    }

    /* ---- spento: il frame non costruisce il pannello ----------------------- */
    gmb_opt_set(GMB_INSPECTOR_OPT, "0");
    check(gmb_inspector_enabled() == 0, "off: inspector spento");

    if (g_fails) { printf("%d verifiche fallite\n", g_fails); return 1; }
    printf("gmb_inspector_selfcheck: OK\n");
    return 0;
}
