/* ============================================================================
 * gmb_a11y.h — ACCESSIBILITA' GLOBALE dell'interfaccia (condivisa da TUI e
 * percorso GRAPHICS/PIXEL, VM/C + C99Native).
 *
 * Quattro flag di interfaccia, non di programma: si impostano dal registro
 * opzioni (`AI SET OPTION "ui.high_contrast", "1"`) o dalla sezione `engine`
 * dei settings, e il runtime li legge a OGNI disegno — quindi cambiano anche a
 * runtime, senza ricreare i controlli.
 *
 *   ui.high_contrast          testo AAA (7:1) e bordi 4.5:1 su ruoli e tema
 *   ui.text_scale             scala del testo (0.75..3, default 1)
 *   ui.reduce_motion          nessun blink del cursore, nessun ridisegno periodico
 *   ui.focus_always_visible   il controllo a fuoco ha sempre un anello leggibile
 *
 * I flag NON sono variabili d'ambiente: passano dal registro dei parametri come
 * ogni altra opzione operativa (AGENTS §"VARIABILI D'AMBIENTE VIETATE").
 * ============================================================================ */
#ifndef GMB_A11Y_H
#define GMB_A11Y_H

#ifdef __cplusplus
extern "C" {
#endif

/* Nomi canonici delle voci del registro opzioni (`AI LIST OPTIONS`). */
#define GMB_A11Y_OPT_HIGH_CONTRAST "ui.high_contrast"
#define GMB_A11Y_OPT_TEXT_SCALE    "ui.text_scale"
#define GMB_A11Y_OPT_REDUCE_MOTION "ui.reduce_motion"
#define GMB_A11Y_OPT_FOCUS_VISIBLE "ui.focus_always_visible"

typedef struct gmb_a11y {
    int high_contrast;         /* 1 = contrasto massimo */
    int reduce_motion;         /* 1 = riduci il movimento */
    int focus_always_visible;  /* 1 = anello di focus sempre distinguibile */
    double text_scale;         /* 0.75 .. 3.0, default 1.0 */
} gmb_a11y_t;

/* Snapshot dei flag attivi. Rilegge il registro quando una voce è cambiata
 * (quindi `AI SET OPTION` ha effetto immediato e una voce mai impostata vale
 * il default). Nessuna allocazione, nessuno stato per-controllo. */
gmb_a11y_t gmb_a11y_get(void);

/* 1 = il movimento è ammesso (flag `ui.reduce_motion` spento). */
int gmb_a11y_motion(void);

/* Applica `ui.text_scale` a una dimensione in pixel (o in celle: è un fattore).
 * `base <= 0` significa "automatico/ereditato" e resta INVARIATO (0 non è una
 * dimensione da scalare); il risultato è sempre almeno 1. */
int gmb_a11y_scale_px(int base);

/* Rapporti di contrasto minimi imposti dal flag `ui.high_contrast`:
 * testo 4.5 (AA) -> 7.0 (AAA), bordi 3.0 -> 4.5. Usati dai ruoli del tema e
 * dalla cascata dello stile, così il flag vale in un punto solo. */
double gmb_a11y_min_text_ratio(void);
double gmb_a11y_min_border_ratio(void);

/* Parse condivisi (case-insensitive), esposti perché li usano anche il registro
 * e i test: booleano con alias (1/on/true/yes/si/attivo, 0/off/false/no/spento);
 * scala con default 1.0 e clamp 0.75..3.0 (0, negativo o non numerico = 1.0). */
int    gmb_a11y_parse_bool(const char* value);
double gmb_a11y_parse_scale(const char* value);

#ifdef __cplusplus
}
#endif
#endif /* GMB_A11Y_H */
