/* gmb_sdl_console_selfcheck.c — verifica HEADLESS della SdlConsole a CELLE + controlli TUI.
 *
 * Driver video "dummy": NESSUNA finestra OS (come gmb_gfx_present_input_selfcheck).
 * Verifica il percorso REALE dell'app (che nel build C99Native gira sul MAIN THREAD):
 *   gmb_tui_sdl_start (crea+mostra)  ->  gmb_tui_sdl_pump (eventi SDL + tasti ai controlli)
 *   ->  gmb_tui_controls_frame (disegno)  ->  gmb_tui_sdl_present (blink + render).
 * Copre: creazione senza finestra; buffer console come target dei controlli; ponte input
 * fifo SDL -> typeahead -> controllo a fuoco; posizione del caret; render headless. */
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_clipper.h"
#include "gmb_gfx_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("ok  : %s\n", msg); } } while (0)

int main(void)
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);

    gmb_tui_sdl_start(40, 12);
    CHECK(gmb_tui_sdl_active() == 1, "start: console creata (headless, nessuna finestra)");

    gmb_screen_buffer_t* cb = gmb_clipper_global_state()->buffer;
    CHECK(cb != NULL && gmb_screen_width(cb) == 40 && gmb_screen_height(cb) == 12,
          "il buffer della console (40x12) e' il target dei controlli");

    gmb_tui_controls_reset();
    gmb_tui_add_codearea(0, 0, 38, 10, "HELLO");
    gmb_tui_add_statusbar(0, 11, 40, "ready");
    gmb_tui_control_focus(0);
    CHECK(gmb_tui_control_count() == 2, "2 controlli registrati");
    CHECK(gmb_tui_control_focused() == 0, "il CodeArea e' a fuoco");

    /* PONTE INPUT: tasti nella fifo SDL -> gmb_tui_sdl_pump li porta ai controlli. */
    gmb_gfx_input_push_key("A");
    gmb_gfx_input_push_key("B");
    gmb_tui_sdl_pump();
    gmb_tui_controls_frame(0);

    { char out[256]; gmb_tui_control_text(0, out, (int)sizeof(out));
      CHECK(strncmp(out, "ABHELLO", 7) == 0, "i tasti dal ponte SDL arrivano al CodeArea");
      if (strncmp(out, "ABHELLO", 7) != 0) printf("     (CodeArea = \"%s\")\n", out); }

    /* Backspace (GMB_K_BS = 127): cancella il carattere a sinistra del cursore. */
    gmb_gfx_input_push_key("\x7f");
    gmb_tui_sdl_pump();
    gmb_tui_controls_frame(0);
    { char out[256]; gmb_tui_control_text(0, out, (int)sizeof(out));
      CHECK(strncmp(out, "AHELLO", 6) == 0, "Backspace cancella a sinistra (127)");
      if (strncmp(out, "AHELLO", 6) != 0) printf("     (CodeArea = \"%s\")\n", out); }

    gmb_tui_sdl_present();
    CHECK(1, "present headless senza crash");

    /* DIAGNOSTICA COLORI: bg/fg della cella dentro il CodeArea (atteso grigio 170, non blu). */
    {
        gmb_screen_buffer_t* sb2 = gmb_clipper_global_state()->buffer;
        int W2 = gmb_screen_width(sb2);
        gmb_screen_cell_t* cc = gmb_screen_cells(sb2);
        gmb_screen_cell_t cel = cc[3 * W2 + 5];
        printf("[col] codearea cell='%c' bg=(%d,%d,%d) fg=(%d,%d,%d) bgpal=%d fgpal=%d\n",
               cel.character ? (char)cel.character : ' ', cel.bg_rgb.r, cel.bg_rgb.g, cel.bg_rgb.b,
               cel.fg_rgb.r, cel.fg_rgb.g, cel.fg_rgb.b, cel.bg_palette, cel.fg_palette);
    }

    { int r = 0, cc = 0; CHECK(gmb_tui_control_cursor(0, &r, &cc) == 1 && r >= 0 && cc >= 0,
                               "posizione cursore del CodeArea disponibile per il caret"); }

    /* --- CURSORE + COPERTURA: layout ESATTO dell'app GMBasicIde (120x40) --- */
    gmb_tui_controls_reset();
    gmb_tui_add_tab(0, 1, 120, "untitled");
    gmb_tui_add_codearea(0, 2, 84, 37, "");
    gmb_tui_control_set_text(1, "SUB Main()\n    DIM x AS LONG\n    PRINT x\nEND SUB");
    gmb_tui_add_listbox(85, 2, 34, 37, "");
    gmb_tui_add_statusbar(0, 39, 120, "ready");
    gmb_tui_add_menu(0, 0, 120, "");
    { int mi = gmb_tui_control_count() - 1;   /* 4 = menu, come l'app */
      gmb_tui_menu_set(mi, "File:Apri|Salva|Esci;Edit:Copia|Taglia|Incolla;Search:Trova|Sostituisci;View:Outline|Lint|Refs;Build:Esegui|Compila");
      gmb_tui_control_focus(1);               /* focus sul CodeArea (come RegisterUI) */
      { int open = 0;
        for (int sel = 0; sel < 5; sel++)
        {
            gmb_tui_control_focus(mi);
            /* apri il menu: DOWN apre; RIGHT sposta la voce selezionata `sel` volte. */
            gmb_gfx_input_push_key("\x18");
            for (int k = 0; k < sel; k++) gmb_gfx_input_push_key("\x04");  /* GMB_K_RIGHT=4 */
            gmb_tui_sdl_pump();
            gmb_tui_controls_frame(0);
            gmb_tui_control_focus(1);
            { int r = 0, c = 0; gmb_tui_control_cursor(1, &r, &c);
              open = gmb_tui_menu_any_open();
              printf("[cov] sel=%d open=%d caret=(r=%d,c=%d) covers=%d covered=%d\n",
                     sel, open, r, c, gmb_tui_menu_covers(c, r), gmb_tui_cursor_covered()); }
            gmb_tui_sdl_present();
            printf("[cov]   sel=%d -> vis=%d\n", sel, gmb_tui_sdl_cursor_visible());
            gmb_gfx_input_push_key("\x1b");    /* ESC: chiude il menu per il giro successivo */
            gmb_tui_sdl_pump(); gmb_tui_controls_frame(0);
        }
        CHECK(1, "layout app: copertura valutata per ogni menu");
        /* Chiuso il menu: il cursore deve RICOMPARIRE (non coperto + fase blink accesa). */
        gmb_tui_control_focus(1);
        gmb_tui_sdl_cursor_solid();      /* ancora il blink a ORA -> acceso */
        gmb_tui_sdl_present();
        printf("[cov] menu chiuso: covered=%d vis=%d\n",
               gmb_tui_cursor_covered(), gmb_tui_sdl_cursor_visible());
        CHECK(gmb_tui_cursor_covered() == 0, "menu chiuso: nessuna copertura");
        CHECK(gmb_tui_sdl_cursor_visible() == 1, "menu chiuso: il cursore RICOMPARE");
      }
    }

    /* --- LOOP REALE dell'app + apertura menu col MOUSE (il caso che l'utente vede) --- */
    {
        extern void gmb_tui_handle_mouse_cell(int x, int y, int kind);
        gmb_tui_controls_reset();
        gmb_tui_add_codearea(0, 2, 84, 36, "SUB Main()\n    DIM x AS LONG\nEND SUB");
        gmb_tui_add_menu(0, 0, 120, "");
        { int mi = gmb_tui_control_count() - 1;
          gmb_tui_menu_set(mi, "File:Apri|Salva|Esci;Edit:Copia|Taglia|Incolla;Search:Trova|Sostituisci;View:Outline|Lint|Refs;Build:Esegui|Compila");
          gmb_tui_control_focus(0);
          /* Apertura col MOUSE: click (DOWN) sulla voce "File" della barra (riga 0). */
          gmb_tui_handle_mouse_cell(1, 0, 1);
          printf("[loop] dopo click su menu: any_open=%d covered=%d\n",
                 gmb_tui_menu_any_open(), gmb_tui_cursor_covered());
          int stuck_off = 0, stuck_on = 0, off = 0, on = 0;
          for (int it = 0; it < 40; it++)   /* loop come l'app: pump -> frame(16) -> present */
          {
              gmb_tui_sdl_pump();
              gmb_tui_controls_frame(16);
              gmb_tui_sdl_present();
              if (gmb_tui_sdl_cursor_visible()) on++; else off++;
          }
          printf("[loop] menu APERTO su 40 frame: vis on=%d off=%d\n", on, off);
          CHECK(off == 40, "loop con menu aperto: cursore SEMPRE spento (40/40)");
          /* Chiudi il menu (click fuori) e verifica che il cursore RICOMPAIA. */
          gmb_tui_handle_mouse_cell(60, 20, 1);
          on = off = 0;
          for (int it = 0; it < 40; it++)
          {
              gmb_tui_sdl_pump();
              gmb_tui_controls_frame(16);
              gmb_tui_sdl_present();
              if (gmb_tui_sdl_cursor_visible()) on++; else off++;
          }
          printf("[loop] menu CHIUSO su 40 frame: vis on=%d off=%d any_open=%d\n",
                 on, off, gmb_tui_menu_any_open());
          CHECK(on > 0, "loop con menu chiuso: il cursore RICOMPARE (blink acceso)");
        }
    }

    /* --- TRACCIAMENTO: la posizione del cursore console DEVE seguire il caret reale --- */
    gmb_tui_controls_reset();
    gmb_tui_add_codearea(0, 1, 40, 10, "LINE1\nLINE2\nLINE3");
    gmb_tui_control_focus(0);
    for (int step = 0; step < 5; step++)
    {
        int r = 0, c = 0; gmb_tui_control_cursor(0, &r, &c);
        gmb_tui_sdl_cursor_solid();
        gmb_tui_sdl_present();
        int cr = gmb_tui_sdl_cursor_row(), cc = gmb_tui_sdl_cursor_col();
        printf("[track] step=%d caret=(r=%d,c=%d) console=(r=%d,c=%d) vis=%d\n",
               step, r, c, cr, cc, gmb_tui_sdl_cursor_visible());
        CHECK(cr == r && cc == c, "cursore console ALLINEATO al caret del CodeArea");
        /* muovi il caret: GMB_K_DOWN=24 (riga giu'), GMB_K_RIGHT=4 (carattere a destra). */
        gmb_gfx_input_push_key("\x18");
        gmb_gfx_input_push_key("\x04");
        gmb_gfx_input_push_key("\x04");
        gmb_tui_sdl_pump();
        gmb_tui_controls_frame(0);
    }

    /* --- ALERTBOX a CELLE nella SDLConsole (via 2) --- */
    gmb_tui_controls_reset();
    gmb_tui_add_alertbox(5, 4, 30, 6, "ciao");
    gmb_tui_controls_frame(0);
    { gmb_screen_buffer_t* sb = gmb_clipper_global_state()->buffer; int W = gmb_screen_width(sb);
      gmb_screen_cell_t* cc = gmb_screen_cells(sb);
      gmb_screen_cell_t tl = cc[4 * W + 5];
      printf("[alert] cell(5,4)='%c' (0x%04x) atteso 0x250c (angolo single: default AlertBox)\n",
             tl.character ? (char)tl.character : '?', (unsigned)tl.character);
      CHECK(tl.character == 0x250c, "alertbox: disegno a CELLE nella SDLConsole (via 2)"); }

    gmb_tui_sdl_stop();
    CHECK(gmb_tui_sdl_active() == 0, "stop: console chiusa");

    if (fails == 0) { printf("gmb_sdl_console_selfcheck: OK\n"); return 0; }
    printf("gmb_sdl_console_selfcheck: %d FAIL\n", fails);
    return 1;
}
