/* gmbkdtree.h — API della lib `kdtree` (shared/libs/kdtree).
 *
 * scipy.spatial.KDTree: build su punti flat row-major (n x m), query k-NN e
 * query_ball_point. Self-contained: tabella statica di handle (int).
 * Le query scrivono nei buffer del chiamante e ritornano il numero di
 * risultati (o -1 su errore).
 */
#ifndef GMBKDTREE_H
#define GMBKDTREE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Crea l'albero da n punti di m dimensioni (flat row-major). Ritorna l'handle
 * (>0) o 0 su errore. */
int ofx_kd_create(const double* flat, int n, int m);

/* Numero di punti dell'albero (0 se handle non valido). */
int ofx_kd_count(int h);

/* k-NN: indici dei k punti più vicini a q, ordinati per distanza crescente.
 * out_cap >= k. Ritorna il numero di risultati (<= k) o -1. */
int ofx_kd_query(int h, const double* q, int k, double* out_idx, int out_cap);

/* k-NN: distanze euclidee dei k punti più vicini a q. */
int ofx_kd_query_dist(int h, const double* q, int k, double* out_d, int out_cap);

/* Tutti i punti entro il raggio r da q. out_cap >= numero di punti
 * dell'albero. Ritorna il numero di risultati o -1. */
int ofx_kd_query_ball(int h, const double* q, double r, double* out_idx, int out_cap);

#ifdef __cplusplus
}
#endif

#endif /* GMBKDTREE_H */
