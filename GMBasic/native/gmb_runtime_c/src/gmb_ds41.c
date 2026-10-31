/* gmb_ds41.c — config + classificazione tensori del DeepSeek V4.1 Flash (arch deepseek_v41).
 * MLA (q_lora/kv_lora), attenzione compressa CSA/HCA (compress_ratios), indexer (top-k), engram,
 * esperti (n_routed + shared), vision. Parser JSON minimale (sufficiente per config.json). */
#include "gmb_ds41.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* rd(const char* path, size_t* out_n) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    b[n] = 0; fclose(f); if (out_n) *out_n = (size_t)n; return b;
}
/* trova "key": e ritorna il puntatore al valore (dopo i ':'), o NULL */
static const char* jv(const char* j, const char* key) {
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = j;
    while ((p = strstr(p, pat)) != NULL) {
        const char* c = p + strlen(pat); while (*c == ' ' || *c == '\t') c++;
        if (*c == ':') { c++; while (*c == ' ' || *c == '\t') c++; return c; }
        p++;
    }
    return NULL;
}
static int jint(const char* j, const char* k, int def) { const char* v = jv(j, k); return v ? atoi(v) : def; }
static double jdbl(const char* j, const char* k, double def) { const char* v = jv(j, k); return v ? atof(v) : def; }

int gmb_ds41_config_load(const char* hf_dir, GmbDs41Config* out) {
    if (!hf_dir || !out) return -1;
    memset(out, 0, sizeof *out);
    char path[2048]; snprintf(path, sizeof path, "%s/config.json", hf_dir);
    size_t n = 0; char* j = rd(path, &n); if (!j) return -1;
    /* il testo e' dentro "text_config" (il top-level ha anche vision_config): cerchiamo li' se c'e' */
    const char* tc = jv(j, "text_config");
    const char* s = (tc && *tc == '{') ? tc : j;
    out->n_layers   = jint(s, "num_hidden_layers", 0);
    out->hidden     = jint(s, "hidden_size", 0);
    out->n_heads    = jint(s, "num_attention_heads", 0);
    out->q_lora     = jint(s, "q_lora_rank", 0);
    out->kv_lora    = jint(s, "kv_lora_rank", 0);
    out->qk_rope    = jint(s, "qk_rope_head_dim", 0);
    out->v_head     = jint(s, "v_head_dim", 0);
    out->n_experts  = jint(s, "n_routed_experts", 0);
    out->top_k      = jint(s, "num_experts_per_tok", 0);
    out->n_shared   = jint(s, "n_shared_experts", 0);
    out->moe_inter  = jint(s, "moe_intermediate_size", 0);
    out->dense_inter= jint(s, "intermediate_size", 0);
    out->vocab      = jint(s, "vocab_size", 0);
    out->index_topk = jint(s, "index_topk", 0);
    out->o_lora   = jint(s, "o_lora_rank", 0);
    out->head_dim = jint(s, "head_dim", 0);
    out->n_kv     = jint(s, "num_key_value_heads", 0);
    out->compress_rope_theta = jint(s, "compress_rope_theta", 0);
    out->index_head_dim = jint(s, "index_head_dim", 0);
    out->index_n_heads  = jint(s, "index_n_heads", 0);
    out->index_topk2    = jint(s, "index_topk", 0);
    out->engram_vocab    = jint(s, "engram_compressed_vocab_size", 0);
    out->engram_head_dim = jint(s, "engram_head_dim", 0);
    out->engram_n_heads  = jint(s, "engram_n_heads", 0);
    out->dspark_experts  = jint(s, "dspark_n_routed_experts", 0);
    out->dspark_top_k    = jint(s, "dspark_num_experts_per_tok", 0);
    out->hc_sinkhorn_iters = jint(s, "hc_sinkhorn_iters", 0);
    out->rope_theta = jdbl(s, "rope_theta", 0.0);
    out->rope_factor= jdbl(s, "factor", 1.0);
    out->has_vision = (strstr(j, "vision_config") != NULL);
    /* compress_ratios: array di int */
    const char* cr = jv(s, "compress_ratios");
    if (cr && *cr == '[') { const char* p = cr + 1; while (*p && *p != ']' && out->n_compress < 64) {
        while (*p == ' ' || *p == ',') p++; if (*p == ']' || !*p) break;
        out->compress_ratios[out->n_compress++] = atoi(p); while (*p && *p != ',' && *p != ']') p++; } }
    const char* isrc = jv(s, "index_source_layer_ids");
    if (isrc && *isrc == '[') { const char* p = isrc + 1; while (*p && *p != ']' && out->n_index_src < 32) {
        while (*p == ' ' || *p == ',') p++; if (*p == ']' || !*p) break;
        out->index_src[out->n_index_src++] = atoi(p); while (*p && *p != ',' && *p != ']') p++; } }
    free(j);
    return out->n_layers > 0 ? 0 : -1;
}

const char* gmb_ds41_tensor_role(const char* name) {
    if (!name) return "OTHER";
    if (strstr(name, "vision") || strstr(name, "aligner") || strstr(name, "image_")) return "VISION";
    if (strstr(name, "attn.wq_a") || strstr(name, "attn.wq_b")) return "ATTN_Q";
    if (strstr(name, "attn.wkv")) return "ATTN_KV";
    if (strstr(name, "attn.wo_a") || strstr(name, "attn.wo_b")) return "ATTN_WO";
    if (strstr(name, "attn.compressor")) return "COMPRESSOR";
    if (strstr(name, "attn.indexer")) return "INDEXER";
    if (strstr(name, "attn_sink")) return "SINK";
    if (strstr(name, "engram")) return "ENGRAM";
    if (strstr(name, "experts.")) return "EXPERT";
    if (strstr(name, "shared")) return "SHARED";
    if (strstr(name, "norm")) return "NORM";
    if (strstr(name, "embed")) return "EMBED";
    if (strstr(name, "head.weight")) return "HEAD";
    return "OTHER";
}
