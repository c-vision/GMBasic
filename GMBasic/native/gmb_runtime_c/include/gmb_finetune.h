/* gmb_finetune.h — fine-tune/deploy LoRA su modelli MLX locali, versione C (mondo C).
 *
 * Esecuzione IN-PROCESS: chiama la dylib `gmb_ai_finetune` (native/gmb_ai_finetune, sorgente
 * `gmb_ai_finetune.c` — la STESSA logica usata dal C# via P/Invoke): dlopen + funzione, nessun
 * subprocess. Usato dai backend C (VM/C, C99Native) per i comandi `AI FINETUNE` e
 * `AI LORA APPLY`. Se la dylib/funzione manca -> errore esplicito (mai stub).
 * La funzione nativa scrive il report `<ckpt>.json` e restituisce la response JSON, qui
 * riepilogata nel buffer `out`.
 */
#ifndef GMB_FINETUNE_H
#define GMB_FINETUNE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Esegue gmb_mlx_finetune (fine-tune LoRA). Parametri CLI posizionali dell'harness,
 * ordine dei campi del request JSON (finetune):
 *   model_dir dataset holdout rank steps batch layer lr seed ckpt
 * `ckpt` e' l'ULTIMO argomento, NON il 4o. La firma C di gmb_ai_finetune_run() ha un
 * ordine diverso (ckpt 4o, layer 6o): l'allineamento avviene solo nel riordino esplicito
 * della snprintf in gmb_finetune.c. Non passare i parametri nell'ordine della firma C:
 * scambierebbe layer<->steps e lr<->seed (bug B1 del TODO AI-Finetune-GPM-Certificazione).
 * Ritorna 0 e popola out ("ok train=.. hold=.. greedyBefore=.. greedyAfter=..") in caso
 * di successo; -1 con errore esplicito in out altrimenti. Mai stub. */
int gmb_ai_finetune_run(const char* model_dir, const char* dataset, const char* holdout,
                        const char* ckpt, int rank, int layer, int steps, int batch,
                        double lr, int seed, const char* frozen, char* out, size_t cap);

/* Esegue gmb_mlx_lora_apply (deploy/verifica adapter: carica <ckpt>_best_{A,B}.raw in
 * una sessione nativa e confronta la generazione OFF vs ON su un prompt dal dataset).
 * Ritorna 0 con out = "ok: ..." se l'adapter cambia/invariato; -1 con errore esplicito. */
int gmb_ai_lora_apply_run(const char* ckpt_prefix, const char* model_dir, const char* dataset,
                          char* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FINETUNE_H */

/* Varianti con opzioni JSON (stringa) — per i backend che passano il dict come JSON:
 * le chiavi note: dataset, holdout, ckpt, rank, layer, steps, batch, lr, seed (finetune)
 * e model, dataset (apply). Il resto usa i default. */
int gmb_ai_finetune_run_opts(const char* model_dir, const char* json_opts, char* out, size_t cap);
int gmb_ai_lora_apply_run_opts(const char* ckpt_prefix, const char* json_opts, char* out, size_t cap);

/* TRAIN DPO: preference optimization. model_dir + json_opts {chosen, rejected, ckpt, rank,
 * layer, steps, batch, lr, beta, seed}. Ritorna 0 con out riepilogo; -1 con errore esplicito. */
int gmb_ai_dpo_run_opts(const char* model_dir, const char* json_opts, char* out, size_t cap);

/* TRAIN GRPO: RLHF senza critic (RLVR). model_dir + json_opts {dataset|train, ckpt, rank, layer,
 * group, maxnew, steps, lr, clip, kl, temperature, seed}. Ritorna 0 con out riepilogo; -1 con errore. */
int gmb_ai_grpo_run_opts(const char* model_dir, const char* json_opts, char* out, size_t cap);

/* Stato dell'ultima run (persistente in-process, come il mondo C#). */
int    gmb_ai_finetune_done(void);      /* -1 ok, 0 no */
double gmb_ai_finetune_loss(void);
double gmb_ai_finetune_hold(void);
long   gmb_ai_finetune_greedy(void);
/* B3: misure di apprendimento (baseline e risultato dopo il fine-tune). */
long   gmb_ai_finetune_greedy_after(void);
long   gmb_ai_finetune_demo_changed(void);
double gmb_ai_finetune_base_train(void);
double gmb_ai_finetune_base_hold(void);
const char* gmb_ai_finetune_ckpt(void); /* statico interno */
const char* gmb_ai_finetune_status(void);
/* R2.4: ultimo job ID dal registry (0 se nessun job). */
int    gmb_ai_finetune_last_job_id(void);

/* Query del registro run (superficie TRAIN RUNS / LLM.RUNS):
 * conteggio e JSON {"count":N,"runs":[...]} (ownership del CHIAMANTE: free()). */
int    gmb_ai_runs_count(void);
char*  gmb_ai_runs_json(void);
/* TRAIN RUNS PRUNE: svuota il registro; ritorna quante run sono state rimosse. */
int    gmb_ai_runs_clear(void);
/* TRAIN RUNS SHOW <id>: JSON di una run ("" se assente). Ownership del chiamante. */
char*  gmb_ai_runs_item(int id);

/* TRAIN LORA LIST: JSON {"count":N,"adapters":[...]} degli adapter da run LORA APPLY. */
char*  gmb_ai_lora_list(void);

/* TRAIN STOP <id>: rimuove la run dal registro (1 se trovata). TRAIN EXPORT <out>: esporta
 * l'adapter dell'ultima run (n file copiati). */
int    gmb_ai_runs_stop(int id);
int    gmb_ai_train_export(const char* out);
/* TRAIN EXPORT <out>[, <run>]: esporta l'adapter della run `id` (id<=0 → l'ultima). */
int    gmb_ai_train_export_run(const char* out, int id);

/* TRAIN DATASET STATS <file>: JSON {"file","bytes"}. Ownership del chiamante. */
char*  gmb_ai_dataset_stats(const char* path);

/* TRAIN CONFIG SHOW: JSON dei default di sessione (rank/layer/steps/batch/seed/lr).
 * Ownership del CHIAMANTE (free()). */
char*  gmb_ai_train_config_json(void);
/* TRAIN CONFIG SET key, value: aggiorna il default e ritorna il JSON aggiornato
 * (ownership del CHIAMANTE). */
char*  gmb_ai_train_config_set(const char* key, const char* value);
