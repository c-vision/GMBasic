/* gmb_form_dialog_selfcheck.c — finestre modali (WindowBase) su schermo virtuale.
 * Richiede SDL_VIDEODRIVER=dummy (nessuna finestra reale). Inietta i tasti.
 *   enter|esc   -> gmb_form_dialog (0 / -1)
 *   inputbox    -> gmb_form_inputbox ("Hello" poi Invio)
 *   printer     -> gmb_form_printerdialog (Invio -> 0) */
#include "gmb_graphics.h"
#include "gmb_form.h"
#include "gmb_gfx_input.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    const char* mode = (argc > 1) ? argv[1] : "enter";
    gmb_gfx_init();
    gmb_gfx_screen(1, 640, 480, 32);
    if (!gmb_gfx_screen_on()) { printf("DIALOG-SKIP no screen\n"); return 0; }
    int rc = 0;

    if (!strcmp(mode, "inputbox")) {
        gmb_gfx_input_push_key("Hello");
        gmb_gfx_input_push_key("\r");
        gmb_string_t r = gmb_form_inputbox("Titolo", "Inserisci:", "", NULL);
        printf("INPUTBOX r='%s' %s\n", r.data, strcmp(r.data, "Hello") ? "FAIL" : "OK");
        rc = strcmp(r.data, "Hello") ? 1 : 0;
    } else if (!strcmp(mode, "printer")) {
        gmb_gfx_input_push_key("\r");
        double copies = 2, duplex = 0;
        int r = gmb_form_printerdialog(NULL, &copies, &duplex);
        printf("PRINTER r=%d copies=%.0f %s\n", r, copies, (r == 0) ? "OK" : "FAIL");
        rc = (r == 0) ? 0 : 1;
    } else {
        int want = (!strcmp(mode, "esc")) ? -1 : 0;
        gmb_gfx_input_push_key(!strcmp(mode, "esc") ? "\x1b" : "\r");
        int r = gmb_form_dialog("Test", "Messaggio di prova", "OK", "Cancel", NULL, NULL, "info");
        printf("DIALOG mode=%s r=%d want=%d %s\n", mode, r, want, (r == want) ? "OK" : "FAIL");
        rc = (r == want) ? 0 : 1;
    }
    gmb_gfx_shutdown();
    return rc;
}
