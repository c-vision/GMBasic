#ifndef GMB_DS41_H
#define GMB_DS41_H
#include <stdint.h>
typedef struct {
    int      n_layers, hidden, n_heads, q_lora, kv_lora, qk_rope, v_head;
    int      n_experts, top_k, n_shared, moe_inter, dense_inter, vocab, index_topk;
    double   rope_theta, rope_factor;
    int      o_lora, head_dim, n_kv, compress_rope_theta;
    int      index_head_dim, index_n_heads, index_topk2, index_src[32], n_index_src;
    int      engram_vocab, engram_head_dim, engram_n_heads;
    int      dspark_experts, dspark_top_k, hc_sinkhorn_iters;
    int      has_vision;
    int      compress_ratios[64]; int n_compress;
} GmbDs41Config;
/* Legge config.json del DeepSeek V4.1 Flash. 0 ok / -1. */
int gmb_ds41_config_load(const char* hf_dir, GmbDs41Config* out);
/* Classifica un tensore: "ATTN_Q", "ATTN_KV", "ATTN_WO", "COMPRESSOR", "INDEXER", "EXPERT_W1",
 * "SHARED", "NORM", "EMBED", "HEAD", "VISION", "OTHER". */
const char* gmb_ds41_tensor_role(const char* name);
#endif
