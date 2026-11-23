/* ============================================================================
 * gmb_matrix_flat.h — ABI PIATTA dei kernel matriciali per i backend MANAGED.
 *
 * Perché esiste: IL e PCodeVM implementavano SINM/TANM/ORTH con MathNet, mentre
 * VM/C e C99Native usano i kernel del runtime C (`gmb_matrix.c`). Sono due
 * implementazioni indipendenti dello stesso algoritmo: i risultati coincidono a
 * meno di pochi ulp, che diventano visibili quando un sample stampa 15 cifre
 * (parity4 confronta il testo esatto) — `MatrixFunctionsSciPyParity2`.
 *
 * Direzione unica (AGENTS): il C# chiama il C. Qui si espone una funzione con
 * buffer piatti (nessun handle, nessuno stato condiviso): il managed appiattisce
 * la matrice, chiama, ricostruisce. Stessa sorgente, stessi bit delle altre due
 * gambe C. Se la libreria non è disponibile il managed ricade sull'implementazione
 * gestita (i backend managed restano autosufficienti).
 *
 * op: 1 = SINM, 2 = TANM, 3 = ORTH.
 * `out` ha capacità `out_cap` (row-major); *out_rows/*out_cols dicono quanto è
 * stato scritto (ORTH può avere meno colonne: n×rank).
 * Ritorna 1 = ok, 0 = errore (input non valido, op sconosciuta, buffer piccolo).
 * ============================================================================ */
#ifndef GMB_MATRIX_FLAT_H
#define GMB_MATRIX_FLAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gmb_matrix_solve_lin(int64_t n, const double* a, const double* b, double* x, double tol);
int gmb_matrix_flat_op(const double* a, int64_t rows, int64_t cols, int op,
                       double* out, int64_t out_cap,
                       int64_t* out_rows, int64_t* out_cols);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MATRIX_FLAT_H */
