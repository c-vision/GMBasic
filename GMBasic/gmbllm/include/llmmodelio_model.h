#include <stddef.h>
#include <stdio.h>
#include "llmengine_plugin.h"
#include <stdint.h>

uint16_t llmmodelio_fp32_to_fp16(float f);
void llmmodelio_model_policy_set(const char* role, const char* fmt);
void llmmodelio_model_policy_reset(void);
/* RESET COMPLETO (2026-10-30): azzera rule-coppie, policy NOMINATE (DSL), policy attiva e
 * override/piano per-tensore. È ciò che serve per rendere ermetico un test o una gamba: lo
 * stato vive in globali di processo. `LLM POLICY RESET|CLEAR` usa questa. */
void llmmodelio_model_policy_reset_all(void);
const char* llmmodelio_model_policy_get(const char* role);
int llmmodelio_model_quantize_auto(const char* in_path, const char* out_path, double target_bpw);
int llmmodelio_model_extract(const char* in_path, const char* out_path, int64_t layer, int64_t expert);
int llmmodelio_model_merge(const char* a_path, const char* b_path, const char* out_path);
int llmmodelio_arch_resolve(const char* hf_dir);
const char* llmmodelio_arch_name(void);
const char* llmmodelio_arch_path(void);
const char* llmmodelio_arch_role_of(const char* tensor);
const char* llmmodelio_arch_fmt_of(const char* role);
void llmmodelio_arch_show(void);
int llmmodelio_arch_check(const char* hf_dir);
int llmmodelio_arch_plan(const char* hf_dir, const char* out_path);
int llmmodelio_arch_support(const char* hf_dir, char* out, size_t cap);
int llmmodelio_codec_resolve(const char* hf_dir, const char* fmt, LlmCodec* out, char* plugin_path, size_t cap);
int llmmodelio_prim_resolve(const char* hf_dir, const char* kind, LlmEnginePluginPrim* out, char* plugin_path, size_t cap);
int llmmodelio_arch_pipeline(const char* hf_dir, const char* kind, char* out, size_t cap);
void llmmodelio_arch_list(void);
int llmmodelio_arch_cmd(const char* sub, const char* a, const char* b);
/* Come sopra ma cattura il testo in out (per i backend gestiti via P/Invoke). */
int llmmodelio_arch_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap);
int llmmodelio_policy_cmd_to(const char* sub, const char* a, const char* b, const char* c, char* out, size_t cap);
int llmmodelio_policy_cmd(const char* sub, const char* a, const char* b, const char* c);
int llmmodelio_model_recover(const char* model, const char* dataset, const char* ref, char* report, size_t cap);
int llmmodelio_policy_set_item(const char* name_glob, const char* fmt);
void llmmodelio_policy_clear_items(void);
int llmmodelio_policy_item_count(void);
/* Piano per-tensore (Policy DIFF/PLAN): popola la tabella interna; accessor per gli enumeratori. */
int llmmodelio_policy_plan_to(const char* in_path, char* out, size_t cap);
int llmmodelio_policy_items_to(char* out, size_t cap);
int llmmodelio_policy_item_to(int idx, char* out, size_t cap);
/* Livello B: applica un callback (function pointer) su tutto il piano. */
int llmmodelio_policy_callback_apply(int (*decide)(const char* name, const char* role, const char* dflt, char* out, size_t cap));
/* Model compiler LLM: verbi TENSOR/KERNEL/MODULE/KV (superficie .llmarch data-driven). */
/* `c` = 4° argomento opzionale (es. il target bpw di `LLM TENSOR AUTOGATE <m>,<ds>,<bpw>`);
 * "" se assente. Il sotto-verbo lo interpreta come parametro numerico quando previsto. */
int llmmodelio_tensor_cmd(const char* sub, const char* a, const char* b, const char* c);
int llmmodelio_kernel_cmd(const char* sub, const char* a, const char* b);
int llmmodelio_module_cmd(const char* sub, const char* a, const char* b);
int llmmodelio_kv_cmd(const char* sub, const char* a, const char* b);
int llmmodelio_tensor_cmd_to(const char* sub, const char* a, const char* b, const char* c, char* out, size_t cap);
/* B2 — tokenizer/vocab come sub-verbi di `LLM TENSOR` (llmmodelio_tokenizer.c). Wiring su
 * `llmtoken` (fonte unica): TOKENIZE|TOKENS / DETOKENIZE|DETOK / VOCAB / TOKENIZER|TOKINFO.
 * `*handled`=1 se il sotto-verbo è riconosciuto (ritorna 0 ok / -1 errore); 0 altrimenti
 * (il dispatcher prosegue). L'output va su `out` (= LOUT, così la variante _to lo cattura). */
int llmmodelio_tokenizer_cmd(const char* sub, const char* a, const char* b, const char* c, FILE* out, int* handled);
/* B2 — `LLM.TOKENCOUNT(<model>,<testo>)`: numero di token (>=0) o -1. */
int llmmodelio_token_count(const char* model, const char* text);
/* B2 (2026-10-30) — versioni che RESTITUISCONO il dato (nessuna stampa): usate da LLM.TOKENS /
 * LLM.DETOKENIZE e dalle forme con INTO. Ritorno: n token (>=0) oppure -1. */
int llmmodelio_tokenize_ids_to(const char* model, const char* text, char* out, size_t cap);
int llmmodelio_detokenize_to(const char* model, const char* ids_csv, char* out, size_t cap);
/* Apre il tokenizer di <model> (dir / .json / .gguf / .model) come handle opaco `llmtoken`
 * (void*): `<dir>/tokenizer.json`, con FALLBACK SentencePiece nativo (`<dir>/tokenizer.model`).
 * NULL su errore. Esposto per i verbi che costruivano il path a mano. */
void* llmmodelio_tokenizer_open(const char* model);
/* B3 — model registry/versioning: `LLM REGISTRY <op>,<a>,<b>` (llmmodelio_registry.c).
 * *handled=1 se <op> è riconosciuto (0 ok / -1 errore). Store: work/models.registry. */
int llmmodelio_registry_cmd(const char* op, const char* a, const char* b, FILE* out, int* handled);
/* B4 — `LLM TENSOR QUANTERROR <ref>,<cand>`: errore di quantizzazione rel-L2 sui tensori F32/F16
 * comuni (dequant-free) + max|Δ| (llmmodelio_quanterror.c). */
int llmmodelio_quant_error(const char* ref, const char* cand, FILE* out, int* handled);
/* B6 — `LLM TENSOR COMPARE <a>,<b>[,<glob>]`: confronto numerico PER-TENSORE tra due GGUF
 * (rel_l2 + avg_diff% normalizzato stile antirez/gguf-tools), filtro glob opzionale.
 * Lineage/frozen-layer e breakdown per-ruolo dopo quant/surgery (llmmodelio_compare.c;
 * core in LLMEnv/LLMCodec, Apache). */
int llmmodelio_compare(const char* a, const char* b, const char* glob, FILE* out, int* handled);
/* B5 — pipeline di derivazione riproducibile (`.gmbderive`): `LLM DERIVE <recipe>[,<out>]`
 * (llmmodelio_derive.c). Esegue BASE → PASS… → OUT. *handled=1 se riconosciuto. */
int llmmodelio_derive(const char* recipe, const char* out_override, FILE* out, int* handled);
/* B5 — delta binario applicabile: `LLM MAKEPATCH <base>,<target>,<patch>` (llmmodelio_patch.c).
 * L'APPLY riusa `llmmodelio_model_add(base, patch, out)`. */
int llmmodelio_makepatch(const char* base, const char* target, const char* patch, FILE* out, int* handled);
int llmmodelio_kernel_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap);
int llmmodelio_module_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap);
int llmmodelio_kv_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap);
/* LLM MODEL CONVERT <src>, <dst>[, <fmt>] — GGUF(-2 = percorso MoE) | GGUF | MLX (codec). */
int llmmodelio_model_convert(const char* src, const char* dst, const char* fmt);
int llmmodelio_model_profile_compute(const char* model_path, const char* ids_path, const char* out_path, int64_t chunk);
int llmmodelio_model_profile(const char* model_path, const char* out_path);
int llmmodelio_model_prune(const char* in_path, const char* out_path, int64_t keep);
void llmmodelio_model_policy_show(void);
/* llmmodelio_model.h — MODEL COMPILER (namespace LLM): operazioni sui file modello. */
#ifndef LLMMODELIO_MODEL_H
#define LLMMODELIO_MODEL_H

/* Round-trip GGUF: legge metadata KV + tensor-info da `in_path` e riscrive un GGUF nuovo
 * (stessi metadata, byte dei tensori) in `out_path`. 0 ok / -1 errore. */
int llmmodelio_model_save(const char* in_path, const char* out_path);

/* Round-trip con QUANTIZE: F32 (ultima dim %32==0) -> Q8_0; altri tensori copiati. 0/-1. */
int llmmodelio_model_quantize(const char* in_path, const char* out_path);
int llmmodelio_model_ablate(const char* in_path, const char* out_path, const char* pattern);
int llmmodelio_model_remove_layer(const char* in_path, const char* out_path, const char* layer_str);
int llmmodelio_model_validate(const char* path, const char* out_path);
int llmmodelio_model_compile(const char* path, const char* out_path);
int llmmodelio_model_quality(const char* model_dir, const char* prompt, const char* out_path, const char* ref_model);
/* PPL (perplexity) del modello su un prompt/dataset, SENZA report su file: `prompt` = prompt
 * singolo, oppure path di un FILE-dataset (una riga per prompt, media). 0 ok / -1 (engine o
 * modello non disponibili). Usata da LLM REGISTRY MEASURE per le metriche automatiche. */
int llmmodelio_model_ppl(const char* model_dir, const char* prompt, double* out_ppl, int* out_tokens);
/* TRAIN EVAL GATE: scrive in `out` (cap byte) il verdetto JSON ACCEPT/REJECT-FLAG/ERROR
 * confrontando la ppl di cand vs ref. Ritorna 0 (anche su modelli mancanti → verdetto ERROR). */
int llmmodelio_model_gate(const char* ref, const char* cand, const char* prompt, char* out, int cap);
int llmmodelio_model_diff(const char* a_path, const char* b_path, const char* out_path);
int llmmodelio_model_checkpoint(const char* src, const char* store_dir, const char* name);
int llmmodelio_model_rollback(const char* store_dir, const char* name, const char* dst);
int llmmodelio_model_add(const char* base_path, const char* module_path, const char* out_path);
int llmmodelio_model_adapter(const char* base_path, const char* adapter_path, const char* out_path, const char* alpha_str);
/* MERGE su un modello MLX (dir MLX/HF): dequant->fuse->requant affine. Vedi llmmodelio_mlx_merge.c. */
int llmmodelio_mlx_adapter(const char* base_dir, const char* adapter_path, const char* out_dir, const char* alpha_str);
/* G6 — DEQUANT: modello MLX affine -> **F16 PLAIN** (pesi F16, no scales/biases, no `quantization`).
 * Prerequisito del full-parameter (MLX QuantizedMatmul::vjp non ha gradiente). */
int llmmodelio_mlx_dequant(const char* base_dir, const char* out_dir);
/* TRAIN LORA SAVE|LOAD: adapter RAW del fine-tune (<ckpt>_best_{A,B}.raw + .meta) <-> .safetensors. */
int llmmodelio_model_adapter_save(const char* ckpt_prefix, const char* out_path);
int llmmodelio_model_adapter_load(const char* adapter_path, const char* out_prefix);
int llmmodelio_model_module(const char* base_path, const char* gmmod_path, const char* module_path, const char* out_path);
int llmmodelio_model_modality(const char* base_path, const char* mmproj_path, const char* out_path);
/* Query della modality dichiarata (per consumatori: loader/scanner). 0/-1. */
int llmmodelio_model_modality_of(const char* path, char* modality, size_t mcap, char* mmproj, size_t mmcap, char* arch, size_t acap);
/* modality dichiarata come stringa statica ("" se non risolta) — per P/Invoke. */
const char* llmmodelio_model_modality_str(const char* path);

/* Q4_K weighted, stessa convenzione di llama.cpp: quant_weights contiene `weight_sets` vettori
 * consecutivi da n_per_row elementi; ogni vettore si applica a nrows/weight_sets righe. */
int llmmodelio_quantize_q4_k_weighted(const float* src, int64_t nrows, int64_t n_per_row,
                                   uint8_t* dst, const float* quant_weights, int64_t weight_sets);

/* Scrive il PIANO (per-tensore: ruolo + formato proposto) in out_path. 0/-1. */
int llmmodelio_model_plan(const char* in_path, const char* out_path);

#endif
