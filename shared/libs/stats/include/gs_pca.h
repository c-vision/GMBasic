/*
 * gs_pca.h — Analisi delle Componenti Principali (porting da gmi_pca legacy).
 *
 * PCA su matrice dati row-major (rows × cols): centratura sulla media,
 * matrice di covarianza campionaria, autodecomposizione simmetrica con
 * rotazioni di Jacobi. Autovalori ordinati in modo decrescente (come SciPy),
 * autovettori per componente (riga i = componente i).
 */
#ifndef GMBSTATS_GS_PCA_H
#define GMBSTATS_GS_PCA_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_features;                    /* numero di colonne */
    int n_components;                  /* = n_features */
    gc_real *mean;                     /* [n_features] media per colonna */
    gc_real *eigenvalues;              /* [n_features] decrescenti */
    gc_real *eigenvectors;             /* [n_features*n_features] riga i = componente i */
    gc_real *explained_variance;       /* [n_features] = autovalori */
    gc_real *explained_variance_ratio; /* [n_features] autovalore / somma */
} gs_pca_result;

/* Calcola la PCA di data (rows × cols, row-major). Alloca i campi di out.
 * Restituisce GC_OK o GC_ERR_ARG / GC_ERR_NO_MEM. */
gc_status gs_pca(const gc_real *data, int rows, int cols, gs_pca_result *out);

/* Proiezione dei dati sulle prime k componenti: scores (rows × k, row-major).
 * scores deve essere preallocato (rows*k). */
gc_status gs_pca_transform(const gc_real *data, int rows, int cols,
                           const gs_pca_result *pca, int k, gc_real *scores);

/* Libera i campi allocati da gs_pca. */
void gs_pca_free(gs_pca_result *out);

/*
 * ==========================================================================
 * PPCA — Probabilistic Principal Component Analysis (Tipping & Bishop, 1999).
 *
 * Modello latente   t = W x + mu + eps,   x ~ N(0, I_q),   eps ~ N(0, sigma2 I_d),
 * covarianza del modello   C = W W^T + sigma2 I.
 * Stima di massima verosimiglianza in forma chiusa dall'autodecomposizione
 * della covarianza campionaria S = (1/rows) sum_n (t_n-mu)(t_n-mu)^T:
 *   sigma2_ML = media degli autovalori scartati (d-q),
 *   colonne di W = sqrt(max(lambda_j - sigma2, 0)) * autovettore_j  (j = 1..q).
 * Il numero di componenti q puo' essere scelto con AIC/BIC (complessita'
 * controllata: dof = d q + 1 - q(q-1)/2).
 * ==========================================================================
 */

#define GS_PPCA_AIC 0
#define GS_PPCA_BIC 1

typedef struct {
    int n_features;    /* d = numero colonne */
    int n_components;  /* q = dimensione latente, 0..d-1 */
    gc_real *mean;     /* [d] media per colonna */
    gc_real *W;        /* [d*q] carichi row-major: W[i*q + j] (NULL se q == 0) */
    gc_real sigma2;    /* varianza del rumore isotropo */
    gc_real loglik;    /* log-verosimiglianza del modello stimato */
    int dof;           /* parametri liberi: d*q + 1 - q*(q-1)/2 */
} gs_ppca_result;

/* Stima PPCA con q componenti (q in [0, cols-1]). Alloca mean/W.
 * Covarianza campionaria con denominatore 1/rows (MLE). Restituisce GC_OK o
 * GC_ERR_ARG / GC_ERR_NO_MEM. */
gc_status gs_ppca_fit(const gc_real *data, int rows, int cols, int q,
                      gs_ppca_result *out);

/* Proiezione latente (media condizionale) x = M^-1 W^T (t - mean), con
 * M = W^T W + sigma2 I (q x q). scores e' preallocato (rows*q). */
gc_status gs_ppca_transform(const gc_real *data, int rows, int cols,
                            const gs_ppca_result *ppca, gc_real *scores);

/* Covarianza del modello C = W W^T + sigma2 I (cols x cols, row-major). */
gc_status gs_ppca_covariance(const gs_ppca_result *ppca, gc_real *out_cov);

/* Seleziona q in [q_min, q_max] minimizzando AIC (criterion = GS_PPCA_AIC) o
 * BIC (GS_PPCA_BIC). out_q obbligatorio, out_score opzionale. */
gc_status gs_ppca_select_q(const gc_real *data, int rows, int cols,
                           int q_min, int q_max, int criterion,
                           int *out_q, gc_real *out_score);

/* Log-verosimiglianza del modello (mean, W, sigma2) sui dati (qualsiasi
 * rotazione R di W). NaN su errore. */
gc_real gs_ppca_loglik(const gc_real *data, int rows, int cols,
                       const gs_ppca_result *ppca);

/* EM per PPCA (Tipping & Bishop 1999, App. B): raffinamento iterativo a costo
 * O(rows*d*q) per iterazione, senza formare la covarianza d x d. Parte dalla
 * stima in forma chiusa e itera fino a iters o finche' |delta loglik| < tol.
 * Richiede q >= 1 (per q = 0 restituisce la stima isotropa). */
gc_status gs_ppca_em(const gc_real *data, int rows, int cols, int q,
                     int iters, gc_real tol, gs_ppca_result *out);

/* Libera i campi allocati da gs_ppca_fit. */
void gs_ppca_free(gs_ppca_result *out);

/*
 * Mistura di PPCA (Tipping & Bishop 1999, sez. 4.2): p(t) = sum_c pi_c N(t | mu_c, C_c)
 * con C_c = W_c W_c^T + sigma2_c I. EM con responsabilita' e aggiornamenti pesati.
 */
typedef struct {
    int n_features;   /* d */
    int n_components; /* q */
    int n_mixtures;   /* k */
    gc_real *weights; /* [k] pi_c */
    gc_real *mean;    /* [k*d] mu_c */
    gc_real *W;       /* [k*d*q] W_c */
    gc_real *sigma2;  /* [k] sigma2_c */
    gc_real loglik;   /* log-verosimiglianza della mistura */
} gs_ppca_mixture_result;

/* Stima una mistura di n_mix PPCA con q componenti latenti ciascuna.
 * q in [0, cols-1]; iters >= 1; tol sul loglik. Alloca i campi di out. */
gc_status gs_ppca_mixture(const gc_real *data, int rows, int cols, int q,
                          int n_mix, int iters, gc_real tol,
                          gs_ppca_mixture_result *out);

/* Responsabilita' posteriori gamma_nc (rows x n_mixtures, row-major). */
gc_status gs_ppca_mixture_responsibilities(const gc_real *data, int rows,
                                           const gs_ppca_mixture_result *m,
                                           gc_real *out_resp);

/* Libera i campi allocati da gs_ppca_mixture. */
void gs_ppca_mixture_free(gs_ppca_mixture_result *out);

/* PPCA con dati mancanti (Tipping & Bishop 1999, sez. 4.1).
 * mask: rows*cols; nonzero = valore presente, 0 = mancante (le celle mancanti
 * in data sono ignorate). EM congiunto esatto su (x, t^M) | t^O (momenti
 * E[x], E[x x^T], E[t^M], E[t^M t^M^T], E[t^M x^T]).
 * q in [0, cols-1]. */
gc_status gs_ppca_fit_missing(const gc_real *data, int rows, int cols, int q,
                              const unsigned char *mask, int iters, gc_real tol,
                              gs_ppca_result *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_PCA_H */
