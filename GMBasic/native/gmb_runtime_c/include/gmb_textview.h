/* ============================================================================
 * gmb_textview.h — VISTA TESTUALE VIRTUALE (Markdown / diff / lista grande).
 *
 * Un controllo LISTBOX può mostrare un testo invece di voci fisse: le righe
 * vengono impaginate una volta e il disegno tocca SOLO quelle visibili, così un
 * documento o un log da centinaia di migliaia di righe non costa né memoria per
 * voce né tempo di layout a ogni frame.
 *
 * Si abilita dallo STYLE (dati, nessun TokenKind nuovo):
 *
 *     LISTBOX 20, 6, 70, 18, "" NAME "doc" STYLE "mode=markdown"
 *     CONTROL SET "doc", "text", testo$
 *
 *     LISTBOX 20, 6, 70, 18, "" NAME "dif" STYLE "mode=diff"
 *     CONTROL SET "dif", "text", diff$
 *
 *     LISTBOX 20, 6, 70, 18, "" NAME "log" STYLE "virtual=1"   ' lista grande
 *     CONTROL SET "log", "text", righe$
 *
 * `mode=plain`/`virtual=1` = una riga per ogni '\n'; `mode=diff` = diff unificato
 * con colori per riga; `mode=markdown` = Markdown impaginato (titoli, liste,
 * citazioni, codice, tabelle), usando l'AST canonico di gmb_md.
 *
 * Questo modulo è PURO (nessuna dipendenza da UI/TUI): produce righe + uno stile
 * simbolico per riga. I colori li decide il disegno, che conosce il tema.
 * ============================================================================ */
#ifndef GMB_TEXTVIEW_H
#define GMB_TEXTVIEW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_TV_PLAIN    0   /* una riga per '\n', nessun impaginato */
#define GMB_TV_DIFF     1   /* diff unificato, stile per riga */
#define GMB_TV_MARKDOWN 2   /* Markdown reso in righe */

/* Stile SIMBOLICO di una riga: il colore lo deriva il disegno dal tema. */
enum {
    GMB_TV_S_NORMAL = 0,
    GMB_TV_S_HEAD1,     /* titolo di livello 1 */
    GMB_TV_S_HEAD2,     /* titolo di livello 2 */
    GMB_TV_S_HEAD3,     /* titoli 3..6 */
    GMB_TV_S_CODE,      /* blocco di codice / tabella */
    GMB_TV_S_QUOTE,     /* citazione */
    GMB_TV_S_RULE,      /* riga orizzontale / separatore di tabella */
    GMB_TV_S_ADD,       /* diff: riga aggiunta */
    GMB_TV_S_DEL,       /* diff: riga rimossa */
    GMB_TV_S_HUNK,      /* diff: @@ … @@ */
    GMB_TV_S_META       /* diff: intestazione (diff/index/---/+++) */
};

typedef struct gmb_textview gmb_textview_t;   /* opaca */

/* Nome del modo → costante; -1 se sconosciuto ("plain"/"text"/"list",
 * "diff", "markdown"/"md"). */
int gmb_textview_parse_mode(const char* name);
const char* gmb_textview_mode_name(int mode);

/* Legge `mode=<nome>` / `virtual=1` da una spec STYLE e ritorna il modo da usare
 * (`current` se la spec non ne parla o il nome è sconosciuto). */
int gmb_textview_style_mode(const char* spec, int current);

/* Crea la vista da `src` (copiato) e la impagina a `width` colonne.
 * Ritorna NULL solo se manca la memoria. */
gmb_textview_t* gmb_textview_new(const char* src, int mode, int width);
void gmb_textview_free(gmb_textview_t* tv);

/* Reimpagina alla nuova larghezza. Per plain/diff è un no-op (nessun wrapping).
 * Ritorna il numero di righe. */
int gmb_textview_layout(gmb_textview_t* tv, int width);

int  gmb_textview_rows(const gmb_textview_t* tv);
int  gmb_textview_mode(const gmb_textview_t* tv);
int  gmb_textview_width(const gmb_textview_t* tv);

/* Riga `idx` (NUL-terminata, senza '\n') e suo stile; "" / NORMAL fuori range. */
const char* gmb_textview_row(const gmb_textview_t* tv, int idx);
int         gmb_textview_row_style(const gmb_textview_t* tv, int idx);

/* Sorgente ORIGINALE (mai NULL dopo new). Per il Markdown è il testo di
 * partenza (serve al re-layout); in plain/diff restituisce il testo corrente
 * con i '\n' ripristinati, così un cambio di `mode=` dallo STYLE non perde i
 * dati. Il puntatore vale fino alla prossima modifica della vista. */
const char* gmb_textview_source_text(gmb_textview_t* tv);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TEXTVIEW_H */
