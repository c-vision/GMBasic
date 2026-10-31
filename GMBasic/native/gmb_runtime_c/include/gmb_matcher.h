/* gmb_matcher.h — Matcher SIMD/DFA per stringhe letterali (sorgente UNICA).
 *
 * Design (originale): un automa DFA a classi di byte (skip table Boyer-Moore-Horspool)
 * + scansione vettoriale dell'ancora (SSE2 su x86, NEON su ARM) con fallback scalare
 * a memchr. Compila il pattern UNA volta e riusa per N ricerche (grep fast-path,
 * ricerca nei documenti/PDF, segnaposto, ecc.).
 *
 * Non usa librerie esterne: intrinsics del compilatore + libc.
 */
#ifndef GMB_MATCHER_H
#define GMB_MATCHER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Matcher compilato per una stringa letterale (ASCII/byte). Opaco. */
typedef struct gmb_matcher gmb_matcher_t;

/* Compila un matcher per `pattern` (bytes), con ignore-case e whole-word opzionali. */
gmb_matcher_t* gmb_matcher_new(const char* pattern, int ignore_case, int whole_word);

/* Libera il matcher. */
void gmb_matcher_free(gmb_matcher_t* m);

/* Cerca la prima occorrenza nell'haystack di lunghezza `len` (puo' contenere NUL).
 * Ritorna l'indice (>=0) o -1. `out_len` = lunghezza del match. */
long gmb_matcher_find(gmb_matcher_t* m, const char* hay, size_t len, long* out_index, long* out_len);

/* Quante occorrenze non sovrapposte in [0,len). */
long gmb_matcher_count(gmb_matcher_t* m, const char* hay, size_t len);

/* True se il matcher e' stato compilato con successo (pattern non vuoto). */
int gmb_matcher_valid(const gmb_matcher_t* m);

/* Self-test: verifica DFA + SIMD su casi noti. Ritorna 0 se ok. */
int gmb_matcher_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MATCHER_H */
