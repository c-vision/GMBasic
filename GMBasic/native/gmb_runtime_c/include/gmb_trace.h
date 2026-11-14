/* gmb_trace.h — STACK TRACE BASIC del runtime C (gamba C99Native e VM/C).
 *
 * Perché: `gmb_raise_error` sapeva solo la RIGA dell'errore. Con un errore in profondità
 * (A → B → C) il messaggio diceva "line 42" senza dire *quale* procedura e da chi era stata
 * chiamata. Qui si tiene una catena di frame (procedura + riga corrente) e la si rende come
 * testo, con la stessa forma su tutte le gambe:
 *
 *     errore 11: Division by zero (line 42)
 *       in C (riga 42)
 *       in B (riga 17)
 *       in Main (riga 8)
 *
 * COSTO. Nessuna allocazione: i frame sono variabili di stack legate con
 * `__attribute__((cleanup(...)))` (emesse dall'emitter C99) e l'aggiornamento di riga è uno
 * store (gmb_trace_line è `static inline`). La VM/C non instrumenta nulla: ricava la catena
 * dai propri frame di chiamata (`gmb_frame_t.return_ip` → nome procedura e riga dal chunk).
 *
 * ATTENZIONE (longjmp): `gmb_raise_error` fa longjmp, quindi i cleanup NON corrono. È
 * esattamente ciò che serve: al momento del raise i frame sono ancora tutti validi e il
 * testo si cattura lì, PRIMA del salto.
 */
#ifndef GMB_TRACE_H
#define GMB_TRACE_H

typedef struct gmb_trace_frame {
    const char* fn;                 /* nome BASIC della procedura (letterale, mai liberato) */
    long line;                      /* riga BASIC corrente nella procedura */
    struct gmb_trace_frame* prev;   /* frame chiamante */
} gmb_trace_frame_t;

/* Cima della catena (NULL = nessuna procedura BASIC in esecuzione). */
extern gmb_trace_frame_t* gmb_trace_top;

/* Entra in una procedura: ritorna il frame (da passare a gmb_trace_leave). */
gmb_trace_frame_t gmb_trace_enter(const char* fn, long line);
/* Esce dalla procedura (usato come `cleanup`): ripristina la cima precedente. */
void gmb_trace_leave(gmb_trace_frame_t* f);
/* Aggiorna la riga corrente del frame in cima (chiamata a ogni statement emesso). */
static inline void gmb_trace_line(long line) {
    if (gmb_trace_top) gmb_trace_top->line = line;
}
int gmb_trace_depth(void);

/* Catena come testo (malloc, "" se vuota), dal frame più interno al più esterno:
 *   "  in C (riga 42)\n  in B (riga 17)\n  in Main (riga 8)\n"
 * `skip` = quanti frame interni saltare (0 = tutti). */
char* gmb_trace_text_skip(int skip);

/* Snapshot della catena all'ultimo raise + lettura ("" se mai sollevato).
 * Dopo il longjmp i frame sono morti: il testo va catturato PRIMA del salto. */
void gmb_trace_snapshot(void);
const char* gmb_err_trace(void);

#endif /* GMB_TRACE_H */
