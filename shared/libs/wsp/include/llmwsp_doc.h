/* llmwsp_doc.h — W2 del piano Weight-Space Programming: WSP Document (`gmb.wsp@1`).
 *
 * Sezionamento COMPLETO di un modello denso (famiglie Qwen3/Llama in formato HF, anche MLX quantizzato):
 * albero modello -> embed / layers[L] -> {attn, mlp, norme} / final_norm / lm_head, con ogni foglia = un
 * tensore del Weight Store (nome, dtype, forma, shard, offset, byte), un RUOLO e le PARTIZIONI logiche
 * (heads, kv_heads, neurons, tokens) con cui il tensore si divide in slice indirizzabili.
 * Invariante di copertura: ogni tensore dello store compare in esattamente una foglia; Σbyte foglie = Σbyte
 * store; le partizioni tassellano esattamente la dimensione. C99 puro. */
#ifndef LLMWSP_DOC_H
#define LLMWSP_DOC_H
#include "llmwsp_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llmwsp_part {
    char    name[12];      /* "heads" | "kv_heads" | "neurons" | "tokens" */
    int     axis;          /* asse del tensore che viene partizionato */
    int64_t count;         /* numero di parti */
    int64_t size;          /* ampiezza di ogni parte lungo l'asse (count*size == dimensione) */
} llmwsp_part;

typedef struct llmwsp_leaf {
    char                *addr;      /* es. "layers.3.attn.q", "embed", "layers.0.mlp.down_scales" */
    char                 role[32];  /* "attn_q", "mlp_gate", "embed", ... oppure "unclassified" */
    int                  layer;     /* -1 se non per-layer */
    const llmwsp_tensor *t;
    int                  nparts;
    llmwsp_part          parts[2];
} llmwsp_leaf;

typedef struct llmwsp_doc llmwsp_doc;

typedef struct llmwsp_dims {
    int64_t hidden, heads, kv_heads, head_dim, inter, vocab;
    int     layers;
} llmwsp_dims;

/* Costruisce il documento da store + config.json (path del file). 0 = ok. */
int  llmwsp_doc_build(const llmwsp_store *s, const char *config_json, llmwsp_doc **out, char *err, size_t errcap);
void llmwsp_doc_free(llmwsp_doc *d);

void               llmwsp_doc_dims(const llmwsp_doc *d, llmwsp_dims *out);
const char        *llmwsp_doc_model_type(const llmwsp_doc *d);
size_t             llmwsp_doc_leaf_count(const llmwsp_doc *d);
const llmwsp_leaf *llmwsp_doc_leaf(const llmwsp_doc *d, size_t i);
const llmwsp_leaf *llmwsp_doc_find(const llmwsp_doc *d, const char *addr);
int                llmwsp_doc_layers(const llmwsp_doc *d);
size_t             llmwsp_doc_unclassified(const llmwsp_doc *d);   /* foglie senza ruolo noto (0 atteso) */

/* Invariante di copertura + completezza dei layer + coerenza forme/config. Numero di violazioni (0 = ok). */
int llmwsp_doc_check(const llmwsp_doc *d, char *err, size_t errcap);

/* Risolve un indirizzo di slice: "<foglia>" oppure "<foglia>.<partizione>.<indice>" (es.
 * "layers.3.attn.q.heads.5", "layers.0.mlp.down.neurons.100"). Restituisce 0 e riempie t/axis/start/end
 * (intervallo [start,end) lungo `axis`; per una foglia intera: axis=0, start=0, end=shape[0]). */
int llmwsp_doc_resolve(const llmwsp_doc *d, const char *addr, const llmwsp_tensor **t, int *axis,
                       int64_t *start, int64_t *end);

/* Serializza in JSON canonico (`gmb.wsp@1`); stringa allocata, da liberare con free(). NULL se errore. */
char *llmwsp_doc_to_json(const llmwsp_doc *d);

#ifdef __cplusplus
}
#endif
#endif
