/* ============================================================================
 * gmb_anim.h — MOTORE DI ANIMAZIONE tick-driven (condiviso VM/C + C99Native).
 *
 * Non è un secondo esecutore: non crea thread e non dorme. Le animazioni sono
 * SLOT di stato (da → a, durata, easing) avanzati dal FRAME con il tempo
 * trascorso (`gmb_anim_tick`, che legge `gmb_ticks_ms`). Chi disegna applica i
 * valori correnti ai controlli (TUI e percorso PIXEL) e il present avviene solo
 * finché qualcosa si muove. Rispetta la GPM: nessun thread esterno, nessuna
 * attesa bloccante; il programma non viene mai "svegliato" da un timer.
 *
 * `ui.reduce_motion` (gmb_a11y.h) è l'interruttore: quando è attivo le
 * animazioni NON partono e il valore va direttamente al target (snap).
 *
 * Superficie BASIC: proprietà `anim=` di CONTROL SET, es.
 *   CONTROL SET "pannello", "anim", 0, "left:300:400:out"
 *   CONTROL SET "pannello", "anim", 0, "width:80:250"      (ms 250, linear)
 * Campi: proprietà:valore_finale:durata_ms[:easing].
 * Proprietà: left/x, top/y, width/w, height/h.
 * Easing: linear (default), out, inout.
 * ============================================================================ */
#ifndef GMB_ANIM_H
#define GMB_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GMB_ANIM_LEFT = 0,
    GMB_ANIM_TOP,
    GMB_ANIM_WIDTH,
    GMB_ANIM_HEIGHT
};

enum {
    GMB_ANIM_EASE_LINEAR = 0,
    GMB_ANIM_EASE_OUT,
    GMB_ANIM_EASE_IN_OUT
};

#define GMB_ANIM_MAX 64

/* Nome proprietà → GMB_ANIM_*, -1 se sconosciuta. */
int gmb_anim_prop_from_name(const char* name);

/* Legge "prop:valore:ms[:easing]"; ritorna 1 se valida.
 * `ms <= 0` è ammesso (snap immediato, gestito dal chiamante). */
int gmb_anim_parse_spec(const char* spec, int* prop, double* to, int* ms, int* easing);

/* Avvia un'animazione. Ritorna l'indice dello slot (>=0) oppure -1 se pieno /
 * argomenti non validi. `from` è il valore corrente della proprietà. */
int gmb_anim_start(int handle, int is_tui, int prop, double from, double to, int ms, int easing);

/* Avanza di `dt_ms` (deterministico: usato dai selfcheck). Ritorna il numero di
 * animazioni ancora attive (incluse quelle appena concluse in questo passo). */
int gmb_anim_advance(int dt_ms);

/* Avanza col tempo di parete trascorso dall'ultima chiamata (frame). */
void gmb_anim_tick(void);

/* 1 se c'è almeno un'animazione in corso (o da applicare in questo frame). */
int gmb_anim_active(void);
int gmb_anim_count(void);
/* 1 se QUEL controllo ha almeno un'animazione attiva (per l'inspector). */
int gmb_anim_active_for(int handle, int is_tui);

/* Lettura di uno slot per chi applica i valori ai controlli.
 * Ritorna 1 se lo slot è in uso. `done` = 1 nell'ultimo frame (valore == to). */
int gmb_anim_get(int index, int* handle, int* is_tui, int* prop, int* done, double* value);

/* Dimentica tutte le animazioni (reset programma/nuova pagina). */
void gmb_anim_clear(void);

/* Dimentica le animazioni di un controllo (controllo distrutto o rinominato). */
void gmb_anim_forget(int handle, int is_tui);

#ifdef __cplusplus
}
#endif
#endif /* GMB_ANIM_H */
