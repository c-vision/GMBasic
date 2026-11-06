/* gmb_form_dropdown_keys_selfcheck.c — controlli pixel: comportamento, non pixel.
 *
 * 1) SELECT (W_DROPDOWN): con il focus, freccia giu' apre, muove l'evidenziazione,
 *    Invio conferma scrivendo il valore nella variabile INTO, Esc chiude senza cambiare.
 *    Prima del fix la tastiera del dropdown non era gestita: il test falliva.
 * 2) CHECKBOX: il caption deve essere QUEL testo (non la larghezza, non vuoto) e la
 *    riga renderizzata dal TUI deve contenerlo accanto alla casella.
 *
 * Headless: usa la factory PIXEL esplicita (senza dispatch TUI/GRAPHICS) e il renderer
 * TUI dei widget, così il test non richiede una finestra. */
#include "gmb_form.h"
#include "gmb_gfx_input.h"
#include <stdio.h>
#include <string.h>
#include "gmb_tui_widgets.h"

int main(void)
{
    double sel = 0.0;
    int h;

    gmb_form_reset();
    h = gmb_form_add_widget_px(GMB_PX_DROPDOWN, 10, 10, 160, 28, "Alfa|Beta|Gamma", &sel);
    if (h < 0) { printf("FAIL: add_widget_px dropdown\n"); return 1; }

    /* Focus esplicito (senza click: il click aprirebbe il popup, che e' cio' che testiamo). */
    gmb_form_focus_widget(h);

    gmb_form_dispatch_key("\x1f");   /* Giu'  -> apre (nessun cambio di valore) */
    if ((int)(sel + 0.5) != 0) { printf("FAIL: apertura ha gia' cambiato sel=%g\n", sel); return 1; }

    gmb_form_dispatch_key("\x1f");   /* Giu'  -> evidenzia la voce 1 */
    gmb_form_dispatch_key("\r");     /* Invio -> conferma */
    if ((int)(sel + 0.5) != 1) { printf("FAIL: sel=%g dopo Giu'+Invio (atteso 1)\n", sel); return 1; }

    gmb_form_dispatch_key("\x1f");   /* Giu'  -> riapre sulla selezione corrente */
    gmb_form_dispatch_key("\x1e");   /* Su    -> torna a 0 */
    gmb_form_dispatch_key("\r");
    if ((int)(sel + 0.5) != 0) { printf("FAIL: sel=%g dopo Su+Invio (atteso 0)\n", sel); return 1; }

    gmb_form_dispatch_key("\x1f");   /* apre */
    gmb_form_dispatch_key("\x1f");   /* evidenzia 1 */
    gmb_form_dispatch_key("\x1b");   /* Esc   -> chiude senza confermare */
    if ((int)(sel + 0.5) != 0) { printf("FAIL: Esc ha cambiato sel=%g\n", sel); return 1; }

    /* CHECKBOX: il caption e' il testo dichiarato e il renderer TUI lo mostra accanto
     * al quadratino (regressione: con 4 argomenti il parser scartava il testo). */
    {
        double cv = 0.0; gmb_checkbox_t cb; char line[128];
        int ci = gmb_form_add_checkbox_px(10, 60, "notify", &cv);
        if (ci < 0 || strcmp(gmb_form_widget_caption(ci), "notify") != 0)
        { printf("FAIL: caption=%s\n", ci < 0 ? "(no widget)" : gmb_form_widget_caption(ci)); return 1; }
        gmb_checkbox_init(&cb, 10, 60, 20, "notify");
        gmb_checkbox_render_line(&cb, line, (int)sizeof(line));
        if (!strstr(line, "[ ]") || !strstr(line, "notify"))
        { printf("FAIL: riga checkbox=%s\n", line); return 1; }
    }
    printf("FORM-DROPDOWN-OK sel=%g caption=notify\n", sel);
    return 0;
}
