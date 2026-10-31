/* gmb_distill.h — Stream D: self-distillation lm_head-only (pipeline AI DISTILL).
 *
 * §0bis: logica condivisa (dataset/report) nel runtime C, chiamata dai 4 backend.
 * Fase D1: SAMPLE (raccolta coppie dal modello, filtro VERIFY) + BUILD (jsonl → .bin).
 * BUILD e' implementato qui (tokenizer BPE + split train/hold, formato .bin di
 * make_lora_dataset.py); SAMPLE delega la generazione (motore MLX) alla dylib
 * gmb_ai_finetune via dlopen. TRAIN/APPLY/EVAL/RESTORE sono fasi successive.
 */
#ifndef GMB_DISTILL_H
#define GMB_DISTILL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BUILD: legge un file .jsonl di coppie (prompt,risposta) e scrive <out>.train.bin e
 * <out>.hold.bin nel formato di make_lora_dataset.py (interi LE: n, poi len,tok...,target).
 * - jsonl_path: righe JSON con campi prompt/response | problem/solution | question/answer.
 * - tokenizer_json: path del tokenizer.json (del model_dir).
 * - out_prefix: prefisso di output (scrive <out_prefix>.train.bin / .hold.bin).
 * - window T (finestra), hold_ratio [0,1), max_pairs (0 = tutte).
 * Ritorna il numero di coppie scritte, o -1 in errore (messaggio via gmb_distill_last_error).
 * L'ordine dei sample non e' mescolato (deterministico: nessun seed -> riproducibile). */
int gmb_distill_build(const char* jsonl_path, const char* tokenizer_json,
                      const char* out_prefix, int window, double hold_ratio, int max_pairs);

/* Stato/report condiviso (JSON) — per AI.DISTILLSTATUS e le altre funzioni AI.DISTILL*. */
int gmb_distill_report(char* out, int cap);

/* Entry point opzioni-JSON (unico per C#/C): opts = {model, tokenizer, out, window, holdout,
 * max}. Ritorna la response JSON in resp ({"type":"result"|"error","data":{...}}). 0 = ok. */
int gmb_distill_build_opts(const char* jsonl_path, const char* opts_json, char* resp, int cap);

/* Accesso ai singoli campi (parita' con le funzioni AI.DISTILL*). */
int   gmb_distill_pairs(void);       /* coppie lette dall'ultimo BUILD/SAMPLE */
int   gmb_distill_trainsamples(void);/* sample scritti nel train */
int   gmb_distill_holdsamples(void); /* sample scritti nell'hold */

/* SAMPLE (D1): genera k risposte a temperatura temp per ogni prompt di `prompts` (.jsonl),
 * filtra con VERIFY (verify:"rule|code|"") e scrive le coppie accettate in opts.out.
 * opts JSON: {model,tokenizer,out,k,temp,maxlen,verify,seed}. La generazione vive nella dylib
 * gmb_ai_finetune (motore MLX). 0 = ok, -1 = errore (messaggio via resp/last_error). */
int   gmb_distill_sample_opts(const char* prompts, const char* opts_json, char* resp, int cap);
int   gmb_distill_samplen(void);      /* coppie accettate dall'ultimo SAMPLE */
int   gmb_distill_samplefail(void);   /* risposte scartate dal filtro VERIFY */
double gmb_distill_sampledone(void);  /* 1 = ultimo SAMPLE completato */

/* EVAL (D4): valutazione paired idx 0 (base) / idx 1 (modello+adapter, schema LORA APPLY).
 * opts JSON: {model,ckpt,out,maxlen,repeat,temp,verify,seed}. Ritorna 0 = ok, -1 = errore. */
int   gmb_distill_eval_opts(const char* prompts, const char* opts_json, char* resp, int cap);
double gmb_distill_evaltok(int idx);  /* token mediani della config idx (0/1), -1 se n/d */
double gmb_distill_evalpass(int idx); /* pass-rate VERIFY della config idx (0/1), -1 se n/d */
double gmb_distill_evalacc(int idx);  /* acceptance rate spec-decode (D5) della config idx (0/1), -1 se n/d */

/* APPLY/RESTORE (D3): APPLY verifica l'adapter (schema LORA APPLY) e, se opts.backup e' dato,
 * salva una copia dei file del checkpoint; RESTORE li ricopia al checkpoint di origine
 * (registrato in <backup>.src). opts JSON: {model,dataset,backup,ngen}. */
int   gmb_distill_apply_opts(const char* ckpt, const char* opts_json, char* resp, int cap);
int   gmb_distill_restore_opts(const char* backup, char* resp, int cap);

/* TRAIN (D2, head-only): opts JSON {model,holdout,ckpt,rank,layer,steps,batch,lr,seed,frozen}.
 * frozen:"all-but-head" addestra l'adapter sull'head (lm_head). Espone loss/hold/ckpt/done. */
int   gmb_distill_train_opts(const char* dataset, const char* opts_json, char* resp, int cap);
double gmb_distill_loss(void);   /* loss train dell'ultimo TRAIN (-1 = n/d) */
double gmb_distill_hold(void);   /* loss hold dell'ultimo TRAIN (-1 = n/d) */
const char* gmb_distill_ckpt(void); /* path checkpoint dell'ultimo TRAIN */
double gmb_distill_done(void);   /* 1 = TRAIN completato */

const char* gmb_distill_last_error(void);
/* Report JSON come stringa statica (per AI.DISTILLSTATUS sui backend C). */
const char* gmb_distill_status(void);

/* Self-check (oracoli: parse jsonl minimale + scrittura/rilettura .bin). 0 = ok. */
int gmb_distill_selfcheck(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DISTILL_H */
