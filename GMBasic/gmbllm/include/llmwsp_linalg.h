/* llmwsp_linalg.h — algebra lineare densa minimale (double, row-major) per gli analizzatori WSP (W4).
 * Nessuna dipendenza esterna: SVD a una sola parte di Jacobi, autovalori non simmetrici (QR complesso con
 * shift di Wilkinson su Hessenberg), angoli principali fra sottospazi. C99 puro. */
#ifndef LLMWSP_LINALG_H
#define LLMWSP_LINALG_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C[m×n] = A[m×k] · B[k×n] */
void llmwsp_la_matmul(const double *A, const double *B, double *C, size_t m, size_t k, size_t n);

/* Valori singolari di A[m×n] in ordine decrescente (min(m,n) valori in s). Se U != NULL e m >= n riceve la
 * base ortonormale dello spazio delle colonne, U[m×n] row-major (colonne corrispondenti a valori nulli:
 * ortonormali ma arbitrarie). 0 = ok, <0 = non convergenza o memoria. */
int llmwsp_la_svd(const double *A, size_t m, size_t n, double *s, double *U);

/* Autovalori di A[n×n] (reale, non simmetrica): parti reali in wr, immaginarie in wi. Ordine non specificato. */
int llmwsp_la_eig(const double *A, size_t n, double *wr, double *wi);

/* Coseni degli angoli principali fra lo spazio delle colonne di A[m×p] e quello di B[m×q] (m >= p, q),
 * min(p,q) valori in ordine decrescente in cos. 1 = direzioni condivise, 0 = ortogonali. */
int llmwsp_la_principal_cosines(const double *A, size_t p, const double *B, size_t q, size_t m, double *cos);

#ifdef __cplusplus
}
#endif
#endif
