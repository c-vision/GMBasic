/* ============================================================================
 * gmb_input_feeder.c — il TERMINALE come PRODUTTORE della coda eventi globale.
 *
 * Un solo imbuto per l'input (stessa forma della pump SDL): i tasti letti dal
 * terminale (raw reader su /dev/tty) vengono IMMESSI in gmb_gfx_input — la coda
 * che alimenta già SDL — e da lì li consumano INKEY/WAITKEY/GET e la console della
 * VM/C. Il dispatch ON KEY non è qui: avviene gia' alla LETTURA del tasto
 * (emitter C99 → gmb_events_onkey_dispatch; VM/C → gmb_vm_events), quindi riceve
 * a sua volta il tasto che esce da questa coda.
 *
 * Nessun consumatore legge più il terminale direttamente: prima INKEY lo faceva
 * (corsia laterale) e i tasti non entravano mai nel gestore eventi.
 *
 * Fonti che NON passano di qui perché sono già iniezioni dichiarate:
 *  - GMB_TUI_SCRIPT (script di test) e CONSOLE.SENDKEY (typeahead);
 *  - pump SDL della finestra (gmb_gfx_present_sdl.c).
 * In grafica (SCREEN ON) la tastiera è quella della finestra: il pump tace.
 * ============================================================================ */
#include "gmb_tui_io.h"
#include "gmb_tui.h"
#include "gmb_gfx_input.h"
#include "gmb_ui_gfx.h"
#include "gmb_runtime.h"

#include <string.h>
#include <stdbool.h>
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
#endif

/* Traduce il keycode GMB_K_* nel tasto UTF-8 della coda: STESSA codifica della pump
 * SDL (gmb_gfx_present_sdl.c) — così la coda è una sola per entrambe le fonti. */
static int gmb_feeder_key_to_utf8(int key, char* out, int cap)
{
    if (cap < 2) return 0;
    switch (key)
    {
        case GMB_K_ENTER: out[0] = '\r';   out[1] = '\0'; return 1;
        case GMB_K_TAB:   out[0] = '\t';   out[1] = '\0'; return 1;
        case GMB_K_BS:    out[0] = '\b';   out[1] = '\0'; return 1;
        case GMB_K_ESC:   out[0] = '\x1b'; out[1] = '\0'; return 1;
        case GMB_K_LEFT:  out[0] = '\x1c'; out[1] = '\0'; return 1;
        case GMB_K_RIGHT: out[0] = '\x1d'; out[1] = '\0'; return 1;
        case GMB_K_UP:    out[0] = '\x1e'; out[1] = '\0'; return 1;
        case GMB_K_DOWN:  out[0] = '\x1f'; out[1] = '\0'; return 1;
        default: break;
    }
    if (key >= 32 && key <= 126) { out[0] = (char)key; out[1] = '\0'; return 1; }
    return 0;
}

/* 1 = c'è un terminale reale da cui leggere. Falso in grafica (tastiera della
 * finestra) e con lo script di test (fonte iniettata, non hardware). */
bool gmb_input_terminal_available(void)
{
    if (GMB_GFX_SCREEN_ON()) return false;
    if (gmb_opt_env("GMB_TUI_SCRIPT") != NULL) return false;
#if defined(_WIN32) || defined(_WIN64)
    return _isatty(0) != 0;
#else
    return isatty(0) != 0;
#endif
}

/* Drena i tasti GIÀ pronti dal terminale e li immette nella coda eventi globale.
 * Mai bloccante (has_byte = select 0). Ritorna quanti tasti ha immesso. */
int gmb_input_pump(void)
{
    if (!gmb_input_terminal_available()) return 0;
    gmb_raw_input_reader_ensure_raw();
    int n = 0;
    while (n < 32 && gmb_raw_input_reader_has_byte())
    {
        gmb_ui_event_t ev;
        memset(&ev, 0, sizeof(ev));
        if (gmb_raw_input_reader_read_event_core(NULL, &ev) != 1) break;
        if (ev.type != GMB_UIEVENT_KEY) continue;
        char k[8];
        if (gmb_feeder_key_to_utf8(ev.key, k, (int)sizeof(k)))
        {
            gmb_gfx_input_push_key(k);
            n++;
        }
    }
    return n;
}
