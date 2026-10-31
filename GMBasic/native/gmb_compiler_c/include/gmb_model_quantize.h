/* gmb_model_quantize.h — quantizza un modello HF/MLX (F32/F16/BF16) in formato MLX affine RTN. */
#ifndef GMB_MODEL_QUANTIZE_H
#define GMB_MODEL_QUANTIZE_H

#include <stdint.h>

/* Converte `src_dir` (HF/MLX safetensors) in `out_dir` con pesi 2D quantizzati a `bits` (2..8)
 * per gruppo `group` (layout MLX affine: weight U32 + scales/biases F16). Copia norm/bias e i file
 * di config/tokenizer (con `quantization` iniettato in config.json). Ritorna 0 su successo. */
int gmb_model_quantize(const char* src_dir, const char* out_dir, uint32_t bits, uint32_t group, int verbose);

/* §E2 step 3: variante con `imatrix_path` (report DS4) ed `expert_bits` dedicato per gli esperti
 * 3D. Con imatrix, l'errore di quantizzazione degli esperti e' pesato per-colonna (gate/up = prime
 * H energie del layer/expert; down = ultime M). `expert_bits` 0/invalido -> usa `bits`. */
int gmb_model_quantize_ex(const char* src_dir, const char* out_dir, uint32_t bits, uint32_t group,
                          const char* imatrix_path, uint32_t expert_bits, int verbose);

/* Mirror: converte una dir MLX affine (weight U32 + scales/biases) in f16 (bits/group da config.json;
 * la chiave `quantization` viene rimossa dal config). Ritorna 0 su successo. */
int gmb_model_dequantize(const char* src_dir, const char* out_dir, int verbose);

/* §E3: mixed-precision per-layer guidata dalla SENSIBILITA' (stile mlx-optiq `static`). Misura
 * l'errore relativo di ogni tensore 2D `.weight` quantizzabile ai due candidati `bits_lo`/`bits_hi`
 * (`llmengine_moe_quant_rel_error`), alloca i bit per-tensore con `gmb_quant_alloc_bits` sotto il budget medio
 * `target_bpw`, quantizza ogni tensore al bit assegnato ed emette `config.json` con `quantization`
 * per-path + `quantization_config` (formato MLX, come `quantize_model` di mlx_lm). I default
 * {4,8}@g64 sono DERIVABILI dall'engine per rapporto weight/scales (nessun cambio al loader).
 * Ritorna 0 su successo (stampa la mappa bit assegnata). */
int gmb_model_quantize_auto(const char* src_dir, const char* out_dir, uint32_t group,
                            uint32_t bits_lo, uint32_t bits_hi, double target_bpw, int verbose);

/* §E4a (Q2-b): come `gmb_model_quantize_auto`, ma la sensibilità per-tensore è PESATA
 * dall'importanza per-colonna misurata sul modello (`imatrix_path`, report DS4). Per i tensori
 * del FFN il peso è l'energia di attivazione che li alimenta (gate_proj/up_proj = prime `cols`
 * energie della riga layer/0; down_proj = ultime `cols`, uscita SwiGLU); gli altri tensori (es.
 * attention, non coperti dal report) restano non pesati -> comportamento di `_auto`.
 * `imatrix_path` NULL/vuoto = identico a `gmb_model_quantize_auto`. */
int gmb_model_quantize_auto_ex(const char* src_dir, const char* out_dir, uint32_t group,
                               uint32_t bits_lo, uint32_t bits_hi, double target_bpw,
                               const char* imatrix_path, int verbose);

#endif
