/*
 * gor_pgm.h — indice PGM (Piecewise Geometric Model) e approssimazione
 * piecewise-lineare di una sequenza monotona.
 *
 * Da IvanIsCoding/AppendPGM (MIT): la funzione rank(x) di un insieme ordinato
 * viene approssimata da segmenti lineari con errore massimo eps (Swing Filter,
 * Ferragina-Vinciguerra). Utile per ricerca approssimata su dati ordinati.
 */
#ifndef GMBORTOOLS_GOR_PGM_H
#define GMBORTOOLS_GOR_PGM_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Segmento: y = a*x + b, valido per x >= pos. */
typedef struct gor_pgm_seg { gor_real a, b, pos; } gor_pgm_seg;

typedef struct gor_pgm {
    gor_pgm_seg *seg;   /* array di segmenti */
    int nseg;           /* numero di segmenti */
    int count;          /* numero di chiavi indicizzate */
    int eps;            /* errore massimo */
    gor_real xmin, xmax;
} gor_pgm;

/* Costruisce l'indice dalle chiavi ordinate x[0..n-1] (crescenza stretta).
 * Ritorna 0 su successo, -1 su errore. */
int gor_pgm_build(gor_pgm *pgm, int n, const gor_real *x, int eps);

/* Libera le risorse. */
void gor_pgm_free(gor_pgm *pgm);

/* Rank approssimato della chiave: ritorna la posizione stimata e, se lo/hi non
 * sono NULL, l'intervallo [lo,hi] in cui la vera posizione è certamente
 * contenuta (garantito se |predizione - rank| <= eps). Ritorna -1 se vuoto. */
long gor_pgm_search(const gor_pgm *pgm, gor_real key, long *lo, long *hi);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_PGM_H */
