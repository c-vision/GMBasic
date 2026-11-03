/* ============================================================================
 * gmbllm/include/train/llm_train_decide.h — T4 (PUL): loss decisionale nel grafo MLX.
 *
 * Il grafo è costruito SOPRA i logits del forward differenziabile Apache:
 *   logits [1,T,V] -> ultima posizione -> gather dei K token opzione -> criterio.
 * La matematica è la stessa di `llm_decide_loss.c` (CE/Brier/RPS/CE+RPS, label smoothing,
 * maschera sulle opzioni valide); cambia solo il supporto: qui è autograd MLX.
 * ============================================================================ */
#ifndef GMBLLM_TRAIN_DECIDE_H
#define GMBLLM_TRAIN_DECIDE_H

#include <stdbool.h>
#include <stdint.h>

#include <mlx/c/array.h>
#include <mlx/c/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Costruisce la loss decisionale (scalare) dal tensore dei logits [1,T,V].
 * - `opt_ids`  : K token id (nel vocabolario del modello) delle opzioni, nell'ordine del dominio
 * - `label`    : indice dell'opzione corretta
 * - `criterion`: `llm_decide_criterion_t` (0 CE, 1 Brier, 2 RPS, 3 CE+RPS)
 * - `opt_mask` : K byte (0 = opzione di padding), NULL = tutte valide
 * La softmax e i target sono normalizzati SOLO sulle opzioni valide, come il readout del
 * motore. Ritorna un array valido (da liberare) e imposta `*ok`; su errore l'array è vuoto.
 */
mlx_array llm_train_decide_loss_from_logits(mlx_array logits, const int32_t* opt_ids, int K,
                                            int label, int criterion,
                                            float label_smoothing, float rps_weight,
                                            const uint8_t* opt_mask, mlx_stream st, bool* ok);

#ifdef __cplusplus
}
#endif
#endif /* GMBLLM_TRAIN_DECIDE_H */
