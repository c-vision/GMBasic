/* gmb_print_using.h — formattatore di PRINT USING (riferimento: GW-BASIC/Engine1999).
 *
 * Il motore sta tutto nel runtime condiviso (§0bis): la VM/C e il C99Native chiamano questa
 * funzione, nessun backend reimplementa i campi. `FORMAT`/.NET resta l'altro formattatore
 * (`gmb_format.c`): qui c'è la sintassi a CAMPI di PRINT USING, che è cosa diversa.
 *
 * SIN TASSI (tutto il resto è testo letterale):
 *   ###      posizioni di cifra; quante sono, tante cifre intere al massimo
 *   .##      cifre decimali (arrotondamento all'ultima)
 *   ,        raggruppa le migliaia nell'intero quando compare fra le cifre (`##,###`)
 *   +        segno SEMPRE, all'inizio del campo (`+###`) o in fondo (`###+`)
 *   -        in fondo (`###-`): meno per i negativi, spazio per i positivi
 *            (con `+`/`-` in fondo il segno è stampato DOPO le cifre e non ripetuto davanti:
 *             -5 con `###-` → "  5-")
 *   $$       dollaro "flottante" subito prima della prima cifra (`$$###.##`); un `$` singolo
 *            è testo letterale
 *   **       riempimento con asterischi (`**$###`); da solo o col dollaro
 *   &        l'intera stringa
 *   !        primo carattere
 *   \ ... \  larghezza fissa = (spazi fra le barre) + 2, troncando o riempiendo di spazi
 *   _        il carattere successivo è letterale (`_#` stampa `#`)
 *
 * SEMANTICA (contratto, verificato da tests/gmb_print_using_selfcheck.c):
 *  - un valore consuma il campo successivo DEL SUO TIPO: numero → campo numerico, stringa →
 *    campo stringa. Incontrando un campo dell'altro tipo si ha ERRORE (`-1`), non una stampa
 *    silenziosamente sbagliata: il chiamante stampa il messaggio di `gmb_print_using_error()`;
 *  - il testo letterale fra i campi viene copiato così com'è;
 *  - quando i campi finiscono ma restano valori, si va a capo e il formato RIPARTE dall'inizio
 *    (è la ripetizione per riga del riferimento); una seconda ripartenza senza campi del tipo
 *    richiesto è errore;
 *  - esauriti i valori si copia la coda letterale del formato e ci si ferma al primo campo
 *    rimasto (che non ha valore): niente campi "riempiti" a caso;
 *  - numeri: il campo ha la larghezza dei caratteri che occupa nel formato; il contenuto è
 *    allineato a destra, con spazio al posto del segno per i positivi (salvo `+`), riempimento
 *    `**` o spazi; se le cifre non entrano il campo stampa `%` davanti al numero completo
 *    (overflow DICHIARATO, non troncamento silenzioso);
 *  - `^^^^` (notazione scientifica) NON è implementato: errore esplicito, si usa `FORMAT` per
 *    l'esponenziale. Nessuno stub: un campo non riconosciuto è errore, non testo.
 *
 * `gmb_print_using_format` scrive in `out` (cap byte, terminazione inclusa) e ritorna la
 * lunghezza scritta oppure -1 (buffer troppo piccolo, formato non valido, tipo incoerente,
 * coda senza campo). `gmb_print_using_error()` dà il motivo dell'ultimo errore ("" se nessuno).
 */
#ifndef GMB_PRINT_USING_H
#define GMB_PRINT_USING_H

/* gmb_print_using_stmt lavora sui VALORI già tipizzati del runtime (gmb_val_t). */
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int is_string;   /* 0 = numero (`num`), 1 = stringa (`str`) */
    double num;
    const char* str;
} gmb_using_value_t;

int gmb_print_using_format(const char* format, const gmb_using_value_t* values, int n,
                           char* out, int cap);
const char* gmb_print_using_error(void);

/* ---- innesto nel linguaggio: `PRINT USING <fmt>; v1; v2 …` ----
 * Unico ingresso per i backend (emesso dall'emitter C99 come una chiamata): prende il
 * formato e i valori già valutati come `gmb_val_t` — la distinzione stringa/numero la fa
 * il VALORE (kind), non una tabella di tipi duplicata per gamba — formatta e restituisce in
 * `*out` una stringa POSseduta dal chiamante (da liberare con gmb_string_free).
 * 0 = fatto; -1 = errore di formato (motivo in gmb_print_using_error()), nessuna stampa
 * parziale. */
int gmb_print_using_stmt(gmb_string_t fmt, const gmb_val_t* vals, int n, gmb_string_t* out);

#ifdef __cplusplus
}
#endif

#endif /* GMB_PRINT_USING_H */
