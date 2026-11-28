/* ============================================================================
 * gmb_palette_selfcheck.c — command palette (parte headless).
 *
 * Verifica il registro, il FILTRO (ordine di pertinenza), la navigazione e il
 * testo dell'overlay. L'esecuzione delle azioni passa dal GPM: qui si verifica
 * che il runner riceva l'id giusto, senza scheduler (percorso lineare: accoda +
 * pump), che è anche il percorso usato dai programmi senza concorrenza.
 * ============================================================================ */
#include "gmb_ui_palette.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

static char g_ran[64];
static int  g_runs = 0;
static void runner(const char* id, void* ctx)
{
    (void)ctx;
    g_runs++;
    snprintf(g_ran, sizeof g_ran, "%s", id ? id : "");
}

static int title_of(int idx, char* out, int cap)
{
    /* il testo dell'elenco è "PALETTE> q" + righe; si legge dal testo ufficiale */
    char text[8192];
    gmb_palette_lines(text, (int)sizeof text);
    (void)idx; (void)out; (void)cap;
    return (int)strlen(text);
}

int main(void)
{
    char text[8192];

    /* ---- registro --------------------------------------------------------- */
    gmb_palette_clear();
    check(gmb_palette_count() == 0, "clear: registro vuoto");
    check(gmb_palette_register("file.save", "Salva il documento", "Ctrl+S", "File") == 0, "reg: save");
    check(gmb_palette_register("file.open", "Apri un file", "Ctrl+O", "File") == 0, "reg: open");
    check(gmb_palette_register("edit.find", "Trova nel testo", "Ctrl+F", "Modifica") == 0, "reg: find");
    check(gmb_palette_register("view.diff", "Mostra le differenze", "Ctrl+D", "Vista") == 0, "reg: diff");
    check(gmb_palette_count() == 4, "reg: quattro azioni");
    check(gmb_palette_register("file.save", "Salva", "", "File") == 0, "reg: stesso id aggiorna");
    check(gmb_palette_count() == 4, "reg: nessun duplicato");
    check(gmb_palette_register("", "x", "", "") < 0, "reg: id vuoto rifiutato");

    /* ---- filtro ----------------------------------------------------------- */
    int sel[GMB_PALETTE_MAX];
    gmb_palette_open();
    check(gmb_palette_is_open() == 1, "open: aperta");
    int n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    check(n == 4, "filtro: query vuota = tutte");

    gmb_palette_set_query("SA");
    n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    check(n == 1, "filtro: prefisso 'sa' (Salva)");
    gmb_palette_set_query("differenze");
    n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    check(n == 1, "filtro: parola nel titolo");
    gmb_palette_set_query("ctrl+d");
    n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    check(n == 1, "filtro: scorciatoia");
    gmb_palette_set_query("modifica");
    n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    check(n == 1, "filtro: categoria");

    /* ordine: prefisso del titolo prima della semplice occorrenza */
    gmb_palette_clear();
    gmb_palette_register("a", "Nuovo file", "", "");
    gmb_palette_register("b", "Apri nuovo", "", "");
    gmb_palette_register("c", "Chiudi", "", "");
    gmb_palette_set_query("n");
    n = gmb_palette_results(sel, GMB_PALETTE_MAX);
    check(n == 2, "ordine: due risultati per 'n'");

    /* ---- navigazione ------------------------------------------------------ */
    gmb_palette_set_query("");
    gmb_palette_move(1);
    check(gmb_palette_selected() == 1, "move: giu");
    gmb_palette_move(-1);
    check(gmb_palette_selected() == 0, "move: su");
    gmb_palette_move(-1);
    check(gmb_palette_selected() == 2, "move: giro circolare");
    gmb_palette_set_selected(99);
    check(gmb_palette_selected() == 2, "set_selected: clamp");

    /* ---- tasti ------------------------------------------------------------ */
    gmb_palette_set_query("");
    gmb_palette_type("Ch");
    check(!strcmp(gmb_palette_query(), "ch"), "type: normalizza minuscole");
    gmb_palette_backspace();
    check(!strcmp(gmb_palette_query(), "c"), "backspace: toglie un carattere");

    /* ---- testo dell'overlay ----------------------------------------------- */
    gmb_palette_set_query("");
    int len = gmb_palette_lines(text, (int)sizeof text);
    check(len > 0, "lines: prodotto");
    check(strstr(text, "PALETTE> ") == text, "lines: riga di ricerca per prima");
    check(strstr(text, "Chiudi") != NULL, "lines: voce presente");
    check(strstr(text, "> ") != NULL, "lines: marcatore di selezione");
    check(title_of(0, NULL, 0) == len, "lines: lunghezza stabile");

    /* ---- esecuzione (percorso lineare: senza scheduler e senza frame) ------ */
    gmb_palette_set_runner(runner, NULL);
    g_runs = 0; g_ran[0] = '\0';
    gmb_palette_set_query("chiud");
    gmb_palette_set_selected(0);
    check(gmb_palette_key(13) == 1, "key: Enter consumato");
    /* niente frame che pompino e nessuno scheduler: il programma esegue subito */
    check(g_runs == 1, "run: azione eseguita inline senza frame né GPM");
    check(!strcmp(g_ran, "c"), "run: id corretti");
    check(gmb_palette_is_open() == 0, "run: la palette si chiude");

    /* ESC chiude; tasto fuori dalla palette non viene catturato */
    gmb_palette_open();
    check(gmb_palette_key(27) == 1, "key: ESC consumato");
    check(gmb_palette_is_open() == 0, "key: ESC chiude");
    check(gmb_palette_key(27) == 0, "key: chiusa non cattura");

    /* nessun risultato: Enter non esegue nulla */
    gmb_palette_open();
    g_runs = 0;
    gmb_palette_set_query("zzz");
    gmb_palette_key(13);
    gmb_palette_pump();
    check(g_runs == 0, "run: nessun match = nessuna azione");

    if (g_fails == 0) printf("gmb_palette_selfcheck: %d controlli, 0 falliti\n", 40);
    return g_fails == 0 ? 0 : 1;
}