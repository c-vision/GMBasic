/* llmwsp_analyze.h — W4 del piano Weight-Space Programming: analizzatori SENZA forward.
 *
 * Lavorano solo sui pesi (slice lette dal Weight Store, indirizzate dal WSP Document), rispettando GQA.
 * Circuiti per testa h (gruppo kv g = h / (heads/kv_heads)):
 *   OV:  W_OV = O_h · V_g   (H×H, rango <= head_dim); autovalori non nulli = autovalori di V_g·O_h (hd×hd)
 *   QK:  W_QK = Q_hᵀ · K_g  (H×H, rango <= head_dim); autovalori non nulli = autovalori di K_g·Q_hᵀ (hd×hd)
 * ATTENZIONE: sono i circuiti "nudi". RoPE e le norme q_norm/k_norm (Qwen3) NON sono incluse: lo spettro QK è
 * quindi un'approssimazione per i modelli che le usano. Nessuna interpretazione senza prova comportamentale. */
#ifndef LLMWSP_ANALYZE_H
#define LLMWSP_ANALYZE_H
#include "llmwsp_doc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Slice di un tensore come double row-major. axis 0: righe [start,end) -> [(end-start) × cols];
 * axis 1 (2D): colonne [start,end) -> [righe × (end-start)]. dtype supportati: F32, F16, BF16. */
int llmwsp_load_f64(const llmwsp_store *s, const llmwsp_tensor *t, int axis, int64_t start, int64_t end,
                    double *out, size_t cap);

/* Norme di Frobenius per testa (baseline ovvia): q[heads], k[kv_heads], v[kv_heads], o[heads]. Ogni puntatore
 * può essere NULL. */
int llmwsp_an_head_norms(const llmwsp_store *s, const llmwsp_doc *d, int layer, double *q, double *k, double *v, double *o);

typedef enum { LLMWSP_CIRCUIT_OV = 0, LLMWSP_CIRCUIT_QK = 1 } llmwsp_circuit;

/* Autovalori (head_dim) del circuito di una testa: wr/wi hanno head_dim elementi. */
int llmwsp_an_circuit_spectrum(const llmwsp_store *s, const llmwsp_doc *d, int layer, int head, llmwsp_circuit c,
                               double *wr, double *wi);

/* DUE definizioni di copying score, calcolate entrambe e dichiarate:
 *   sum_ratio = Σ Re(λ) / Σ |λ|          (Elhage et al.; in [-1,1])
 *   frac_pos  = #{Re(λ) > 0} / n         (frazione di autovalori a parte reale positiva)
 * Con spettro nullo (|λ| trascurabile) entrambe valgono 0. */
void llmwsp_an_copy_scores(const double *wr, const double *wi, size_t n, double *sum_ratio, double *frac_pos);

/* Composizione fra teste: coseni degli angoli principali fra lo spazio scritto da (l1,h1) (colonne di O_h1) e lo
 * spazio letto da (l2,h2) come 'q', 'k' o 'v' (colonne di Xᵀ). cos ha head_dim valori decrescenti.
 * pk = Σ cos² (Projection Kernel, in [0, head_dim]); NULL ammesso. */
int llmwsp_an_compose_cosines(const llmwsp_store *s, const llmwsp_doc *d, int l1, int h1, int l2, int h2, char which,
                              double *cos, double *pk);

#ifdef __cplusplus
}
#endif
#endif
