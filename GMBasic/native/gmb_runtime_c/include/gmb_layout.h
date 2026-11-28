/* ============================================================================
 * gmb_layout.h — LAYOUT MANAGER dei controlli (condiviso VM/C + C99Native).
 *
 * Le clausole BASIC `EXPAND`, `ANCHOR`, `ALIGN` sono PROPRIETÀ del controllo,
 * nella stessa posizione sintattica di NAME/TAG/STYLE. Il parser le fonde nella
 * spec CSS-like (`expand=… anchor=… align=…`) e il runtime le applica qui.
 *
 * Modello (deliberatamente semplice e simmetrico):
 *  - alla creazione si memorizza il RECT DICHIARATO e la dimensione della
 *    finestra in quel momento (base);
 *  - al resize il rect viene ricalcolato in base al DELTA della finestra
 *    rispetto alla base: la stessa formula funziona in crescita e in riduzione;
 *  - ANCHOR = quali bordi restano agganciati al contenitore. Se sono agganciati
 *    entrambi i bordi di un asse il controllo si ALLARGA di quel delta; se ne è
 *    agganciato uno solo si sposta; se nessuno, decide ALIGN;
 *  - EXPAND è una scorciatoia: EXPAND H = LEFT+RIGHT, EXPAND V = TOP+BOTTOM,
 *    EXPAND BOTH = tutti e quattro, EXPAND NONE = nessuno. ANCHOR esplicito ha
 *    la precedenza sull'asse che specifica;
 *  - ALIGN (start/center/end) sceglie dove sta il controllo quando su quell'asse
 *    non è agganciato a un bordo;
 *  - MINW/MINH/MAXW/MAXH limitano il rect calcolato: il controllo non si stringe
 *    sotto un minimo leggibile né si allarga oltre un tetto (0 = nessun limite);
 *  - GROW (percento, default 100) è la QUOTA del delta che il controllo assorbe:
 *    due pannelli affiancati che crescono entrambi "tutto" si sovrappongono,
 *    con GROW=50 ognuno prende metà della crescita. Il residuo resta al
 *    contenitore, quindi la somma delle quote non deve fare 100.
 *
 * I limiti e la quota sono PROPRIETÀ del controllo come EXPAND/ANCHOR/ALIGN:
 * arrivano dalla stessa spec (`STYLE "expand=h grow=50 minw=240"`), quindi non
 * servono né nuove clausole BASIC né nuovi syscall.
 *
 * Unità: la stessa del rect del controllo (pixel nel percorso GRAPHICS, celle
 * nel percorso TEXT). Nessuna conversione qui.
 * ============================================================================ */
#ifndef GMB_LAYOUT_H
#define GMB_LAYOUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ANCHOR: bit dei bordi agganciati. */
#define GMB_LAYOUT_LEFT    1
#define GMB_LAYOUT_RIGHT   2
#define GMB_LAYOUT_TOP     4
#define GMB_LAYOUT_BOTTOM  8
#define GMB_LAYOUT_ALL     15

/* EXPAND: assi che crescono con la finestra. */
#define GMB_LAYOUT_EXPAND_NONE 0
#define GMB_LAYOUT_EXPAND_H    1
#define GMB_LAYOUT_EXPAND_V    2
#define GMB_LAYOUT_EXPAND_BOTH 3

/* ALIGN: posizione sull'asse non agganciato. */
#define GMB_LAYOUT_ALIGN_START  0   /* left / top */
#define GMB_LAYOUT_ALIGN_CENTER 1   /* center / middle */
#define GMB_LAYOUT_ALIGN_END    2   /* right / bottom */

/* Clausole lette dalla spec; -1 = non specificata (non tocca la configurazione
 * corrente, così più passaggi di proprietà sullo stesso controllo si sommano). */
typedef struct gmb_layout_spec {
    int expand;    /* GMB_LAYOUT_EXPAND_* oppure -1 */
    int anchor;    /* bitmask GMB_LAYOUT_* oppure -1 */
    int align_h;   /* GMB_LAYOUT_ALIGN_* oppure -1 */
    int align_v;   /* GMB_LAYOUT_ALIGN_* oppure -1 */
    int min_w, min_h;   /* dimensione minima, -1 = non specificata (0 = nessun limite) */
    int max_w, max_h;   /* dimensione massima, -1 = non specificata (0 = nessun limite) */
    int grow;           /* percento del delta assorbito, -1 = non specificato (100) */
} gmb_layout_spec_t;

struct gmb_control;   /* gmb_tui_widgets.h definisce la struct completa */

/* Legge expand=/anchor=/align= dalla spec. Ritorna 1 se almeno una chiave è
 * presente, 0 altrimenti. Non modifica `out` se non trova nulla. */
int gmb_layout_parse(const char* spec, gmb_layout_spec_t* out);

/* Configura il controllo: memorizza il rect dichiarato e la finestra base alla
 * PRIMA chiamata, poi aggiorna solo i campi specificati in `ls`.
 * `is_tui` 0 = pixel/graphics, 1 = celle/text. `win_w`/`win_h` sono le
 * dimensioni correnti del contenitore nell'unità del controllo. */
void gmb_layout_configure(struct gmb_control* c, int is_tui,
                          const gmb_layout_spec_t* ls, int win_w, int win_h);

/* Ricalcola x/y/width/height dal rect base e dal delta della finestra.
 * Ritorna 1 se qualcosa è cambiato, 0 altrimenti (incluso: layout non attivo). */
int gmb_layout_reflow(struct gmb_control* c, int is_tui, int win_w, int win_h);

/* 1 se il controllo partecipa al layout (EXPAND/ANCHOR/ALIGN dichiarato). */
int gmb_layout_active(const struct gmb_control* c);

#ifdef __cplusplus
}
#endif
#endif /* GMB_LAYOUT_H */
