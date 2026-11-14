/* gmb_c99_emit.h — emitter C99 nativo (AST BASIC → sorgente C).
 * FASE 1 del piano C99Native: subset core (PRINT, LET/ASSIGN, DIM scalare,
 * IF, FOR, WHILE, SUB/FUNCTION, CALL, RETURN, END). Il resto produce un
 * errore di compilazione chiaro con numero di riga (strategia piano §1.7).
 * L'AST C è untyped: i tipi sono dedotti per convenzione BASIC (suffisso
 * $ = stringa, % = intero) e per inferenza dagli inizializzatori. */
#ifndef GMB_C99_EMIT_H
#define GMB_C99_EMIT_H

#include "gmb_parser.h"

/* Genera il sorgente C99 completo (malloc, NUL-terminated) in *out_src.
 * Ritorna 0 su successo; altrimenti ritorna != 0 e *error contiene il
 * messaggio (malloc, da liberare a cura del chiamante). */
int gmb_c99_emit(const gmb_program_t* prog, char** out_src, char** error);
/* §3.6 PARSEC: nome del file sorgente (per LLM.FILEOUTLINE nel fold semantico). */
void gmb_c99_set_source_file(const char* f);
/* GPM: 0 = auto (rilevamento dal programma), 1 = always (forza lo scheduler anche se il
 * rilevamento non vede concorrenza), 2 = linear (percorso diretto quando è lineare;
 * ripiega su auto se c'è concorrenza). Da chiamare PRIMA di gmb_c99_emit. */
void gmb_c99_set_gpm_mode(int mode);

/* O8 — mappa diagnostica C→BASIC. `line` è la prima riga (1-based) che il file occupa nel
 * testo concatenato passato al parser; dentro il file la riga riparte da 1. Passata al
 * driver (che la ricava da gmb_resolve_includes_mapped), fa emettere `#line <riga>
 * "<file>"` prima di ogni statement, così gli errori di `cc` citano il `.bas`.
 * I puntatori restano di proprietà del chiamante e devono vivere per tutta l'emissione. */
typedef struct {
    int line;
    const char* file;
} gmb_c99_anchor_t;
void gmb_c99_set_source_anchors(const gmb_c99_anchor_t* anchors, int n);
/* TESTO SORGENTE per ERR.SourceLineText: il sorgente CONCATENATO passato al parser (indice 0 =
 * riga 1, la stessa numerazione dei `#line`). Il puntatore resta di proprietà del chiamante e
 * deve vivere per tutta l'emissione. NULL = nessun blob incorporato. */
void gmb_c99_set_source_text(const char* text);

#endif
