/*
 * gn_fusedmm.h — SDDMM-SpMM fuso (da HipGraph/FusedMM).
 *
 * Un unico passaggio sugli archi: per ogni arco (i,j) calcola il prodotto
 * interno X[i]·Y[j] (SDDMM) e lo aggrega nella riga i pesato per il valore
 * dell'arco (SpMM). Usato in GAT/GraphSAGE/embedding su grafi.
 */
#ifndef GMBNEURAL_GN_FUSEDMM_H
#define GMBNEURAL_GN_FUSEDMM_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A sparsa in CSR (rowptr[rows+1], colidx[nnz], val[nnz]). X (rows×k), Y (cols×k).
 * Z (rows×k) = beta*Z + alpha * sum_{(i,j)} val(i,j) * (X[i]·Y[j]) * Y[j].
 * val puo' essere NULL (peso 1). */
gn_status gn_fusedmm_csr(const int *rowptr, const int *colidx, const gn_real *val,
                         int rows, int cols, int k,
                         const gn_real *X, const gn_real *Y,
                         gn_real alpha, gn_real beta, gn_real *Z);

/* Variante GraphSAGE senza attention: Z[i] = beta*Z + alpha * sum_j val(i,j)*Y[j]
 * (ovvero SpMM puro con normalizzazione opzionale). */
gn_status gn_aggregate_csr(const int *rowptr, const int *colidx, const gn_real *val,
                           int rows, int cols, int k, const gn_real *Y,
                           gn_real alpha, gn_real beta, gn_real *Z);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_FUSEDMM_H */
