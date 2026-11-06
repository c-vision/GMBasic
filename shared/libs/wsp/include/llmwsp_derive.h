/* llmwsp_derive.h — W7 del piano Weight-Space Programming: chirurgia di architettura (modello derivato).
 *
 * Da un modello denso (Qwen3/Llama, pesi F32/BF16/F16) deriva un NUOVO checkpoint con:
 *   - LAYER: rimozione, duplicazione, riordino (mappa layer di uscita -> layer sorgente);
 *   - HEADS.PRUNE: si tolgono interi GRUPPI kv (un gruppo = 1 testa k/v + le sue teste q), così il rapporto GQA
 *     resta valido; stesso numero di gruppi in ogni layer, selezione libera per layer;
 *   - MLP.PRUNE: si tolgono neuroni dell'intermedia (righe di gate/up + colonne di down), stesso numero per layer.
 * Rinumera i nomi, rigenera config.json (num_hidden_layers, num_attention_heads, num_key_value_heads,
 * intermediate_size, head_dim se assente, layer_types) e i .gmbarch, scrive provenance.json. Il piano è VALIDATO
 * prima di scrivere; il risultato è rivalidato (WSP Document + copertura) dopo. Fuori da questo giro: VOCAB.TRIM
 * (tokenizer), EXPERT.PRUNE, cambio di hidden_size, modelli quantizzati con potatura (solo le operazioni sui layer
 * funzionano su qualunque dtype). Un derivato è un modello NUOVO: la qualità va misurata (PPL/QA), non presunta. */
#ifndef LLMWSP_DERIVE_H
#define LLMWSP_DERIVE_H
#include "llmwsp_doc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llmwsp_derive_ops {
    const int *layer_map;     /* [nl]: layer di uscita j <- layer sorgente layer_map[j]; NULL = identità */
    size_t     nl;
    const int *kv_keep;       /* [nl_uscita × kv_m]: gruppi kv tenuti (0..kv_heads-1), per layer di USCITA; NULL = nessuna potatura */
    size_t     kv_m;
    const int *neuron_keep;   /* [nl_uscita × neu_k]: neuroni tenuti (0..inter-1); NULL = nessuna potatura */
    size_t     neu_k;
} llmwsp_derive_ops;

typedef struct llmwsp_derive_report {
    size_t   out_layers, tensors;
    uint64_t bytes_before, bytes_after;
    int64_t  heads, kv_heads, inter;
} llmwsp_derive_report;

/* Solo validazione dei parametri contro il documento (nessuna scrittura). 0 = ok. */
int llmwsp_derive_validate(const llmwsp_store *s, const llmwsp_doc *d, const llmwsp_derive_ops *ops, char *err, size_t ec);

/* Valida, scrive il nuovo checkpoint (singolo model.safetensors + file accessori) in `outdir` (nuova/vuota) e
 * lo rivalida. */
int llmwsp_derive(const llmwsp_store *s, const llmwsp_doc *d, const char *srcdir, const llmwsp_derive_ops *ops,
                  const char *outdir, llmwsp_derive_report *rep, char *err, size_t ec);

/* Baseline di importanza SENZA dati (solo pesi), dichiarata come tale:
 *   neurone i: ||gate_i|| · ||up_i|| · ||down_:,i||           (score[inter])
 *   gruppo kv g: ||V_g||_F · sqrt(Σ_{h in g} ||O_h||_F²)      (score[kv_heads]) */
int llmwsp_mlp_importance(const llmwsp_store *s, const llmwsp_doc *d, int layer, double *score);
int llmwsp_kvgroup_importance(const llmwsp_store *s, const llmwsp_doc *d, int layer, double *score);
/* Indici dei k valori più alti, restituiti in ordine CRESCENTE di indice. */
void llmwsp_topk_sorted(const double *score, size_t n, size_t k, int *out);

#ifdef __cplusplus
}
#endif
#endif
