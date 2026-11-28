/* ============================================================================
 * gmb_ui_palette.h — COMMAND PALETTE (overlay di comandi cercabili).
 *
 * Una palette stile editor: il programma registra delle AZIONI (id, titolo,
 * scorciatoia, categoria) e l'utente apre l'overlay, scrive qualche lettera e
 * sceglie. Non è un controllo BASIC: come l'inspector è un overlay del frame,
 * non entra nei registri, non prende il fuoco dei controlli e non genera eventi.
 *
 * L'ESECUZIONE di un'azione è l'unico punto che tocca il programma, e passa dal
 * GPM: `gmb_palette_pump()` (chiamata dal frame) accoda l'azione scelta e la
 * esegue con `gmb_task_spawn` — mai inline nel thread che ha letto il tasto.
 * Senza scheduler attivo (programma lineare) l'azione viene eseguita sul posto
 * subito dopo il frame: è comunque il programma, non un thread esterno.
 *
 *     gmb_palette_register("file.save", "Salva il documento", "Ctrl+S", "File");
 *     gmb_palette_open();                      ' Ctrl+K
 *     gmb_palette_set_runner(fn, ctx);         ' chi esegue le azioni
 *     gmb_palette_key(key);                    ' dentro il loop di input
 *     gmb_palette_pump();                      ' a ogni frame
 *
 * Il FILTRO è deterministico e verificabile headless: `gmb_palette_results()`
 * dà l'elenco filtrato in ordine di pertinenza (prefisso sul titolo, poi
 * parole del titolo, poi scorciatoia/categoria).
 * ============================================================================ */
#ifndef GMB_UI_PALETTE_H
#define GMB_UI_PALETTE_H

#include "gmb_tui.h"   /* gmb_screen_buffer_t */

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_PALETTE_MAX       64
#define GMB_PALETTE_ROW_MAX   96   /* caratteri di una riga dell'elenco */
#define GMB_PALETTE_QUERY_MAX 64
#define GMB_PALETTE_MAXROWS   12

/* Registro delle azioni (una per id; registrare di nuovo lo stesso id lo aggiorna). */
int  gmb_palette_register(const char* id, const char* title,
                          const char* shortcut, const char* category);
int  gmb_palette_count(void);
void gmb_palette_clear(void);

/* Varianti con argomenti opzionali (le usa l'emitter C per `PALETTE ADD` a 2/3 campi). */
int  gmb_palette_register3(const char* id, const char* title, const char* shortcut);
int  gmb_palette_register4(const char* id, const char* title, const char* shortcut, const char* category);

/* Overlay aperto/chiuso. `open` azzera la query e la selezione. */
void gmb_palette_open(void);
void gmb_palette_close(void);
int  gmb_palette_is_open(void);

/* Testo di ricerca: append (un carattere o una stringa), backspace, reset. */
void gmb_palette_type(const char* text);
void gmb_palette_backspace(void);
void gmb_palette_set_query(const char* text);
const char* gmb_palette_query(void);

/* Sposta la selezione nell'elenco filtrato (posizioni negative fanno il giro). */
void gmb_palette_move(int delta);
int  gmb_palette_selected(void);
void gmb_palette_set_selected(int idx);

/* Tasto: 1 se consumato dalla palette (ESC, frecce, Enter, Backspace, caratteri).
 * 0 se il tasto non è della palette (overlay chiuso). Enter esegue la selezione. */
int  gmb_palette_key(int key);

/* Elenco filtrato: indice dell'azione in `out` (fino a `cap`), ritorna quante.
 * Ordine: titolo che inizia con la query, poi contenga la query, poi categoria/
 * scorciatoia; a pari merito l'ordine di registrazione. Query vuota = tutte. */
int  gmb_palette_results(int* out, int cap);

/* Esecutore delle azioni: chiamato (dal GPM) con l'azione scelta. */
typedef void (*gmb_palette_runner_t)(const char* id, void* ctx);
void gmb_palette_set_runner(gmb_palette_runner_t fn, void* ctx);

/* Overlay abilitato dal programma (emesso dal driver per `PALETTE …`): il frame live
 * disegna la palette e pompa le azioni. 0 = disabilitato (default: headless/test). */
void gmb_ui_palette_wire(int on);
int  gmb_ui_palette_wired(void);

/* Esegue l'azione selezionata. Se lo scheduler GPM è attivo la esegue come TASK
 * (e ritorna subito); altrimenti la accoda in modo che `pump` la esegua subito. */
void gmb_palette_run_selected(void);

/* Chiamata dal frame: esegue un'azione accodata (fuori dal percorso del tasto). */
void gmb_palette_pump(void);

/* Testo dell'overlay (righe separate da '\n'), anche senza disegnare: è la parte
 * verificabile in modo deterministico da un selfcheck. */
int  gmb_palette_lines(char* out, int cap);

/* Disegno: TUI a celle o percorso grafico (pixel). */
void gmb_palette_draw_cells(gmb_screen_buffer_t* b);
void gmb_palette_draw_pixels(void);

#ifdef __cplusplus
}
#endif
#endif /* GMB_UI_PALETTE_H */