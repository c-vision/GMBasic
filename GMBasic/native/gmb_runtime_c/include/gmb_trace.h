/* gmb_trace.h — STACK TRACE BASIC del runtime C (gamba C99Native e VM/C).
 *
 * Perché: `gmb_raise_error` sapeva solo la RIGA dell'errore. Con un errore in profondità
 * (A → B → C) il messaggio diceva "line 42" senza dire *quale* procedura e da chi era stata
 * chiamata. Qui si tiene una catena di frame (procedura + riga corrente) e la si rende come
 * testo, con la stessa forma su tutte le gambe:
 *
 *     [GMBASIC] ERROR 11: Division by zero (line 42)
 *       in C (riga 42)
 *       in B (riga 17)
 *       in Main (riga 8)
 *
 * COSTO. Nessuna allocazione per frame: gli entry vivono in un array thread-local e il
 * frame è solo un indice (`__attribute__((cleanup(...)))` emesso dall'emitter C99). La
 * riga si aggiorna con uno store (gmb_trace_line è `static inline`, nessuna chiamata).
 * La VM/C non instrumenta nulla: ricava la catena dai propri frame di chiamata.
 *
 * LONGJMP. `gmb_raise_error` fa longjmp: i `cleanup` NON corrono e la profondità resta
 * "alta" (frame morti). Non è un crash (l'array è statico, non punta a stack morto), ma
 * dopo un errore catturato la catena va RIPRISTINATA al frame della procedura che ha
 * catturato: lo fa `gmb_trace_restore_frame(&__gmb_tf)` (emesso all'ingresso del CATCH e
 * alla ripresa di ON ERROR RESUME NEXT). Il testo del trace si cattura PRIMA del salto
 * (gmb_trace_snapshot), quando i frame sono ancora tutti validi.
 */
#ifndef GMB_TRACE_H
#define GMB_TRACE_H

/* Un entry della catena: nome BASIC della procedura + riga corrente. */
typedef struct gmb_trace_ent {
    const char* fn;
    long line;
} gmb_trace_ent_t;

#define GMB_TRACE_MAX 64

/* Stato thread-local (stesso canone di g_err_number/g_err_jmp in gmb_core.c: lo stato
 * d'errore appartiene al thread che esegue la procedura BASIC). */
extern __thread gmb_trace_ent_t gmb_trace_stack[GMB_TRACE_MAX];
extern __thread int gmb_trace_n;

/* Handle di un frame: indice nell'array. */
typedef struct gmb_trace_frame {
    int idx;
} gmb_trace_frame_t;

/* Entra in una procedura: ritorna il frame (da passare a gmb_trace_leave). */
gmb_trace_frame_t gmb_trace_enter(const char* fn, long line);
/* Esce dalla procedura (usato come `cleanup`): ripristina la profondità del chiamante. */
void gmb_trace_leave(gmb_trace_frame_t* f);
/* Riporta la catena a `f` (incluso): usato DOPO un longjmp, quando i frame più interni
 * sono morti e i loro `cleanup` non sono passati. */
void gmb_trace_restore_frame(const gmb_trace_frame_t* f);
/* Riporta la catena a `n` frame (usato a livello di modulo, dove non c'è un frame). */
void gmb_trace_restore_depth(int n);
int gmb_trace_depth(void);

/* Aggiorna la riga corrente del frame in cima (chiamata a ogni statement emesso). */
static inline void gmb_trace_line(long line) {
    if (gmb_trace_n > 0) gmb_trace_stack[gmb_trace_n - 1].line = line;
}

/* Catena come testo (malloc, "" se vuota), dal frame più interno al più esterno:
 *   "  in C (riga 42)\n  in B (riga 17)\n  in Main (riga 8)\n"
 * `skip` = quanti frame interni saltare (0 = tutti). */
char* gmb_trace_text_skip(int skip);

/* Snapshot della catena all'ultimo raise + lettura ("" se mai sollevato).
 * Dopo il longjmp i frame sono morti: il testo va catturato PRIMA del salto. */
void gmb_trace_snapshot(void);
const char* gmb_err_trace(void);

/* ── TESTO SORGENTE (ERR.SourceLineText) ──────────────────────────────────────────────────
 * Il codice generato incorpora le righe del sorgente concatenato in UN letterale con le
 * righe separate da '\n' (indice 0 = riga 1) e lo registra all'avvio. */
void gmb_src_text_register(const char* blob);
/* Variante ad ARRAY (chunk A1: righe già separate, indice 0 = riga 1). */
void gmb_src_lines_register(const char* const* lines, int count);
/* Testo (trimato) della riga `line` 1-based; "" se il blob non c'è o la riga è fuori range. */
const char* gmb_src_line_text(long line);

#endif /* GMB_TRACE_H */
