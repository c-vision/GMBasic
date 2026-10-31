/* gmb_ai_finetune.h — fine-tune LoRA e apply/deploy di adapter, IN-PROCESS (dylib `gmb_ai_finetune`).
 *
 * PERCHÉ: il fine-tune era una POC in tools/ eseguita come SUBPROCESS (gmb_mlx_finetune /
 * gmb_mlx_lora_apply da libs-runtime/). La logica di produzione sta in native/ ed è chiamata
 * come funzione: dal runtime C (dlopen) e dal C# (P/Invoke) — una sola sorgente, nessun processo.
 *
 * Contratto (lo stesso del protocollo IPC): request JSON in ingresso, response JSON in uscita.
 *   finetune request : {"id":1,"method":"finetune","params":{"model_dir":..,"dataset":..,
 *                       "holdout":..,"ckpt":..,"rank":..,"layer":..,"steps":..,"batch":..,
 *                       "lr":..,"seed":..}}
 *   apply   request  : {"id":1,"method":"apply","params":{"ckpt":..,"model_dir":..,"dataset":..}}
 *   response         : {"ok":true,"ckpt":..,"train":..,"hold":..,"baseTrain":..,"baseHold":..,
 *                       "greedyBefore":..,"greedyAfter":..,"demoChanged":0|1}
 * Ritorno 0 su successo, !=0 su errore (messaggio JSON con "error" in `resp`). */
#ifndef GMB_AI_FINETUNE_H
#define GMB_AI_FINETUNE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Esegue un fine-tune LoRA. `req` = request JSON; scrive la response JSON in `resp` (cap byte). */
int gmb_ai_finetune_exec(const char* req, char* resp, size_t cap);

/* Ricarica un adapter LoRA salvato e verifica OFF vs ON. Stesso contratto JSON. */
int gmb_ai_lora_apply_exec(const char* req, char* resp, size_t cap);

/* TRAIN DPO: preference optimization (coppie chosen/rejected). Stesso contratto JSON.
 * Request params: model_dir, chosen, rejected, ckpt, rank, layer, steps, batch, lr, beta, seed. */
int gmb_ai_dpo_exec(const char* req, char* resp, size_t cap);
/* G-opt2 — GRPO (RLHF senza critic, RLVR): {model_dir,dataset,ckpt,rank,layer,group,maxnew,lr,clip,kl,temperature,seed}. */
int gmb_ai_grpo_exec(const char* req, char* resp, size_t cap);

/* G5 — distill teacher-student (KL soft-logit). Request params: model_dir (student), teacher, dataset,
 * ckpt, rank, layer, steps, batch, lr, temperature, seed. Salva <ckpt>_best_{A,B}.raw + .meta. */
int gmb_ai_distill_kl_exec(const char* req, char* resp, size_t cap);

/* Stream D (D1) SAMPLE: genera k risposte a temperatura temp per ogni prompt di un .jsonl e
 * filtra con VERIFY, scrivendo le coppie accettate in `out`. Request JSON (params):
 *   {"model_dir":..,"prompts":..,"out":..,"k":4,"temp":0.7,"maxlen":256,"verify":"rule","seed":..}
 * Response: {"pairs":..,"fail":..,"gen":..,"skip":..,"out":..}. */
int gmb_ai_distill_sample_exec(const char* req, char* resp, size_t cap);

/* Stream D (D4) EVAL paired: idx 0 = modello base, idx 1 = modello + adapter (schema LORA APPLY).
 * Request JSON (params): {"model_dir":..,"prompts":..,"ckpt":..,"out":..,"maxlen":256,
 * "repeat":3,"temp":0.7,"verify":"rule","seed":..}. Response con baseTok/basePass/distTok/distPass. */
int gmb_ai_distill_eval_exec(const char* req, char* resp, size_t cap);
double gmb_ai_distill_eval_tok(int idx);
double gmb_ai_distill_eval_pass(int idx);
double gmb_ai_distill_eval_acc(int idx);

#ifdef __cplusplus
}
#endif

#endif /* GMB_AI_FINETUNE_H */
