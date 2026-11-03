/* ============================================================================
 * gmbllm/include/train/llm_train.h — TRAINING (PUL) — orchestrazione loss/grad/DPO.
 *
 * L'orchestrazione (closure, value_and_grad, readback, DPO) vive qui (PUL); i PRIMITIVI
 * (forward differenziabile + hook SDPA + setup) sono l'ABI Apache `llmengine_mlx_train.h`.
 * ============================================================================ */
#ifndef GMBLLM_TRAIN_H
#define GMBLLM_TRAIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int llm_ai_mlx_train_lora_loss_grad_mask(void* mh,
                                         const int32_t* toks, const int32_t* targets, const uint8_t* mask, int T,
                                         int layer, const char* proj,
                                         const float* A, int Ar, int Ac,
                                         const float* B, int Br, int Bc,
                                         float* out_loss, float* out_dA, float* out_dB);

int llm_ai_mlx_train_lora_loss_grad(void* mh,
                                    const int32_t* toks, const int32_t* targets, int T,
                                    int layer, const char* proj,
                                    const float* A, int Ar, int Ac,
                                    const float* B, int Br, int Bc,
                                    float* out_loss, float* out_dA, float* out_dB);

int llm_ai_mlx_train_dpo_step(void* mh,
                              const int32_t* tw, const int32_t* tgw, const uint8_t* mw, int Tw,
                              const int32_t* tl, const int32_t* tgl, const uint8_t* ml, int Tl,
                              int layer, const char* proj,
                              const float* A, int Ar, int Ac, const float* B, int Br, int Bc,
                              float beta, float* out_loss, float* out_dA, float* out_dB);

/* G-opt2 — GRPO (RLHF senza critic): gruppo di G completamenti dello stesso prompt, con vantaggi
 * group-relative `adv` e log-prob della policy vecchia `old_logp` (somma, [G]). loss = clipped
 * surrogate + kl_coef*k3_KL(pi||ref); ref = adapter OFF. `toks`/`tgt`/`mask` = [G*T] (T fisso). */
int llm_ai_mlx_train_grpo_step(void* mh,
                               const int32_t* toks, const int32_t* tgt, const uint8_t* mask, int T, int G,
                               const float* old_logp, const float* adv, float clip, float kl_coef,
                               int layer, const char* proj,
                               const float* A, int Ar, int Ac, const float* B, int Br, int Bc,
                               float* out_loss, float* out_dA, float* out_dB);

/* G5 — distill teacher-student (KL soft-logit). teacher_logits [T,V] (host, detach),
 * temperature T (soft targets). Gradiente w.r.t. LoRA A/B. 0 ok / -1. */
int llm_ai_mlx_train_kl_step(void* mh,
                             const int32_t* toks, int T,
                             const float* teacher_logits, int V, float temperature,
                             const uint8_t* mask,
                             int layer, const char* proj,
                             const float* A, int Ar, int Ac,
                             const float* B, int Br, int Bc,
                             float* out_loss, float* out_dA, float* out_dB);

/* T4 — passo di training DECISIONALE (JEV): forward differenziabile sull'ultima posizione,
 * gather dei K token delle opzioni e loss CE/Brier/RPS/CE+RPS nello stesso grafo autograd.
 * `option_ids` = K token id; `option_mask` (K byte, opzionale) esclude i pad; `criterion` =
 * `llm_decide_criterion_t` (0 CE, 1 Brier, 2 RPS, 3 CE+RPS). Aggiorna dA/dB (LoRA) via
 * `mlx_value_and_grad`. 0 ok / -1 su errore. */
int llm_ai_mlx_train_decide_step(void* mh,
                                 const int32_t* toks, int T,
                                 const int32_t* option_ids, int K,
                                 int label, int criterion,
                                 const uint8_t* option_mask,
                                 float label_smoothing, float rps_weight,
                                 int layer, const char* proj,
                                 const float* A, int Ar, int Ac,
                                 const float* B, int Br, int Bc,
                                 float* out_loss, float* out_dA, float* out_dB);

/* T4 — campione già TOKENIZZATO per il training decisionale (il pipeline testo→token vive nel
 * tool/driver; qui la matematica e l'ottimizzazione). `tokens` = prompt (T id), `option_ids` =
 * K token id delle opzioni, `label` = indice corretto. */
typedef struct {
    const int32_t* tokens;
    int n_tokens;
    const int32_t* option_ids;
    int n_options;
    int label;
} llm_decide_train_sample_t;

/* T4 — loop di training LoRA con la loss decisionale: per `steps` passi sceglie un campione,
 * calcola loss+dA/dB (`llm_ai_mlx_train_decide_step`) e aggiorna A/B con AdamW. `out_A`/`out_B`
 * sono IN/OUT (contengono i pesi correnti e vengono aggiornati in place). `out_loss` = media
 * delle loss dei passi riusciti. 0 ok / -1 su errore (nessun aggiornamento parziale silenzioso:
 * A/B restano quelli dell'ultimo passo completato). */
int llm_ai_mlx_decide_train_lora(void* mh,
                                 const llm_decide_train_sample_t* samples, int n_samples,
                                 int criterion, float label_smoothing, float rps_weight,
                                 int steps, float lr, int seed,
                                 int layer, const char* proj,
                                 int Ar, int Ac, int Br, int Bc,
                                 float* out_loss, float* out_A, float* out_B);

/* G5 — logits del TEACHER per la distillazione (forward differenziabile -> copia host [T,V], detach).
 * Isola mlx-c in gmbllm (la dylib finetune non include headers mlx). `out_logits` = T*V float. */
int llm_train_teacher_logits(void* mh, const int32_t* toks, int T, float* out_logits, int V);

/* G6 — full-parameter fit (Adam su TUTTI i pesi via l'ABI diff). Salva `<out_prefix>.names` +
 * `<out_prefix>.NNN.raw` (pesi F32). Metti gli MLX array dietro (gmbllm). FP16/BF16. 0 ok / -1. */
int llm_ai_mlx_train_full_fit(void* mh, const int32_t* toks, const int32_t* tgt, const uint8_t* mask,
                              int T, int steps, float lr, const char* out_prefix);

#ifdef __cplusplus
}
#endif
#endif /* GMBLLM_TRAIN_H */
