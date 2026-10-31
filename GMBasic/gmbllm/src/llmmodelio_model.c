/* llmmodelio_model.c — MODEL COMPILER (namespace LLM): operazioni sui file modello.
 *
 * Primo comportamento reale: `llmmodelio_model_save(in, out)` — round-trip GGUF (legge metadata
 * KV + tensor-info dall'input e RISCRIVE un GGUF nuovo con gli stessi metadata e i byte dei
 * tensori). È il mattone di `LLM MODEL SAVE` (metodo antirez: template = metadata dall'input,
 * byte rigenerati). §0bis: comportamento reale, niente stub.
 */
#include "llmmodelio_model.h"
#include "quant/gmbllm_quant.h"    /* API PUL encode (produce) */
#include "llmcodec_ggml_quants.h"      /* llmcodec_ggml_type_of_fmt, LLMCODEC_GGML_T_* */
#include "llmcodec_fp16.h"
#include "llmcodec_ggml_quants.h"   /* helper K-quant ggml (CORE codec, §0bis) */
#include <sys/stat.h>
#include "llmcodec_gguf_mlx.h"   /* llmcodec_conv_mlx_to_gguf_arch_ex / llmcodec_conv_gguf_to_mlx_arch (progetto codec) */
#include "llmtoken.h"   /* unica sorgente tokenizer.json (export per i metadata GGUF) */
#include "llmcodec_gguf_writer.h"
#include "llmcodec_safetensors.h"   /* adapter LoRA (.safetensors) */
#include "llmcodec_iq2_xxs.h"
#include "llmmodelio_imatrix.h"
#include "llmmodelio_infer.h"
/* Policy DSL (Livello A): decisione per-tensore definita piu' sotto (policy NOMINATA a regole). */
static const char* pol_decide(const char* role, const char* name, int layer, const char* arch, double imp);
#include "llmengine_moe_quant_alloc.h"




/* AUTO (§E3): tabella formato per-tensore decisa dall'allocatore, consultata dal quantize. */
static const int* g_auto_type = (const int*)0;
double llmmodelio_roundtrip_rel(int ty, const float* x, uint64_t numel, uint32_t cols, const float* imp);
double llmmodelio_roundtrip_rel_cov(int ty, const float* x, uint64_t numel, uint32_t cols,
                             const float* samp, int64_t n_samp, int64_t rows_per_expert);

#include <stdio.h>

/* Output delle funzioni di ispezione (ARCH/POLICY): di norma stdout; se `g_llm_out` e' impostato
 * (fmemopen) la scrittura va nel buffer — cosi' i backend gestiti (IL/PCodeVM via P/Invoke), che
 * non catturano lo stdout nativo del processo C, ricevono il testo e lo stampano col proprio
 * console. Stesso comportamento dei backend C (VM/C, C99*), che stampano diretto. */
static FILE* g_llm_out = NULL;
#define LOUT (g_llm_out ? g_llm_out : stdout)
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>   /* LLM QUALITY: engine MLX via dlopen (dipendenza debole) */   /* mkdir (checkpoint store) */   /* getcwd (arch-resolution da file .gguf) */

static uint64_t llm_numel(const uint64_t* dims, int ndim) {
    uint64_t numel = 1; for (int d = 0; d < ndim; d++) numel *= dims[d]; return numel;
}
/* 0 = "non calcolabile" (tipo ignoto o numel non multiplo del blocco): i chiamatori qui sotto
 * già trattano 0 come errore (`b == 0 -> rc = -1`), e la distinzione fra le due cause non serve
 * a loro — serve a chi deve DARE un messaggio, che chiama llmcodec_ggml_type_bytes direttamente. */
static uint64_t llm_ts_bytes(int ggml_type, const uint64_t* dims, int ndim) {
    uint64_t numel = 1, b = 0;
    for (int d = 0; d < ndim; d++) numel *= dims[d];
    return llmcodec_ggml_type_bytes(ggml_type, numel, &b) == 0 ? b : 0;
}

static int llmmodelio_arch_resolve_gguf(const char* path);
int llmmodelio_model_save(const char* in_path, const char* out_path) {
    if (in_path == NULL || out_path == NULL) return -1;
    /* Residuo minore 2026-10-01: SAVE <src>, <dst> con dst "MLX" (dir: suffisso .mlx, trailing '/'
     * o directory esistente) -> EMISSIONE MLX DIRETTA (via codec), niente round-trip GGUF. */
    { size_t lo = strlen(out_path);
      int dst_mlx = (lo >= 4 && strcasecmp(out_path + lo - 4, ".mlx") == 0)
                 || (lo >= 1 && out_path[lo-1] == '/');
      if (!dst_mlx && lo > 0) { struct stat _st; if (stat(out_path, &_st) == 0 && S_ISDIR(_st.st_mode)) dst_mlx = 1; }
      if (dst_mlx) {
          if (llmmodelio_arch_resolve_gguf(in_path) != 0) return -1;
          return llmcodec_conv_gguf_to_mlx_arch(in_path, out_path, llmmodelio_arch_path(), 32, 4);
      } }

    LlmCodecGgufFileInfo fi;
    LlmCodecGgufMetaKVOwned* kv = NULL;
    int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }

    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)fi.n_tensors * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    if (ts == NULL || dims == NULL || wkv == NULL) {
        free(ts); free(dims); free(wkv); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1;
    }
    FILE* src = fopen(in_path, "rb");
    if (src == NULL) { free(ts); free(dims); free(wkv); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }

    int rc = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        ts[i].name = ti->name;
        ts[i].ggml_type = ti->ggml_type;
        ts[i].ndim = ti->ndim;
        ts[i].dims = dims + i * 8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[i].dims)[d] = ti->dims[d];
        uint64_t b = llm_ts_bytes(ti->ggml_type, ti->dims, ti->ndim);
        if (b == 0 || b > (1ull << 34)) { rc = -1; break; }   /* tipo non supportato / >16GB */
        void* buf = malloc((size_t)b);
        if (buf == NULL) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 ||
            fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        ((LlmCodecGgufTensor*)ts)[i].data = buf;   /* ownership: liberato sotto */
    }
    if (rc == 0) for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)fi.n_tensors, 32);

    for (uint64_t i = 0; i < fi.n_tensors; i++) free((void*)ts[i].data);
    fclose(src);
    free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv);
    llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}

/* ---- QUANTIZE: F32 -> Q8_0 (block_q8_0: F16 scale + 32 int8 = 34 byte / 32 valori) ---- */
#include <math.h>

/* h2f: delega all'atomo header-only di llmcodec_fp16.h (§0bis: una sola sorgente). */
static float llmmodelio_h2f(uint16_t h) { return llmcodec_f16_to_f32(h); }
uint16_t llmmodelio_fp32_to_fp16(float f);

uint16_t llmmodelio_fp32_to_fp16(float f) { return llmcodec_f16_from_f32(f); }

/* q8_0: delega al codec GGML (§0bis). round-to-nearest di llama.cpp, non lrintf. */
static void llmmodelio_q8_0(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q8_0, x, out, n, NULL);
}

/* Round-trip con QUANTIZE: i tensori F32 con ultima dim multipla di 32 -> Q8_0 (scritti in
 * formato GGUF Q8_0), gli altri copiati invariati. 0 ok / -1. */
static const char* llmmodelio_role_of(const char* name);   /* definita sotto (PLAN) */
/* MXFP4 (OCP microscaling, ggml tipo 39): blocco = {e:E8M0, qs[16]} = 17B/32.
 * Port da quantize_row_mxfp4_ref: e = floor(log2(amax))-2+127; d = 2^(e-128) (kvalues sono DOUBLED);
 * q = argmin |kvalues[i]*d - x|; packing a META' (low=j, high=j+16). */
/* NVFP4 (ggml tipo 40): blocco = {d[4]:UE4M3, qs[32]} = 36B/64; 4 sotto-blocchi di 16; scala UE4M3
 * (E4M3 unsigned) con amax/6; stesso lookup FP4 di MXFP4. Port di quantize_row_nvfp4_ref + le UE4M3
 * da ggml-impl.h (verbatim). */

static uint8_t llmmodelio_f32_to_ue4m3(float x) { return llmcodec_ue4m3_from_f32(x); }
/* nvfp4: delega al codec GGML (§0bis). */
static void llmmodelio_nvfp4(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_NVFP4, x, out, n, NULL);
}
/* mxfp4: delega al codec GGML (§0bis). */
static void llmmodelio_mxfp4(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_MXFP4, x, out, n, NULL);
}
/* bf16: delega all'atomo header-only di llmcodec_fp16.h (§0bis: una sola sorgente). */
static void llmmodelio_bf16(const float* x, int64_t n, uint8_t* out) {
    uint16_t* o = (uint16_t*)out;
    for (int64_t i = 0; i < n; i++) o[i] = llmcodec_bf16_from_f32(x[i]);
}
static void llmmodelio_q4_0(const float* x, int64_t n, uint8_t* out);   /* definita sotto */
static void llm_q4_K(const float* x, int64_t n, uint8_t* out);   /* definita sotto */
static void llm_q2_K(const float* x, int64_t n, uint8_t* out);   /* definita sotto */
static void llm_q6_K(const float* x, int64_t n, uint8_t* out);
static void llm_q5_K(const float* x, int64_t n, uint8_t* out);
static void llm_q3_K(const float* x, int64_t n, uint8_t* out);
static int llmmodelio_K_imatrix(const float* x, int64_t n, uint8_t* out, const float* qw,
                         int64_t row_len, int64_t weight_sets, int bpb, int ggml_type);
/* ABLATE (model surgery: MODIFICARE) — azzera i tensori il cui nome contiene `pattern`
 * (per-ruolo/esperto, es. "wo_b", "layers.17.mlp"). Byte-zero: F32/F16 -> 0.0 esatto;
 * quantizzati -> scale 0 -> dequant ~0. Scrive un GGUF nuovo (metadata preservati).
 * Se nessun tensore combacia -> -1 (mai un risultato silenzioso). */
static int model_layer_of(const char* name);
/* RIMUOVERE LAYER (§4.1): elimina tutti i tensori del layer N + RINUMERA i layer > N (shift -1)
 * + aggiorna block_count. Produce un modello con depth-1 (il gate QUALITY misura il degrado). */
static int llm_rename_layer(const char* name, int from_layer, char* out, size_t cap) {
    /* trova "layers.<N>." oppure "blk.<N>." e sostituisce N con N-1 (se N > from_layer). */
    const char* pats[2] = { "layers.", "blk." };
    for (int k = 0; k < 2; k++) {
        const char* p = strstr(name, pats[k]); if (!p) continue;
        size_t off = (size_t)(p - name) + strlen(pats[k]);
        char* end = NULL; long idx = strtol(name + off, &end, 10);
        if (end == name + off) continue;
        if (idx <= from_layer) { snprintf(out, cap, "%s", name); return 0; }   /* tieni (o sara' scartato se ==) */
        snprintf(out, cap, "%.*s%ld%s", (int)off, name, idx - 1, end);
        return 0;
    }
    snprintf(out, cap, "%s", name); return 0;
}
int llmmodelio_model_remove_layer(const char* in_path, const char* out_path, const char* layer_str) {
    int layer = (layer_str && *layer_str) ? atoi(layer_str) : -1;
    if (in_path == NULL || out_path == NULL || layer < 0) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)fi.n_tensors * 8, sizeof(uint64_t));
    char (*names)[256] = (char(*)[256])calloc((size_t)fi.n_tensors, 256);
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (!ts || !dims || !names || !wkv || !src) { free(ts); free(dims); free(names); free(wkv); if (src) fclose(src); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int rc = 0; uint64_t out_n = 0; int removed = 0, shift_count = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        int L = model_layer_of(ti->name);
        if (L == layer) { removed++; continue; }   /* SCARTA il layer N */
        uint64_t numel = llm_numel(ti->dims, ti->ndim); uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (!buf) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        if (L > layer) shift_count++;
        llm_rename_layer(ti->name, layer, names[out_n], 256);
        ts[out_n].name = names[out_n]; ts[out_n].ggml_type = ti->ggml_type; ts[out_n].ndim = ti->ndim; ts[out_n].dims = dims + out_n*8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
        ts[out_n].data = buf; out_n++;
    }
    if (rc == 0 && removed == 0) rc = -1;   /* layer inesistente -> errore */
    if (rc == 0) for (int i = 0; i < nkv; i++) {
        llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
        /* aggiorna block_count (-1) se presente */
        if (strstr(kv[i].key, "block_count") && wkv[i].type == 4) { wkv[i].u32 = (kv[i].u32 > 0) ? kv[i].u32 - 1 : kv[i].u32; }
    }
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)out_n, 32);
    for (uint64_t i = 0; i < out_n; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(names); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    (void)shift_count;
    return rc == 0 ? 0 : -1;
}
/* L7/G4a — PRUNING STRUTTURATO delle TESTE di attenzione (GGUF). Tiene le prime `n_keep` teste.
 * q: ne[1]=H*D -> prefisso contiguo; attn_output: ne[0]=H*D -> copia per-colonna; k/v: ne[1]=Hkv*D.
 * Aggiorna attention.head_count / head_count_kv. F16/F32. Deterministico, testabile su fixture. */
int llmmodelio_model_prune_heads(const char* in_path, const char* out_path, int n_keep) {
    if (in_path == NULL || out_path == NULL || n_keep <= 0) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int H = 0, Hkv = 0, D = 0, EMB = 0;
    for (int i = 0; i < nkv; i++) {
        if (kv[i].type != 4) continue;
        if (strstr(kv[i].key, "attention.head_count_kv")) Hkv = (int)kv[i].u32;
        else if (strstr(kv[i].key, "attention.head_count")) H = (int)kv[i].u32;
        if (strstr(kv[i].key, "attention.key_length")) D = (int)kv[i].u32;
        if (strstr(kv[i].key, "embedding_length") && !strstr(kv[i].key, "_out")) EMB = (int)kv[i].u32;
    }
    /* head_dim non dichiarato (es. OLMoE): derivalo da hidden/n_heads. */
    if (D <= 0 && H > 0 && EMB > 0) D = EMB / H;
    if (H <= 0 || D <= 0 || n_keep > H) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    if (Hkv <= 0 || Hkv > H) Hkv = H;
    int n_keep_kv = (Hkv == H) ? n_keep : (n_keep * Hkv) / H; if (n_keep_kv < 1) n_keep_kv = 1;
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors + 1, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc(((size_t)fi.n_tensors + 1) * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)nkv + 4, sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (!ts || !dims || !wkv || !src) { free(ts); free(dims); free(wkv); if (src) fclose(src); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int rc = 0; uint64_t out_n = 0; int pruned = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (!buf) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        int is_q = (strstr(ti->name, "attn_q.") != NULL);
        int is_k = (strstr(ti->name, "attn_k.") != NULL);
        int is_v = (strstr(ti->name, "attn_v.") != NULL);
        int is_o = (strstr(ti->name, "attn_output") != NULL);
        int is_qn = (strstr(ti->name, "attn_q_norm") != NULL);
        int is_kn = (strstr(ti->name, "attn_k_norm") != NULL);
        int es = (ti->ggml_type == LLMCODEC_GGML_T_F32) ? 4 : (ti->ggml_type == LLMCODEC_GGML_T_F16 ? 2 : 0);
        void* nb = buf; int free_out = 0;
        /* QK-norm per-head (1D, [H*D] q / [Hkv*D] k): va tagliata con le teste, altrimenti la
         * forward (che applica la norm per testa) legge un tensore di lunghezza sbagliata -> il
         * modello NON esegue. (QLoE/OLMoE/…) */
        if (es && ti->ndim == 1) {
            uint64_t k = 0;
            if (is_qn && (int)ti->dims[0] == H * D) k = (uint64_t)n_keep * D;
            else if (is_kn && (int)ti->dims[0] == Hkv * D) k = (uint64_t)n_keep_kv * D;
            if (k > 0) { uint64_t nbytes = k * es; nb = malloc((size_t)nbytes); free_out = 1;
                if (nb) memcpy(nb, buf, (size_t)nbytes); else rc = -1; }
        }
        if (rc == 0 && es && ti->ndim == 2) {
            uint64_t ne0 = ti->dims[0], ne1 = ti->dims[1];
            if (is_q && (int)ne1 == H * D) {                 /* q: slice ne1 (prefisso contiguo) */
                uint64_t k = (uint64_t)n_keep * D; uint64_t nbytes = k * ne0 * es;
                nb = malloc((size_t)nbytes); free_out = 1;
                if (nb) memcpy(nb, buf, (size_t)nbytes); else rc = -1;
            } else if (is_o && (int)ne0 == H * D) {          /* o: slice ne0 (per-colonna) */
                uint64_t k = (uint64_t)n_keep * D; uint64_t nbytes = ne1 * k * es;
                nb = malloc((size_t)nbytes); free_out = 1;
                if (nb) { for (uint64_t c = 0; c < ne1; c++) memcpy((uint8_t*)nb + c*k*es, (uint8_t*)buf + c*ne0*es, (size_t)(k*es)); } else rc = -1;
            } else if ((is_k || is_v) && (int)ne1 == Hkv * D) {  /* k/v: slice ne1 */
                uint64_t k = (uint64_t)n_keep_kv * D; uint64_t nbytes = k * ne0 * es;
                nb = malloc((size_t)nbytes); free_out = 1;
                if (nb) memcpy(nb, buf, (size_t)nbytes); else rc = -1;
            }
        }
        if (rc != 0) { if (free_out) free(nb); free(buf); break; }
        ts[out_n].name = ti->name; ts[out_n].ggml_type = ti->ggml_type; ts[out_n].ndim = ti->ndim; ts[out_n].dims = dims + out_n*8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
        if (free_out) {
            if (is_q) ((uint64_t*)ts[out_n].dims)[1] = (uint64_t)n_keep * D;
            else if (is_o) ((uint64_t*)ts[out_n].dims)[0] = (uint64_t)n_keep * D;
            else if (is_qn) ((uint64_t*)ts[out_n].dims)[0] = (uint64_t)n_keep * D;
            else if (is_kn) ((uint64_t*)ts[out_n].dims)[0] = (uint64_t)n_keep_kv * D;
            else ((uint64_t*)ts[out_n].dims)[1] = (uint64_t)n_keep_kv * D;
            ts[out_n].data = nb; free(buf); pruned++;
        } else ts[out_n].data = buf;
        out_n++;
    }
    if (rc == 0 && pruned == 0) rc = -1;   /* nessuna testa toccata -> errore */
    int nkv_out = nkv;
    if (rc == 0) {
        int have_kl = 0, have_vl = 0; char arch[64] = "";
        for (int i = 0; i < nkv; i++) {
            llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
            if (strstr(kv[i].key, "attention.head_count_kv") && wkv[i].type == 4) wkv[i].u32 = (uint32_t)n_keep_kv;
            else if (strstr(kv[i].key, "attention.head_count") && wkv[i].type == 4) wkv[i].u32 = (uint32_t)n_keep;
            if (strstr(kv[i].key, "attention.key_length")) have_kl = 1;
            if (strstr(kv[i].key, "attention.value_length")) have_vl = 1;
            const char* dot = strchr(kv[i].key, '.');
            if (arch[0] == 0 && dot && strstr(kv[i].key, "attention.head_count")) { size_t l = (size_t)(dot - kv[i].key); if (l < 63) { memcpy(arch, kv[i].key, l); arch[l] = 0; } }
        }
        /* head_dim: se `key_length`/`value_length` mancano, il motore lo deriva = hidden/head_count
         * -> dopo il prune (head_count ridotto) sarebbe SBAGLIATO (reshape mismatch). Scrivilo
         * esplicito col valore ORIGINALE D. */
        if (!have_kl && arch[0]) { char* kk = (char*)malloc(96); snprintf(kk, 96, "%s.attention.key_length", arch); wkv[nkv_out].key = kk; wkv[nkv_out].type = 4; wkv[nkv_out].u32 = (uint32_t)D; nkv_out++; }
        if (!have_vl && arch[0]) { char* kk = (char*)malloc(96); snprintf(kk, 96, "%s.attention.value_length", arch); wkv[nkv_out].key = kk; wkv[nkv_out].type = 4; wkv[nkv_out].u32 = (uint32_t)D; nkv_out++; }
    }
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv_out, ts, (int)out_n, 32);
    for (uint64_t i = 0; i < out_n; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}
/* G4b — MoE -> DENSE: fondi gli E esperti in UN FFN denso (media sugli esperti), elimina il router
 * (ffn_gate_inp) e imposta expert_count=1. Tensor `*_exps` -> `ffn_gate/up/down.weight` (E-asse = ultima
 * dim). F16/F32. Cosi' un MoE diventa un modello denso piu' piccolo ("smontare i pezzi"). */
int llmmodelio_model_moe_to_dense(const char* in_path, const char* out_path) {
    if (in_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    uint64_t E = 0;
    for (int i = 0; i < nkv; i++) if (kv[i].type == 4 && strstr(kv[i].key, "expert_count") && !strstr(kv[i].key, "expert_used")) E = kv[i].u32;
    if (E == 0) for (uint64_t i = 0; i < fi.n_tensors; i++) if (strstr(fi.tensors[i].name, "_exps") && fi.tensors[i].ndim >= 3) { E = fi.tensors[i].dims[fi.tensors[i].ndim-1]; break; }
    if (E < 2) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors + 1, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc(((size_t)fi.n_tensors + 1) * 8, sizeof(uint64_t));
    char (*names)[256] = (char(*)[256])calloc((size_t)fi.n_tensors + 1, 256);
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (!ts || !dims || !names || !wkv || !src) { free(ts); free(dims); free(names); free(wkv); if (src) fclose(src); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int rc = 0; uint64_t out_n = 0; int dense_layers = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        if (strstr(ti->name, "ffn_gate_inp")) continue;   /* ROUTER: eliminato */
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (!buf) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        int es = (ti->ggml_type == LLMCODEC_GGML_T_F32) ? 4 : (ti->ggml_type == LLMCODEC_GGML_T_F16 ? 2 : 0);
        int is_exps = (strstr(ti->name, "_exps") != NULL) && ti->ndim >= 3 && es > 0 && ti->dims[ti->ndim-1] == E;
        ts[out_n].ggml_type = ti->ggml_type; ts[out_n].ndim = ti->ndim; ts[out_n].dims = dims + out_n*8;
        if (is_exps) {   /* media sugli esperti (asse = ultima dim): [d0..d_{n-2}, E] -> [d0..d_{n-2}] */
            uint64_t per = 1; for (int d = 0; d < ti->ndim - 1; d++) per *= ti->dims[d];
            uint64_t nb = per * (uint64_t)es;
            void* out = malloc((size_t)nb);
            if (!out) { free(buf); rc = -1; break; }
            const uint8_t* s = (const uint8_t*)buf; uint8_t* o = (uint8_t*)out;
            if (es == 4) { const float* sf = (const float*)buf; float* of = (float*)out; for (uint64_t p = 0; p < per; p++) { double s2 = 0; for (uint64_t e = 0; e < E; e++) s2 += sf[p + e*per]; of[p] = (float)(s2 / (double)E); } }
            else { const uint16_t* sh = (const uint16_t*)buf; uint16_t* oh = (uint16_t*)out; for (uint64_t p = 0; p < per; p++) { double s2 = 0; for (uint64_t e = 0; e < E; e++) s2 += llmcodec_f16_to_f32(sh[p + e*per]); oh[p] = llmcodec_f16_from_f32((float)(s2 / (double)E)); } }
            (void)s; (void)o;
            const char* ex = strstr(ti->name, "_exps"); size_t pre = (size_t)(ex - ti->name);
            memcpy(names[out_n], ti->name, pre); snprintf(names[out_n] + pre, 256 - pre, "%s", ex + 5);   /* "_exps" rimosso */
            ts[out_n].name = names[out_n];
            ts[out_n].ndim = ti->ndim - 1;
            for (int d = 0; d < ti->ndim - 1; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
            ts[out_n].data = out; free(buf); dense_layers++;
        } else {
            ts[out_n].name = ti->name;
            for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
            ts[out_n].data = buf;
        }
        out_n++;
    }
    if (rc == 0 && dense_layers == 0) rc = -1;   /* nessun MoE -> errore */
    /* MOEDENSE -> controparte DENSE: il forward MoE cercherebbe `ffn_gate_exps` (eliminati) -> il
     * modello NON eseguirebbe. Cambia `general.architecture` alla dense (l'attention deve combaciare)
     * e rinomina le chiavi `<arch>.*`. Default: qwen3_moe->qwen3, olmoe->qwen3 (stessa attention);
     * override POLICY "MOEDENSE_ARCH". */
    char orig_arch[64] = "", dense_arch[64] = "";
    for (int i = 0; i < nkv; i++) if (strcmp(kv[i].key, "general.architecture") == 0 && kv[i].type == 8 && kv[i].str) { snprintf(orig_arch, sizeof orig_arch, "%s", kv[i].str); break; }
    if (orig_arch[0]) {
        const char* ov = llmmodelio_model_policy_get("MOEDENSE_ARCH");
        if (ov && *ov) snprintf(dense_arch, sizeof dense_arch, "%s", ov);
        else if (strcmp(orig_arch, "olmoe") == 0 || strcmp(orig_arch, "qwen3_moe") == 0) snprintf(dense_arch, sizeof dense_arch, "%s", "qwen3");
        else { size_t l = strlen(orig_arch); if (l > 4 && strcmp(orig_arch + l - 4, "_moe") == 0) { snprintf(dense_arch, sizeof dense_arch, "%.*s", (int)(l - 4), orig_arch); } }
    }
    if (rc == 0) for (int i = 0; i < nkv; i++) {
        llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
        if (wkv[i].type == 4 && strstr(kv[i].key, "expert_count") && !strstr(kv[i].key, "expert_used")) wkv[i].u32 = 1;
        else if (wkv[i].type == 4 && strstr(kv[i].key, "expert_used_count")) wkv[i].u32 = 1;
        if (dense_arch[0] && orig_arch[0] && strcmp(dense_arch, orig_arch) != 0) {
            size_t ol = strlen(orig_arch);
            if (strcmp(kv[i].key, "general.architecture") == 0 && wkv[i].type == 8) { char* s = (char*)malloc(64); snprintf(s, 64, "%s", dense_arch); wkv[i].str = s; }
            else if (strncmp(kv[i].key, orig_arch, ol) == 0 && kv[i].key[ol] == '.') { size_t l = strlen(kv[i].key) + 64; char* nk = (char*)malloc(l); snprintf(nk, l, "%s%s", dense_arch, kv[i].key + ol); wkv[i].key = nk; }
        }
    }
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)out_n, 32);
    for (uint64_t i = 0; i < out_n; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(names); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}
/* G4b-variant (antirez/gguf-tools `split-mixtral`, MIT): MoE -> DENSE estraendo UN esperto per
 * layer (non la MEDIA come moe_to_dense). `spec`: ""|"0" -> esperto 0 ovunque; "d" -> esperto d
 * ovunque; "d0d1d2..." -> UNA cifra per LAYER (l'ultima si ripete per i layer mancanti), stile
 * antirez. Router (`ffn_gate_inp`) rimosso, `*_exps` -> `ffn_{gate,up,down}`, arch densa. F16/F32. */
int llmmodelio_model_dense_expert(const char* in_path, const char* out_path, const char* spec) {
    if (in_path == NULL || out_path == NULL) return -1;
    int speclen = spec ? (int)strlen(spec) : 0;
    int single = (speclen <= 1);
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    uint64_t E = 0;
    for (int i = 0; i < nkv; i++) if (kv[i].type == 4 && strstr(kv[i].key, "expert_count") && !strstr(kv[i].key, "expert_used")) E = kv[i].u32;
    if (E == 0) for (uint64_t i = 0; i < fi.n_tensors; i++) if (strstr(fi.tensors[i].name, "_exps") && fi.tensors[i].ndim >= 3) { E = fi.tensors[i].dims[fi.tensors[i].ndim-1]; break; }
    if (E < 2) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors + 1, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc(((size_t)fi.n_tensors + 1) * 8, sizeof(uint64_t));
    char (*names)[256] = (char(*)[256])calloc((size_t)fi.n_tensors + 1, 256);
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (!ts || !dims || !names || !wkv || !src) { free(ts); free(dims); free(names); free(wkv); if (src) fclose(src); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int rc = 0; uint64_t out_n = 0; int dense_layers = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        if (strstr(ti->name, "ffn_gate_inp")) continue;   /* ROUTER: eliminato */
        uint64_t b = llmcodec_gguf_tensor_bytes(ti);
        if (b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (!buf) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        int es = (ti->ggml_type == LLMCODEC_GGML_T_F32) ? 4 : (ti->ggml_type == LLMCODEC_GGML_T_F16 ? 2 : 0);
        int is_exps = (strstr(ti->name, "_exps") != NULL) && ti->ndim >= 3 && es > 0 && ti->dims[ti->ndim-1] == E;
        ts[out_n].ggml_type = ti->ggml_type; ts[out_n].ndim = ti->ndim; ts[out_n].dims = dims + out_n*8;
        if (is_exps) {
            /* indice esperto per QUESTO layer (blk.N.) dalla spec */
            int layer = 0; { const char* bp = strstr(ti->name, "blk."); if (bp) layer = atoi(bp + 4); }
            int e = 0;
            if (speclen == 0) e = 0;
            else if (single) e = spec[0] - '0';
            else e = (layer < speclen ? spec[layer] - '0' : spec[speclen-1] - '0');
            if (e < 0) e = 0; if ((uint64_t)e >= E) e = (int)E - 1;
            uint64_t per = 1; for (int d = 0; d < ti->ndim - 1; d++) per *= ti->dims[d];
            uint64_t nb = per * (uint64_t)es;
            void* out = malloc((size_t)nb);
            if (!out) { free(buf); rc = -1; break; }
            memcpy(out, (const uint8_t*)buf + (uint64_t)e * per * (uint64_t)es, (size_t)nb);   /* SELEZIONE, non media */
            const char* ex = strstr(ti->name, "_exps"); size_t pre = (size_t)(ex - ti->name);
            memcpy(names[out_n], ti->name, pre); snprintf(names[out_n] + pre, 256 - pre, "%s", ex + 5);
            ts[out_n].name = names[out_n];
            ts[out_n].ndim = ti->ndim - 1;
            for (int d = 0; d < ti->ndim - 1; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
            ts[out_n].data = out; free(buf); dense_layers++;
        } else {
            ts[out_n].name = ti->name;
            for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
            ts[out_n].data = buf;
        }
        out_n++;
    }
    if (rc == 0 && dense_layers == 0) rc = -1;
    char orig_arch[64] = "", dense_arch[64] = "";
    for (int i = 0; i < nkv; i++) if (strcmp(kv[i].key, "general.architecture") == 0 && kv[i].type == 8 && kv[i].str) { snprintf(orig_arch, sizeof orig_arch, "%s", kv[i].str); break; }
    if (orig_arch[0]) {
        const char* ov = llmmodelio_model_policy_get("MOEDENSE_ARCH");
        if (ov && *ov) snprintf(dense_arch, sizeof dense_arch, "%s", ov);
        else if (strcmp(orig_arch, "olmoe") == 0 || strcmp(orig_arch, "qwen3_moe") == 0) snprintf(dense_arch, sizeof dense_arch, "%s", "qwen3");
        else { size_t l = strlen(orig_arch); if (l > 4 && strcmp(orig_arch + l - 4, "_moe") == 0) snprintf(dense_arch, sizeof dense_arch, "%.*s", (int)(l - 4), orig_arch); }
    }
    if (rc == 0) for (int i = 0; i < nkv; i++) {
        llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
        if (wkv[i].type == 4 && strstr(kv[i].key, "expert_count") && !strstr(kv[i].key, "expert_used")) wkv[i].u32 = 1;
        else if (wkv[i].type == 4 && strstr(kv[i].key, "expert_used_count")) wkv[i].u32 = 1;
        if (dense_arch[0] && orig_arch[0] && strcmp(dense_arch, orig_arch) != 0) {
            size_t ol = strlen(orig_arch);
            if (strcmp(kv[i].key, "general.architecture") == 0 && wkv[i].type == 8) { char* s = (char*)malloc(64); snprintf(s, 64, "%s", dense_arch); wkv[i].str = s; }
            else if (strncmp(kv[i].key, orig_arch, ol) == 0 && kv[i].key[ol] == '.') { size_t l = strlen(kv[i].key) + 64; char* nk = (char*)malloc(l); snprintf(nk, l, "%s%s", dense_arch, kv[i].key + ol); wkv[i].key = nk; }
        }
    }
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)out_n, 32);
    for (uint64_t i = 0; i < out_n; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(names); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}
/* 6.1 [P2] — SPLITEXPERTS: esperti STACKATI `blk.N.ffn_{gate,up,down}_exps.weight [d0,d1,E]` ->
 * E tensori per-esperto `blk.N.ffn_{gate,up,down}_exps.<e>.weight [d0,d1]`. Resta MoE (non denso).
 * Abilita formati MISTI per-esperto + un layout ispezionabile/aggregabile (reference). F16/F32. */
int llmmodelio_model_split_experts(const char* in_path, const char* out_path) {
    if (in_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    uint64_t E = 0;
    for (int i = 0; i < nkv; i++) if (kv[i].type == 4 && strstr(kv[i].key, "expert_count") && !strstr(kv[i].key, "expert_used")) E = kv[i].u32;
    if (E == 0) for (uint64_t i = 0; i < fi.n_tensors; i++) if (strstr(fi.tensors[i].name, "_exps") && fi.tensors[i].ndim >= 3) { E = fi.tensors[i].dims[fi.tensors[i].ndim-1]; break; }
    if (E < 2) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    uint64_t cap = fi.n_tensors * E + 1;
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)cap, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)cap * 8, sizeof(uint64_t));
    char (*names)[256] = (char(*)[256])calloc((size_t)cap, 256);
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (!ts || !dims || !names || !wkv || !src) { free(ts); free(dims); free(names); free(wkv); if (src) fclose(src); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int rc = 0; uint64_t out_n = 0; int split = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (!buf) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        int es = (ti->ggml_type == LLMCODEC_GGML_T_F32) ? 4 : (ti->ggml_type == LLMCODEC_GGML_T_F16 ? 2 : 0);
        int is_exps = (strstr(ti->name, "_exps") != NULL) && ti->ndim >= 3 && es > 0 && ti->dims[ti->ndim-1] == E;
        if (is_exps) {
            uint64_t per = 1; for (int d = 0; d < ti->ndim - 1; d++) per *= ti->dims[d];
            const uint8_t* sb = (const uint8_t*)buf;
            const char* dot = strrchr(ti->name, '.');   /* "..._exps.weight" -> prefisso + suffisso */
            size_t pre = dot ? (size_t)(dot - ti->name) : strlen(ti->name);
            const char* suf = dot ? dot : "";
            for (uint64_t e = 0; e < E; e++) {
                void* out = malloc((size_t)(per * (uint64_t)es));
                if (!out) { rc = -1; break; }
                memcpy(out, sb + e * per * (uint64_t)es, (size_t)(per * (uint64_t)es));
                memcpy(names[out_n], ti->name, pre);
                snprintf(names[out_n] + pre, 256 - pre, ".%llu%s", (unsigned long long)e, suf);
                ts[out_n].ggml_type = ti->ggml_type; ts[out_n].name = names[out_n];
                ts[out_n].ndim = ti->ndim - 1; ts[out_n].dims = dims + out_n * 8;
                for (int d = 0; d < ti->ndim - 1; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
                ts[out_n].data = out; out_n++; split++;
            }
            if (rc != 0 && out_n > 0) { /* libera i parziali */ }
            free(buf);
        } else {
            ts[out_n].ggml_type = ti->ggml_type; ts[out_n].name = ti->name;
            ts[out_n].ndim = ti->ndim; ts[out_n].dims = dims + out_n * 8;
            for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
            ts[out_n].data = buf; out_n++;
        }
    }
    if (rc == 0 && split == 0) rc = -1;
    if (rc == 0) for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)out_n, 32);
    for (uint64_t i = 0; i < out_n; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(names); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}

/* G4a — PRUNING STRUTTURATO dell'HIDDEN (dimensione INTERMEDIA del FFN denso). Tiene le prime `k`
 * unità: ffn_gate/ffn_up (ne[1]=inter) -> prefisso; ffn_down (ne[0]=inter) -> prefisso. Aggiorna
 * `feed_forward_length`=k. F16/F32. Deterministico. (Dopo MOEDENSE per un modello denso.) */
int llmmodelio_model_prune_ffn(const char* in_path, const char* out_path, int k) {
    if (in_path == NULL || out_path == NULL || k <= 0) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors + 1, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc(((size_t)fi.n_tensors + 1) * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (!ts || !dims || !wkv || !src) { free(ts); free(dims); free(wkv); if (src) fclose(src); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    int rc = 0; uint64_t out_n = 0; int pruned = 0;
    /* IMPORTANZA (non first-N): per ogni layer si tengono i top-k canali intermedi per |gate[:,u]|.
     * La selezione (sel[]) si calcola UNA volta dal primo tensore ffn del layer (gate) e si applica
     * a gate/up/down (gather dei blocchi ne1 per gate/up, ne0 per down). */
    char cur[64] = ""; int nsel = 0; int* sel = NULL;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (!buf) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        int is_gate = (strstr(ti->name, "ffn_gate.") != NULL);
        int is_up = (strstr(ti->name, "ffn_up.") != NULL);
        int is_down = (strstr(ti->name, "ffn_down.") != NULL);
        int es = (ti->ggml_type == LLMCODEC_GGML_T_F32) ? 4 : (ti->ggml_type == LLMCODEC_GGML_T_F16 ? 2 : 0);
        char lay[64] = ""; { const char* bp = strstr(ti->name, "blk."); if (bp) { bp += 4; int q = 0; while (bp[q] && bp[q] != '.' && q < 62) { lay[q] = bp[q]; q++; } lay[q] = 0; } }
        if ((is_gate || is_up || is_down) && strcmp(lay, cur) != 0) {
            snprintf(cur, sizeof cur, "%s", lay); free(sel); sel = NULL; nsel = 0;
            /* Importanza = ||down[:, u]|| (contributo dell'unita' intermedia). Il `down` NON e' sempre
             * il primo tensore ffn (ordine: down/gate/up): se non e' quello corrente, lo si legge. */
            const void* dsrc = NULL; uint64_t d_ne0 = 0, d_ne1 = 1; int d_es = 0; void* dbuf = NULL;
            if (is_down && ti->ndim == 2) { dsrc = buf; d_ne0 = ti->dims[0]; d_ne1 = (ti->ndim > 1) ? ti->dims[1] : 1; d_es = es; }
            else { char rn[256]; snprintf(rn, sizeof rn, "blk.%s.ffn_down.weight", lay); int ri = -1;
                   for (uint64_t j = 0; j < fi.n_tensors; j++) if (strcmp(fi.tensors[j].name, rn) == 0) { ri = (int)j; break; }
                   if (ri >= 0) { LlmCodecGgufTensorInfo* td = &fi.tensors[ri];
                       uint64_t db = llmcodec_gguf_tensor_bytes(td);
                       if (db > 0) { dbuf = malloc((size_t)db);
                           if (dbuf && fseek(src, (long)(fi.data_start + td->offset), SEEK_SET) == 0 && fread(dbuf, 1, (size_t)db, src) == (size_t)db) {
                               dsrc = dbuf; d_ne0 = td->dims[0]; d_ne1 = (td->ndim > 1) ? td->dims[1] : 1;
                               d_es = (td->ggml_type == LLMCODEC_GGML_T_F32) ? 4 : (td->ggml_type == LLMCODEC_GGML_T_F16 ? 2 : 0);
                           } } } }
            if (dsrc && d_es && d_ne0 > (uint64_t)k) {
                int inter = (int)d_ne0;
                float* imp = (float*)malloc((size_t)inter * 4); sel = (int*)malloc((size_t)inter * sizeof(int));
                if (imp && sel) {
                    for (int u = 0; u < inter; u++) { double s = 0; for (uint64_t c = 0; c < d_ne1; c++) { float v = (d_es == 4) ? ((const float*)dsrc)[c*d_ne0 + u] : llmmodelio_h2f(((const uint16_t*)dsrc)[c*d_ne0 + u]); s += (double)v * v; } imp[u] = (float)sqrt(s); }
                    for (int u = 0; u < inter; u++) sel[u] = u;
                    for (int a = 0; a < k; a++) { int bi = a; for (int bb = a + 1; bb < inter; bb++) if (imp[sel[bb]] > imp[sel[bi]]) bi = bb; int t2 = sel[a]; sel[a] = sel[bi]; sel[bi] = t2; }
                    for (int a = 0; a < k; a++) for (int bb = a + 1; bb < k; bb++) if (sel[bb] < sel[a]) { int t2 = sel[a]; sel[a] = sel[bb]; sel[bb] = t2; }
                    nsel = k;
                }
                free(imp);
            }
            free(dbuf);
            if (nsel == 0 && ti->ndim == 2) { int i0 = (is_gate || is_up) ? (int)ti->dims[1] : (int)ti->dims[0]; if (i0 > k) { sel = (int*)malloc((size_t)k * sizeof(int)); if (sel) { for (int a = 0; a < k; a++) sel[a] = a; nsel = k; } } }
        }
        void* nb = buf; int free_out = 0;
        if (es && ti->ndim == 2 && nsel > 0) {
            uint64_t ne0 = ti->dims[0], ne1 = ti->dims[1];
            if (is_gate || is_up) {        /* inter in ne1 -> gather dei blocchi */
                uint64_t nbytes = (uint64_t)nsel * ne0 * es; nb = malloc((size_t)nbytes); free_out = 1;
                if (nb) { for (int a = 0; a < nsel; a++) memcpy((uint8_t*)nb + (uint64_t)a*ne0*es, (uint8_t*)buf + (uint64_t)sel[a]*ne0*es, (size_t)(ne0*es)); } else rc = -1;
            } else if (is_down) {           /* inter in ne0 -> gather per-colonna */
                uint64_t nbytes = ne1 * (uint64_t)nsel * es; nb = malloc((size_t)nbytes); free_out = 1;
                if (nb) { for (uint64_t c = 0; c < ne1; c++) for (int a = 0; a < nsel; a++) memcpy((uint8_t*)nb + (c*(uint64_t)nsel + (uint64_t)a)*es, (uint8_t*)buf + (c*ne0 + (uint64_t)sel[a])*es, (size_t)es); } else rc = -1;
            }
        }
        if (rc != 0) { if (free_out) free(nb); free(buf); break; }
        ts[out_n].name = ti->name; ts[out_n].ggml_type = ti->ggml_type; ts[out_n].ndim = ti->ndim; ts[out_n].dims = dims + out_n*8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[out_n].dims)[d] = ti->dims[d];
        if (free_out) {
            if (is_gate || is_up) ((uint64_t*)ts[out_n].dims)[1] = (uint64_t)nsel;
            else ((uint64_t*)ts[out_n].dims)[0] = (uint64_t)nsel;
            ts[out_n].data = nb; free(buf); pruned++;
        } else ts[out_n].data = buf;
        out_n++;
    }
    free(sel);
    if (rc == 0 && pruned == 0) rc = -1;
    if (rc == 0) for (int i = 0; i < nkv; i++) {
        llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
        if (wkv[i].type == 4 && strstr(kv[i].key, "feed_forward_length")) wkv[i].u32 = (uint32_t)nsel;
    }
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)out_n, 32);
    for (uint64_t i = 0; i < out_n; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}
int llmmodelio_model_ablate(const char* in_path, const char* out_path, const char* pattern) {
    if (in_path == NULL || out_path == NULL || pattern == NULL || !*pattern) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)fi.n_tensors * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (ts == NULL || dims == NULL || wkv == NULL || src == NULL) {
        free(ts); free(dims); free(wkv); if (src) fclose(src);
        llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1;
    }
    int rc = 0; uint64_t hit = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t numel = llm_numel(ti->dims, ti->ndim);
        ts[i].name = ti->name; ts[i].ggml_type = ti->ggml_type; ts[i].ndim = ti->ndim; ts[i].dims = dims + i*8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[i].dims)[d] = ti->dims[d];
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b);
        if (buf == NULL) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        if (strstr(ti->name, pattern) != NULL) { memset(buf, 0, (size_t)b); hit++; }
        ts[i].data = buf;
    }
    if (rc == 0 && hit == 0) rc = -1;   /* pattern inesistente -> errore esplicito */
    if (rc == 0) for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)fi.n_tensors, 32);
    for (uint64_t i = 0; i < fi.n_tensors; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}
/* ── AGGIUNGERE adapter (LoRA .safetensors): W += (alpha/r) * B@A ── */
static int llm_st_esize(const char* dt) {
    if (!strcmp(dt, "F32")) return 4; if (!strcmp(dt, "F16") || !strcmp(dt, "BF16")) return 2;
    if (!strcmp(dt, "F64") || !strcmp(dt, "I64")) return 8; if (!strcmp(dt, "I32")) return 4;
    if (!strcmp(dt, "I16")) return 2; if (!strcmp(dt, "I8") || !strcmp(dt, "U8") || !strcmp(dt, "BOOL")) return 1;
    return 0;
}
static float llm_st_elem_f32(const unsigned char* p, const char* dt) {
    if (!strcmp(dt, "F32")) { float f; memcpy(&f, p, 4); return f; }
    if (!strcmp(dt, "F16")) { uint16_t h; memcpy(&h, p, 2); return llmcodec_f16_to_f32(h); }
    if (!strcmp(dt, "BF16")) { uint16_t h; memcpy(&h, p, 2); return llmcodec_bf16_to_f32(h); }
    if (!strcmp(dt, "F64")) { double d; memcpy(&d, p, 8); return (float)d; }
    return 0.0f;
}
static float* llm_st_load_f32(FILE* fp, uint64_t data_base, const LlmCodecStTensorInfo* t, uint64_t* out_numel) {
    uint64_t numel = 1; for (int d = 0; d < t->ndim; d++) numel *= t->shape[d];
    int es = llm_st_esize(t->dtype); if (es <= 0) return NULL;
    uint64_t nbytes = numel * (uint64_t)es;
    if (t->end < t->begin || t->end - t->begin < nbytes) return NULL;
    unsigned char* buf = (unsigned char*)malloc((size_t)nbytes); if (buf == NULL) return NULL;
    if (fseek(fp, (long)(data_base + t->begin), SEEK_SET) != 0 || fread(buf, 1, (size_t)nbytes, fp) != (size_t)nbytes) { free(buf); return NULL; }
    float* f = (float*)malloc((size_t)(numel * sizeof(float))); if (f == NULL) { free(buf); return NULL; }
    for (uint64_t i = 0; i < numel; i++) f[i] = llm_st_elem_f32(buf + (size_t)i * es, t->dtype);
    free(buf); if (out_numel) *out_numel = numel; return f;
}
static int llm_st_find(const LlmCodecStTensorInfo* st, int n, const char* name) {
    for (int i = 0; i < n; i++) if (strcmp(st[i].name, name) == 0) return i;
    return -1;
}
static uint64_t llm_ggml_esize(int t) { return (t == LLMCODEC_GGML_T_F32) ? 4u : ((t == LLMCODEC_GGML_T_F16 || t == LLMCODEC_GGML_T_BF16) ? 2u : 0u); }
/* per una base tensor 2D, ritorna A/B nello store (o -1). */
static int llm_lora_pair(const LlmCodecStTensorInfo* st, int n, const char* base_name, int* ia, int* ib) {
    const char* suf = ".weight"; size_t bl = strlen(base_name), sl = strlen(suf);
    char prefix[256];
    if (bl > sl && strcmp(base_name + bl - sl, suf) == 0) { size_t pl = bl - sl; if (pl > 255) pl = 255; memcpy(prefix, base_name, pl); prefix[pl] = 0; }
    else { if (bl > 255) return -1; strcpy(prefix, base_name); }
    char a[600], b[600], a2[600], b2[600];
    snprintf(a, sizeof a, "%s.lora_A.weight", prefix); snprintf(b, sizeof b, "%s.lora_B.weight", prefix);
    snprintf(a2, sizeof a2, "base_model.model.%s.lora_A.weight", prefix); snprintf(b2, sizeof b2, "base_model.model.%s.lora_B.weight", prefix);
    int x = llm_st_find(st, n, a), y = llm_st_find(st, n, b);
    if (x < 0 || y < 0) { x = llm_st_find(st, n, a2); y = llm_st_find(st, n, b2); }
    if (x < 0 || y < 0) return -1;
    *ia = x; *ib = y; return 0;
}
int llmmodelio_model_adapter(const char* base_path, const char* adapter_path, const char* out_path, const char* alpha_str) {
    if (base_path == NULL || adapter_path == NULL || out_path == NULL) return -1;
    { struct stat stb; if (stat(base_path, &stb) == 0 && S_ISDIR(stb.st_mode)) return llmmodelio_mlx_adapter(base_path, adapter_path, out_path, alpha_str); }
    double alpha = alpha_str ? atof(alpha_str) : 1.0;
    LlmCodecGgufFileInfo fb; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(base_path, &kv, &nkv, &fb) != 0) return -1;
    LlmCodecStTensorInfo* st = NULL; int nst = 0; uint64_t dbase = 0;
    if (llmcodec_safetensors_read(adapter_path, &st, &nst, &dbase) != 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fb); return -1; }
    uint64_t cap = fb.n_tensors;
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)cap, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)cap * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* bfp = fopen(base_path, "rb"); FILE* afp = fopen(adapter_path, "rb");
    if (ts == NULL || dims == NULL || wkv == NULL || bfp == NULL || afp == NULL) {
        free(ts); free(dims); free(wkv); if (bfp) fclose(bfp); if (afp) fclose(afp);
        llmcodec_safetensors_info_free(st); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fb); return -1;
    }
    int rc = 0; uint64_t nout = 0, merged = 0;
    for (uint64_t i = 0; i < fb.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* tb = &fb.tensors[i];
        uint64_t numel = llm_numel(tb->dims, tb->ndim), b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(tb), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (buf == NULL) { rc = -1; break; }
        int did_merge = 0;
        int ia = -1, ib = -1;
        if (tb->ndim == 2 && llm_ggml_esize(tb->ggml_type) != 0 && llm_lora_pair(st, nst, tb->name, &ia, &ib) == 0) {
            uint64_t in = tb->dims[0], out = tb->dims[1];
            uint64_t An = 0, Bn = 0;
            float* A = llm_st_load_f32(afp, dbase, &st[ia], &An);
            float* B = llm_st_load_f32(afp, dbase, &st[ib], &Bn);
            if (A == NULL || B == NULL || st[ia].ndim != 2 || st[ib].ndim != 2
                || st[ia].shape[1] != in || st[ib].shape[0] != out || st[ia].shape[0] != st[ib].shape[1]
                || An != st[ia].shape[0] * in || Bn != out * st[ib].shape[1]) {
                free(A); free(B); rc = -1; free(buf); break;   /* coppia presente ma forma incoerente -> errore esplicito */
            }
            uint64_t r = st[ia].shape[0];
            if (fseek(bfp, (long)(fb.data_start + tb->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, bfp) != (size_t)b) { free(A); free(B); rc = -1; free(buf); break; }
            unsigned char* bb = (unsigned char*)buf; int et = tb->ggml_type;
            double scale = alpha / (double)(r ? r : 1);
            for (uint64_t o = 0; o < out; o++)
                for (uint64_t ii = 0; ii < in; ii++) {
                    double acc = 0.0;
                    for (uint64_t k = 0; k < r; k++) acc += (double)B[o * r + k] * (double)A[k * in + ii];
                    float cur;
                    if (et == LLMCODEC_GGML_T_F32) { float t; memcpy(&t, bb + (size_t)(o * in + ii) * 4, 4); cur = t; }
                    else if (et == LLMCODEC_GGML_T_F16) { uint16_t h; memcpy(&h, bb + (size_t)(o * in + ii) * 2, 2); cur = llmcodec_f16_to_f32(h); }
                    else { uint16_t h; memcpy(&h, bb + (size_t)(o * in + ii) * 2, 2); cur = llmcodec_bf16_to_f32(h); }
                    float nv = (float)((double)cur + scale * acc);
                    if (et == LLMCODEC_GGML_T_F32) memcpy(bb + (size_t)(o * in + ii) * 4, &nv, 4);
                    else if (et == LLMCODEC_GGML_T_F16) { uint16_t h = llmcodec_f16_from_f32(nv); memcpy(bb + (size_t)(o * in + ii) * 2, &h, 2); }
                    else { uint16_t h = llmcodec_bf16_from_f32(nv); memcpy(bb + (size_t)(o * in + ii) * 2, &h, 2); }
                }
            free(A); free(B); did_merge = 1; merged++;
        }
        if (!did_merge) {
            if (fseek(bfp, (long)(fb.data_start + tb->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, bfp) != (size_t)b) { free(buf); rc = -1; break; }
        }
        ts[nout].name = tb->name; ts[nout].ggml_type = tb->ggml_type; ts[nout].ndim = tb->ndim; ts[nout].dims = dims + nout * 8; ts[nout].data = buf;
        for (int d = 0; d < tb->ndim; d++) ((uint64_t*)ts[nout].dims)[d] = tb->dims[d];
        nout++;
    }
    if (rc == 0) for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)nout, 32);
    for (uint64_t i = 0; i < nout; i++) free((void*)ts[i].data);
    fclose(bfp); fclose(afp); free(ts); free(dims); free(wkv);
    llmcodec_safetensors_info_free(st); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fb);
    (void)merged;
    return rc == 0 ? 0 : -1;
}
/* AGGIUNGERE (model surgery): aggiunge/sovrascrive in <base> i tensori di <module> (per nome),
 * scrivendo un GGUF nuovo (<out>). Metadata da <base>. Building block di adapter/moduli.
 * Base/modulo non GGUF o illeggibili -> -1 (mai silenzioso). */
static LlmCodecGgufTensorInfo* llm_find_tensor(LlmCodecGgufFileInfo* fi, const char* name) {
    for (uint64_t i = 0; i < fi->n_tensors; i++) if (strcmp(fi->tensors[i].name, name) == 0) return &fi->tensors[i];
    return NULL;
}
/* Nome presente in una lista separata da virgole (per `gmbasic.patch.removed`). */
static int llm_removed_contains(const char* list, const char* name) {
    if (list == NULL || name == NULL || !*name) return 0;
    size_t nl = strlen(name); const char* p = list;
    while (*p) {
        const char* e = strchr(p, ',');
        size_t cl = e ? (size_t)(e - p) : strlen(p);
        if (cl == nl && strncmp(p, name, nl) == 0) return 1;
        if (!e) break; p = e + 1;
    }
    return 0;
}

int llmmodelio_model_add(const char* base_path, const char* module_path, const char* out_path) {
    if (base_path == NULL || module_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fb, fm;
    LlmCodecGgufMetaKVOwned *kvb = NULL, *kvm = NULL; int nkvb = 0, nkvm = 0;
    if (llmcodec_gguf_read_meta(base_path, &kvb, &nkvb, &fb) != 0) return -1;
    if (llmcodec_gguf_read_meta(module_path, &kvm, &nkvm, &fm) != 0) {
        llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_free_info(&fb); return -1;
    }
    uint64_t cap = fb.n_tensors + fm.n_tensors;
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)cap, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)cap * 8, sizeof(uint64_t));
    uint64_t nmaxkv = (uint64_t)(nkvb > nkvm ? nkvb : nkvm);
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nmaxkv ? nmaxkv : 1), sizeof(*wkv));
    FILE* bfp = fopen(base_path, "rb"); FILE* mfp = fopen(module_path, "rb");
    if (ts == NULL || dims == NULL || wkv == NULL || bfp == NULL || mfp == NULL) {
        free(ts); free(dims); free(wkv); if (bfp) fclose(bfp); if (mfp) fclose(mfp);
        llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_meta_free(kvm, nkvm);
        llmcodec_gguf_free_info(&fb); llmcodec_gguf_free_info(&fm); return -1;
    }
    int rc = 0; uint64_t nout = 0; uint64_t replaced = 0, added = 0;
    /* PATCH (`LLM MAKEPATCH`): il delta porta nel KV `gmbasic.patch.removed` i tensori presenti
     * nella BASE ma ASSENTI nel TARGET (es. `LLM MODEL REMOVE` di un layer) e nel resto del KV il
     * metadata del TARGET. In tal caso: (a) RIMUOVE quei tensori, (b) usa il metadata del patch
     * (target) al posto di quello della base. Un modulo normale (LoRA/adapter) NON ha il marker. */
    const char* removed_list = NULL;
    for (int i = 0; i < nkvm; i++)
        if (kvm[i].key && strcmp(kvm[i].key, "gmbasic.patch.removed") == 0) { removed_list = kvm[i].str ? kvm[i].str : ""; break; }
    int is_patch = (removed_list != NULL);
    /* 1) tensori della base NON presenti nel modulo -> copiati dalla base. */
    for (uint64_t i = 0; i < fb.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* tb = &fb.tensors[i];
        if (llm_find_tensor(&fm, tb->name) != NULL) continue;   /* sara' sovrascritto dal modulo */
        if (is_patch && llm_removed_contains(removed_list, tb->name)) continue;   /* RIMOSSO nel target */
        uint64_t numel = llm_numel(tb->dims, tb->ndim), b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(tb), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (buf == NULL) { rc = -1; break; }
        if (fseek(bfp, (long)(fb.data_start + tb->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, bfp) != (size_t)b) { free(buf); rc = -1; break; }
        ts[nout].name = tb->name; ts[nout].ggml_type = tb->ggml_type; ts[nout].ndim = tb->ndim; ts[nout].dims = dims + nout * 8; ts[nout].data = buf;
        for (int d = 0; d < tb->ndim; d++) ((uint64_t*)ts[nout].dims)[d] = tb->dims[d];
        nout++;
    }
    /* 2) tensori del modulo -> aggiunti/sovrascritti. */
    for (uint64_t j = 0; j < fm.n_tensors && rc == 0; j++) {
        LlmCodecGgufTensorInfo* tm = &fm.tensors[j];
        uint64_t numel = llm_numel(tm->dims, tm->ndim), b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(tm), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (buf == NULL) { rc = -1; break; }
        if (fseek(mfp, (long)(fm.data_start + tm->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, mfp) != (size_t)b) { free(buf); rc = -1; break; }
        if (llm_find_tensor(&fb, tm->name) != NULL) replaced++; else added++;
        ts[nout].name = tm->name; ts[nout].ggml_type = tm->ggml_type; ts[nout].ndim = tm->ndim; ts[nout].dims = dims + nout * 8; ts[nout].data = buf;
        for (int d = 0; d < tm->ndim; d++) ((uint64_t*)ts[nout].dims)[d] = tm->dims[d];
        nout++;
    }
    uint64_t noutkv = 0;
    if (rc == 0 && is_patch) {
        /* patch: metadata del TARGET (KV del patch), senza il marker interno */
        for (int i = 0; i < nkvm; i++) { if (kvm[i].key && strcmp(kvm[i].key, "gmbasic.patch.removed") == 0) continue; llmcodec_gguf_meta_to_writer(&kvm[i], &wkv[noutkv++]); }
    } else if (rc == 0) {
        /* modulo normale: metadata della base */
        for (int i = 0; i < nkvb; i++) llmcodec_gguf_meta_to_writer(&kvb[i], &wkv[noutkv++]);
    }
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, (int)noutkv, ts, (int)nout, 32);
    for (uint64_t i = 0; i < nout; i++) free((void*)ts[i].data);
    fclose(bfp); fclose(mfp); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_meta_free(kvm, nkvm);
    llmcodec_gguf_free_info(&fb); llmcodec_gguf_free_info(&fm);
    (void)replaced; (void)added;
    return rc == 0 ? 0 : -1;
}
/* ── Checkpoint/rollback: copia file + hash di integrita' (FNV-1a 64) ── */
static int llm_copy_file(const char* src, const char* dst) {
    FILE* a = fopen(src, "rb"); if (a == NULL) return -1;
    FILE* b = fopen(dst, "wb"); if (b == NULL) { fclose(a); return -1; }
    const size_t BUF = (size_t)1 << 20; unsigned char* buf = (unsigned char*)malloc(BUF);
    if (buf == NULL) { fclose(a); fclose(b); return -1; }
    size_t n; int rc = 0;
    while ((n = fread(buf, 1, BUF, a)) > 0) if (fwrite(buf, 1, n, b) != n) { rc = -1; break; }
    if (ferror(a)) rc = -1;
    free(buf); fclose(a); if (fclose(b) != 0) rc = -1;
    return rc;
}
static int llm_fnv64_file(const char* path, unsigned long long* out_size, unsigned long long* out_hash) {
    FILE* f = fopen(path, "rb"); if (f == NULL) return -1;
    const size_t BUF = (size_t)1 << 20; unsigned char* buf = (unsigned char*)malloc(BUF);
    if (buf == NULL) { fclose(f); return -1; }
    unsigned long long h = 1469598103934665603ULL, sz = 0; size_t n;
    while ((n = fread(buf, 1, BUF, f)) > 0) { for (size_t i = 0; i < n; i++) { h ^= buf[i]; h *= 1099511628211ULL; } sz += n; }
    int err = ferror(f); free(buf); fclose(f);
    if (err) return -1;
    if (out_size) *out_size = sz; if (out_hash) *out_hash = h;
    return 0;
}
/* nome-checkpoint sicuro (nessun path traversal). */
static int llm_name_safe(const char* n) {
    if (n == NULL || !*n) return 0;
    if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0) return 0;
    for (const char* p = n; *p; p++) if (*p == '/' || *p == '\\') return 0;
    return 1;
}
/* CHECKPOINT: snapshot di <src> nel checkpoint-store <store_dir> con nome <name>
 * (+ <name>.meta con size/fnv64 = integrita' per il rollback). */
int llmmodelio_model_checkpoint(const char* src, const char* store_dir, const char* name) {
    if (src == NULL || store_dir == NULL || name == NULL || !llm_name_safe(name)) return -1;
    unsigned long long sz = 0, h = 0;
    if (llm_fnv64_file(src, &sz, &h) != 0) return -1;   /* src inesistente/illeggibile -> errore */
    mkdir(store_dir, 0777);   /* -1 se esiste: ok (single-level store) */
    char ckpt[4096], meta[4096];
    snprintf(ckpt, sizeof ckpt, "%s/%s.ckpt", store_dir, name);
    snprintf(meta, sizeof meta, "%s/%s.meta", store_dir, name);
    if (llm_copy_file(src, ckpt) != 0) return -1;
    FILE* m = fopen(meta, "w"); if (m == NULL) return -1;
    fprintf(m, "name=%s\nsize=%llu\nfnv64=%016llx\nsrc=%s\n", name, sz, h, src);
    fclose(m);
    return 0;
}
/* ROLLBACK: ripristina <store_dir>/<name>.ckpt su <dst>, verificando l'integrita' col meta. */
int llmmodelio_model_rollback(const char* store_dir, const char* name, const char* dst) {
    if (store_dir == NULL || name == NULL || dst == NULL || !llm_name_safe(name)) return -1;
    char ckpt[4096], meta[4096];
    snprintf(ckpt, sizeof ckpt, "%s/%s.ckpt", store_dir, name);
    snprintf(meta, sizeof meta, "%s/%s.meta", store_dir, name);
    unsigned long long sz = 0, h = 0;
    if (llm_fnv64_file(ckpt, &sz, &h) != 0) return -1;   /* checkpoint assente */
    FILE* m = fopen(meta, "r");
    if (m != NULL) {
        char line[512]; unsigned long long msz = 0, mh = 0;
        while (fgets(line, sizeof line, m)) {
            unsigned long long v = 0;
            if (sscanf(line, "size=%llu", &v) == 1) msz = v;
            if (sscanf(line, "fnv64=%llx", &v) == 1) mh = v;
        }
        fclose(m);
        if (msz != 0 && (msz != sz || mh != h)) return -1;   /* checkpoint corrotto -> errore */
    }
    return llm_copy_file(ckpt, dst);
}
/* DIFF (model surgery: checkpoint/rollback + quality gate) — confronta due artefatti GGUF:
 * tensori presenti solo in A/B, e per i comuni differenze di tipo/forma/contenuto (byte).
 * Scrive un report testuale in out_path. A/B non GGUF o illeggibili -> -1 (mai silenzioso). */
/* VALIDATE (model surgery: integrita' strutturale di un artefatto) — per ogni tensore: forma/
 * tipo calcolabili e dati dentro il file; + conteggio kv. Report in out_path; issues>0 -> -1. */
int llmmodelio_model_validate(const char* path, const char* out_path) {
    if (path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    long fsz = 0; { FILE* f = fopen(path, "rb"); if (f) { fseek(f, 0, SEEK_END); fsz = ftell(f); fclose(f); } }
    FILE* o = fopen(out_path, "w");
    if (o == NULL) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    fprintf(o, "# LLM MODEL VALIDATE %s\n", path);
    fprintf(o, "tensors=%llu kv=%d data_start=%llu file_size=%ld\n",
            (unsigned long long)fi.n_tensors, nkv, (unsigned long long)fi.data_start, fsz);
    int issues = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        LlmCodecGgufTensorInfo* t = &fi.tensors[i];
        uint64_t numel = 1; for (int d = 0; d < t->ndim; d++) numel *= t->dims[d];
        uint64_t b = 0; const char* why = NULL;
        if (t->ndim <= 0) why = "ndim<=0";
        else if (numel == 0) why = "numel=0";
        else if ((b = llmcodec_gguf_tensor_bytes(t), 0) != 0 || b == 0) why = "tipo/byte non calcolabile";
        else if (fsz > 0 && (long)(fi.data_start + t->offset + b) > fsz) why = "offset+dati oltre la fine del file";
        if (why) { issues++; fprintf(o, "ISSUE %s: %s\n", t->name, why); }
    }
    fprintf(o, "SUMMARY tensors=%llu issues=%d -> %s\n", (unsigned long long)fi.n_tensors, issues, issues ? "INVALID" : "VALID");
    fclose(o);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return issues ? -1 : 0;
}
int llmmodelio_model_diff(const char* a_path, const char* b_path, const char* out_path) {
    if (a_path == NULL || b_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fa, fb;
    LlmCodecGgufMetaKVOwned* kva = NULL; LlmCodecGgufMetaKVOwned* kvb = NULL; int nkva = 0, nkvb = 0;
    if (llmcodec_gguf_read_meta(a_path, &kva, &nkva, &fa) != 0) return -1;
    if (llmcodec_gguf_read_meta(b_path, &kvb, &nkvb, &fb) != 0) {
        llmcodec_gguf_meta_free(kva, nkva); llmcodec_gguf_free_info(&fa); return -1;
    }
    FILE* os = fopen(out_path, "w");
    FILE* afp = fopen(a_path, "rb");
    FILE* bfp = fopen(b_path, "rb");
    if (os == NULL || afp == NULL || bfp == NULL) {
        if (os) fclose(os); if (afp) fclose(afp); if (bfp) fclose(bfp);
        llmcodec_gguf_meta_free(kva, nkva); llmcodec_gguf_meta_free(kvb, nkvb);
        llmcodec_gguf_free_info(&fa); llmcodec_gguf_free_info(&fb); return -1;
    }
    uint64_t eq = 0, differ = 0, only_a = 0, only_b = 0, bytes_differ = 0;
    for (uint64_t i = 0; i < fa.n_tensors; i++) {
        LlmCodecGgufTensorInfo* ta = &fa.tensors[i];
        LlmCodecGgufTensorInfo* tb = NULL;
        for (uint64_t j = 0; j < fb.n_tensors; j++)
            if (strcmp(fa.tensors[i].name, fb.tensors[j].name) == 0) { tb = &fb.tensors[j]; break; }
        if (tb == NULL) { only_a++; fprintf(os, "ONLY_A %s\n", ta->name); continue; }
        if (ta->ggml_type != tb->ggml_type || ta->ndim != tb->ndim) {
            differ++; fprintf(os, "META_DIFF %s (type %d/%d ndim %d/%d)\n", ta->name, ta->ggml_type, tb->ggml_type, ta->ndim, tb->ndim); continue;
        }
        int dims_ok = 1;
        for (int d = 0; d < ta->ndim; d++) if (ta->dims[d] != tb->dims[d]) dims_ok = 0;
        if (!dims_ok) { differ++; fprintf(os, "SHAPE_DIFF %s\n", ta->name); continue; }
        uint64_t b = 0, numel = llm_numel(ta->dims, ta->ndim);
        if ((b = llmcodec_gguf_tensor_bytes(ta), 0) != 0 || b == 0) { differ++; fprintf(os, "UNKNOWN_TYPE %s\n", ta->name); continue; }
        unsigned char* buf = (unsigned char*)malloc((size_t)b);
        unsigned char* buf2 = (unsigned char*)malloc((size_t)b);
        if (buf == NULL || buf2 == NULL) { free(buf); free(buf2); differ++; continue; }
        int ok = (fseek(afp, (long)(fa.data_start + ta->offset), SEEK_SET) == 0) && (fread(buf, 1, (size_t)b, afp) == (size_t)b)
              && (fseek(bfp, (long)(fb.data_start + tb->offset), SEEK_SET) == 0) && (fread(buf2, 1, (size_t)b, bfp) == (size_t)b);
        if (!ok) { free(buf); free(buf2); differ++; fprintf(os, "READ_ERR %s\n", ta->name); continue; }
        if (memcmp(buf, buf2, (size_t)b) == 0) eq++;
        else { differ++; bytes_differ += b; fprintf(os, "DIFF %s (%llu bytes)\n", ta->name, (unsigned long long)b); }
        free(buf); free(buf2);
    }
    for (uint64_t j = 0; j < fb.n_tensors; j++) {
        int found = 0;
        for (uint64_t i = 0; i < fa.n_tensors; i++) if (strcmp(fa.tensors[i].name, fb.tensors[j].name) == 0) { found = 1; break; }
        if (!found) { only_b++; fprintf(os, "ONLY_B %s\n", fb.tensors[j].name); }
    }
    fprintf(os, "SUMMARY tensors_a=%llu tensors_b=%llu equal=%llu differ=%llu only_a=%llu only_b=%llu bytes_differ=%llu\n",
            (unsigned long long)fa.n_tensors, (unsigned long long)fb.n_tensors, (unsigned long long)eq,
            (unsigned long long)differ, (unsigned long long)only_a, (unsigned long long)only_b, (unsigned long long)bytes_differ);
    fclose(afp); fclose(bfp); fclose(os);
    llmcodec_gguf_meta_free(kva, nkva); llmcodec_gguf_meta_free(kvb, nkvb);
    llmcodec_gguf_free_info(&fa); llmcodec_gguf_free_info(&fb);
    return 0;
}
static const char* llmmodelio_arch_recipe(void);   /* recipe dalla .gmbarch ("_M"); definita sotto */

int llmmodelio_model_quantize(const char* in_path, const char* out_path) {
    if (in_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    /* Risolvi l'arch dall'input: serve per la RECIPE di default ("_M") dalla .gmbarch. */
    { size_t l = strlen(in_path); if (l > 5 && strcasecmp(in_path + l - 5, ".gguf") == 0) llmmodelio_arch_resolve_gguf(in_path); }

    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)fi.n_tensors * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    FILE* src = fopen(in_path, "rb");
    if (ts == NULL || dims == NULL || wkv == NULL || src == NULL) {
        free(ts); free(dims); free(wkv); if (src) fclose(src);
        llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1;
    }
    int rc = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        int auto_ty = (g_auto_type && g_auto_type[i] >= 0) ? g_auto_type[i] : -1;   /* AUTO: formato deciso */
        ts[i].name = ti->name; ts[i].ndim = ti->ndim; ts[i].dims = dims + i * 8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[i].dims)[d] = ti->dims[d];
        uint64_t last = ti->ndim > 0 ? ti->dims[0] : 0;   /* asse blocco = ne[0] (GGML) */
        const char* role = llmmodelio_role_of(ti->name);
        int isf32 = (ti->ggml_type == LLMCODEC_GGML_T_F32), isf16 = (ti->ggml_type == LLMCODEC_GGML_T_F16);
        int canq = (isf32 || isf16) && (last > 0) && (last % 32 == 0);
        static const char* efmt_env = NULL; static int efmt_init = 0;
        if (!efmt_init) { efmt_env = llmmodelio_model_policy_get("EXPERT_FMT"); efmt_init = 1; }
        const char* pol = llmmodelio_model_policy_get("ROUTED_EXPERT");
        const char* efmt = pol ? pol : (efmt_env ? efmt_env : "q4k");
        /* Policy esplicita (ROUTED_EXPERT/DSL/EXPERT_TAIL/DOWN_FMT/ATTN_FMT/EMBD_FMT): vince sulla
         * RECIPE di default (arch). La RECIPE si applica solo dove l'utente non ha deciso. */
        int explicit_fmt = (pol != NULL);
        /* Policy DSL ATTIVA (Livello A): una regola che matcha HA PRECEDENZA sul formato deciso
         * da recipe/tail/down/role. Non attiva -> comportamento invariato. */
        { int lay0 = -1; if (strncmp(ti->name, "blk.", 4) == 0) lay0 = atoi(ti->name + 4);
          const char* pd = pol_decide(role, ti->name, lay0, "", 1.0);
          if (pd && *pd) { efmt = pd; explicit_fmt = 1; } }
        int is_exp = canq && strcmp(role, "ROUTED_EXPERT") == 0;
        /* S1 (todo auto-migliorare): formato esperti PER-FASCIA — gli ultimi layer piu' sensibili.
         * LLMMODELIO_EXPERT_TAIL="<N>|<fmt>": gli ultimi N layer (blk.M., M>=n_layer-N) usano <fmt>,
         * gli altri il default efmt. Bassa dimensionalita' -> robusto al rumore. */
        { static int tail_n = -2; static char tail_fmt[16] = "q4k";
          if (tail_n == -2) { const char* t = llmmodelio_model_policy_get("EXPERT_TAIL"); tail_n = 0;
              if (t) { const char* bar = strchr(t, '|'); if (bar) { tail_n = atoi(t); snprintf(tail_fmt, sizeof tail_fmt, "%s", bar+1); } } }
          if (tail_n > 0 && is_exp) { int lay = -1;
              if (strncmp(ti->name, "blk.", 4) == 0) lay = atoi(ti->name+4);
              if (lay >= 0) { int nlay = 16;
                  for (int k=0;k<nkv;k++) if (strstr(kv[k].key,"block_count") && kv[k].type==LLMCODEC_GGUF_MT_U32) nlay=(int)kv[k].u32;
                  if (lay >= nlay - tail_n) { efmt = tail_fmt; explicit_fmt = 1; } } } }
        /* Ricetta-baseline (todo auto-migliorare): il down_exps e' piu' sensibile degli altri esperti
         * (la sua uscita entra diretta nel residuo). La baseline Q4_K_M usa Q6_K per il down e Q4_K per
         * gate/up. LLMMODELIO_DOWN_FMT (es. q6k) applica un formato dedicato ai soli *_down_exps. */
        { static const char* down_fmt = (const char*)-1;
          if (down_fmt == (const char*)-1) { down_fmt = llmmodelio_model_policy_get("DOWN_FMT"); }
          if (down_fmt != NULL && is_exp && strstr(ti->name, "_down") != NULL) { efmt = down_fmt; explicit_fmt = 1; } }
        int e4k = is_exp && (last % 256 == 0) && strcmp(efmt, "q4k") == 0;
        int e2k = is_exp && (last % 256 == 0) && strcmp(efmt, "q2k") == 0;
        int e6k = is_exp && (last % 256 == 0) && strcmp(efmt, "q6k") == 0;
        int e5k = is_exp && (last % 256 == 0) && strcmp(efmt, "q5k") == 0;
        int e3k = is_exp && (last % 256 == 0) && strcmp(efmt, "q3k") == 0;
        int eix = is_exp && (last % 256 == 0) && strcmp(efmt, "iq2xxs") == 0;
        int ei1s = is_exp && (last % 256 == 0) && strcmp(efmt, "iq1s") == 0;   /* IQ1_S  1.5625 bpw */
        int ei1m = is_exp && (last % 256 == 0) && strcmp(efmt, "iq1m") == 0;   /* IQ1_M  1.75 bpw   */
        int ei2s = is_exp && (last % 256 == 0) && strcmp(efmt, "iq2s") == 0;   /* IQ2_S  2.5625 bpw */
        int ei2x = is_exp && (last % 256 == 0) && strcmp(efmt, "iq2xs") == 0;  /* IQ2_XS 2.3125 bpw */
        int ei3x = is_exp && (last % 256 == 0) && strcmp(efmt, "iq3xxs") == 0; /* IQ3_XXS 3.0625 bpw */
        int ei3s = is_exp && (last % 256 == 0) && strcmp(efmt, "iq3s") == 0;   /* IQ3_S  3.4375 bpw */
        int ei4x = is_exp && (last % 256 == 0) && strcmp(efmt, "iq4xs") == 0;  /* IQ4_XS 4.25 bpw  */
        int ei4nl = is_exp && (last % 32 == 0) && strcmp(efmt, "iq4nl") == 0;  /* IQ4_NL 4.5 bpw   */
        int emx = is_exp && (last % 32 == 0) && strcmp(efmt, "mxfp4") == 0;
        int env = is_exp && (last % 64 == 0) && strcmp(efmt, "nvfp4") == 0;
        int q4 = canq && strcmp(role, "FFN") == 0 && (last % 32 == 0);                  /* FFN: Q4_0 */
        int gt = -1;   /* tipo del formato nativo del codec (path generico q4_1/q5_0/q5_1/q8_k/...) */
        int q8 = canq && (strcmp(role, "ATTENTION") == 0 || strcmp(role, "SHARED_EXPERT") == 0
                          || strcmp(role, "OUTPUT") == 0);                         /* sensibili: Q8 */
        int emb16 = (isf32 || (isf16 && 0)) && strcmp(role, "EMBEDDING") == 0;     /* embedding -> F16 */
        if (auto_ty >= 0) {   /* AUTO (§E3): formato deciso per-tensore dall'allocatore a sensibilità */
            q4 = (auto_ty == 2); q8 = (auto_ty == 8); emb16 = 0;
            e2k = (auto_ty == 10); e3k = (auto_ty == 11); e4k = (auto_ty == 12);
            e5k = (auto_ty == 13); e6k = (auto_ty == 14); eix = (auto_ty == 16);
            explicit_fmt = 1;   /* l'allocatore AUTO vince sulla recipe di default */
        }
        /* Ricetta baseline (todo auto-migliorare): attn e embedding con formato esplicito.
         * LLMMODELIO_ATTN_FMT="q6k" -> attenzione a Q6_K (la baseline non usa Q8_0); LLMMODELIO_EMBD_FMT="q5k"
         * -> embedding QUANTIZZATO (oggi escluso: resta F16, +206MB). Riusa i flag e4k/e5k/e6k. */
        { static const char* a_fmt=(const char*)-1, *e_fmt=(const char*)-1, *f_fmt=(const char*)-1;
          if (a_fmt==(const char*)-1) { a_fmt=llmmodelio_model_policy_get("ATTN_FMT"); e_fmt=llmmodelio_model_policy_get("EMBD_FMT");
                                        f_fmt=llmmodelio_model_policy_get("FFN_FMT"); }
          const char* rf = NULL;
          if (strcmp(role,"ATTENTION")==0) rf=a_fmt; else if (strcmp(role,"EMBEDDING")==0) rf=e_fmt;
          else if (strcmp(role,"FFN")==0) rf=f_fmt;   /* FFN dense: leva della compressione densa */
          if (rf!=NULL) { q8=0; emb16=0; if (strcmp(role,"FFN")==0) q4=0;   /* FFN_FMT vince su Q4_0 */
              if (last%256==0) { if(strcmp(rf,"q6k")==0) e6k=1; else if(strcmp(rf,"q4k")==0) e4k=1;
                                 else if(strcmp(rf,"q5k")==0) e5k=1; else if(strcmp(rf,"q3k")==0) e3k=1;
                                 else if(strcmp(rf,"q2k")==0) e2k=1; }
              else if (strcmp(rf,"q8_0")==0) q8=1; explicit_fmt = 1; } }
        /* RICETTA ESATTA Q4_K_M (dal dump della baseline, non per ruolo): pesi Q4_K; **attn_v** e
         * **ffn_down_exps** a Q6_K per la **META' FINALE** dei layer (il "medium" di llama.cpp); il
         * **output.weight** a Q6_K; token_embd Q4_K; norm/router F32. Evita il +9% del per-ruolo.
         * LLMMODELIO_RECIPE=q4km. */
        /* RICETTE llama.cpp (regole da llama-quant.cpp:llama_tensor_get_type_impl, verificate col dump):
         * base X_K; attn_v e ffn_down con boost per-fascia; output Q6_K; norm/router F32.
         * LLMMODELIO_RECIPE = q2k|q2ks|q3ks|q3km|q3kl|q4ks|q4km|q5ks|q5km|q6k (le _S/_L nuove sono best-effort). */
        { static int s_rcp=-1; static char s_ft[16]="";
          if (s_rcp<0){ const char* r=llmmodelio_model_policy_get("RECIPE");
              if (r==NULL || *r=='\0') r = llmmodelio_arch_recipe();   /* default: recipe della .gmbarch ("_M") */
              s_rcp=(r&&strlen(r)>0)?1:0; if(s_rcp) snprintf(s_ft,sizeof s_ft,"%s",r); }
          if (s_rcp && !explicit_fmt && canq && last%256==0 && ti->ndim >= 2 && strstr(ti->name,"gate_inp")==NULL) {
              int lay=-1; if (strncmp(ti->name,"blk.",4)==0) lay=atoi(ti->name+4);
              int nlay=16; for(int k=0;k<nkv;k++) if(strstr(kv[k].key,"block_count")&&kv[k].type==LLMCODEC_GGUF_MT_U32) nlay=(int)kv[k].u32;
              int more = (lay>=0) && (lay < nlay/8 || lay >= 7*nlay/8 || ((lay-nlay/8)%3==2));
              int is_v = strstr(ti->name,"attn_v.")!=NULL, is_down = strstr(ti->name,"ffn_down")!=NULL;  /* denso E esperti */
              int is_out = strcmp(ti->name,"output.weight")==0;
              int t = 0;   /* ggml type scelto */
              if (strcmp(s_ft,"q6k")==0) t = 14;
              else if (strcmp(s_ft,"q5ks")==0) t = is_out ? 14 : 13;
              else if (strcmp(s_ft,"q5km")==0) t = is_out ? 14 : (more && (is_v||is_down) ? 14 : 13);
              else if (strcmp(s_ft,"q4km")==0 || strcmp(s_ft,"_M")==0) t = is_out ? 14 : (more && (is_v||is_down) ? 14 : 12);
              else if (strcmp(s_ft,"q4ks")==0 || strcmp(s_ft,"_S")==0) t = is_out ? 14 : ((is_v && lay>=0 && lay<4) || (is_down && lay>=0 && lay<nlay/8) ? 13 : 12);
              else if (strcmp(s_ft,"q3km")==0) t = is_out ? 14 : (is_v ? (lay>=0 && lay<2 ? 13 : 12) : (is_down ? (lay>=0 && lay<1 ? 13 : 12) : 11));
              else if (strcmp(s_ft,"q2k")==0)  t = is_out ? 14 : ((is_v||is_down) ? 11 : 10);
              else if (strcmp(s_ft,"q2ks")==0) t = is_out ? 14 : ((is_v && lay>=0 && lay<2) || (is_down && lay>=0 && lay<nlay/8) ? 11 : 10);  /* Q2_K_S */
              else if (strcmp(s_ft,"q3ks")==0) t = is_out ? 14 : ((is_v && lay>=0 && lay<2) || (is_down && lay>=0 && lay<nlay/8) ? 12 : 11);  /* Q3_K_S */
              else if (strcmp(s_ft,"q3kl")==0) t = is_out ? 14 : (is_v ? 14 : (is_down ? (lay>=0 && lay<nlay/4 ? 13 : 12) : 12));                  /* Q3_K_L */
              else t = -1;
              if (t > 0) { q4=0;q8=0;emb16=0;e2k=e3k=e5k=e6k=e4k=eix=0;
                  if(t==10)e2k=1; else if(t==11)e3k=1; else if(t==12)e4k=1; else if(t==13)e5k=1; else if(t==14)e6k=1; }
          } }
        /* LLMMODELIO_ROUTER_FMT=bf16: il router (ffn_gate_inp) a BF16 (antirez DS4 usa il router BF16 piatto). */
        { static int s_bft=-1; if(s_bft<0){ const char* r=llmmodelio_model_policy_get("ROUTER_FMT"); s_bft=(r&&strcmp(r,"bf16")==0)?1:0; }
          if (s_bft && strstr(ti->name,"gate_inp")!=NULL && (isf32||isf16)) {
              uint64_t numel=1; for(int d=0;d<ti->ndim;d++) numel*=ti->dims[d];
              float* xf=(float*)malloc((size_t)numel*4);
              uint8_t* sraw=malloc((size_t)llm_ts_bytes(ti->ggml_type,ti->dims,ti->ndim));
              if(xf&&sraw){ FILE* fs=fopen(in_path,"rb");
                  if(fs){ fseek(fs,(long)(fi.data_start+ti->offset),SEEK_SET);
                      uint64_t sb=llm_ts_bytes(ti->ggml_type,ti->dims,ti->ndim);
                      if(fread(sraw,1,(size_t)sb,fs)==(size_t)sb){
                          if(isf32) memcpy(xf,sraw,(size_t)numel*4);
                          else { const uint16_t* hb=(const uint16_t*)sraw; for(uint64_t k=0;k<numel;k++) xf[k]=llmmodelio_h2f(hb[k]); }
                          uint8_t* q=(uint8_t*)malloc((size_t)numel*2); llmmodelio_bf16(xf,(int64_t)numel,q);
                          ts[i].name=ti->name; ts[i].ggml_type=30; ts[i].ndim=ti->ndim; ts[i].dims=dims+i*8;
                          for(int d=0;d<ti->ndim;d++)((uint64_t*)ts[i].dims)[d]=ti->dims[d];
                          ts[i].data=q; free(sraw); free(xf); fclose(fs); continue;
                      } fclose(fs); }
              } free(sraw); free(xf); }
        }
        int quant = q4 || e4k || e2k || e6k || e5k || e3k || eix || ei1s || ei1m || ei2s || ei2x || ei3x || ei3s || ei4x || ei4nl
                    || (is_exp && llmcodec_ggml_type_of_fmt(efmt) >= 0)   /* formati nativi codec (q4_1/q5_0/q5_1/q8_k/q1_0/q2_0/tq1_0/tq2_0) */
                    || emx || env || q8 || emb16;
        uint64_t srcb = 0;
        { uint64_t numel = 1; for (int d = 0; d < ti->ndim; d++) numel *= ti->dims[d];
          if ((srcb = llmcodec_gguf_tensor_bytes(ti), 0) != 0) {
              /* Il messaggio è il punto: prima questo stesso caso ritornava 0 e finiva nel
               * `srcb == 0` qui sotto, identico a un tensore vuoto, senza dire QUALE tipo. */
              fprintf(stderr, "LLM MODEL: %s: tipo GGML %d con %llu elementi: dimensione non calcolabile\n",
                      ti->name, ti->ggml_type, (unsigned long long)numel);
              srcb = 0;
          } }
        if (srcb == 0 || srcb > (1ull << 34)) { rc = -1; break; }
        void* sraw = malloc((size_t)srcb);
        if (sraw == NULL) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 ||
            fread(sraw, 1, (size_t)srcb, src) != (size_t)srcb) { free(sraw); rc = -1; break; }
        if (quant) {
            uint64_t numel = 1; for (int d = 0; d < ti->ndim; d++) numel *= ti->dims[d];
            /* sorgente -> float[] */
            float* xf = (float*)malloc((size_t)numel * sizeof(float));
            if (xf == NULL) { free(sraw); rc = -1; break; }
            if (isf32) memcpy(xf, sraw, (size_t)numel * 4);
            else { const uint16_t* hb = (const uint16_t*)sraw; for (uint64_t k = 0; k < numel; k++) xf[k] = llmmodelio_h2f(hb[k]); }
            if (emb16) {                                  /* embedding -> F16 (2x) */
                uint16_t* hh = (uint16_t*)malloc((size_t)numel * 2);
                if (hh == NULL) { free(xf); free(sraw); rc = -1; break; }
                for (uint64_t k = 0; k < numel; k++) hh[k] = llmcodec_f16_from_f32(xf[k]);
                ts[i].ggml_type = LLMCODEC_GGML_T_F16; ts[i].data = hh;
                free(xf); free(sraw);
            } else if (env) {   /* esperti -> NVFP4 (36B/64) */
                uint64_t qb = ((numel + 63) / 64) * 36;
                uint8_t* q = (uint8_t*)malloc((size_t)qb);
                if (q == NULL) { free(xf); free(sraw); rc = -1; break; }
                llmmodelio_nvfp4(xf, (int64_t)numel, q);
                ts[i].ggml_type = 40; ts[i].data = q;
                free(xf); free(sraw);
            } else if (emx) {   /* esperti -> MXFP4 (17B/32) */
                uint64_t qb = ((numel + 31) / 32) * 17;
                uint8_t* q = (uint8_t*)malloc((size_t)qb);
                if (q == NULL) { free(xf); free(sraw); rc = -1; break; }
                llmmodelio_mxfp4(xf, (int64_t)numel, q);
                ts[i].ggml_type = 39; ts[i].data = q;
                free(xf); free(sraw);
            } else if (e2k || e4k || e6k || e5k || e3k || eix || ei1s || ei1m || ei2s || ei2x || ei3x || ei3s || ei4x || ei4nl) {   /* esperti -> formato selezionato */
                int bpb = eix?66:ei1s?50:ei1m?56:ei2s?82:ei2x?74:ei3x?98:ei3s?110:ei4x?136:ei4nl?18:e2k?84:e4k?144:e6k?210:e5k?176:110;
                int ty  = eix?16:ei1s?19:ei1m?29:ei2s?22:ei2x?17:ei3x?18:ei3s?21:ei4x?23:ei4nl?20:e2k?10:e4k?12:e6k?14:e5k?13:11;
                uint64_t qb = ((numel + 255) / 256) * (uint64_t)bpb;
                uint8_t* q = (uint8_t*)malloc((size_t)qb);
                if (q == NULL) { free(xf); free(sraw); rc = -1; break; }
                const char* imx = llmmodelio_model_policy_get("IMATRIX"); if (imx == NULL) imx = llmmodelio_model_policy_get("IMATRIX");
                int64_t rl = 0; float* wts = imx ? llmmodelio_imatrix_weights(imx, ts[i].name, &rl) : NULL;
                int64_t weight_sets = ti->ndim > 2 ? (int64_t)ti->dims[2] : 1;
                int imx_ok = (wts != NULL && last % 256 == 0 && weight_sets > 0 &&
                              rl == (int64_t)last*weight_sets && numel % last == 0);
                if (eix) llmcodec_iq2_xxs_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei1s) llmcodec_iq1_s_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei1m) llmcodec_iq1_m_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei2s) llmcodec_iq2_s_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei2x) llmcodec_iq2_xs_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei3x) llmcodec_iq3_xxs_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei3s) llmcodec_iq3_s_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei4x) llmcodec_iq4_xs_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (ei4nl) llmcodec_iq4_nl_quantize(xf, (int64_t)numel, q,
                                              imx_ok ? wts : NULL, imx_ok ? rl : 0, (int64_t)last);
                else if (e2k) { if (imx_ok) llmmodelio_K_imatrix(xf, (int64_t)numel, q, wts, (int64_t)last, weight_sets, 84, LLMCODEC_GGML_T_Q2_K); else llm_q2_K(xf, (int64_t)numel, q); }
                else if (e4k) { if (imx_ok) llmmodelio_K_imatrix(xf, (int64_t)numel, q, wts, (int64_t)last, weight_sets, 144, LLMCODEC_GGML_T_Q4_K); else llm_q4_K(xf, (int64_t)numel, q); }
                else if (e6k) { if (imx_ok) llmmodelio_K_imatrix(xf, (int64_t)numel, q, wts, (int64_t)last, weight_sets, 210, LLMCODEC_GGML_T_Q6_K); else llm_q6_K(xf, (int64_t)numel, q); }
                else if (e5k) { if (imx_ok) llmmodelio_K_imatrix(xf, (int64_t)numel, q, wts, (int64_t)last, weight_sets, 176, LLMCODEC_GGML_T_Q5_K); else llm_q5_K(xf, (int64_t)numel, q); }
                else { if (imx_ok) llmmodelio_K_imatrix(xf, (int64_t)numel, q, wts, (int64_t)last, weight_sets, 110, LLMCODEC_GGML_T_Q3_K); else llm_q3_K(xf, (int64_t)numel, q); }
                free(wts);
                ts[i].ggml_type = ty; ts[i].data = q;
                free(xf); free(sraw);
            } else if (is_exp && (gt = llmcodec_ggml_type_of_fmt(efmt)) >= 0) {
                /* Formati NATIVI del codec GGML (q4_1/q5_0/q5_1/q8_k/q1_0/q2_0/tq1_0/tq2_0):
                 * encode generico llmcodec_ggml_quant_row + byte/blocco da llmcodec_ggml_block.
                 * Completano la SIMMETRIA encode/decode: ogni tipo del decoder ha il suo encoder. */
                uint64_t bb = 0, bv = 0;
                if (llmcodec_ggml_block(gt, &bb, &bv) != 0 || bv == 0 || bb == 0 || (numel % bv) != 0) { free(xf); free(sraw); rc = -1; break; }
                uint64_t qb2 = (numel / bv) * bb;
                uint8_t* q2 = (uint8_t*)malloc((size_t)qb2);
                if (q2 == NULL) { free(xf); free(sraw); rc = -1; break; }
                if (llmcodec_ggml_quant_row(gt, xf, q2, (int64_t)numel, NULL) != 0) { free(q2); free(xf); free(sraw); rc = -1; break; }
                ts[i].ggml_type = gt; ts[i].data = q2;
                free(xf); free(sraw);
            } else {
                uint64_t qb = ((numel + 31) / 32) * (q4 ? 18 : 34);
                uint8_t* q = (uint8_t*)malloc((size_t)qb);
                if (q == NULL) { free(xf); free(sraw); rc = -1; break; }
                if (q4) llmmodelio_q4_0(xf, (int64_t)numel, q);
                else    llmmodelio_q8_0(xf, (int64_t)numel, q);
                ts[i].ggml_type = q4 ? LLMCODEC_GGML_T_Q4_0 : LLMCODEC_GGML_T_Q8_0;
                ts[i].data = q;                        /* ownership */
                free(xf); free(sraw);
            }
        } else {
            ts[i].ggml_type = ti->ggml_type;
            ts[i].data = sraw;
        }
    }
    if (rc == 0) for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
    if (rc == 0) rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)fi.n_tensors, 32);
    for (uint64_t i = 0; i < fi.n_tensors; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}

/* ---- PLAN (model compiler): classifica i tensori per ruolo + formato proposto ----
 * Regole per NOME (pattern, ispirate alla ricetta DS4/antirez): gli esperti routed (la
 * maggioranza dei parametri) si comprimono aggressivamente; attention/shared/output restano
 * ad alta precisione; norm/router/stato esatti. Scrive un report per-tensore in out_path. */
static const char* llmmodelio_role_of(const char* name) {
    if (name == NULL) return "OTHER";
    if (strstr(name, "shared_expert") || strstr(name, "shexp")) return "SHARED_EXPERT";
    if (strstr(name, "_exps") || strstr(name, "exps.") || strstr(name, ".experts")) return "ROUTED_EXPERT";
    if (strstr(name, "router") || strstr(name, ".gate.") || strstr(name, "gate_inp")) return "ROUTER";
    /* NORM PRIMA di ATTENTION: 'attn_q_norm'/'output_norm' contengono 'norm' e vanno tenuti F32
     * (una norm quantizzata = modello rotto), anche se il nome contiene 'attn'. */
    if (strstr(name, "token_embd") || strstr(name, "patch_embd") || strstr(name, "position_embd")
        || strstr(name, "pos_embd") || strstr(name, "word_embeddings") || strstr(name, "embed_tokens")
        || strstr(name, "wte") || strstr(name, "wpe") || strstr(name, "embd")) return "EMBEDDING";
    if (strstr(name, "norm") || strstr(name, ".ln1") || strstr(name, ".ln2") || strstr(name, ".ln_")
        || strstr(name, "layer_norm") || strstr(name, "layernorm")) return "NORM";
    if (strstr(name, "attn") || strstr(name, "attention")) return "ATTENTION";
    if (strstr(name, "lm_head") || strstr(name, "output")) return "OUTPUT";
    if (strstr(name, "ssm") || strstr(name, "conv") || strstr(name, "dt_")) return "STATE";
    /* FFN/MLP denso (non-esperti): il 'knowledge' bulk di un modello denso -> quantizzabile. */
    if (strstr(name, "ffn_") || strstr(name, "mlp") || strstr(name, "fc1") || strstr(name, "fc2")
        || strstr(name, "feed_forward") || strstr(name, ".w1") || strstr(name, ".w2") || strstr(name, ".w3"))
        return "FFN";
    return "OTHER";
}
static const char* llmmodelio_proposed_of(const char* role) {
    if (strcmp(role, "ROUTED_EXPERT") == 0) return "IQ2_XXS|Q2_K";   /* bulk: aggressivo */
    if (strcmp(role, "EMBEDDING") == 0)      return "F16";            /* antirez: embedding F16 */
    if (strcmp(role, "SHARED_EXPERT") == 0) return "Q8_0";
    if (strcmp(role, "FFN") == 0)            return "Q8_0";
    if (strcmp(role, "ATTENTION") == 0)      return "Q8_0";
    if (strcmp(role, "OUTPUT") == 0)         return "Q8_0";
    if (strcmp(role, "ROUTER") == 0)         return "F32";
    if (strcmp(role, "NORM") == 0)           return "F32";
    if (strcmp(role, "STATE") == 0)          return "F32";
    return "F16";
}
/* Scrive il piano (testo per-tensore) in out_path. 0 ok / -1. */
int llmmodelio_model_plan(const char* in_path, const char* out_path) {
    if (in_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fi;
    if (llmcodec_gguf_read_info(in_path, &fi) != 0) return -1;
    FILE* f = fopen(out_path, "w");
    if (f == NULL) { llmcodec_gguf_free_info(&fi); return -1; }
    fprintf(f, "# LLM MODEL PLAN (role/format proposto) — %llu tensori\n", fi.n_tensors);
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        const char* role = llmmodelio_role_of(ti->name);
        fprintf(f, "%s\trole=%s\ttype=%d\tproposed=%s\tndim=%d\n",
                ti->name, role, ti->ggml_type, llmmodelio_proposed_of(role), ti->ndim);
    }
    fclose(f);
    llmcodec_gguf_free_info(&fi);
    return 0;
}

/* ---- Q4_0: delega al codec GGML (§0bis). ---- */
static void llmmodelio_q4_0(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q4_0, x, out, n, NULL);
}

/* ---- POLICY (role -> formato): l'utente guida la scelta di formato per ruolo ---- */
#define LLMMODELIO_POLICY_MAX 16
static char s_pol_role[LLMMODELIO_POLICY_MAX][24];
static char s_pol_fmt[LLMMODELIO_POLICY_MAX][512];   /* i valori includono PATH (imatrix), non solo formati brevi */
static int  s_pol_n = 0;
void llmmodelio_model_policy_set(const char* role, const char* fmt) {
    if (role == NULL || fmt == NULL) return;
    for (int i = 0; i < s_pol_n; i++) if (strcmp(s_pol_role[i], role) == 0) { snprintf(s_pol_fmt[i], 512, "%s", fmt); return; }
    if (s_pol_n >= LLMMODELIO_POLICY_MAX) return;
    snprintf(s_pol_role[s_pol_n], 24, "%s", role);
    snprintf(s_pol_fmt[s_pol_n], 512, "%s", fmt);
    s_pol_n++;
}
void llmmodelio_model_policy_reset(void) { s_pol_n = 0; }
const char* llmmodelio_model_policy_get(const char* role) {
    if (role == NULL) return NULL;
    for (int i = 0; i < s_pol_n; i++) if (strcmp(s_pol_role[i], role) == 0) return s_pol_fmt[i];
    return NULL;
}
void llmmodelio_model_policy_show(void) {
    fprintf(LOUT, "LLM POLICY (role -> formato):\n");
    if (s_pol_n == 0) fprintf(LOUT, "  (nessuna regola esplicita: si usano i default)\n");
    for (int i = 0; i < s_pol_n; i++) fprintf(LOUT, "  %-16s -> %s\n", s_pol_role[i], s_pol_fmt[i]);
    fprintf(LOUT, "  default: ROUTED_EXPERT->q4k  FFN->q4_0  EMBEDDING->f16  ATTENTION->q8_0  NORM->f32\n");
}

/* ---- POLICY DSL — Livello A (regole dichiarative WHEN/THEN, §4.3) ---------------------------
 * Una policy NOMINATA e' una lista ordinata di regole (condizioni -> azione); la prima che
 * matcha vince, altrimenti DEFAULT. Testo compatto (un solo argomento stringa, nessun token nuovo):
 *   LLM POLICY DEFINE "nome", "ROLE=ROUTED_EXPERT&LAYER>=12:q6k;LIKE=*_down_exps:q6k;DEFAULT:q4k"
 * Condizioni (AND con '&'):  ROLE=<r> · LIKE=<glob con * ?> · ARCH=<a> · LAYER<op><n> ·
 *   IMP<op><v> (imatrix) ;  op in  = != >= <= > <
 * Azione (dopo ':'): un formato (q2k/q3k/q4k/q5k/q6k/q4_0/q8_0/iq2xxs/mxfp4/nvfp4/f16/f32/bf16)
 *   oppure NOQUANT (lascia il tensore al tipo sorgente).
 * APPLY: `LLM POLICY APPLY "nome"[, "DRYRUN"]` -> attiva la policy (o solo report).
 * La decisione si integra col resto (recipe/tail/down): la REGOLA ha precedenza se matcha. */
#define POL_RULE_MAX 64
typedef struct { char cond[192]; char act[32]; } pol_rule_t;
typedef struct { char name[48]; pol_rule_t rules[POL_RULE_MAX]; int n; int has_def; char def_act[32]; } pol_def_t;
#define POL_DEF_MAX 16
static pol_def_t s_pols[POL_DEF_MAX]; static int s_pols_n = 0;
static int s_pol_active = -1;   /* indice della policy attiva, -1 = nessuna (usa role->fmt) */

/* OVERRIDE PER-TENSORE (Livello B): name(glob) -> fmt. Priorita' MASSIMA in pol_decide: e' il
 * meccanismo con cui il codice BASIC (in GPM) applica le decisioni di una SUB. `LLM POLICY
 * SETITEM "<name-glob>", "<fmt>"` aggiunge/sostituisce; `CLEARITEM` azzera. */
#define POL_ITEM_MAX 512
static char s_item_glob[POL_ITEM_MAX][160];
static char s_item_fmt[POL_ITEM_MAX][32];
static int s_item_n = 0;

static pol_def_t* pol_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < s_pols_n; i++) if (strcmp(s_pols[i].name, name) == 0) return &s_pols[i];
    return NULL;
}
/* glob semplice: '*' (qualsiasi seq) e '?' (un char). */
static int pol_glob(const char* g, const char* s) {
    if (!*g) return !*s;
    if (*g == '*') { if (pol_glob(g+1, s)) return 1; if (*s && pol_glob(g, s+1)) return 1; return 0; }
    if (*g == '?') return *s ? pol_glob(g+1, s+1) : 0;
    return (*g == *s) ? pol_glob(g+1, s+1) : 0;
}
static int pol_cmp(const char* p, int* op, double* v) {  /* parse "<op><num>" -> op(0=,1!=,2>=,3<=,4>,5<), v */
    while (*p == ' ') p++;
    if (p[0]=='!' && p[1]=='=') { *op=1; p+=2; } else if (p[0]=='>'&&p[1]=='=') { *op=2; p+=2; }
    else if (p[0]=='<'&&p[1]=='=') { *op=3; p+=2; } else if (*p=='>') { *op=4; p++; }
    else if (*p=='<') { *op=5; p++; } else if (*p=='=') { *op=0; p++; } else return 0;
    *v = atof(p); return 1;
}
static int pol_ok(int op, double x, double v) {
    switch (op) { case 0: return x==v; case 1: return x!=v; case 2: return x>=v;
                  case 3: return x<=v; case 4: return x>v;  case 5: return x<v; }
    return 0;
}
/* valuta le condizioni (AND con '&') di una regola. */
static int pol_eval(const char* cond, const char* role, const char* name, int layer, const char* arch, double imp) {
    if (!cond || !*cond) return 1;
    char buf[192]; snprintf(buf, sizeof buf, "%s", cond);
    char* save = NULL;
    for (char* t = strtok_r(buf, "&", &save); t; t = strtok_r(NULL, "&", &save)) {
        while (*t == ' ') t++;
        int op; double v;
        if      (strncasecmp(t, "ROLE=", 5) == 0) { if (strcasecmp(t+5, role ? role : "") != 0) return 0; }
        else if (strncasecmp(t, "LIKE=", 5) == 0) { if (!pol_glob(t+5, name ? name : "")) return 0; }
        else if (strncasecmp(t, "ARCH=", 5) == 0) { if (strcasecmp(t+5, arch ? arch : "") != 0) return 0; }
        else if (strncasecmp(t, "LAYER", 5) == 0) { if (!pol_cmp(t+5, &op, &v) || !pol_ok(op, (double)layer, v)) return 0; }
        else if (strncasecmp(t, "IMP",   3) == 0) { if (!pol_cmp(t+3, &op, &v) || !pol_ok(op, imp, v)) return 0; }
        else return 0;   /* condizione ignota -> non matcha (conservativo, mai match permissivo) */
    }
    return 1;
}
/* QuantIR I11 — risolve i 4 assi (pesi/activation/accum/output) dell'azione della policy
 * (SETITEM > DSL > ruolo). Azione: "q4_k" (solo pesi, retro-compat) OPPURE
 * "w=q4_k,a=fp8,acc=f16,o=q8_0" (assi del CALCOLO). Default: f16/f32/pesi. */
static void pol_decide_axes(const char* role, const char* name, int layer, const char* arch, double imp,
                            char* w, size_t wc, char* ac, size_t acc_, char* am, size_t amc, char* ou, size_t oc)
{
    const char* act = NULL;
    for (int i = 0; i < s_item_n; i++) if (pol_glob(s_item_glob[i], name ? name : "")) { act = s_item_fmt[i]; break; }
    if (!act && s_pol_active >= 0) {
        pol_def_t* d = &s_pols[s_pol_active];
        for (int i = 0; i < d->n; i++) if (pol_eval(d->rules[i].cond, role, name, layer, arch, imp)) { act = d->rules[i].act; break; }
        if (!act && d->has_def) act = d->def_act;
    }
    w[0]=ac[0]=am[0]=ou[0]=0;
    if (act && strchr(act, '=')) {
        char buf[192]; snprintf(buf, sizeof buf, "%s", act);
        char* save = NULL;
        for (char* t = strtok_r(buf, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
            char* eq = strchr(t, '='); if (!eq) continue; *eq = 0;
            const char* k = t; const char* v = eq + 1;
            if      (strcasecmp(k,"w")==0   || strcasecmp(k,"weights")==0)    snprintf(w, wc, "%s", v);
            else if (strcasecmp(k,"a")==0   || strcasecmp(k,"activation")==0) snprintf(ac, acc_, "%s", v);
            else if (strcasecmp(k,"acc")==0 || strcasecmp(k,"accum")==0)      snprintf(am, amc, "%s", v);
            else if (strcasecmp(k,"o")==0   || strcasecmp(k,"output")==0)     snprintf(ou, oc, "%s", v);
        }
    } else if (act) { snprintf(w, wc, "%s", act); }   /* forma stringa: solo pesi */
    /* B4 — chiave di policy di prima classe "ACTIVATION" (fallback se l'azione a 4 assi non la
     * specifica). Es. `POLICY SET "ACTIVATION", "fp8"` == `a=fp8` nell'azione. */
    if (!ac[0]) { const char* ap = llmmodelio_model_policy_get("ACTIVATION"); if (ap && *ap) snprintf(ac, acc_, "%s", ap); }
    /* i DEFAULT (ruolo/arch) li applica il CHIAMANTE: qui solo gli assi DECISI dalla policy. */
}
/* decisione della policy ATTIVA per un tensore (solo PESI); NULL se nessuna policy o nessun match.
 * Ritorna un buffer statico valido fino alla chiamata successiva. */
static const char* pol_decide(const char* role, const char* name, int layer, const char* arch, double imp) {
    static char bw[16], ba[16], bm[16], bo[16];
    pol_decide_axes(role, name, layer, arch, imp, bw, sizeof bw, ba, sizeof ba, bm, sizeof bm, bo, sizeof bo);
    return bw[0] ? bw : NULL;
}
/* DEFINE: parsa il testo compatto in s_pols[]. Ritorna 0/-1. */
static int pol_define(const char* name, const char* text) {
    if (!name || !*name || !text) return -1;
    pol_def_t* d = pol_find(name);
    if (!d) { if (s_pols_n >= POL_DEF_MAX) return -1; d = &s_pols[s_pols_n++]; snprintf(d->name, sizeof d->name, "%s", name); }
    d->n = 0; d->has_def = 0; d->def_act[0] = 0;
    char* copy = strdup(text); if (!copy) return -1;
    char* save = NULL;
    for (char* seg = strtok_r(copy, ";", &save); seg; seg = strtok_r(NULL, ";", &save)) {
        while (*seg == ' ') seg++;
        if (!*seg) continue;
        char* colon = strrchr(seg, ':');
        if (!colon) { free(copy); return -1; }   /* regola senza azione */
        *colon = 0;
        const char* cond = seg; const char* act = colon + 1;
        while (*act == ' ') act++;
        if (strncasecmp(cond, "DEFAULT", 7) == 0 || !*cond) { d->has_def = 1; snprintf(d->def_act, sizeof d->def_act, "%s", act); continue; }
        if (d->n >= POL_RULE_MAX) break;
        snprintf(d->rules[d->n].cond, sizeof d->rules[d->n].cond, "%s", cond);
        snprintf(d->rules[d->n].act,  sizeof d->rules[d->n].act,  "%s", act);
        d->n++;
    }
    free(copy);
    return 0;
}
/* PIANO della POLICY ATTIVA vs DEFAULT (report-only, nessuna scrittura): per ogni tensore del
 * modello, il formato deciso dalla policy vs quello di default (per ruolo). Ritorna il numero di
 * tensori cambiati, scrivendo il report in `out`. Base del gate: quanti/come cambiano i formati.
 * Gli item sono MEMORIZZATI (s_plan[]) per gli accessor PLANITEMS/PLANITEM. */
#define POL_PLAN_MAX 8192
typedef struct { char name[96]; char role[24]; char dflt[16]; char use[16]; } pol_plan_item_t;
static pol_plan_item_t s_plan[POL_PLAN_MAX]; static int s_plan_n = 0; static int s_plan_changed = 0;
int llmmodelio_policy_plan_to(const char* in_path, char* out, size_t cap) {
    if (!in_path || !out || !cap) return -1;
    LlmCodecGgufFileInfo fi;
    if (llmcodec_gguf_read_info(in_path, &fi) != 0) {
        snprintf(out, cap, "LLM POLICY DIFF: modello non leggibile '%s'\n", in_path);
        return -1;
    }
    size_t o = 0; int changed = 0; s_plan_n = 0; s_plan_changed = 0;
    const char* nm = (s_pol_active >= 0) ? s_pols[s_pol_active].name : "(nessuna policy attiva)";
    o += (size_t)snprintf(out+o, cap-o, "LLM POLICY DIFF '%s' vs DEFAULT — %llu tensori\n",
                          nm, (unsigned long long)fi.n_tensors);
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        const char* role = llmmodelio_role_of(ti->name);
        const char* dflt = llmmodelio_proposed_of(role);
        int lay = -1; if (strncmp(ti->name, "blk.", 4) == 0) lay = atoi(ti->name + 4);
        const char* pol = pol_decide(role, ti->name, lay, "", 1.0);
        const char* use = pol ? pol : dflt;
        int ch = (strcasecmp(use, dflt) != 0);
        if (ch) changed++;
        if (s_plan_n < POL_PLAN_MAX) {
            pol_plan_item_t* it = &s_plan[s_plan_n++];
            snprintf(it->name, sizeof it->name, "%s", ti->name);
            snprintf(it->role, sizeof it->role, "%s", role);
            snprintf(it->dflt, sizeof it->dflt, "%s", dflt);
            snprintf(it->use,  sizeof it->use,  "%s", use);
        }
        if (ch && o + 160 < cap)
            o += (size_t)snprintf(out+o, cap-o, "  %-40s %-16s %s -> %s\n", ti->name, role, dflt, use);
    }
    s_plan_changed = changed;
    o += (size_t)snprintf(out+o, cap-o, "  -> %d tensori cambiati dal default\n", changed);
    llmcodec_gguf_free_info(&fi);
    return changed;
}
/* PLANITEMS: JSON del piano memorizzato dall'ultimo DIFF. PLANITEM(i): un item ("name\trole\tdflt\tuse"). */
int llmmodelio_policy_items_to(char* out, size_t cap) {
    if (!out || !cap) return -1;
    size_t o = 0;
    o += (size_t)snprintf(out+o, cap-o, "{\"count\":%d,\"changed\":%d,\"items\":[", s_plan_n, s_plan_changed);
    for (int i = 0; i < s_plan_n && o + 128 < cap; i++) {
        pol_plan_item_t* it = &s_plan[i];
        o += (size_t)snprintf(out+o, cap-o, "%s{\"name\":\"%s\",\"role\":\"%s\",\"default\":\"%s\",\"policy\":\"%s\"}",
                              i ? "," : "", it->name, it->role, it->dflt, it->use);
    }
    o += (size_t)snprintf(out+o, cap-o, "]}\n");
    return s_plan_n;
}
int llmmodelio_policy_item_to(int idx, char* out, size_t cap) {
    if (!out || !cap) return -1;
    if (idx < 0 || idx >= s_plan_n) { snprintf(out, cap, "LLM POLICY ITEM: indice %d fuori range (0..%d)\n", idx, s_plan_n - 1); return -1; }
    pol_plan_item_t* it = &s_plan[idx];
    snprintf(out, cap, "%s\t%s\t%s\t%s\n", it->name, it->role, it->dflt, it->use);
    return 0;
}
/* GATE INTEGRATO (apply->quant->misura->reject/rollback): quantizza `base` in `out` con la policy
 * ATTIVA (il quantize consulta pol_decide), poi misura col gate (ppl di `ref` vs `out`). Se il gate
 * NON accetta -> ROLLBACK (rimuove `out`). Verdetto JSON in `report`. `ref` opzionale (vuoto ->
 * accetta senza misura). Report-only lato modello se rifiutato (nessun artefatto lasciato). */
int llmmodelio_policy_gate_to(const char* base, const char* ref, const char* out, char* report, size_t cap) {
    if (!report || !cap) return -1;
    report[0] = 0;
    if (!base || !*base || !out || !*out) { snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"missing base/out\"}"); return -1; }
    if (llmmodelio_model_quantize(base, out) != 0) { snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"quantize failed\"}"); return -1; }
    if (!ref || !*ref) { snprintf(report, cap, "{\"verdict\":\"ACCEPT\",\"reason\":\"no ref (quantized only)\",\"out\":\"%s\"}", out); return 0; }
    char g[512]; llmmodelio_model_gate(ref, out, NULL, g, sizeof g);
    if (strstr(g, "\"ACCEPT\"") == NULL) {
        remove(out);   /* rollback: il candidato rifiutato non resta */
        snprintf(report, cap, "{\"verdict\":\"REJECT-FLAG\",\"rolled_back\":true,\"gate\":%s}", g);
        return 0;
    }
    snprintf(report, cap, "{\"verdict\":\"ACCEPT\",\"out\":\"%s\",\"gate\":%s}", out, g);
    return 0;
}

/* get_scale_min_k4: decodifica scale/min di un sotto-blocco Q4_K (usata anche fuori dai kernel). */
static void llmmodelio_get_scale_min_k4(int j, const uint8_t* q, uint8_t* d, uint8_t* m) {
    if (j < 4) { *d = q[j] & 63; *m = q[j + 4] & 63; }
    else { *d = (q[j+4] & 0xF) | ((q[j-4] >> 6) << 4); *m = (q[j+4] >> 4) | ((q[j] >> 6) << 4); }
}

/* ---- Q4_K: delega al codec GGML (§0bis). ---- */
static void llm_q4_K(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q4_K, x, out, n, NULL);
}

/* ---- Q2_K: delega al codec GGML (§0bis). ---- */
static void llm_q2_K(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q2_K, x, out, n, NULL);
}

/* ---- make_qx_quants (Q6_K) / make_q3_quants (Q3_K) — port fedele ggml ---- */
#define LLMMODELIO_GROUP_MAX_EPS 1e-15f



/* ---- Q6_K: delega al codec GGML (§0bis). ---- */
static void llm_q6_K(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q6_K, x, out, n, NULL);
}
/* ---- Q5_K: delega al codec GGML (§0bis). ---- */
static void llm_q5_K(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q5_K, x, out, n, NULL);
}
/* ---- Q3_K: delega al codec GGML (§0bis). ---- */
static void llm_q3_K(const float* x, int64_t n, uint8_t* out) {
    llmcodec_ggml_quant_row(LLMCODEC_GGML_T_Q3_K, x, out, n, NULL);
}

/* ---- Q4_K/Q5_K con imatrix: una riga per volta e una matrice pesi per esperto, come ggml. ---- */
static int llmmodelio_K_imatrix(const float* x, int64_t n, uint8_t* out, const float* qw,
                         int64_t row_len, int64_t weight_sets, int bpb, int ggml_type) {
    if (!x || !out || row_len <= 0 || row_len % 256 != 0 || n % row_len != 0 ||
        weight_sets <= 0) return -1;
    int64_t nbl = row_len/256, nrows = n/row_len;
    if (nrows % weight_sets != 0) return -1;
    const int64_t rows_per_set = nrows/weight_sets;
    for (int64_t row = 0; row < nrows; row++)
        for (int64_t b = 0; b < nbl; b++) {
            const float* xb = x + row*row_len + b*256;
            uint8_t* ob = out + (row*nbl + b)*bpb;
            const float* wb = qw ? qw + (row/rows_per_set)*row_len + b*256 : NULL;
            llmcodec_ggml_quant_row(ggml_type, xb, ob, 256, wb);
        }
    return 0;
}

int llmmodelio_quantize_q4_k_weighted(const float* x, int64_t nrows, int64_t n_per_row,
                                    uint8_t* out, const float* quant_weights, int64_t weight_sets) {
    if (nrows <= 0 || n_per_row <= 0 || nrows > INT64_MAX/n_per_row) return -1;
    return llmmodelio_K_imatrix(x, nrows*n_per_row, out, quant_weights, n_per_row, weight_sets, 144, LLMCODEC_GGML_T_Q4_K);
}

/* ---- EXTRACT: estrae l'esperto `expert` del layer `layer` come FFN denso (fragment GGUF).
 * I tensori exps hanno dims [in,out,n_experts] (expert = ultima dim, blocco contiguo finale). ---- */
int llmmodelio_model_extract(const char* in_path, const char* out_path, int64_t layer, int64_t expert) {
    if (in_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    char prefix[64]; snprintf(prefix, sizeof prefix, "blk.%lld.", (long long)layer);
    static const char* suf[4] = { "ffn_gate_exps.weight", "ffn_up_exps.weight", "ffn_down_exps.weight", "ffn_gate_inp.weight" };
    int idx[4] = { -1, -1, -1, -1 };
    for (uint64_t i = 0; i < fi.n_tensors; i++)
        for (int s = 0; s < 4; s++) { char want[256]; snprintf(want, sizeof want, "%s%s", prefix, suf[s]); if (strcmp(fi.tensors[i].name, want) == 0) idx[s] = (int)i; }
    if (idx[0] < 0 || idx[1] < 0 || idx[2] < 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    FILE* src = fopen(in_path, "rb");
    if (src == NULL) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc(4, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc(4 * 8, sizeof(uint64_t));
    static char names[4][256];
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    int nout = 0, rc = 0;
    for (int s = 0; s < 4 && rc == 0; s++) {
        if (idx[s] < 0) continue;
        LlmCodecGgufTensorInfo* ti = &fi.tensors[idx[s]];
        uint64_t numel = 1; for (int d = 0; d < ti->ndim; d++) numel *= ti->dims[d];
        uint64_t E = ti->ndim > 0 ? ti->dims[ti->ndim-1] : 0;
        if (E == 0 || expert < 0 || (uint64_t)expert >= E) continue;
        /* Prima: `tot` 0 (tipo ignoto) → `per` 0 → `fread` di 0 byte RIESCE e produce un tensore
         * VUOTO in silenzio. Lo slicing dell'esperto è il punto in cui lo 0 diventava un file
         * scritto, non un errore: qui si ferma. NON è stata aggiunta la pretesa che `tot % E == 0`
         * (che prima non c'era): sarebbe severità nuova su un path non testabile qui. */
        uint64_t tot = 0, per = 0;
        if ((tot = llmcodec_gguf_tensor_bytes(ti), 0) != 0) {
            fprintf(stderr, "LLM MODEL EXPERT: %s: tipo GGML %d con %llu elementi: dimensione non calcolabile\n",
                    ti->name, ti->ggml_type, (unsigned long long)numel);
            rc = -1; break;
        }
        per = tot / E;
        if (per == 0) { rc = -1; break; }
        void* buf = malloc((size_t)per);
        if (buf == NULL) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset + (uint64_t)expert * per), SEEK_SET) != 0 || fread(buf, 1, (size_t)per, src) != (size_t)per) { free(buf); rc = -1; break; }
        char nm[256]; snprintf(nm, sizeof nm, "%s", ti->name);
        char* p = strstr(nm, "_exps"); if (p) memmove(p, p + 5, strlen(p + 5) + 1);
        snprintf(names[nout], sizeof names[nout], "%s", nm);
        ts[nout].name = names[nout]; ts[nout].ggml_type = ti->ggml_type;
        ts[nout].ndim = ti->ndim - 1; ts[nout].dims = dims + nout * 8;
        for (int d = 0; d < ti->ndim - 1; d++) ((uint64_t*)ts[nout].dims)[d] = ti->dims[d];
        ts[nout].data = buf;
        nout++;
    }
    if (rc == 0 && nout > 0) { for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]); rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, nout, 32); }
    for (int i = 0; i < nout; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 && nout > 0 ? 0 : -1;
}

/* ---- MERGE: media dei tensori (F32/F16) presenti in entrambi i modelli (model soup). ---- */
int llmmodelio_model_merge(const char* a_path, const char* b_path, const char* out_path) {
    if (a_path == NULL || b_path == NULL || out_path == NULL) return -1;
    LlmCodecGgufFileInfo fa, fb; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(a_path, &kv, &nkv, &fa) != 0) return -1;
    if (llmcodec_gguf_read_info(b_path, &fb) != 0) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fa); return -1; }
    FILE* sa = fopen(a_path, "rb"); FILE* sb = fopen(b_path, "rb");
    if (sa == NULL || sb == NULL) { if (sa) fclose(sa); if (sb) fclose(sb); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fa); llmcodec_gguf_free_info(&fb); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fa.n_tensors, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)fa.n_tensors * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    int rc = 0, merged = 0;
    /* F6/G-opt1: MODEL SOUP. `MERGE_METHOD` = mean (default) | dare | ties (dare/ties operano sul
     * task-vector delta=(b-a) con base a). `MERGE_P`=P(drop) DARE (def .1); `MERGE_K`=frazione
     * tenuta TIES (def .2). Deterministico (rng per tensore). */
    const char* mmeth = llmmodelio_model_policy_get("MERGE_METHOD");
    int method = 0; if (mmeth) { if (strcasecmp(mmeth, "dare") == 0) method = 1; else if (strcasecmp(mmeth, "ties") == 0) method = 2; }
    double p_drop = 0.1; { const char* pp = llmmodelio_model_policy_get("MERGE_P"); if (pp && *pp) p_drop = atof(pp); } if (p_drop < 0.0) p_drop = 0.0; if (p_drop > 0.99) p_drop = 0.99;
    double k_keep = 0.2; { const char* kk = llmmodelio_model_policy_get("MERGE_K"); if (kk && *kk) k_keep = atof(kk); } if (k_keep < 0.0) k_keep = 0.0; if (k_keep > 1.0) k_keep = 1.0;
    unsigned rng = 0x9e3779b9u;
    for (uint64_t i = 0; i < fa.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fa.tensors[i];
        ts[i].name = ti->name; ts[i].ggml_type = ti->ggml_type; ts[i].ndim = ti->ndim; ts[i].dims = dims + i * 8;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[i].dims)[d] = ti->dims[d];
        uint64_t b = 0;
        if (llmcodec_ggml_type_bytes(ti->ggml_type, llm_numel(ti->dims, ti->ndim), &b) != 0) { rc = -1; break; }
        if (b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b);
        if (buf == NULL) { rc = -1; break; }
        if (fseek(sa, (long)(fa.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, sa) != (size_t)b) { free(buf); rc = -1; break; }
        /* match in B con stesso nome/tipo/dims -> media (solo F32=0 / F16=1) */
        int mb = -1;
        for (uint64_t j = 0; j < fb.n_tensors; j++) {
            LlmCodecGgufTensorInfo* tj = &fb.tensors[j];
            if (strcmp(tj->name, ti->name) != 0 || tj->ggml_type != ti->ggml_type || tj->ndim != ti->ndim) continue;
            int same = 1; for (int d = 0; d < ti->ndim; d++) if (tj->dims[d] != ti->dims[d]) same = 0;
            if (same) { mb = (int)j; break; }
        }
        if (mb >= 0 && (ti->ggml_type == 0 || ti->ggml_type == 1)) {
            LlmCodecGgufTensorInfo* tj = &fb.tensors[mb];
            uint64_t nb2 = 0;
            if (llmcodec_ggml_type_bytes(tj->ggml_type, llm_numel(tj->dims, tj->ndim), &nb2) != 0) nb2 = 0;
            void* buf2 = malloc((size_t)nb2);
            /* `nb2 > 0` è parte della guardia: un `malloc(0)` che ritorna non-NULL con un fread
             * di 0 byte "riesce", e la media sotto leggerebbe una zona vuota come se fosse il
             * tensore. (La media scatta solo per F32/F16, che sono sempre in tabella: qui è la
             * forma corretta della guardia, non un caso atteso.) */
            if (buf2 && nb2 > 0) {
                if (fseek(sb, (long)(fb.data_start + tj->offset), SEEK_SET) == 0 && fread(buf2, 1, (size_t)nb2, sb) == (size_t)nb2) {
                    uint64_t n = llm_numel(ti->dims, ti->ndim);
                    float* o = (float*)buf;
                    #define VA(kk) ((ti->ggml_type == 0) ? o[kk] : llmmodelio_h2f(((uint16_t*)buf)[kk]))
                    #define VB(kk) ((tj->ggml_type == 0) ? ((float*)buf2)[kk] : llmmodelio_h2f(((uint16_t*)buf2)[kk]))
                    if (method == 2 && n > 0) {   /* TIES: trim (soglia = (1-k_keep)-quantile di |delta|) + rescale */
                        float mx = 0.0f; for (uint64_t k = 0; k < n; k++) { float d = fabsf(VB(k) - VA(k)); if (d > mx) mx = d; }
                        long want = (long)((1.0 - k_keep) * (double)n);
                        double thr = 0.0; long kept = 0;
                        if (mx > 0.0f && want > 0) {
                            int hist[256]; memset(hist, 0, sizeof hist);
                            for (uint64_t k = 0; k < n; k++) { int bb = (int)(fabsf(VB(k) - VA(k)) / mx * 255.0f); if (bb < 0) bb = 0; if (bb > 255) bb = 255; hist[bb]++; }
                            long acc = 0; int tb = 0; for (tb = 0; tb < 256; tb++) { acc += hist[tb]; if (acc >= want) break; }
                            thr = (double)tb / 255.0 * (double)mx;
                            for (uint64_t k = 0; k < n; k++) if ((double)fabsf(VB(k) - VA(k)) >= thr) kept++;
                        }
                        double rescale = kept > 0 ? (double)n / (double)kept : 0.0;
                        for (uint64_t k = 0; k < n; k++) { float d = VB(k) - VA(k); float m = VA(k) + ((kept > 0 && (double)fabsf(d) >= thr) ? (float)(d * rescale) : 0.0f); if (ti->ggml_type == 0) o[k] = m; else ((uint16_t*)buf)[k] = llmcodec_f16_from_f32(m); }
                    } else {
                        for (uint64_t k = 0; k < n; k++) {
                            float va = VA(k), vb = VB(k);
                            float m;
                            if (method == 1) {   /* DARE: drop+rescale del delta */
                                float d = vb - va;
                                rng = rng * 1103515245u + 12345u;
                                double r = (double)((rng >> 8) & 0xffffffu) / (double)0x1000000;
                                if (r < p_drop) d = 0.0f; else d = (float)(d / (1.0 - p_drop));
                                m = va + d;
                            } else m = 0.5f*(va + vb);
                            if (ti->ggml_type == 0) o[k] = m; else ((uint16_t*)buf)[k] = llmcodec_f16_from_f32(m);
                        }
                    }
                    #undef VA
                    #undef VB
                    merged++;
                }
                free(buf2);
            }
        }
        ts[i].data = buf;
    }
    if (rc == 0) { for (int i = 0; i < nkv; i++) llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]); rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)fa.n_tensors, 32); }
    for (uint64_t i = 0; i < fa.n_tensors; i++) free((void*)ts[i].data);
    fclose(sa); fclose(sb); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fa); llmcodec_gguf_free_info(&fb);
    return rc == 0 ? 0 : -1;
}

/* ---- PRUNE: riduce gli esperti a `keep` (i primi), con router COERENTE (stessa fetta) e
 * aggiornamento di `*.expert_count` / `*.expert_used_count`. I tensori esperti hanno l'esperto
 * nell'ultima dim: la fetta e' il prefisso contiguo. ---- */
/* Ritorna una copia (malloc) di `json` con il valore intero della chiave `"key"` -> `val`.
 * Usato dal PRUNE per aggiornare il config.json VERBATIM dopo la riduzione esperti. */
static char* llm_json_set_int(const char* json, const char* key, long val) {
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(json, pat); if (!p) return NULL;
    p += strlen(pat);
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') ++p;
    if (*p != ':') return NULL; ++p;
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') ++p;
    const char* d0 = p; while (*p=='-'||(*p>='0'&&*p<='9')) ++p; const char* d1 = p;
    if (d0 == d1) return NULL;
    size_t pre = (size_t)(d0 - json), post = strlen(d1);
    char buf[32]; int bl = snprintf(buf, sizeof buf, "%ld", val);
    char* o = (char*)malloc(pre + (size_t)bl + post + 1); if (!o) return NULL;
    memcpy(o, json, pre); memcpy(o + pre, buf, (size_t)bl); memcpy(o + pre + bl, d1, post + 1);
    return o;
}
static int llm_is_expert_tensor(const char* name, int ndim, uint64_t last) {
    if (ndim < 2 || last == 0) return 0;
    return (strstr(name, "_exps") != NULL || strstr(name, "experts") != NULL
            || strstr(name, "gate_inp") != NULL || strstr(name, "expert") != NULL
            /* naming MLX/HF (modelli importati): esperti switch_mlp.*_proj, router mlp.gate. */
            || strstr(name, "switch_mlp") != NULL || strstr(name, ".mlp.gate.") != NULL);
}
/* PRUNE per IMPORTANZA: per ogni layer sceglie i `keep` esperti con la norma di colonna del
 * router piu' alta (proxy di uso), e fa una GATHER coerente (stessi indici) su esperti + router.
 * LLMMODELIO_PRUNE=first -> invece i primi N. Esps [in,out,E]: esperto = blocco contiguo finale. */
int llmmodelio_model_prune(const char* in_path, const char* out_path, int64_t keep) {
    if (in_path == NULL || out_path == NULL || keep <= 0) return -1;
    /* Criterio di selezione: POLICY("PRUNE_EXPERT") > LLMMODELIO_PRUNE > default "first".
 * Cosi' si sceglie da BASIC:  LLM POLICY SET "PRUNE_EXPERT", "norm_asc"  */
    int mode = 0; { const char* m = llmmodelio_model_policy_get("PRUNE_EXPERT");
                    if (m == NULL) m = llmmodelio_model_policy_get("PRUNE_EXPERT"); if (m) { if (strcmp(m,"top")==0) mode=1; else if (strcmp(m,"bottom")==0) mode=2; else if (strcmp(m,"rand")==0) mode=3; else if (strcmp(m,"ctop")==0) mode=4; else if (strcmp(m,"cbot")==0) mode=5; else if (strcmp(m,"usage")==0) mode=6; else if (strcmp(m,"imp")==0) mode=7; } }
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    uint64_t E = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        const LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t last = ti->ndim > 0 ? ti->dims[ti->ndim-1] : 0;
        if (last && (strstr(ti->name, "_exps") || strstr(ti->name, "gate_inp") || strstr(ti->name, "switch_mlp"))) { E = last; break; }
    }
    if (E == 0 || (uint64_t)keep > E) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    FILE* src = fopen(in_path, "rb");
    if (src == NULL) { llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)fi.n_tensors, sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)fi.n_tensors * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkv ? nkv : 1), sizeof(*wkv));
    int rc = 0, pruned = 0;
    char cur_layer[64] = ""; int64_t sel[512]; int nsel = 0;
    for (uint64_t i = 0; i < fi.n_tensors && rc == 0; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t numel = llm_numel(ti->dims, ti->ndim);
        uint64_t last = ti->ndim > 0 ? ti->dims[ti->ndim-1] : 0;
        int eax = (ti->ndim > 0 && ti->dims[ti->ndim-1] == E) ? ti->ndim-1 : 0;
        int is_exp = llm_is_expert_tensor(ti->name, ti->ndim, last) && (last == E || ti->dims[0] == E);
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0) {
            fprintf(stderr, "LLM MODEL: %s: tipo GGML %d con %llu elementi: dimensione non calcolabile\n",
                    ti->name, ti->ggml_type, (unsigned long long)numel);
            rc = -1; break;
        }
        if (b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b);
        if (buf == NULL) { rc = -1; break; }
        if (fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, src) != (size_t)b) { free(buf); rc = -1; break; }
        ts[i].name = ti->name; ts[i].ggml_type = ti->ggml_type; ts[i].dims = dims + i*8; ts[i].ndim = ti->ndim;
        for (int d = 0; d < ti->ndim; d++) ((uint64_t*)ts[i].dims)[d] = ti->dims[d];
        if (is_exp) {
            /* layer corrente dal nome (blk.N.) */
            char lay[64] = ""; { const char* bp = strstr(ti->name, "blk."); if (bp) bp += 4; else { bp = strstr(ti->name, ".layers."); if (bp) bp += 8; }
                if (bp) { int k = 0; while (bp[k] && bp[k] != '.' && k < 62) { lay[k] = bp[k]; k++; } lay[k] = 0; } }
            if (strcmp(lay, cur_layer) != 0) {
                snprintf(cur_layer, sizeof cur_layer, "%s", lay); nsel = 0;
                char rn[256]; snprintf(rn, sizeof rn, "blk.%s.ffn_gate_inp.weight", lay);
                int ri = -1; for (uint64_t j = 0; j < fi.n_tensors; j++) if (strcmp(fi.tensors[j].name, rn) == 0) { ri = (int)j; break; }
                if (mode == 0 || ri < 0) { for (int64_t e = 0; e < keep; e++) sel[e] = e; nsel = (int)keep; }
                else {
                    if (mode == 6) {   /* USO REALE: counts del routing dall'imatrix per questo layer */
                        const char* imx = llmmodelio_model_policy_get("IMATRIX"); if (!imx) imx = llmmodelio_model_policy_get("IMATRIX");
                        char xt[256]; snprintf(xt, sizeof xt, "blk.%s.ffn_gate_exps.weight", lay);
                        int64_t nu = 0; float* use = imx ? llmmodelio_imatrix_expert_usage(imx, xt, &nu) : NULL;
                        if (use && nu == (int64_t)E) {
                            for (int64_t a = 0; a < keep; a++) { int64_t best = -1; float bv = -1; for (int64_t e = 0; e < (int64_t)E; e++) { int u = 0; for (int q = 0; q < a; q++) if (sel[q] == e) { u = 1; break; } if (!u && use[e] > bv) { bv = use[e]; best = e; } } sel[a] = best < 0 ? a : best; }
                            nsel = (int)keep;
                        }
                        free(use);
                        if (nsel == 0) { for (int64_t e = 0; e < keep; e++) sel[e] = e; nsel = (int)keep; }
                    } else {
                    LlmCodecGgufTensorInfo* rt = &fi.tensors[ri];
                    uint64_t rh = rt->ndim > 0 ? rt->dims[0] : 0;
                    float* rw = (float*)malloc((size_t)(rh * E) * 4);
                    if (rw) {
                        int r16 = (rt->ggml_type == 1), r32 = (rt->ggml_type == 0);
                        if (r16 || r32) {
                            uint64_t rb = rh*E*(r16?2:4); uint8_t* rb8 = malloc(rb);
                            if (rb8 && fseek(src,(long)(fi.data_start+rt->offset),SEEK_SET)==0 && fread(rb8,1,rb,src)==rb) {
                                if (mode == 4 || mode == 5 || mode == 7) {   /* norma CENTRATA: ||col_e - media_col|| (toglie la componente comune) */
                                    double* mean = (double*)calloc(rh, sizeof(double));
                                    for (uint64_t hh = 0; hh < rh; hh++) { double s = 0; for (uint64_t e = 0; e < E; e++) s += r16? llmmodelio_h2f(((uint16_t*)rb8)[e*rh+hh]) : ((float*)rb8)[e*rh+hh]; mean[hh] = s/(double)E; }
                                    for (uint64_t e = 0; e < E; e++) { double s = 0; for (uint64_t hh = 0; hh < rh; hh++) { double v = (r16? llmmodelio_h2f(((uint16_t*)rb8)[e*rh+hh]) : ((float*)rb8)[e*rh+hh]) - mean[hh]; s += v*v; } rw[e] = (float)sqrt(s); }
                                    free(mean);
                                    if (mode == 7) {   /* QUALITY-GUIDED: importanza = uso_reale(routing) x norma_router_centrata */
                                        const char* imx7 = llmmodelio_model_policy_get("IMATRIX");
                                        char xt7[256]; snprintf(xt7, sizeof xt7, "blk.%s.ffn_gate_exps.weight", lay);
                                        int64_t nu7 = 0; float* use7 = imx7 ? llmmodelio_imatrix_expert_usage(imx7, xt7, &nu7) : NULL;
                                        if (use7 && nu7 == (int64_t)E) for (uint64_t e = 0; e < E; e++) rw[e] *= use7[e];
                                        free(use7);
                                    }
                                } else { for (uint64_t e = 0; e < E; e++) { double s = 0; for (uint64_t hh = 0; hh < rh; hh++) { float v = r16? llmmodelio_h2f(((uint16_t*)rb8)[e*rh+hh]) : ((float*)rb8)[e*rh+hh]; s += (double)v*v; } rw[e] = (float)sqrt(s); } }
                                if (mode == 4 || mode == 5 || mode == 7) { int asc = (mode == 5); for (int64_t a = 0; a < keep; a++) { int64_t best = -1; float bv = asc ? 3.4e38f : -1.0f; for (uint64_t e = 0; e < E; e++) { int used = 0; for (int q = 0; q < a; q++) if (sel[q] == (int64_t)e) { used = 1; break; } if (used) continue; if (asc ? (rw[e] < bv) : (rw[e] > bv)) { bv = rw[e]; best = (int64_t)e; } } sel[a] = best < 0 ? a : best; } }
                                    else if (mode == 3) { unsigned st = 12345u; for (int64_t a = 0; a < keep; a++) { st = st*1103515245u + 12345u; uint64_t cand = (st >> 8) % E; int used = 0; for (int q = 0; q < a; q++) if (sel[q] == (int64_t)cand) { used = 1; break; } if (used) { a--; continue; } sel[a] = (int64_t)cand; } }
                                    else for (int64_t a = 0; a < keep; a++) { int64_t best = -1; float bv = (mode == 2) ? 3.4e38f : -1.0f; for (uint64_t e = 0; e < E; e++) { int used = 0; for (int q = 0; q < a; q++) if (sel[q] == (int64_t)e) { used = 1; break; } if (used) continue; if (mode == 2 ? (rw[e] < bv) : (rw[e] > bv)) { bv = rw[e]; best = (int64_t)e; } } sel[a] = best < 0 ? a : best; }
                                nsel = (int)keep;
                            }
                            free(rb8);
                        }
                        free(rw);
                    }
                    fseek(src, (long)(fi.data_start + ti->offset), SEEK_SET);   /* ripristina */
                    }
                }
                if (nsel == 0) { for (int64_t e = 0; e < keep; e++) sel[e] = e; nsel = (int)keep; }
            }
            /* gather: esps [d0,d1,E] con esperto = ultima dim (blocco contiguo) */
            uint64_t per = numel / E;
            uint64_t esz = (b / E);
            uint8_t* nb = (uint8_t*)malloc((size_t)(esz * (uint64_t)keep));
            if (nb == NULL) { free(buf); rc = -1; break; }
            for (int q = 0; q < nsel; q++) memcpy(nb + (size_t)q*esz, (uint8_t*)buf + (size_t)sel[q]*esz, (size_t)esz);
            (void)per;
            free(buf);
            ts[i].data = nb;
            ((uint64_t*)ts[i].dims)[eax] = (uint64_t)keep;
            pruned++;
        } else {
            ts[i].data = buf;
        }
    }
    if (rc == 0) {
        for (int i = 0; i < nkv; i++) {
            if (strstr(kv[i].key, ".expert_count") || strstr(kv[i].key, ".expert_used_count")) {
                if (kv[i].type == LLMCODEC_GGUF_MT_U32 && kv[i].u32 > (uint32_t)keep) kv[i].u32 = (uint32_t)keep;
                else if (kv[i].type == LLMCODEC_GGUF_MT_I32 && kv[i].i32 > (int32_t)keep) kv[i].i32 = (int32_t)keep;
            }
            if (strcmp(kv[i].key, "model.gmb.config") == 0 && kv[i].type == LLMCODEC_GGUF_MT_STRING && kv[i].str) {
                char* patched = llm_json_set_int(kv[i].str, "n_routed_experts", (long)keep);
                if (patched == NULL) patched = llm_json_set_int(kv[i].str, "num_experts", (long)keep);
                if (patched) kv[i].str = patched;
            }
            llmcodec_gguf_meta_to_writer(&kv[i], &wkv[i]);
        }
        rc = llmcodec_gguf_write(out_path, wkv, nkv, ts, (int)fi.n_tensors, 32);
    }
    for (uint64_t i = 0; i < fi.n_tensors; i++) free((void*)ts[i].data);
    fclose(src); free(ts); free(dims); free(wkv);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return rc == 0 ? 0 : -1;
}


/* ---- PROFILE CHE CALCOLA: forward sul corpus (token ids) -> conteggi di routing.
 * Chiude il cerchio BASIC: nessun imatrix esterno. ids_path: interi separati da spazi/virgole. ---- */
int llmmodelio_model_profile_compute(const char* model_path, const char* ids_path, const char* out_path, int64_t chunk) {
    if (model_path == NULL || ids_path == NULL || out_path == NULL) return -1;
    FILE* fi = fopen(ids_path, "r");
    if (fi == NULL) return -1;
    int cap = 1 << 16, n = 0; int32_t* toks = (int32_t*)malloc((size_t)cap * sizeof(int32_t));
    int v;
    while (fscanf(fi, "%d", &v) == 1) { if (n >= cap) { cap *= 2; toks = (int32_t*)realloc(toks, (size_t)cap*sizeof(int32_t)); } toks[n++] = v; }
    /* salta i separatori: fscanf("%d") ignora [,] e spazi */
    fclose(fi);
    if (n <= 0) { free(toks); return -1; }
    if (chunk <= 0) chunk = 512;
    int nl = 0, ne = 0; double* acc = NULL;
    for (int off = 0; off < n; off += (int)chunk) {
        int m = (n - off) < chunk ? (n - off) : (int)chunk;
        int a = 0, b = 0; double* c = llmmodelio_routing_counts(model_path, toks + off, m, &a, &b);
        if (c == NULL) { free(acc); free(toks); return -1; }
        if (acc == NULL) { acc = (double*)calloc((size_t)a * (size_t)b, sizeof(double)); nl = a; ne = b; }
        for (int k = 0; k < a * b; k++) acc[k] += c[k];
        free(c);
    }
    free(toks);
    if (acc == NULL) return -1;
    FILE* f = fopen(out_path, "w");
    if (f == NULL) { free(acc); return -1; }
    fprintf(f, "# LLM ROUTING PROFILE (CALCOLATO dal forward sui token di %s)\n", ids_path);
    double grand = 0;
    for (int l = 0; l < nl; l++) {
        double tot = 0; int zeros = 0;
        for (int e = 0; e < ne; e++) { tot += acc[(size_t)l*ne+e]; if (acc[(size_t)l*ne+e] <= 0.0) zeros++; }
        grand += tot;
        fprintf(f, "blk.%d  esperto: %d, attivazioni: %.0f, mai usati: %d\n", l, ne, tot, zeros);
        if (zeros) { fprintf(f, "  mai usati:"); for (int e = 0; e < ne; e++) if (acc[(size_t)l*ne+e] <= 0.0) fprintf(f, " %d", e); fprintf(f, "\n"); }
        fprintf(f, "  top-8:"); { double* cp = (double*)malloc((size_t)ne*sizeof(double)); memcpy(cp, acc+(size_t)l*ne, (size_t)ne*sizeof(double));
            for (int a2 = 0; a2 < 8 && a2 < ne; a2++) { int bi = -1; double bv = -1; for (int e = 0; e < ne; e++) if (cp[e] > bv) { bv = cp[e]; bi = e; } if (bi < 0) break; fprintf(f, " %d(%.0f)", bi, bv); cp[bi] = -1.0; } free(cp); }
        fprintf(f, "\n");
    }
    fprintf(f, "# layer: %d, attivazioni totali: %.0f\n", nl, grand);
    fclose(f);
    /* Scrivo anche il profilo come GGUF (i counts per layer) COSI' il prune lo legge via IMATRIX:
     * chiude il cerchio BASIC:  LLM PROFILE <model>,<ids>,<profile.gguf>  ->  IMATRIX=profile.gguf */
    {
        LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)nl, sizeof(*ts));
        uint64_t* dstore = (uint64_t*)calloc((size_t)nl * 2, sizeof(uint64_t));
        char (*names)[64] = (char(*)[64])calloc((size_t)nl, 64);
        float** datas = (float**)calloc((size_t)nl, sizeof(float*));
        LlmCodecGgufMetaKV mkv[2]; memset(mkv, 0, sizeof mkv);
        mkv[0].key = "general.architecture"; mkv[0].type = LLMCODEC_GGUF_MT_STRING; mkv[0].str = "routing-profile";
        mkv[1].key = "profile.n_layer"; mkv[1].type = LLMCODEC_GGUF_MT_U32; mkv[1].u32 = (uint32_t)nl;
        if (ts && dstore && names && datas) {
            for (int l = 0; l < nl; l++) {
                datas[l] = (float*)malloc((size_t)ne * sizeof(float));
                for (int e = 0; e < ne; e++) datas[l][e] = (float)acc[(size_t)l*ne+e];
                snprintf(names[l], 64, "blk.%d.ffn_gate_exps.weight.counts", l);
                dstore[l*2] = (uint64_t)ne; dstore[l*2+1] = 1;
                ts[l].name = names[l]; ts[l].ggml_type = 0; ts[l].ndim = 2; ts[l].dims = dstore + l*2; ts[l].data = datas[l];
            }
            if (llmcodec_gguf_write(out_path, mkv, 2, ts, nl, 32) == 0)
                fprintf(stderr, "[profile] scritto profilo routing GGUF: %s\n", out_path);
            for (int l = 0; l < nl; l++) free(datas[l]);
        }
        free(ts); free(dstore); free(names); free(datas);
    }
    free(acc);
    return 0;
}

/* ---- PROFILE: report del routing reale (uso per esperto dai counts dell'imatrix). Scrive un
 * file di testo: per ogni layer, esperti mai usati, top usati, totale. Rende ispezionabile da
 * BASIC la calibrazione che guida il prune. ---- */
int llmmodelio_model_profile(const char* model_path, const char* out_path) {
    if (model_path == NULL || out_path == NULL) return -1;
    const char* imx = llmmodelio_model_policy_get("IMATRIX"); if (imx == NULL) imx = llmmodelio_model_policy_get("IMATRIX");
    if (imx == NULL) return -1;
    LlmCodecGgufFileInfo fi;
    if (llmcodec_gguf_read_info(model_path, &fi) != 0) return -1;
    FILE* f = fopen(out_path, "w");
    if (f == NULL) { llmcodec_gguf_free_info(&fi); return -1; }
    fprintf(f, "# LLM ROUTING PROFILE — uso reale per esperto (counts del routing, imatrix)\n");
    fprintf(f, "# imatrix: %s\n", imx);
    int nlay = 0; double grand = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        const char* nm = fi.tensors[i].name;
        const char* p = strstr(nm, "ffn_gate_exps");
        if (p == NULL || fi.tensors[i].ndim < 2) continue;
        int64_t E = (int64_t)fi.tensors[i].dims[fi.tensors[i].ndim-1];
        int64_t nu = 0; float* use = llmmodelio_imatrix_expert_usage(imx, nm, &nu);
        if (use == NULL || nu != E) { free(use); continue; }
        double tot = 0; int zeros = 0;
        for (int64_t e = 0; e < E; e++) { tot += use[e]; if (use[e] <= 0.0f) zeros++; }
        grand += tot; nlay++;
        fprintf(f, "%-32s esperto: %lld, attivazioni: %.0f, mai usati: %d\n", nm, (long long)E, tot, zeros);
        if (zeros > 0) { fprintf(f, "  mai usati:"); for (int64_t e = 0; e < E; e++) if (use[e] <= 0.0f) fprintf(f, " %lld", (long long)e); fprintf(f, "\n"); }
        /* top-8 per uso */
        fprintf(f, "  top-8:");
        for (int a = 0; a < 8 && a < E; a++) { int64_t best = -1; float bv = -1; for (int64_t e = 0; e < E; e++) { int u = 0; for (int q = 0; q < a; q++) { /* evita ripetizioni marcando con NaN sotto */ } (void)u; if (use[e] > bv) { bv = use[e]; best = e; } } if (best < 0) break; fprintf(f, " %lld(%.0f)", (long long)best, use[best]); use[best] = -1.0f; }
        fprintf(f, "\n");
        free(use);
    }
    fprintf(f, "# layer analizzati: %d, attivazioni totali: %.0f\n", nlay, grand);
    fclose(f); llmcodec_gguf_free_info(&fi);
    return nlay > 0 ? 0 : -1;
}


/* ---- AUTO (§E3): allocazione bit per-tensore guidata dalla SENSIBILITA' sotto un budget bpw.
 * Pre-pass: misura l'errore relativo di ricostruzione (proxy affine group=32, pesato dall'imatrix)
 * per ogni tensore eleggibile ai candidati {2,3,4,5,6,8} -> allocatore greedy -> formato per-tensore
 * (K-quant per la misura; Q8_0 per il non-eleggibile). Poi richiama il quantize col formato deciso. */
static int llmmodelio_bits_to_ggml(int b) { (void)b; return -1; }  /* ora cand[] sono gia' ggml type */

int llmmodelio_model_quantize_auto(const char* in_path, const char* out_path, double target_bpw) {
    if (in_path == NULL || out_path == NULL || target_bpw <= 0.0) return -1;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(in_path, &kv, &nkv, &fi) != 0) return -1;
    int n = (int)fi.n_tensors;
    /* 6 candidati (§ 2026-09-22, riabilitati ora che la misura usa la forma quadratica ΔWᵀΣΔW su
     * campioni reali invece del diagonale: il 5-cand {Q2K,Q4K,Q5K,Q6K,Q8_0} era stato escluso sotto
     * il VECCHIO misuratore diagonale — dava PPL peggiore e un Q3_K anomalo (rel>Q2_K). Col misuratore
     * a campioni, attn_v/ffn_down_exps hanno rel[Q4_K] alto (~0.05-0.08): con SOLO {Q2K,Q4K,Q8_0}
     * l'allocatore doveva scegliere fra "troppo poco" (Q4_K) e "troppo caro" (Q8_0, +4bpw), lasciando
     * la maggior parte a Q4_K sotto-precisa -> PPL 2x peggio del baseline. Q5_K/Q6_K colmano il salto
     * (la baseline Q4_K_M usa proprio Q6_K per attn_v/output/down_exps). */
    /* BUDGET IN bpw REALI x1000 (non i bit nominali): Q2_K=84/256=2.625, Q3_K=110/256=3.4375,
     * Q4_K=144/256=4.5, Q5_K=176/256=5.5, Q6_K=210/256=6.5625, Q8_0=34/32=8.5. */
    const int cand[6] = { 2625, 3438, 4500, 5500, 6563, 8500 };
    static const int cand_ty[6] = { 10, 11, 12, 13, 14, 8 };   /* -> Q2_K/Q3_K/Q4_K/Q5_K/Q6_K/Q8_0 */
    const int NCAND = 6;
    int* ftab = (int*)malloc((size_t)n * sizeof(int));
    int* elig = (int*)malloc((size_t)n * sizeof(int)); int nel = 0;
    for (int i = 0; i < n; i++) ftab[i] = -1;
    const char* imx = llmmodelio_model_policy_get("IMATRIX"); if (imx == NULL) imx = llmmodelio_model_policy_get("IMATRIX");
    FILE* f = fopen(in_path, "rb");
    if (f == NULL) { free(ftab); free(elig); llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi); return -1; }
    /* elenco eleggibili: pesi 2D/3D quantizzabili, blocco ne[0] % 256 (K-quant), non norm/embedding */
    for (int i = 0; i < n; i++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[i];
        uint64_t last = ti->ndim > 0 ? ti->dims[0] : 0;
        if (!(ti->ggml_type == 0 || ti->ggml_type == 1) || ti->ndim < 2 || last == 0 || last % 256 != 0) continue;
        const char* role = llmmodelio_role_of(ti->name);
        if (strcmp(role, "EMBEDDING") == 0 || strcmp(role, "NORM") == 0) continue;
        /* il ROUTER (ffn_gate_inp) NON si quantizza: decide l'instradamento (la baseline Q4_K_M lo
         * tiene F32). Quantizzarlo degrada la selezione degli esperti -> era un pezzo del gap. */
        if (strstr(ti->name, "gate_inp") != NULL || strcmp(role, "ROUTER") == 0) continue;
        /* OUTPUT (lm_head): l'imatrix NON lo copre (8 tensori/layer, non l'output) -> la metrica
         * ricade sul diagonale NON pesato e lo sottostima -> Q4_K sul lm_head = logits corrotti
         * (verificato: PPL "negative std dev"). Senza dati di calibrazione NON si rialloca: resta
         * il default per-ruolo (Q8_0). Principio: se manca la misura, non decidere. */
        if (strcmp(role, "OUTPUT") == 0) continue;
        elig[nel++] = i;
    }
    uint64_t* npar = (uint64_t*)calloc((size_t)(nel ? nel : 1), 8);
    double* rel = (double*)calloc((size_t)(nel ? nel : 1) * (size_t)NCAND, 8);
    int rc = 0;
    for (int q = 0; q < nel && rc == 0; q++) {
        LlmCodecGgufTensorInfo* ti = &fi.tensors[elig[q]];
        uint64_t numel = llm_numel(ti->dims, ti->ndim), last = ti->dims[0];
        uint64_t b = 0;
        if ((b = llmcodec_gguf_tensor_bytes(ti), 0) != 0 || b == 0 || b > (1ull << 34)) { rc = -1; break; }
        uint8_t* raw = (uint8_t*)malloc((size_t)b);
        float* xf = (float*)malloc((size_t)numel * 4);
        if (raw == NULL || xf == NULL) { free(raw); free(xf); rc = -1; break; }
        if (fseek(f, (long)(fi.data_start + ti->offset), SEEK_SET) != 0 || fread(raw, 1, (size_t)b, f) != (size_t)b) { free(raw); free(xf); rc = -1; break; }
        if (ti->ggml_type == 0) memcpy(xf, raw, (size_t)numel * 4);
        else { const uint16_t* hb = (const uint16_t*)raw; for (uint64_t k = 0; k < numel; k++) xf[k] = llmmodelio_h2f(hb[k]); }
        free(raw);
        uint32_t cols = (uint32_t)last, rows = (uint32_t)(numel / last);
        int64_t nl = 0; float* imp = imx ? llmmodelio_imatrix_weights(imx, ti->name, &nl) : NULL;
        const float* impu = (imp && nl == (int64_t)cols) ? imp : NULL;
        /* Campioni Monte Carlo (§ diagnosi 2026-09-22): tensori per-esperto (ndim==3, ffn_*_exps) usano
         * la forma quadratica ΔWᵀΣΔW stimata su LLMMODELIO_SAMP_K_EXPERT campioni/esperto; i condivisi
         * (attn_q/k/v/output) su LLMMODELIO_SAMP_K_SHARED. Mismatch di forma o imatrix vecchio (senza
         * ".samples") -> ricade sul diagonale invariato (llmmodelio_roundtrip_rel via llmmodelio_roundtrip_rel_cov). */
        int64_t rows_per_expert = (ti->ndim == 3) ? (int64_t)ti->dims[1] : 0;
        int64_t expect_k = rows_per_expert > 0 ? (int64_t)LLMMODELIO_SAMP_K_EXPERT : (int64_t)LLMMODELIO_SAMP_K_SHARED;
        int64_t n_expert_s = rows_per_expert > 0 ? (int64_t)ti->dims[2] : 1;
        int64_t samp_kt = 0, samp_cols = 0;
        float* samp = imx ? llmmodelio_imatrix_samples(imx, ti->name, &samp_kt, &samp_cols) : NULL;
        int samp_ok = samp && samp_cols == (int64_t)cols && samp_kt == expect_k * n_expert_s;
        /* PAVIMENTO DIAGONALE (suggerimento diagnosi 2026-09-22): la metrica a campioni soffre di
         * cancellazione eccessiva (Q4_K..Q8_0 appaiono quasi uguali). Si prende il MAX fra la stima
         * a campioni e la diagonale scontata: la diagonale (che distingue 10-50x) fa da pavimento.
         * LLM_AUTO_FLOOR = sconto (default 0.5; 0 = solo campioni, 1 = diagonale piena). */
        static double s_floor = -1.0;
        if (s_floor < 0.0) { const char* e = llmmodelio_model_policy_get("AUTO_FLOOR"); s_floor = e ? atof(e) : 0.5; }
        for (int c = 0; c < NCAND; c++) {
            double rd = llmmodelio_roundtrip_rel(cand_ty[c], xf, numel, cols, impu);
            static int s_nosamp = -1; if (s_nosamp < 0) s_nosamp = llmmodelio_model_policy_get("AUTO_NOSAMP") != NULL;
            double r = (samp_ok && !s_nosamp) ? llmmodelio_roundtrip_rel_cov(cand_ty[c], xf, numel, cols, samp, expect_k, rows_per_expert) : rd;
            if (s_floor > 0.0 && rd > 0.0 && rd * s_floor > r) r = rd * s_floor;
            if (r < 0) r = 1e9; rel[(size_t)q*NCAND+c] = r;
        }
        for (int c = 1; c < NCAND; c++) if (rel[(size_t)q*NCAND+c] > rel[(size_t)q*NCAND+c-1]) rel[(size_t)q*NCAND+c] = rel[(size_t)q*NCAND+c-1];
        npar[q] = numel;
        free(imp); free(samp); free(xf);
    }
    fclose(f);
    int* oc = (int*)calloc((size_t)(nel ? nel : 1), 4);
    int* floorc = (int*)calloc((size_t)(nel ? nel : 1), 4);
    /* PAVIMENTO PER RUOLO (diagnosi 2026-09-22): la metrica per-tensore SOTTOSTIMA l'attenzione ->
     * Q4_K/Q6_K dove la baseline Q4_K_M usa Q8_0/Q6_K = errore ~2x. Il floor per ruolo, preso dalla
     * baseline, è l'ANCORA dell'allocazione (S3): l'allocatore parte DA QUI e promuove solo sopra,
     * entro il budget — così il risultato è "baseline + deviazioni nette" e rispetta il bpw target.
     * Disattivabile con POLICY AUTO_NOFLOOR. Indici candidati: 0=Q2_K 1=Q3_K 2=Q4_K 3=Q5_K 4=Q6_K 5=Q8_0 */
    static int s_nofloor = -1; if (s_nofloor < 0) s_nofloor = llmmodelio_model_policy_get("AUTO_NOFLOOR") != NULL;
    int nlay_f = 16; for (int k = 0; k < nkv; k++)
        if (strstr(kv[k].key, "block_count") && kv[k].type == LLMCODEC_GGUF_MT_U32) nlay_f = (int)kv[k].u32;
    if (!s_nofloor) for (int q = 0; q < nel; q++) {
        const char* nm = fi.tensors[elig[q]].name;
        const char* ro = llmmodelio_role_of(nm);
        /* Ricetta baseline _M REPLICATA ESATTAMENTE (llama-quant.cpp): base Q4_K (cand 2); OUTPUT
         * (cand 4=Q6_K) e attn_v/ffn_down (denso E esperti) SOLO nelle fasce "more"
         * (lay<nlay/8 || lay>=7·nlay/8 || (lay-nlay/8)%3==2) -> Q6_K. Così l'ancora ha lo STESSO
         * bpw della baseline e le promozioni avvengono DENTRO quel budget (win a pari bpw). */
        int lay = -1; if (strncmp(nm, "blk.", 4) == 0) lay = atoi(nm + 4);
        int more = (lay >= 0) && (lay < nlay_f/8 || lay >= 7*nlay_f/8 || ((lay - nlay_f/8) % 3 == 2));
        int is_v = strstr(nm, "attn_v.") != NULL, is_down = strstr(nm, "ffn_down") != NULL;
        int is_out = strcmp(nm, "output.weight") == 0;
        int fl = is_out ? 4 : (more && (is_v || is_down) ? 4 : 2);
        /* floor DATO-DRIVEN: override per-ruolo da POLICY `FLOOR_<RUOLO>` (sensibilità empirica). */
        { char k[64]; snprintf(k, sizeof k, "FLOOR_%s", ro);
          const char* ov = llmmodelio_model_policy_get(k);
          if (ov && *ov) fl = atoi(ov); }
        floorc[q] = fl;
    }
    /* AUTO_SWAP (opt-in, default OFF): abilita la deviazione verso il basso (swap). Di default
     * l'ancora = baseline resta un VINCOLO (AUTO >= baseline, pari bpw): lo swap col proxy `rel`
     * peggiora la PPL al pari bpw (misurato), quindi è sperimentale. */
    int allow_swap = llmmodelio_model_policy_get("AUTO_SWAP") != NULL;
    if (rc == 0 && nel > 0)
        rc = llmengine_moe_quant_alloc_bits_min(npar, rel, nel, NCAND, cand, floorc, target_bpw * 1000.0, oc, allow_swap);
    if (rc == -2) {   /* il floor (baseline) da solo supera il target: si usa il floor */
        /* NESSUN messaggio: era una diagnostica su LOUT (poi stderr) e finiva nell'output del
         * programma BASIC solo su alcune gambe (VM/C stdout; runner C99Native che fa 2>&1) →
         * divergenza di parità su llm/llm_quant_auto_ok.bas. Il comportamento (uso del floor)
         * resta; se serve la diagnostica si espone dal registro opzioni, non come stampa. */
        rc = 0;
    }
    if (rc == 0) for (int q = 0; q < nel; q++) {
        ftab[elig[q]] = cand_ty[oc[q]];
    }
    g_auto_type = ftab;
    int qrc = (rc == 0) ? llmmodelio_model_quantize(in_path, out_path) : -1;
    g_auto_type = NULL;
    free(ftab); free(elig); free(npar); free(rel); free(oc); free(floorc);
    llmcodec_gguf_meta_free(kv, nkv); llmcodec_gguf_free_info(&fi);
    return qrc;
}

/* ---- Errore di ricostruzione REALE per formato (quantize -> dequantize), per l'allocatore §E3.
 * Dequant portati da llama.cpp ggml-quants.c (Q2_K/Q4_K/Q8_0; gli altri formati sono i candidati).
 * Fattorizzato in llmmodelio_quant_dequant (condiviso fra llmmodelio_roundtrip_rel e la variante _cov sotto) per
 * non duplicare il porting bit-per-bit. ---- */
static int llmmodelio_quant_dequant(int ty, const float* x, uint64_t numel, float* dq) {
    uint64_t bb = 0; uint32_t bv = 0;
    switch (ty) { case 10: bb = 84; bv = 256; break;   /* Q2_K */
                  case 11: bb = 110; bv = 256; break;  /* Q3_K */
                  case 12: bb = 144; bv = 256; break;  /* Q4_K */
                  case 13: bb = 176; bv = 256; break;  /* Q5_K */
                  case 14: bb = 210; bv = 256; break;  /* Q6_K */
                  case 8:  bb = 34; bv = 32; break;    /* Q8_0 */
                  default: return -1; }
    if (bv == 0 || numel % bv != 0) return -1;
    uint64_t nb = numel / bv;
    uint8_t* q = (uint8_t*)malloc((size_t)nb * bb);
    if (!q) return -1;
    if (ty == 10) llm_q2_K(x, (int64_t)numel, q);
    else if (ty == 11) llm_q3_K(x, (int64_t)numel, q);
    else if (ty == 12) llm_q4_K(x, (int64_t)numel, q);
    else if (ty == 13) llm_q5_K(x, (int64_t)numel, q);
    else if (ty == 14) llm_q6_K(x, (int64_t)numel, q);
    else llmmodelio_q8_0(x, (int64_t)numel, q);
    if (ty == 8) {                                    /* Q8_0: {d:F16, qs[32]:i8} */
        for (uint64_t b = 0; b < nb; b++) {
            uint16_t dh; memcpy(&dh, q + b*34, 2); float d = llmmodelio_h2f(dh);
            const int8_t* qs = (const int8_t*)(q + b*34 + 2);
            for (int j = 0; j < 32; j++) dq[b*32 + j] = qs[j] * d;
        }
    } else if (ty == 10) {                            /* Q2_K: {scales[16], qs[64], d, dmin} @0,16,80,82 */
        for (uint64_t b = 0; b < nb; b++) {
            const uint8_t* blk = q + b*84;
            uint16_t h1, h2; memcpy(&h1, blk+80, 2); memcpy(&h2, blk+82, 2);
            float d = llmmodelio_h2f(h1), mn = llmmodelio_h2f(h2);
            const uint8_t* sc = blk; const uint8_t* qq = blk + 16; float* y = dq + b*256;
            int is = 0;
            for (int n = 0; n < 256; n += 128) {
                int shift = 0;
                for (int j = 0; j < 4; j++) {
                    for (int pass = 0; pass < 2; pass++) {
                        uint8_t s = sc[is++]; float dl = d*(s & 0xF), ml = mn*(s >> 4);
                        for (int l = 0; l < 16; l++) *y++ = dl * ((qq[l] >> shift) & 3) - ml;
                    }
                    shift += 2;
                }
                qq += 32;
            }
        }
    } else if (ty == 12) {                            /* Q4_K: {d, dmin, scales[12], qs[128]} @0,2,4,16 */
        for (uint64_t b = 0; b < nb; b++) {
            const uint8_t* blk = q + b*144;
            uint16_t h1, h2; memcpy(&h1, blk, 2); memcpy(&h2, blk+2, 2);
            float d = llmmodelio_h2f(h1), mn = llmmodelio_h2f(h2);
            const uint8_t* sc12 = blk + 4; const uint8_t* qq = blk + 16; float* y = dq + b*256;
            int is = 0;
            for (int n = 0; n < 256; n += 64) {
                uint8_t sc, m;
                llmmodelio_get_scale_min_k4(is + 0, sc12, &sc, &m); float d1 = d*sc, m1 = mn*m;
                llmmodelio_get_scale_min_k4(is + 1, sc12, &sc, &m); float d2 = d*sc, m2 = mn*m;
                for (int l = 0; l < 32; l++) y[n + l] = d1 * (qq[l] & 0xF) - m1;
                for (int l = 0; l < 32; l++) y[n + 32 + l] = d2 * (qq[l] >> 4) - m2;
                qq += 32; is += 2;
            }
        }
    }
    else if (ty == 11) {                              /* Q3_K: {hmask[32], qs[64], scales[12], d} @0,32,96,108 */
        for (uint64_t b = 0; b < nb; b++) {
            const uint8_t* blk = q + b*110;
            uint16_t h; memcpy(&h, blk+108, 2); float dall = llmmodelio_h2f(h);
            const uint8_t* hm = blk; const uint8_t* qq = blk+32;   /* hmask@0, qs@32 (v. commento riga sopra) — invertiti: bug */
            uint32_t aux[4]; memcpy(aux, blk+96, 12); uint32_t tmp = aux[2];
            aux[2] = ((aux[0] >> 4) & 0x0f0f0f0fu) | (((tmp >> 4) & 0x03030303u) << 4);
            aux[3] = ((aux[1] >> 4) & 0x0f0f0f0fu) | (((tmp >> 6) & 0x03030303u) << 4);
            aux[0] = (aux[0] & 0x0f0f0f0fu) | (((tmp >> 0) & 0x03030303u) << 4);
            aux[1] = (aux[1] & 0x0f0f0f0fu) | (((tmp >> 2) & 0x03030303u) << 4);
            const int8_t* scales3 = (const int8_t*)aux;
            float* y = dq + b*256; int is = 0; uint8_t m = 1;
            for (int n = 0; n < 256; n += 128) {
                int shift = 0;
                for (int j = 0; j < 4; j++) {
                    float dl = dall*(scales3[is++]-32);
                    for (int l = 0; l < 16; l++) y[n + j*32 + l] = dl * ((int8_t)((qq[l] >> shift) & 3) - ((hm[l] & m) ? 0 : 4));
                    dl = dall*(scales3[is++]-32);
                    for (int l = 0; l < 16; l++) y[n + j*32 + 16 + l] = dl * ((int8_t)((qq[l+16] >> shift) & 3) - ((hm[l+16] & m) ? 0 : 4));
                    shift += 2; m <<= 1;
                }
                qq += 32;
            }
        }
    } else if (ty == 13) {                            /* Q5_K: {d,dmin,scales[12],qh[32],qs[128]} @0,2,4,16,48 */
        for (uint64_t b = 0; b < nb; b++) {
            const uint8_t* blk = q + b*176;
            uint16_t h1,h2; memcpy(&h1,blk,2); memcpy(&h2,blk+2,2);
            float d = llmmodelio_h2f(h1), mn = llmmodelio_h2f(h2);
            const uint8_t* sc12 = blk+4; const uint8_t* qh = blk+16; const uint8_t* ql = blk+48;
            float* y = dq + b*256; int is = 0; uint8_t u1 = 1, u2 = 2;
            for (int n = 0; n < 256; n += 64) {
                uint8_t sc, m;
                llmmodelio_get_scale_min_k4(is+0, sc12, &sc, &m); float d1 = d*sc, m1 = mn*m;
                llmmodelio_get_scale_min_k4(is+1, sc12, &sc, &m); float d2 = d*sc, m2 = mn*m;
                for (int l = 0; l < 32; l++) y[n+l] = d1 * ((ql[l] & 0xF) + ((qh[l] & u1) ? 16 : 0)) - m1;
                for (int l = 0; l < 32; l++) y[n+32+l] = d2 * ((ql[l] >> 4) + ((qh[l] & u2) ? 16 : 0)) - m2;
                ql += 32; is += 2; u1 <<= 2; u2 <<= 2;
            }
        }
    } else if (ty == 14) {                            /* Q6_K: {ql[128], qh[64], scales[16]:i8, d} @0,128,192,208 */
        for (uint64_t b = 0; b < nb; b++) {
            const uint8_t* blk = q + b*210;
            const uint8_t* ql = blk; const uint8_t* qh = blk+128; const int8_t* sc = (const int8_t*)(blk+192);
            uint16_t h; memcpy(&h, blk+208, 2); float d = llmmodelio_h2f(h);
            float* y = dq + b*256;
            for (int n = 0; n < 256; n += 128) {
                for (int l = 0; l < 32; l++) {
                    int is2 = l/16;
                    int8_t q1 = (int8_t)((ql[l+0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
                    int8_t q2 = (int8_t)((ql[l+32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
                    int8_t q3 = (int8_t)((ql[l+0] >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
                    int8_t q4 = (int8_t)((ql[l+32] >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
                    y[l] = d*sc[is2+0]*q1; y[l+32] = d*sc[is2+2]*q2; y[l+64] = d*sc[is2+4]*q3; y[l+96] = d*sc[is2+6]*q4;
                }
                y += 128; ql += 64; qh += 32; sc += 8;
            }
        }
    }
    free(q);
    return 0;
}

double llmmodelio_roundtrip_rel(int ty, const float* x, uint64_t numel, uint32_t cols, const float* imp) {
    float* dq = (float*)malloc((size_t)numel * 4);
    if (!dq) return -1.0;
    if (llmmodelio_quant_dequant(ty, x, numel, dq) != 0) { free(dq); return -1.0; }
    double err = 0, sig = 0;
    for (uint64_t i = 0; i < numel; i++) { double w = imp ? (double)imp[i % cols] : 1.0; double d = (double)x[i] - dq[i]; err += w*d*d; sig += w*(double)x[i]*x[i]; }
    free(dq);
    return sig > 0 ? sqrt(err / sig) : 0.0;
}

/* ---- Variante "covarianza" di llmmodelio_roundtrip_rel (§ diagnosi 2026-09-22): invece di pesare l'errore
 * per-canale con la sola varianza (approssimazione diagonale di E[(ΔW·x)²]), stima la forma quadratica
 * COMPLETA ΔWᵀΣΔW via Monte Carlo su campioni REALI di attivazione catturati durante il forward pass
 * (llmmodelio_infer.c, "<tensor>.samples" nell'imatrix) — cattura le correlazioni fra canali che la
 * diagonale ignora, senza mai materializzare la matrice cols×cols (intrattabile per i tensori esperti:
 * vedi AiWiki). samp: [n_samp*cols] (tensori condivisi) o [n_expert*n_samp*cols] (tensori per-esperto,
 * rows_per_expert>0 -> riga o appartiene all'esperto o/rows_per_expert). Nessun campione -> ricade sul
 * diagonale (llmmodelio_roundtrip_rel), esattamente come prima di questa estensione. */
double llmmodelio_roundtrip_rel_cov(int ty, const float* x, uint64_t numel, uint32_t cols,
                             const float* samp, int64_t n_samp, int64_t rows_per_expert) {
    if (!samp || n_samp <= 0 || cols == 0 || numel % cols != 0) return llmmodelio_roundtrip_rel(ty, x, numel, cols, NULL);
    float* dq = (float*)malloc((size_t)numel * 4);
    if (!dq) return -1.0;
    if (llmmodelio_quant_dequant(ty, x, numel, dq) != 0) { free(dq); return -1.0; }
    uint64_t rows = numel / cols;
    double err = 0, sig = 0;
    for (uint64_t o = 0; o < rows; o++) {
        const float* xr = x + (size_t)o * cols;
        const float* dr = dq + (size_t)o * cols;
        const float* S = samp;
        if (rows_per_expert > 0) {
            uint64_t e = o / (uint64_t)rows_per_expert;
            S = samp + (size_t)e * (size_t)n_samp * cols;
        }
        for (int64_t s = 0; s < n_samp; s++) {
            const float* xs = S + (size_t)s * cols;
            double de = 0, ds = 0;
            for (uint32_t j = 0; j < cols; j++) {
                double diff = (double)xr[j] - (double)dr[j];
                de += diff * (double)xs[j];
                ds += (double)xr[j] * (double)xs[j];
            }
            err += de * de; sig += ds * ds;
        }
    }
    free(dq);
    return sig > 0 ? sqrt(err / sig) : 0.0;
}

/* ---- ARCH: superficie runtime del .llmarch (descrizione d'architettura data-driven) ----
 * Risolve la descrizione del modello (dal config) e l'espone: ruolo di un tensore, formato per ruolo,
 * show/list/check. E' quello che il converter e le funzioni BASIC (LLM.ARCH/LLM.ROLE, LLM ARCH ...)
 * usano — lo script BASIC non vede mai il file. */
#include "llmdesc.h"
#include "llmengine_plugin.h"
#include <dirent.h>
#include <stdlib.h>
static LlmDesc g_arch; static int g_arch_ok = 0; static char g_arch_path[4096] = "";
static const char* llmo_role_of(const char* nm);
/* Ruolo CATEGORIA modello-aware: usa l'arch; se IGNOTA (nessun .gmbarch, es. GGUF "deepseek4")
 * cade sul ruolo per NOME (llmo_role_of) -> i report non mostrano tutti "OTHER". */
static const char* llmmo_role_cat(const char* name) {
    const char* r = g_arch_ok ? llmdesc_role(&g_arch, name) : "OTHER";
    if (strcmp(r, "OTHER") != 0) return r;
    const char* n = llmo_role_of(name);
    if (!n) return "OTHER";
    if (strncmp(n, "experts", 7) == 0) return "ROUTED_EXPERT";
    if (strncmp(n, "shared", 6) == 0) return "SHARED_EXPERT";
    if (strncmp(n, "attn", 4) == 0) return "ATTENTION";
    if (strcmp(n, "ffn.router") == 0) return "ROUTER";
    if (strcmp(n, "norm") == 0) return "NORM";
    if (strcmp(n, "embed") == 0) return "EMBEDDING";
    if (strcmp(n, "output") == 0) return "OUTPUT";
    if (strncmp(n, "ffn", 3) == 0) return "FFN";
    return "OTHER";
}

int llmmodelio_arch_resolve(const char* hf_dir) {
    if (llmdesc_resolve(hf_dir, &g_arch, g_arch_path, sizeof g_arch_path) != 0) { g_arch_ok = 0; return -1; }
    g_arch_ok = 1; return 0;
}
const char* llmmodelio_arch_name(void) { return g_arch_ok ? g_arch.arch : ""; }
static const char* llmmodelio_arch_recipe(void) { return g_arch_ok ? g_arch.recipe : ""; }
const char* llmmodelio_arch_path(void) { return g_arch_path; }
/* Applica le regole: se g_arch non e' risolta, ritorna "OTHER". */
const char* llmmodelio_arch_role_of(const char* tensor) { return g_arch_ok ? llmdesc_role(&g_arch, tensor) : "OTHER"; }
const char* llmmodelio_arch_fmt_of(const char* role) { return g_arch_ok ? llmdesc_fmt(&g_arch, role) : NULL; }

void llmmodelio_arch_show(void) {
    if (!g_arch_ok) { fprintf(LOUT, "LLM ARCH: nessuna descrizione risolta (usa LLM ARCH RESOLVE <dir>)\n"); return; }
    fprintf(LOUT, "LLM ARCH '%s' (%s)\n", g_arch.arch, g_arch_path);
    fprintf(LOUT, "  ruoli (%d):\n", g_arch.n_roles);
    for (int i = 0; i < g_arch.n_roles; i++)
        fprintf(LOUT, "    %-22s -> %s%s\n", g_arch.roles[i].match, g_arch.roles[i].role,
               g_arch.roles[i].submodel[0] ? " (submodel)" : "");
    fprintf(LOUT, "  quant (%d):", g_arch.n_quant);
    for (int i = 0; i < g_arch.n_quant; i++) fprintf(LOUT, " %s=%s", g_arch.quant[i].role, g_arch.quant[i].fmt);
    fprintf(LOUT, "\n  recipe=%s boost_fmt=%s scale_suffix=%s\n", g_arch.recipe, g_arch.boost_fmt, g_arch.scale_suffix);
    fprintf(LOUT, "  modality=%s source.format=%s mmproj=%s\n", llmdesc_modality(&g_arch),
            g_arch.source_format[0] ? g_arch.source_format : "-", llmdesc_source_mmproj(&g_arch));
}
/* Valida un modello: ogni tensore dell'index ha un ruolo noto (non OTHER)? Ritorna il n. di OTHER.
 * DETERMINISTICO sull'argomento: `CHECK <hf_dir>` risolve SEMPRE la descrizione di QUELLA dir
 * (non riusa uno stato risolto da un'altra chiamata). Senza, in un processo che esegue più
 * comandi (IL/PCodeVM nel runner) il nome/ruoli stampati dipendevano dal `g_arch` precedente
 * (divergenza vs VM/C, processo fresco: `ARCH CHECK tiny_arch` stampava 'qwen3_moe'). */
int llmmodelio_arch_check(const char* hf_dir) {
    if (hf_dir && hf_dir[0]) {
        if (llmmodelio_arch_resolve(hf_dir) != 0) return -1;   /* errore = codice di ritorno; niente diagnostica in LOUT/stderr (inquinava l'output del comando) */
    } else if (!g_arch_ok) { fprintf(LOUT, "LLM ARCH CHECK: nessuna descrizione risolta (usa LLM ARCH RESOLVE <dir>)\n"); return -1; }
    char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", hf_dir);
    FILE* f = fopen(idx, "rb");
    int other = 0, tot = 0;
    if (f) {
        fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
        char* j=(char*)malloc((size_t)n+1); if (j && fread(j,1,(size_t)n,f)==(size_t)n) { j[n]=0;
            char* p = j; const char* KEYS[2] = { "\"layers.", "\"model." };
            for (int ki = 0; ki < 2; ki++) { p = j;
            while ((p = strstr(p, KEYS[ki])) != NULL) {
                char* q = strchr(p+1,'"'); if (!q) break;
                char nm[256]; size_t L=(size_t)(q-p-1); if(L>255)L=255; memcpy(nm,p+1,L); nm[L]=0;
                /* SOLO le CHIAVI. Il pattern `"model.` combacia anche con i VALORI che iniziano
                 * per "model." (es. `"model.safetensors"`): senza questo controllo ogni valore
                 * veniva contato come tensore e finiva in `OTHER` (misurato 2026-10-29 su
                 * Fixtures/models/tiny_hf: 18 tensori di cui 9 "senza ruolo", mentre i tensori
                 * veri sono 9 e hanno tutti un ruolo). Una chiave è seguita da `:` (dopo spazi). */
                const char* c2 = q + 1;
                while (*c2 == ' ' || *c2 == '\t' || *c2 == '\n' || *c2 == '\r') c2++;
                if (*c2 != ':') { p = q + 1; continue; }
                tot++; if (strcmp(llmdesc_role(&g_arch, nm), "OTHER") == 0) other++;
                p = q+1;
            } }
            free(j);
        }
        fclose(f);
    }
    fprintf(LOUT, "LLM ARCH CHECK '%s': %d tensori, %d senza ruolo (OTHER)\n", g_arch.arch, tot, other);
    return other;
}

/* Elenca le descrizioni .llmarch disponibili (LLMDESC_ARCH_DIR o ./arch). */
static void arch_list_dir(const char* d) {
    fprintf(LOUT, "LLM ARCH: descrizioni in '%s':\n", d);
    DIR* dp = opendir(d);
    if (!dp) { fprintf(LOUT, "  (directory non trovata)\n"); return; }
    struct dirent* e; int cnt = 0;
    while ((e = readdir(dp)) != NULL) {
        const char* n = e->d_name; size_t L = strlen(n);
        if (L > 8 && strcmp(n + L - 8, ".llmarch") == 0) {
            char nm[256]; snprintf(nm, sizeof nm, "%.*s", (int)(L - 8), n);
            fprintf(LOUT, "  %s\n", nm); cnt++;
        }
    }
    closedir(dp);
    if (!cnt) fprintf(LOUT, "  (nessuna)\n");
}

void llmmodelio_arch_list(void) {
    const char* dir = llmmodelio_model_policy_get("ARCH_DIR"); const char* d = (dir && *dir) ? dir : "arch";
    arch_list_dir(d);
    /* ABI/ canonica del repo (risalendo dal cwd): dove vivono TUTTE le descrizioni locali. */
    char abi[4096];
    if (llmdesc_abi_dir(abi, sizeof abi) && strcmp(abi, d) != 0) arch_list_dir(abi);
}

/* Esegue un comando LLM ARCH a runtime: sub = RESOLVE|SHOW|LIST|CHECK, path = dir modello (puo' essere "").
 * Unico punto d'ingresso per TUTTI i backend (C99Native, VM/C): cosi' nessuno
 * reimplementa il dispatch del sotto-verbo. Ritorna 0 ok, -1 errore. */
int llmmodelio_arch_cmd(const char* sub, const char* a, const char* b)
{
    const char* s = sub ? sub : "";
    const char* p = a ? a : "";
    const char* o = b ? b : "";
    if (strcasecmp(s, "RESOLVE") == 0) { if (!*p) return -1; return llmmodelio_arch_resolve(p); }
    if (strcasecmp(s, "SHOW") == 0) { llmmodelio_arch_show(); return 0; }
    if (strcasecmp(s, "LIST") == 0) { llmmodelio_arch_list(); return 0; }
    if (strcasecmp(s, "CHECK") == 0) { if (!*p) return -1; return llmmodelio_arch_check(p) < 0 ? -1 : 0; }
    if (strcasecmp(s, "PLAN") == 0) { if (!*p || !*o) return -1; return llmmodelio_arch_plan(p, o); }
    fprintf(stderr, "LLM ARCH: sotto-verbo '%s' non supportato (RESOLVE|SHOW|LIST|CHECK|PLAN)\n", s);
    return -1;
}

/* Esegue un comando LLM POLICY a runtime: sub = SET|SHOW|RESET|CLEAR, a=ruolo, b=formato.
 * Unico punto d'ingresso per tutti i backend (come llmmodelio_arch_cmd). */
/* ---- OVERRIDE PER-TENSORE (Livello B) — superficie usata dai verbi SETITEM/CLEARITEM ---- */
int llmmodelio_policy_set_item(const char* name_glob, const char* fmt) {
    if (name_glob == NULL || !*name_glob || fmt == NULL || !*fmt) return -1;
    for (int i = 0; i < s_item_n; i++)
        if (strcmp(s_item_glob[i], name_glob) == 0) { snprintf(s_item_fmt[i], sizeof s_item_fmt[i], "%s", fmt); return 0; }
    if (s_item_n >= POL_ITEM_MAX) return -1;
    snprintf(s_item_glob[s_item_n], sizeof s_item_glob[s_item_n], "%s", name_glob);
    snprintf(s_item_fmt[s_item_n],  sizeof s_item_fmt[s_item_n],  "%s", fmt);
    s_item_n++; return 0;
}
void llmmodelio_policy_clear_items(void) { s_item_n = 0; }
/* APPLICA un CALLBACK (Livello B) sul PIANO: per ogni item chiama `decide(name,role,dflt)->fmt`
 * (function pointer) e registra l'override per-tensore. Il PROGRAMMA compilato passa la SUB; il
 * runtime NON chiama BASIC (nessun C->C#): nei backend C il puntatore e' la SUB compilata in C,
 * nei backend gestiti (IL/PCode) e' un delegate passato dal codice emesso. Ritorna il numero di
 * override applicati (-1 se il piano/argomento mancano). */
int llmmodelio_policy_callback_apply(int (*decide)(const char* name, const char* role,
                                                   const char* dflt, char* out, size_t cap)) {
    if (decide == NULL) return -1;
    int applied = 0;
    for (int i = 0; i < s_plan_n; i++) {
        char out[32]; out[0] = 0;
        if (decide(s_plan[i].name, s_plan[i].role, s_plan[i].dflt, out, sizeof out) == 0 && out[0])
            { llmmodelio_policy_set_item(s_plan[i].name, out); applied++; }
    }
    return applied;
}
int llmmodelio_policy_item_count(void) { return s_plan_n; }   /* voci del PIANO (non override) */

int llmmodelio_policy_cmd(const char* sub, const char* a, const char* b, const char* c)
{
    const char* s = sub ? sub : "";
    if (strcasecmp(s, "SET") == 0) {
        if (!a || !b || !*a || !*b) return -1;
        llmmodelio_model_policy_set(a, b); return 0;
    }
    if (strcasecmp(s, "RESET") == 0 || strcasecmp(s, "CLEAR") == 0) { llmmodelio_model_policy_reset(); return 0; }
    /* Livello B: override PER-TENSORE (il codice BASIC/GPM decide name->fmt). */
    if (strcasecmp(s, "SETITEM") == 0) { if (llmmodelio_policy_set_item(a, b) != 0) { fprintf(stderr, "LLM POLICY SETITEM: attesi (name-glob, fmt)\n"); return -1; } return 0; }
    if (strcasecmp(s, "CLEARITEM") == 0 || strcasecmp(s, "CLEARITEMS") == 0) { llmmodelio_policy_clear_items(); return 0; }
    if (strcasecmp(s, "SHOW") == 0 || !*s) { llmmodelio_model_policy_show(); return 0; }
    /* §4.3: policy serializzabile `.gmbpolicy` (role->fmt) + DRYRUN (report, nessuna scrittura). */
    if (strcasecmp(s, "SAVE") == 0 || strcasecmp(s, "EXPORT") == 0) {
        if (!a || !*a) { fprintf(stderr, "LLM POLICY SAVE: atteso <file.gmbpolicy>\n"); return -1; }
        FILE* f = fopen(a, "w"); if (f == NULL) { fprintf(stderr, "LLM POLICY SAVE: non scrivibile '%s'\n", a); return -1; }
        fprintf(f, "{\n  \"artifact\": \"gmbpolicy\",\n  \"version\": 1,\n  \"rules\": [\n");
        for (int i = 0; i < s_pol_n; i++)
            fprintf(f, "%s    { \"role\": \"%s\", \"fmt\": \"%s\" }", i ? ",\n" : "", s_pol_role[i], s_pol_fmt[i]);
        /* Policy NOMINATE (DSL Livello A): name + regole ricostruite (cond:act;...;DEFAULT:act). */
        fprintf(f, "\n  ],\n  \"policies\": [\n");
        for (int i = 0; i < s_pols_n; i++) {
            char rl[4096]; size_t ro = 0; rl[0] = 0;
            for (int k = 0; k < s_pols[i].n; k++)
                ro += (size_t)snprintf(rl + ro, sizeof rl - ro, "%s%s:%s", k ? ";" : "", s_pols[i].rules[k].cond, s_pols[i].rules[k].act);
            if (s_pols[i].has_def) snprintf(rl + ro, sizeof rl - ro, "%sDEFAULT:%s", ro ? ";" : "", s_pols[i].def_act);
            fprintf(f, "%s    { \"name\": \"%s\", \"rules\": \"%s\" }", i ? ",\n" : "", s_pols[i].name, rl);
        }
        fprintf(f, "\n  ]\n}\n"); fclose(f);
        fprintf(LOUT, "LLM POLICY SAVE '%s': %d rule-coppie + %d policy DSL\n", a, s_pol_n, s_pols_n); return 0;
    }
    if (strcasecmp(s, "LOAD") == 0 || strcasecmp(s, "IMPORT") == 0) {
        if (!a || !*a) { fprintf(stderr, "LLM POLICY LOAD: atteso <file.gmbpolicy>\n"); return -1; }
        FILE* f = fopen(a, "rb"); if (f == NULL) { fprintf(stderr, "LLM POLICY LOAD: file assente '%s'\n", a); return -1; }
        fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
        if (n <= 0 || n > (1<<20)) { fclose(f); return -1; }
        char* j=(char*)malloc((size_t)n+1); if(!j||fread(j,1,(size_t)n,f)!=(size_t)n){free(j);fclose(f);return -1;} j[n]=0; fclose(f);
        if (strstr(j, "\"gmbpolicy\"") == NULL) { free(j); fprintf(stderr,"LLM POLICY LOAD: non e' un artefatto gmbpolicy\n"); return -1; }
        llmmodelio_model_policy_reset();
        int loaded = 0;
        for (const char* p = j; (p = strstr(p, "\"role\"")) != NULL; p++) {
            char role[24]="", fmt[512]="";
            const char* r = strchr(p, ':'); if (r) { const char* q=strchr(r,'"'); if (q) { q++; size_t i=0; while (*q && *q!='"' && i<23) role[i++]=*q++; role[i]=0; } }
            const char* fp = strstr(p, "\"fmt\""); if (fp) { const char* c=strchr(fp,':'); if (c) { const char* q=strchr(c,'"'); if (q) { q++; size_t i=0; while (*q && *q!='"' && i<511) fmt[i++]=*q++; fmt[i]=0; } } }
            if (*role && *fmt) { llmmodelio_model_policy_set(role, fmt); loaded++; }
        }
        /* Policy NOMINATE (DSL): {"name":"...","rules":"cond:act;...;DEFAULT:act"} */
        int dsl = 0;
        for (const char* p = j; (p = strstr(p, "\"name\"")) != NULL; p++) {
            char nm[48] = "", rl[4096] = "";
            const char* q = strchr(p, ':'); if (q) { const char* c = strchr(q, '"'); if (c) { c++; size_t i=0; while (*c && *c != '"' && i < sizeof nm - 1) nm[i++] = *c++; nm[i]=0; } }
            const char* rp = strstr(p, "\"rules\""); if (rp) { const char* c = strchr(rp, ':'); if (c) { c = strchr(c, '"'); if (c) { c++; size_t i=0; while (*c && *c != '"' && i < sizeof rl - 1) rl[i++] = *c++; rl[i]=0; } } }
            if (*nm && *rl && pol_define(nm, rl) == 0) dsl++;
        }
        free(j);
        fprintf(LOUT, "LLM POLICY LOAD '%s': %d rule-coppie + %d policy DSL\n", a, loaded, dsl); return 0;
    }
    if (strcasecmp(s, "DRYRUN") == 0) {
        fprintf(LOUT, "LLM POLICY DRYRUN: %d regole attive%s\n", s_pol_n, s_pol_n ? "" : " (nessuna -> default)");
        for (int i = 0; i < s_pol_n; i++) fprintf(LOUT, "  WHEN role=%s THEN fmt=%s\n", s_pol_role[i], s_pol_fmt[i]);
        return 0;
    }
    /* Livello A (§4.3): policy NOMINATA a regole WHEN/THEN. DEFINE parsa il testo compatto;
     * APPLY la rende attiva (l'ultima vince) o, con DRYRUN, ne mostra le regole senza attivarla. */
    if (strcasecmp(s, "DEFINE") == 0) {
        if (!a || !b) { fprintf(stderr, "LLM POLICY DEFINE: attesi (nome, regole)\n"); return -1; }
        if (pol_define(a, b) != 0) { fprintf(stderr, "LLM POLICY DEFINE: regole non valide\n"); return -1; }
        const pol_def_t* d = pol_find(a);
        fprintf(LOUT, "LLM POLICY DEFINE '%s': %d regole%s\n", a, d ? d->n : 0, (d && d->has_def) ? " (+DEFAULT)" : "");
        return 0;
    }
    if (strcasecmp(s, "APPLY") == 0) {
        if (!a || !*a) { fprintf(stderr, "LLM POLICY APPLY: atteso <nome>\n"); return -1; }
        pol_def_t* d = pol_find(a);
        if (!d) { fprintf(stderr, "LLM POLICY APPLY: policy '%s' non definita\n", a); return -1; }
        if (b && strcasecmp(b, "DRYRUN") == 0) {
            fprintf(LOUT, "LLM POLICY APPLY '%s' DRYRUN: %d regole, default=%s\n", a, d->n, d->has_def ? d->def_act : "(nessuno)");
            for (int i = 0; i < d->n; i++) fprintf(LOUT, "  WHEN %s THEN %s\n", d->rules[i].cond, d->rules[i].act);
            return 0;
        }
        s_pol_active = (int)(d - s_pols);
        fprintf(LOUT, "LLM POLICY APPLY '%s': ATTIVA (%d regole%s)\n", a, d->n, d->has_def ? " +DEFAULT" : "");
        return 0;
    }
    /* DIFF/PLAN <modello>: confronto della policy ATTIVA col default, per-tensore (report-only). */
    if (strcasecmp(s, "DIFF") == 0 || strcasecmp(s, "PLAN") == 0) {
        if (!a || !*a) { fprintf(stderr, "LLM POLICY DIFF: atteso <modello>\n"); return -1; }
        static char buf[65536];
        int ch = llmmodelio_policy_plan_to(a, buf, sizeof buf);
        fputs(buf, LOUT);
        return ch < 0 ? -1 : 0;
    }
    /* ITEMS: il piano memorizzato (JSON). ITEM <i>: un item del piano. */
    if (strcasecmp(s, "ITEMS") == 0) {
        static char buf[256000];
        int n = llmmodelio_policy_items_to(buf, sizeof buf);
        fputs(buf, LOUT);
        return n < 0 ? -1 : 0;
    }
    if (strcasecmp(s, "ITEM") == 0) {
        char buf[256];
        int rc = llmmodelio_policy_item_to(a && *a ? atoi(a) : -1, buf, sizeof buf);
        fputs(buf, LOUT);
        return rc;
    }
    /* GATE <base>[, <ref>]: quantizza base-><base>.out.gguf con la policy attiva, misura col gate
     * (ppl ref vs out) e, se rifiutato, fa ROLLBACK (rimuove l'output). */
    if (strcasecmp(s, "GATE") == 0) {
        if (!a || !*a) { fprintf(stderr, "LLM POLICY GATE: atteso <base>[, <ref>]\n"); return -1; }
        char gout[1200]; snprintf(gout, sizeof gout, "%s.out.gguf", a);
        static char rep[1200];
        llmmodelio_policy_gate_to(a, b, gout, rep, sizeof rep);
        fputs(rep, LOUT);
        if (rep[0] && rep[strlen(rep) - 1] != '\n') fputc('\n', LOUT);
        return 0;
    }
    /* RECOVER <model>, <dataset>, <ref>: composizione gate->retrain->re-eval->ACCEPT/ROLLBACK (§9.B).
     * Se il gate rifiuta `model` vs `ref`, fine-tune LoRA su `dataset`, fonde e ri-misura; ACCEPT
     * tiene il modello recuperato, altrimenti rollback. Report JSON su LOUT. */
    if (strcasecmp(s, "RECOVER") == 0) {
        /* Argomenti mancanti -> JSON deterministico (parità 4 backend): NON un'eccezione
         * backend-specifica. */
        static char rep[2048];
        if (!a || !*a || !b || !*b || !c || !*c)
            snprintf(rep, sizeof rep, "{\"verdict\":\"ERROR\",\"reason\":\"attesi <model>, <dataset>, <ref>\"}");
        else
            llmmodelio_model_recover(a, b, c, rep, sizeof rep);
        fputs(rep, LOUT);
        if (rep[0] && rep[strlen(rep) - 1] != '\n') fputc('\n', LOUT);
        return 0;
    }
    fprintf(stderr, "LLM POLICY: sotto-verbo '%s' non supportato (SET|SHOW|RESET|SAVE|LOAD|DRYRUN|DEFINE|APPLY|DIFF|ITEMS|ITEM|GATE|RECOVER)\n", s);
    return -1;
}

/* ---- CONSUMER del .llmarch: piano guidato dalla DESCRIZIONE --------------------------------
 * Per ogni tensore dell'index: ruolo + formato proposto (dal `quant` della descrizione) +
 * accoppiamento con la sua `.scale`. NON legge i pesi (solo i nomi): è il consumer verificabile
 * senza I/O, ed è la base per la mappatura verso il container (data-driven, non per-arch). */
/* Sezione del container di destinazione per RUOLO: il modello testo, il vision tower, l'aligner.
 * (Il .llmarch può raffinare via `submodel`; qui la mappa role->sezione è la base del container.) */
static const char* llmmodelio_arch_dest(const char* role)
{
    if (role == NULL) return "text";
    if (strcmp(role, "VISION") == 0) return "vision";
    if (strcmp(role, "VISION_ALIGNER") == 0) return "aligner";
    if (strncmp(role, "AUDIO", 5) == 0) return "audio";
    if (strcmp(role, "MTP") == 0) return "mtp";
    if (strcmp(role, "ENGRAM") == 0) return "engram";
    return "text";
}

int llmmodelio_arch_plan(const char* hf_dir, const char* out_path)
{
    if (!hf_dir || !out_path) return -1;
    if (llmmodelio_arch_resolve(hf_dir) != 0) return -1;
    char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", hf_dir);
    FILE* f = fopen(idx, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* j = (char*)malloc((size_t)n + 1);
    if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; }
    j[n] = 0; fclose(f);
    char* wm = strstr(j, "\"weight_map\"");
    if (!wm) { free(j); return -1; }
    FILE* o = fopen(out_path, "w");
    if (!o) { free(j); return -1; }
    fprintf(o, "# LLM ARCH PLAN — arch=%s (ruolo/formato/scale/destinazione per tensore)\n# name\trole\tfmt\tscale\tdst\n", g_arch.arch);
    struct { const char* role; int num; } cnt[96]; int nc = 0; int total = 0, scales = 0;
    const char* ss = g_arch.scale_suffix[0] ? g_arch.scale_suffix : ".scale";
    size_t ssl = strlen(ss);
    char* colon = strchr(wm, ':');           /* parti DOPO "weight_map": la chiave stessa non è un tensore */
    char* p = colon ? colon + 1 : wm;
    while ((p = strchr(p, '"')) != NULL) {
        char* q = strchr(p + 1, '"');
        if (!q) break;
        if (q[1] != ':') { p = q + 1; continue; }          /* è un valore (nome file), non una chiave */
        size_t L = (size_t)(q - p - 1);
        char nm[512]; if (L > 511) L = 511; memcpy(nm, p + 1, L); nm[L] = 0;
        int is_scale = 0;
        if (ssl && strlen(nm) > ssl && strcmp(nm + strlen(nm) - ssl, ss) == 0) { is_scale = 1; nm[strlen(nm) - ssl] = 0; }
        const char* role = llmdesc_role(&g_arch, nm);
        const char* fmt = llmdesc_fmt(&g_arch, role);
        fprintf(o, "%s\t%s\t%s\t%s\t%s\n", nm, role, fmt ? fmt : "-", is_scale ? "scale" : "", llmmodelio_arch_dest(role));
        total++; if (is_scale) scales++;
        int found = 0;
        for (int i = 0; i < nc; i++) if (strcmp(cnt[i].role, role) == 0) { cnt[i].num++; found = 1; break; }
        if (!found && nc < 96) { cnt[nc].role = role; cnt[nc].num = 1; nc++; }
        p = q + 1;
    }
    fclose(o); free(j);
    fprintf(LOUT, "LLM ARCH PLAN '%s': %d tensori (%d scale) -> %s\n", g_arch.arch, total, scales, out_path);
    for (int i = 0; i < nc; i++) fprintf(LOUT, "  %-18s %d\n", cnt[i].role, cnt[i].num);
    /* destinazioni nel container (sezioni) */
    { struct { const char* d; int n; } dc[4]; int nd = 0; (void)dc;
      fprintf(LOUT, "  destinazioni:");
      const char* ds[3] = { "text", "vision", "aligner" };
      for (int k = 0; k < 3; k++) { int n = 0; for (int i = 0; i < nc; i++) if (strcmp(llmmodelio_arch_dest(cnt[i].role), ds[k]) == 0) n += cnt[i].num; if (n) fprintf(LOUT, " %s=%d", ds[k], n); }
      fprintf(LOUT, "\n"); (void)nd; }
    return 0;
}


/* ---- varianti "_to": eseguono il comando e CATTURANO il testo in `out` (per IL/PCodeVM) ---- */
int llmmodelio_arch_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap)
{
    if (!out || cap == 0) return -1;
    out[0] = 0;
    FILE* f = fmemopen(out, cap - 1, "w");
    if (!f) return llmmodelio_arch_cmd(sub, a, b);
    g_llm_out = f;
    int rc = llmmodelio_arch_cmd(sub, a, b);
    fflush(f); fclose(f); g_llm_out = NULL;
    out[cap - 1] = 0;
    return rc;
}
int llmmodelio_policy_cmd_to(const char* sub, const char* a, const char* b, const char* c, char* out, size_t cap)
{
    if (!out || cap == 0) return -1;
    out[0] = 0;
    FILE* f = fmemopen(out, cap - 1, "w");
    if (!f) return llmmodelio_policy_cmd(sub, a, b, c);
    g_llm_out = f;
    int rc = llmmodelio_policy_cmd(sub, a, b, c);
    fflush(f); fclose(f); g_llm_out = NULL;
    out[cap - 1] = 0;
    return rc;
}

/* Verifica quali STRUTTURE dichiarate dalla .llmarch il CONVERTER sa mappare verso il container.
 * Scrive in `out` un report (supported / NOT supported) e ritorna il numero di NON supportate.
 * Onesto per costruzione (§0bis): le strutture non ancora gestite (compression/indexer/engram/
 * vision) sono dichiarate esplicitamente, mai ignorate in silenzio. */
int llmmodelio_arch_support(const char* hf_dir, char* out, size_t cap)
{
    if (llmmodelio_arch_resolve(hf_dir) != 0) { snprintf(out, cap, "LLM ARCH SUPPORT: descrizione non risolta\n"); return -1; }
    /* strutture che il converter gestisce oggi (mappatura tensori -> container);
     * le altre sono dichiarate NON supportate (gap visibile). */
    /* Stesse strutture del gate `llmdesc_unsupported` (llmdesc.c): il converter (ruoli .llmarch +
     * ricetta) mappa i TENSORI di tutte queste; il compute resta dell'engine. */
    static const struct { const char* key; const char* type; int ok; } known[] = {
        { "attention",   "mla",                 1 },
        { "attention",   "gqa",                 1 },
        { "moe",         NULL,                  1 },
        { "norm",        NULL,                  1 },
        { "compression", "csa",                 1 },
        { "compression", "csa_hca",             1 },
        { "indexer",     "indexer",             1 },
        { "indexer",     "topk_selector",       1 },
        { "engram",      "engram",              1 },
        { "engram",      "deterministic_router",1 },
        { "vision",      "vit",                 1 },
    };
    int nbad = 0; size_t o = 0;
    o += (size_t)snprintf(out+o, cap-o, "LLM ARCH SUPPORT '%s':\n", g_arch.arch);
    for (int i = 0; i < g_arch.n_struct && o < cap; i++) {
        const char* key = g_arch.structures[i].key;
        const char* type = g_arch.structures[i].type;
        int supported = 0;
        for (size_t k = 0; k < sizeof(known)/sizeof(known[0]); k++)
            if (strcmp(known[k].key, key) == 0 && (known[k].type == NULL || strcmp(known[k].type, type) == 0))
                { supported = known[k].ok; break; }
        if (!supported) nbad++;
        o += (size_t)snprintf(out+o, cap-o, "  %-12s %-22s %s\n", key, type, supported ? "supportata" : "NON supportata");
    }
    if (o < cap) snprintf(out+o, cap-o, "  -> %d struttura/e non supportata/e\n", nbad);
    return nbad;
}


/* Risolve il CODEC di un formato: PRIMA un PLUGIN (dichiarato dal .llmarch in `codecs`, oppure per
 * convenzione `libllmcodec_<fmt>`), POI il built-in (i formati nativi del runtime/GGUF).
 * Ritorna 0 se un PLUGIN fornisce `fmt` (e scrive il suo LlmCodec in `out` + il percorso in
 * `plugin_path`); -1 se il chiamante deve usare il BUILT-IN (nessun plugin). Cosi' un formato
 * nuovo = una dylib, senza ricompilare il core. */
int llmmodelio_codec_resolve(const char* hf_dir, const char* fmt, LlmCodec* out, char* plugin_path, size_t cap)
{
    if (fmt == NULL || *fmt == 0 || out == NULL) return -1;
    if (plugin_path && cap) plugin_path[0] = 0;
    char pname[64] = "";
    LlmDesc arch;
    if (hf_dir && llmdesc_resolve(hf_dir, &arch, NULL, 0) == 0) {
        const char* pl = llmdesc_codec_plugin(&arch, fmt);
        if (pl && *pl) snprintf(pname, sizeof pname, "%s", pl);
    }
    const char* cands[2] = { pname[0] ? pname : NULL, fmt };
    for (int i = 0; i < 2; i++) {
        if (cands[i] == NULL) continue;
        const LlmCodec* c = llmengine_plugin_codec_get(cands[i]);
        if (c != NULL) {
            *out = *c;
            if (plugin_path && cap) { const char* pp = llmengine_plugin_path("codec", cands[i]); snprintf(plugin_path, cap, "%s", pp ? pp : ""); }
            return 0;
        }
    }
    return -1;   /* built-in */
}


/* Risolve la PRIMITIVA dichiarata per una struttura (kind) nel .llmarch: il campo `prim` della
 * struttura -> il plugin (llmengine_plugin_prim_get). Ritorna 0 e il descrittore in `out` (+ `plugin_path`);
 * -1 se non dichiarata o plugin assente (il chiamante usa la primitiva built-in, se ne ha una). */
int llmmodelio_prim_resolve(const char* hf_dir, const char* kind, LlmEnginePluginPrim* out, char* plugin_path, size_t cap)
{
    if (hf_dir == NULL || kind == NULL || out == NULL) return -1;
    if (plugin_path && cap) plugin_path[0] = 0;
    LlmDesc arch;
    if (llmdesc_resolve(hf_dir, &arch, NULL, 0) != 0) return -1;
    const char* pname = llmdesc_struct_prim(&arch, kind);
    if (pname == NULL || *pname == 0) return -1;
    const LlmEnginePluginPrim* pr = llmengine_plugin_prim_get(pname);
    if (pr == NULL) return -1;
    *out = *pr;
    if (plugin_path && cap) { const char* pp = llmengine_plugin_path("prim", pname); snprintf(plugin_path, cap, "%s", pp ? pp : ""); }
    return 0;
}


/* Risolve la PIPELINE (composizione, livello 3) di una struttura: per ogni step carica la primitiva
 * dichiarata (plugin MLX -> plugin arithmetica) e scrive il report in `out`. Ritorna il n. di step
 * risolti (-1 se la struttura non ha steps). Così una struttura nuova = una LISTA di primitive. */
int llmmodelio_arch_pipeline(const char* hf_dir, const char* kind, char* out, size_t cap)
{
    if (hf_dir == NULL || kind == NULL || out == NULL) return -1;
    LlmDesc arch;
    if (llmdesc_resolve(hf_dir, &arch, NULL, 0) != 0) return -1;
    const LlmDescStruct* S = llmdesc_struct(&arch, kind);
    if (S == NULL || S->n_steps == 0) return -1;
    size_t o = 0; int found = 0;
    o += (size_t)snprintf(out + o, cap - o, "pipeline '%s' (%s):", kind, S->type[0] ? S->type : "-");
    for (int i = 0; i < S->n_steps && o < cap; i++) {
        const char* pr = S->steps[i].prim;
        const char* src = "-";
        if (pr && *pr) {
            if (llmengine_plugin_prim_mlx_get(pr) != NULL) src = "mlx";
            else if (llmengine_plugin_prim_get(pr) != NULL) src = "prim";
            else if (llmengine_plugin_codec_get(pr) != NULL) src = "codec";
            else src = "ASSENTE";
            if (strcmp(src, "ASSENTE") != 0) found++;
        }
        o += (size_t)snprintf(out + o, cap - o, " %s->%s(%s)", S->steps[i].op, pr && *pr ? pr : "-", src);
    }
    if (o < cap) snprintf(out + o, cap - o, " [%d/%d risolti]", found, S->n_steps);
    return found;
}

/* ---- MODEL COMPILER LLM: verbi TENSOR/KERNEL/MODULE/KV (superficie BASIC) -------------------
 * Stessa forma `llmmodelio_<x>_cmd(sub, a, b)` di ARCH/POLICY: UN solo punto d'ingresso per tutti i
 * backend (IL/PCodeVM/VM-C/C99Native), nessuna reimplementazione per backend (§0bis).
 * Il report va su LOUT; le varianti `_to` lo catturano in un buffer (i backend gestiti via P/Invoke
 * non hanno lo stdout nativo del processo C). Descrizione d'architettura data-driven (.llmarch):
 * lo script BASIC non vede mai il file. */

/* TENSOR: sub=ROLE a=<dir> b=<tensor> -> ruolo+formato del tensore; sub=LIST a=<dir> -> tutti i
 * ruoli (match->ruolo + formato proposto). */
/* ── MODEL IR: tabella per-tensore del MODELLO (ruolo/geometria/tipo/byte) ──
 * GGUF: legge i tensor-info + ruolo (dall'arch del gguf). Dir MLX/HF: legge weight_map
 * (index.json) + ruolo/formato dall'arch risolta. Formato-agnostico (§4.2 Model IR). */
static int llmmodelio_arch_resolve_gguf(const char* gguf_path);

static int llmmodelio_model_ir_print(const char* path) {
    size_t ln = strlen(path);
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        fprintf(LOUT, "# LLM TENSOR IR '%s' GGUF arch=%s tensori=%llu\n# name\trole\ttype\tndim\tdims\tbytes\n",
                path, g_arch_ok ? g_arch.arch : "-", (unsigned long long)fi.n_tensors);
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            LlmCodecGgufTensorInfo* t = &fi.tensors[i];
            const char* role = llmmo_role_cat(t->name);
            char tn[24]; if (llmcodec_ggml_type_name(t->ggml_type, tn, sizeof tn) != 0) snprintf(tn, sizeof tn, "t%d", t->ggml_type);
            char ds[96]; size_t o = 0; ds[0] = 0;
            for (int d = 0; d < t->ndim; d++) o += (size_t)snprintf(ds + o, sizeof ds - o, "%s%llu", d ? "x" : "", (unsigned long long)t->dims[d]);
            uint64_t numel = 1; for (int d = 0; d < t->ndim; d++) numel *= t->dims[d];
            uint64_t b = 0; (b = llmcodec_gguf_tensor_bytes(t), 0);
            fprintf(LOUT, "%s\t%s\t%s\t%d\t%s\t%llu\n", t->name, role, tn, t->ndim, ds, (unsigned long long)b);
        }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
        return 0;
    }
    if (llmmodelio_arch_resolve(path) != 0) return -1;
    char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", path);
    FILE* f = fopen(idx, "rb"); if (f == NULL) { fprintf(LOUT, "# LLM TENSOR IR '%s' MLX arch=%s (index assente)\n", path, g_arch.arch); return 0; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* j = (char*)malloc((size_t)n + 1); if (j == NULL || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; }
    j[n] = 0; fclose(f);
    char* wm = strstr(j, "\"weight_map\""); if (wm == NULL) { free(j); return -1; }
    fprintf(LOUT, "# LLM TENSOR IR '%s' MLX arch=%s\n# name\trole\tfmt\n", path, g_arch.arch);
    char* colon = strchr(wm, ':'); char* p = colon ? colon + 1 : wm; int nt = 0;
    while ((p = strchr(p, '"')) != NULL) {
        char* q = strchr(p + 1, '"'); if (!q) break;
        if (q[1] != ':') { p = q + 1; continue; }
        size_t L = (size_t)(q - p - 1); char nm[512]; if (L > 511) L = 511; memcpy(nm, p + 1, L); nm[L] = 0;
        const char* role = llmdesc_role(&g_arch, nm); const char* fmt = llmdesc_fmt(&g_arch, role);
        fprintf(LOUT, "%s\t%s\t%s\n", nm, role, fmt ? fmt : "-"); nt++; p = q + 1;
    }
    free(j);
    return 0;
}
static int model_layer_of(const char* name) {
    const char* p = strstr(name, "blk.");    if (p) return atoi(p + 4);
    p = strstr(name, "layers.");             if (p) return atoi(p + 7);
    p = strstr(name, "mtp.");                if (p) return atoi(p + 4);
    return -1;
}
/* LAYERS: indici di layer presenti + n. tensori per layer (GGUF o dir MLX). */
static int llmmodelio_model_ir_layers(const char* path) {
    int cnt[1024]; memset(cnt, 0, sizeof cnt); int maxl = -1, glob = 0;
    size_t ln = strlen(path);
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) { int L = model_layer_of(fi.tensors[i].name); if (L < 0) { glob++; continue; } if (L < 1024) cnt[L]++; if (L > maxl) maxl = L; }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    } else {
        if (llmmodelio_arch_resolve(path) != 0) return -1;
        char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", path);
        FILE* f = fopen(idx, "rb"); if (f == NULL) return -1;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char* j = (char*)malloc((size_t)n + 1); if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; } j[n] = 0; fclose(f);
        char* wm = strstr(j, "\"weight_map\""); char* p = wm ? strchr(wm, ':') : NULL;
        while (p && (p = strchr(p, '"')) != NULL) { char* q = strchr(p + 1, '"'); if (!q) break; if (q[1] != ':') { p = q + 1; continue; }
            size_t L = (size_t)(q - p - 1); char nm[512]; if (L > 511) L = 511; memcpy(nm, p + 1, L); nm[L] = 0;
            int lyr = model_layer_of(nm); if (lyr < 0) glob++; else { if (lyr < 1024) cnt[lyr]++; if (lyr > maxl) maxl = lyr; } p = q + 1; }
        free(j);
    }
    fprintf(LOUT, "# LLM TENSOR LAYERS '%s' arch=%s layers=0..%d globals=%d\n", path, g_arch_ok ? g_arch.arch : "-", maxl, glob);
    for (int i = 0; i <= maxl && i < 1024; i++) if (cnt[i] > 0) fprintf(LOUT, "layer %d: %d tensori\n", i, cnt[i]);
    return 0;
}
/* CHECK (spec-vs-modello): ogni tensore ha un ruolo noto? Ritorna -1 se l'arch non e' risolta o
 * se restano tensori OTHER (errore esplicito). */
static int llmmodelio_model_ir_check(const char* path) {
    size_t ln = strlen(path); int tot = 0, other = 0;
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) return -1;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) { tot++; const char* r = llmdesc_role(&g_arch, fi.tensors[i].name); if (strcmp(r, "OTHER") == 0) { other++; if (other <= 8) fprintf(LOUT, "  OTHER: %s\n", fi.tensors[i].name); } }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    } else {
        if (llmmodelio_arch_resolve(path) != 0) return -1;
        char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", path);
        FILE* f = fopen(idx, "rb"); if (f == NULL) return -1;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char* j = (char*)malloc((size_t)n + 1); if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; } j[n] = 0; fclose(f);
        char* wm = strstr(j, "\"weight_map\""); char* p = wm ? strchr(wm, ':') : NULL;
        while (p && (p = strchr(p, '"')) != NULL) { char* q = strchr(p + 1, '"'); if (!q) break; if (q[1] != ':') { p = q + 1; continue; }
            size_t L = (size_t)(q - p - 1); char nm[512]; if (L > 511) L = 511; memcpy(nm, p + 1, L); nm[L] = 0;
            tot++; const char* r = llmdesc_role(&g_arch, nm); if (strcmp(r, "OTHER") == 0) { other++; if (other <= 8) fprintf(LOUT, "  OTHER: %s\n", nm); } p = q + 1; }
        free(j);
    }
    fprintf(LOUT, "# LLM TENSOR CHECK '%s' arch=%s tensori=%d OTHER=%d -> %s\n", path, g_arch_ok ? g_arch.arch : "-", tot, other, other ? "INVALID" : "OK");
    return other ? -1 : 0;
}
/* INFO: riepilogo del modello (arch, tensori, layer, kv, byte, modality) — GGUF o dir MLX. */
/* Vero se `path` e' una DIRECTORY (checkpoint MLX/HF): i verbi IR analitici, nati GGUF-only,
 * devono o gestirla (via `model.safetensors.index.json`) o fallire ESPLICITAMENTE — MAI in
 * silenzio (era il bug: una guardia costante faceva prendere sempre il ramo GGUF, `read_meta`
 * falliva sulla dir e si tornava -1 senza un messaggio). */
static int llmo_is_dir(const char* path) { struct stat st; return path != NULL && *path && stat(path, &st) == 0 && S_ISDIR(st.st_mode); }

static int llmmodelio_model_ir_info(const char* path) {
    size_t ln = strlen(path);
    if (!llmo_is_dir(path) /* GGUF */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        int maxl = -1; unsigned long long totb = 0;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            int L = model_layer_of(fi.tensors[i].name); if (L > maxl) maxl = L;
            uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
            uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) totb += b;
        }
        fprintf(LOUT, "LLM TENSOR INFO '%s': format=GGUF arch=%s tensori=%llu kv=%d layers=%d modality=%s tensor_bytes=%llu\n",
                path, g_arch_ok ? g_arch.arch : "-", (unsigned long long)fi.n_tensors, nkv, maxl + 1,
                g_arch_ok ? llmdesc_modality(&g_arch) : "-", totb);
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
        return 0;
    }
    if (llmmodelio_arch_resolve(path) != 0) return -1;
    char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", path);
    FILE* f = fopen(idx, "rb"); if (f == NULL) { fprintf(LOUT, "LLM TENSOR INFO '%s': format=MLX arch=%s (index assente) modality=%s\n", path, g_arch.arch, llmdesc_modality(&g_arch)); return 0; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* j = (char*)malloc((size_t)n + 1); if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; } j[n] = 0; fclose(f);
    char* wm = strstr(j, "\"weight_map\""); char* p = wm ? strchr(wm, ':') : NULL; int nt = 0, maxl = -1;
    while (p && (p = strchr(p, '"')) != NULL) { char* q = strchr(p + 1, '"'); if (!q) break; if (q[1] != ':') { p = q + 1; continue; }
        size_t L = (size_t)(q - p - 1); char nm[512]; if (L > 511) L = 511; memcpy(nm, p + 1, L); nm[L] = 0;
        nt++; int lyr = model_layer_of(nm); if (lyr > maxl) maxl = lyr; p = q + 1; }
    free(j);
    fprintf(LOUT, "LLM TENSOR INFO '%s': format=MLX arch=%s tensori=%d layers=%d modality=%s\n", path, g_arch.arch, nt, maxl + 1, llmdesc_modality(&g_arch));
    return 0;
}
/* GRAPH (I1, report-only): struttura del modello per layer (attention/moe/ffn/norm) dall arch. */
static const char* llmo_role_of(const char* nm);
/* NUMERIC GRAPH IR (§4.9): per-layer catena di OP numeriche + precisione (weights/accum/out) + roofline. */
/* OPTIMIZATION RECORD (GMB-AUTOTUNE, §4.5): record serializzabile dell ottimizzazione del modello. */
static int llmmodelio_model_ir_oprecord(const char* path, const char* out) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR OPRECORD: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0; long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    FILE* o = (out && *out) ? fopen(out, "w") : NULL;
    FILE* dst = o ? o : LOUT;
    fprintf(dst, "{\n  \"model\": \"%s\",\n  \"arch\": \"%s\",\n  \"weights_bytes\": %llu,\n  \"kv_per_token\": %.0f,\n  \"kv_dtype\": \"%s\",\n",
            path, g_arch_ok?g_arch.arch:"-", wb, kvpt, (g_arch_ok && g_arch.runtime_kv_dtype[0]) ? g_arch.runtime_kv_dtype : "f16");
    fprintf(dst, "  \"record\": {\n    \"planner\": \"MPLAN(compile-time)\",\n    \"residency\": \"EXPOLICY(router->prefetch)\",\n    \"kv_mode\": \"FULL|PAGED|ROTATING|DISK_BACKED\",\n    \"autotune\": \"llm_autotune.sh (kvq)\",\n    \"speculation\": \"SPEC(NONE|MTP|DSPARK|...)\"\n  }\n}\n");
    if (o) fclose(o);
    fprintf(LOUT, "# LLM TENSOR OPRECORD '%s' -> %s (GMB-AUTOTUNE record)\n", path, (out && *out) ? out : "stdout");
    return 0;
}
static int llmmodelio_model_ir_numgraph(const char* path) {
    int F[1024]; memset(F, 0, sizeof F); int maxl = -1;
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR NUMGRAPH: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        const char* r = llmo_role_of(fi.tensors[i].name);
        int L = model_layer_of(fi.tensors[i].name); if (L < 0) L = 0; if (L > maxl) maxl = L; if (L > 1023) continue;
        if (strncmp(r, "attn", 4) == 0) F[L] |= 1;
        else if (strncmp(r, "experts", 7) == 0 || strncmp(r, "shared", 6) == 0 || strcmp(r, "ffn.router") == 0) F[L] |= 2;
        else if (strncmp(r, "ffn.gate", 8) == 0 || strncmp(r, "ffn.up", 6) == 0 || strncmp(r, "ffn.down", 8) == 0) F[L] |= 4;
        else if (strcmp(r, "norm") == 0) F[L] |= 8;
        else if (strcmp(r, "embed") == 0 || strcmp(r, "output") == 0) F[L] |= 16;
    }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    const char* wfmt = (g_arch_ok && g_arch.n_quant > 0) ? g_arch.quant[0].fmt : "-";
    fprintf(LOUT, "# LLM TENSOR NUMGRAPH '%s' arch=%s layers=%d (numeric graph IR §4.9)\n", path, g_arch_ok?g_arch.arch:"-", maxl+1);
    fprintf(LOUT, "# layer\top-chain (weights=%s, activation=f16, accum=f32, out=f16)\n", wfmt);
    const char* g0 = "EMBED(table lookup; memory-bound) -> ";
    fprintf(LOUT, "global\t%sRMSNORM(f32) -> [per layer] -> FINAL RMSNORM -> LMHEAD(gemm; weights=%s)\n", g0, wfmt);
    for (int L = 0; L <= maxl && L < 8; L++) {
        char ops[512]; ops[0] = 0;
        if (F[L] & 8) strcat(ops, "RMSNORM(f32) -> ");
        if (F[L] & 1) strcat(ops, "QKV/gemm(attn; memory-bound) -> ROPE(f32) -> SOFTMAX(f32) -> ATTN_O(gemm) -> ");
        if (F[L] & 8) strcat(ops, "RMSNORM(f32) -> ");
        if (F[L] & 2) strcat(ops, "ROUTER(gemm+TOPK+softmax f32) -> MoE(gather experts) -> SWIGLU(geglu) -> DOWN(gemm) -> ");
        else if (F[L] & 4) strcat(ops, "GATE/UP(gemm) -> SWIGLU(geglu) -> DOWN(gemm) -> ");
        strcat(ops, "RESIDUAL(add f32)");
        fprintf(LOUT, "L%d\t%s\n", L, ops);
    }
    if (maxl >= 8) fprintf(LOUT, "...\t(altre %d layer, catena identica)\n", maxl-7);
    fprintf(LOUT, "# roofline: DECODE memory-bound (GEMV), PREFILL compute-bound (GEMM); attn O(ctx) -> softmax.\n");
    return 0;
}
static int llmmodelio_model_ir_graph(const char* path) {
    int F[1024]; memset(F, 0, sizeof F); int maxl = -1;
    /* bit: 1=attn 2=moe 4=ffn_dense 8=norm 16=embed/out */
    size_t ln = strlen(path);
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            const char* r = llmmo_role_cat(fi.tensors[i].name);
            int L = model_layer_of(fi.tensors[i].name); if (L < 0) L = 0; if (L > maxl) maxl = L; if (L > 1023) continue;
            if (strncmp(r, "ATTN", 4) == 0) F[L] |= 1;
            else if (strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0||strcmp(r,"ROUTER")==0||strstr(fi.tensors[i].name,"exps")) F[L] |= 2;
            else if (strcmp(r,"FFN")==0||strcmp(r,"FFN_GATE")==0||strcmp(r,"FFN_UP")==0||strcmp(r,"FFN_DOWN")==0) F[L] |= 4;
            else if (strstr(r,"NORM")) F[L] |= 8;
            else if (strcmp(r,"EMBEDDING")==0||strcmp(r,"OUTPUT")==0) F[L] |= 16;
        }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    } else {
        if (llmmodelio_arch_resolve(path) != 0) return -1;
        char idx[2048]; snprintf(idx, sizeof idx, "%s/model.safetensors.index.json", path);
        FILE* f = fopen(idx, "rb"); if (f == NULL) return -1;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char* j = (char*)malloc((size_t)n + 1); if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; } j[n] = 0; fclose(f);
        char* wm = strstr(j, "\"weight_map\""); char* p = wm ? strchr(wm, ':') : NULL;
        while (p && (p = strchr(p, '"')) != NULL) { char* q = strchr(p + 1, '"'); if (!q) break; if (q[1] != ':') { p = q + 1; continue; }
            size_t L2 = (size_t)(q - p - 1); char nm[512]; if (L2 > 511) L2 = 511; memcpy(nm, p + 1, L2); nm[L2] = 0;
            const char* r = llmdesc_role(&g_arch, nm); int L = model_layer_of(nm); if (L < 0) L = 0; if (L > maxl) maxl = L; if (L > 1023) { p = q + 1; continue; }
            if (strncmp(r, "ATTN", 4) == 0) F[L] |= 1;
            else if (strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0||strcmp(r,"ROUTER")==0||strstr(nm,"exps")) F[L] |= 2;
            else if (strcmp(r,"FFN")==0||strcmp(r,"FFN_GATE")==0||strcmp(r,"FFN_UP")==0||strcmp(r,"FFN_DOWN")==0) F[L] |= 4;
            else if (strstr(r,"NORM")) F[L] |= 8; else if (strcmp(r,"EMBEDDING")==0||strcmp(r,"OUTPUT")==0) F[L] |= 16;
            p = q + 1; }
        free(j);
    }
    const char* atn = g_arch_ok ? llmdesc_struct_type(&g_arch,"attention") : NULL;
    const char* moe = g_arch_ok ? llmdesc_struct_type(&g_arch,"moe") : NULL;
    fprintf(LOUT, "# LLM TENSOR GRAPH '%s' arch=%s attention=%s moe=%s layers=0..%d\n", path, g_arch_ok?g_arch.arch:"-", atn?atn:"-", moe?moe:"-", maxl);
    for (int L = 0; L <= maxl && L < 1024; L++) {
        if (!F[L]) continue;
        fprintf(LOUT, "layer %d:", L);
        if (F[L]&1) fprintf(LOUT, " attention%s", atn?atn:"");
        if (F[L]&2) fprintf(LOUT, " moe%s", moe?moe:"");
        if (F[L]&4) fprintf(LOUT, " ffn");
        if (F[L]&8) fprintf(LOUT, " norm");
        if (F[L]&16) fprintf(LOUT, " embed/out");
        fprintf(LOUT, "\n");
    }
    return 0;
}
/* MEMORY IR (I2, report-only): byte per categoria (embed/attn/experts/router/ffn/norm/output/other). */
static int llmmodelio_model_ir_memory(const char* path) {
    unsigned long long cat[8] = {0}; /* 0 embed 1 attn 2 experts 3 router 4 ffn 5 norm 6 output 7 other */
    const char* cn[8] = {"embedding","attention","experts","router","ffn_dense","norm","output","other"};
    size_t ln = strlen(path); int n = 0;
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            const char* r = llmmo_role_cat(fi.tensors[i].name);
            uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
            uint64_t b = 0; (b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0);
            int c = 7;
            if (strcmp(r,"EMBEDDING")==0) c=0; else if (strncmp(r,"ATTN",4)==0) c=1;
            else if (strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0) c=2; else if (strcmp(r,"ROUTER")==0) c=3;
            else if (strncmp(r,"FFN",3)==0) c=4; else if (strstr(r,"NORM")) c=5; else if (strcmp(r,"OUTPUT")==0) c=6;
            cat[c] += b; n++;
        }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    } else { fprintf(LOUT, "LLM TENSOR MEMORY: solo GGUF (per ora) — '%s'\n", path); return -1; }
    unsigned long long tot = 0; for (int c = 0; c < 8; c++) tot += cat[c];
    fprintf(LOUT, "# LLM TENSOR MEMORY '%s' tensori=%d total_bytes=%llu (%.2f GiB)\n", path, n, tot, (double)tot/(1024.0*1024.0*1024.0));
    for (int c = 0; c < 8; c++) if (cat[c]) fprintf(LOUT, "  %-10s %llu bytes (%.1f%%)\n", cn[c], cat[c], tot ? 100.0*(double)cat[c]/(double)tot : 0.0);
    return 0;
}
/* COST MODEL (I3, report-only): pesi + KV byte/token (da metadati GGUF) + totale per contesto. */
static int llmmodelio_model_ir_cost(const char* path, int ctx) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(LOUT, "LLM TENSOR COST: solo GGUF (per ora)\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    long bc = -1, hkv = -1, hd = -1, kl = -1, vl = -1, hl = -1, en = -1;
    for (int i = 0; i < nkv; i++) {
        const char* k = kv[i].key; long v = (long)kv[i].u32;
        if (strstr(k, "attention.head_count_kv")) hkv = v; else if (strstr(k, "attention.head_count")) hd = v;
        else if (strstr(k, "attention.key_length")) kl = v; else if (strstr(k, "attention.value_length")) vl = v;
        else if (strstr(k, "block_count")) bc = v; else if (strstr(k, "embedding_length")) en = v;
    }
    if (ctx <= 0) ctx = 2048;
    double kvpt = -1.0;
    if (bc > 0 && hkv > 0) { long kd = (kl > 0) ? kl : ((en > 0 && hd > 0) ? en / hd : -1); long vd = (vl > 0) ? vl : kd;
        if (kd > 0 && vd > 0) kvpt = (double)bc * (double)hkv * (double)(kd + vd) * 2.0; }  /* 2 = F16 KV */
    fprintf(LOUT, "# LLM TENSOR COST '%s' arch=%s ctx=%d\n  weights: %llu bytes (%.2f GiB)\n",
            path, g_arch_ok?g_arch.arch:"-", ctx, wb, (double)wb/(1024.0*1024.0*1024.0));
    if (kvpt >= 0) fprintf(LOUT, "  kv/token: %.0f bytes (%.2f MiB) -> kv@ctx: %.2f GiB\n  total@ctx: %.2f GiB\n",
                           kvpt, kvpt/(1024.0*1024.0), kvpt*(double)ctx/(1024.0*1024.0*1024.0), ((double)wb + kvpt*(double)ctx)/(1024.0*1024.0*1024.0));
    else fprintf(LOUT, "  kv/token: n/d (metadati KV assenti: block_count/head_count_kv/key_length)\n");
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
/* SPECULATION IR (I9, report-only): modalita' dichiarata dall ABI (mtp/dspark) + ruoli MTP. */
static int llmmodelio_model_ir_spec(const char* path) {
    size_t ln = strlen(path);
    if (1 /* read_meta gestisce anche le dir MLX */) { if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0; }
    else { if (llmmodelio_arch_resolve(path) != 0) return -1; }
    if (!g_arch_ok) return -1;   /* errore silenzioso: viene dal codice di ritorno (regola parity) */
    int has_mtp = g_arch.mm_mtp || 0;
    for (int i = 0; i < g_arch.n_roles && !has_mtp; i++) if (strcmp(g_arch.roles[i].role, "MTP") == 0) has_mtp = 1;
    const char* mode = has_mtp ? (g_arch.mm_dspark ? "DSPARK" : "MTP") : "NONE";
    const char* rb = has_mtp ? "ACCEPT/ROLLBACK su token" : "n/a";
    fprintf(LOUT, "# LLM TENSOR SPEC '%s' arch=%s speculation=%s mtp=%d dspark=%d (§4.5 I9 SPECULATION IR)\n",
            path, g_arch_ok?g_arch.arch:"-", mode, g_arch.mm_mtp, g_arch.mm_dspark);
    fprintf(LOUT, "  TASSONOMIA: NONE | SELF | MTP | DFLASH | DSPARK | EAGLE | TREE | EXTERNAL_DRAFT | ANE | HYBRID\n");
    fprintf(LOUT, "  primitiva: DRAFT (proposta) -> VERIFY (1 forward) -> ACCEPT (n token) | ROLLBACK (rollback_mode=%s)\n", rb);
    fprintf(LOUT, "  ngram/spec: llm_ai_mlx_session_build_ngram + generate_ngram (n-gram draft); MTP head per-family.\n");
    return 0;
}
/* KV IR (report-only, §4.6): formato/dtype KV dichiarati dall ABI (runtime.kv.dtype/state). */
static int llmmodelio_model_ir_kv(const char* path, const char* spec) {
    size_t ln = strlen(path);
    { FILE* tf = fopen(path, "rb"); if (tf == NULL) return -1; fclose(tf); }   /* fail-fast: file esistente */
    if (1 /* read_meta gestisce anche le dir MLX */) { if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0; }
    else { if (llmmodelio_arch_resolve(path) != 0) g_arch_ok = 0; }
    /* leniente: l'arch puo' mancare (report comunque utile con i default). */
    const char* mode = (spec && *spec) ? spec : "FULL";
    const char* dtype = g_arch.runtime_kv_dtype[0] ? g_arch.runtime_kv_dtype : "f16";
    fprintf(LOUT, "# LLM TENSOR KV '%s' arch=%s mode=%s (§4.6 KV IR)\n", path, g_arch_ok?g_arch.arch:"-", mode);
    fprintf(LOUT, "  KV_FORMAT dtype=%s  activation.dtype=%s  state=%s\n", dtype,
            g_arch.runtime_activation_dtype[0] ? g_arch.runtime_activation_dtype : "-",
            g_arch.runtime_state[0] ? g_arch.runtime_state : "-");
    fprintf(LOUT, "  KV_MODE {FULL, PAGED, ROTATING, DISK_BACKED} -> scelto=%s\n", mode);
    if (strcasecmp(mode,"PAGED")==0)        fprintf(LOUT, "   PAGED       : pagine fisse; MAP/UNMAP block<->asse; EVICT LRU; PREFETCH on-miss\n");
    else if (strcasecmp(mode,"ROTATING")==0) fprintf(LOUT, "   ROTATING    : finestra scorrevole (KV oltre la finestra evinto); niente prefix-cache\n");
    else if (strcasecmp(mode,"DISK_BACKED")==0) fprintf(LOUT, "   DISK_BACKED : KV su SSD; PREFETCH on-demand; kvSwapMaxKvMb/kvSwapPath\n");
    else                                     fprintf(LOUT, "   FULL        : KV interamente in RAM (nessun page/evict)\n");
    fprintf(LOUT, "  KV_FORMAT opzioni: f16 (default) | q8_0 | q4_0 | kvquant (rotazione+tipo); KV_MODE quello sopra\n");
    return 0;
}
/* EXECPLAN (GMB_AI_PLAN serializzabile, §4.5): piano esecutivo del modello in JSON (report-only). */
static int llmmodelio_model_ir_execplan(const char* path, const char* out_path) {
    if (!out_path || !*out_path) { fprintf(stderr, "LLM TENSOR EXECPLAN: atteso <path>, <out>\n"); return -1; }
    size_t ln = strlen(path); int F[1024]; memset(F, 0, sizeof F); int maxl = -1;
    int layer_skip[1024]; int layer_nten[1024];
    memset(layer_skip, 0, sizeof layer_skip); memset(layer_nten, 0, sizeof layer_nten);
    unsigned long long wb = 0; int nten = 0; long bc = -1, hkv = -1, hd = -1, kl = -1, vl = -1, en = -1;
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            const char* r = llmmo_role_cat(fi.tensors[i].name);
            uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
            uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b;
            int L = model_layer_of(fi.tensors[i].name); if (L < 0) L = 0; if (L > maxl) maxl = L; if (L > 1023) { nten++; continue; }
            if (strncmp(r,"ATTN",4)==0) F[L]|=1; else if (strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0||strcmp(r,"ROUTER")==0||strstr(fi.tensors[i].name,"exps")) F[L]|=2;
            else if (strncmp(r,"FFN",3)==0) F[L]|=4; else if (strstr(r,"NORM")) F[L]|=8; nten++;
        }
        for (int i = 0; i < nkv; i++) { const char* k = kv[i].key; long v = (long)kv[i].u32;
            if (strstr(k,"attention.head_count_kv")) hkv=v; else if (strstr(k,"attention.head_count")) hd=v;
            else if (strstr(k,"attention.key_length")) kl=v; else if (strstr(k,"attention.value_length")) vl=v;
            else if (strstr(k,"block_count")) bc=v; else if (strstr(k,"embedding_length")) en=v; }
        /* §9.A M10 (produttore di SKIP / CED): un layer i cui tensori sono TUTTI ALL-ZERO e'
         * saltabile (identita'). Campionato start/mid/end (4KB) come CED. Solo GGUF. Deterministico. */
        { FILE* fz = fopen(path, "rb");
          if (fz != NULL) {
            for (uint64_t i = 0; i < fi.n_tensors; i++) {
                int L2 = model_layer_of(fi.tensors[i].name); if (L2 < 0 || L2 > 1023) continue;
                layer_nten[L2]++;
                uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
                uint64_t bb = 0; if (llmcodec_ggml_type_bytes(fi.tensors[i].ggml_type, numel, &bb) != 0 || bb == 0) { layer_skip[L2] = -1; continue; }
                size_t win = bb < 4096 ? (size_t)bb : 4096; uint64_t offs[3]; int no = 0;
                if (bb <= win) offs[no++] = 0; else { offs[no++] = 0; offs[no++] = bb/2 - win/2; offs[no++] = bb - win; }
                unsigned char* buf = (unsigned char*)malloc(win); if (buf == NULL) continue;
                int z = 1;
                for (int w = 0; w < no && z; w++) {
                    if (fseek(fz, (long)(fi.data_start + fi.tensors[i].offset + offs[w]), SEEK_SET) != 0 || fread(buf, 1, win, fz) != win) { z = 0; break; }
                    for (size_t k2 = 0; k2 < win; k2++) if (buf[k2]) { z = 0; break; }
                }
                free(buf);
                if (!z) layer_skip[L2] = -1;   /* almeno un tensore non-zero -> NON saltabile */
            }
            fclose(fz);
          }
        }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    } else { fprintf(stderr, "LLM TENSOR EXECPLAN: solo GGUF (per ora)\n"); return -1; }
    double kvpt = -1.0;
    if (bc > 0 && hkv > 0) { long kd = kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd = vl>0?vl:kd; if (kd>0&&vd>0) kvpt = (double)bc*(double)hkv*(double)(kd+vd)*2.0; }
    const char* atn = g_arch_ok ? llmdesc_struct_type(&g_arch,"attention") : NULL;
    const char* moe = g_arch_ok ? llmdesc_struct_type(&g_arch,"moe") : NULL;
    FILE* o = fopen(out_path, "w"); if (o == NULL) return -1;
    fprintf(o, "{\n  \"format\": \"GGUF\",\n  \"arch\": \"%s\",\n  \"tensors\": %d,\n  \"n_layers\": %d,\n", g_arch_ok?g_arch.arch:"-", nten, maxl+1);
    fprintf(o, "  \"attention\": \"%s\",\n  \"moe\": \"%s\",\n  \"modality\": \"%s\",\n", atn?atn:"-", moe?moe:"-", g_arch_ok?llmdesc_modality(&g_arch):"-");
    int mtp = g_arch_ok ? (g_arch.mm_mtp || 0) : 0;
    fprintf(o, "  \"speculation\": \"%s\",\n", mtp ? (g_arch.mm_dspark?"DSPARK(MTP)":"MTP") : "NONE");
    fprintf(o, "  \"memory\": { \"weights_bytes\": %llu, \"kv_per_token_bytes\": ", wb);
    if (kvpt >= 0) fprintf(o, "%.0f", kvpt); else fprintf(o, "null");
    fprintf(o, " },\n");
    /* §9.A M10 EXECUTION PLANNER: piano per-layer in FORMATO ENGINE (`layers` posizionale,
     * indice = layer) — consumabile direttamente dal runtime MLX via opzione `execplan`
     * (llm_mlx_execplan). Denso 0..maxl; un `skip:true` (non emesso qui) salta il layer. */
    int nskip = 0;
    fprintf(o, "  \"layers\": [\n");
    for (int LE = 0; LE <= maxl && LE < 1024; LE++) {
        int skip_ok = (layer_nten[LE] > 0 && layer_skip[LE] == 0);
        if (skip_ok) { nskip++; fprintf(o, "%s    { \"skip\": true }", LE ? ",\n" : ""); continue; }
        int ha = (F[LE] & 1) != 0, hf = (F[LE] & 4) != 0, hm = (F[LE] & 2) != 0;
        fprintf(o, "%s    { \"ops\": [", LE ? ",\n" : "");
        int f1 = 1;
        if (ha) { fprintf(o, "\"attention\""); f1 = 0; }
        if (hf || hm) { fprintf(o, "%s\"ffn\"", f1 ? "" : ", "); f1 = 0; }
        fprintf(o, "] }");
    }
    fprintf(o, "\n  ],\n");
    fprintf(o, "  \"layer_ops\": [\n");
    int first = 1;
    for (int L = 0; L <= maxl && L < 1024; L++) { if (!F[L]) continue;
        fprintf(o, "%s    { \"layer\": %d, \"ops\": [", first?"":",\n", L);
        int f1 = 1; if (F[L]&1){fprintf(o,"%s\"attention\"",f1?"":" ,");f1=0;} if (F[L]&4){fprintf(o,"%s\"ffn\"",f1?"":" ,");f1=0;} if (F[L]&2){fprintf(o,"%s\"moe\"",f1?"":" ,");f1=0;} if (F[L]&8){fprintf(o,"%s\"norm\"",f1?"":" ,");f1=0;}
        fprintf(o, "] }"); first = 0; }
    fprintf(o, "\n  ]\n}\n");
    fclose(o);
    fprintf(LOUT, "LLM TENSOR EXECPLAN '%s' -> '%s' (arch=%s layers=%d weights=%llu, kv/token=%.0f skip=%d)\n", path, out_path, g_arch_ok?g_arch.arch:"-", maxl+1, wb, kvpt, nskip);
    return 0;
}
/* CED (§4.8) — STATO del kill-switch: quando `LLM TENSOR CED <path>, "on"` e' invocato, COMPILE
 * applica l'elimination plan (esclude i tensori ALL-ZERO dal peso/capacita' + lo registra). */
static char s_ced_path[4096] = ""; static int s_ced_on = 0;
/* CED scan CONDIVISO (da CED e da COMPILE): tensori ALL-ZERO + byte eliminabili. Deterministico.
 * Campionato start/mid/end (4KB) per non leggere modelli enormi. */
static int llmmod_is_dir(const char* p);
static int llmmodelio_ced_scan(const char* path, const LlmCodecGgufFileInfo* fi,
                               unsigned long long* out_bytes, int* out_checked,
                               unsigned long long* out_total, int* out_layers, int print_cand,
                               float eps, int* out_bounded, unsigned long long* out_bounded_bytes) {
    int allzero = 0, checked = 0, layers = 0, bounded = 0; unsigned long long elim = 0, total = 0, boundb = 0;
    int seen[1024]; memset(seen, 0, sizeof seen);
    if (llmmod_is_dir(path)) {   /* DIR MLX/HF: leggi i byte REALI dagli shard safetensors */
        DIR* dp = opendir(path); if (!dp) { if(out_bytes)*out_bytes=0; if(out_checked)*out_checked=0; if(out_total)*out_total=0; if(out_layers)*out_layers=0; return 0; }
        struct dirent* e;
        while ((e = readdir(dp)) != NULL) {
            size_t L = strlen(e->d_name); if (!(L > 12 && strcmp(e->d_name + L - 12, ".safetensors") == 0)) continue;
            char sp[2048]; snprintf(sp, sizeof sp, "%s/%s", path, e->d_name);
            LlmCodecStTensorInfo* ti = NULL; int nt = 0; uint64_t base = 0;
            if (llmcodec_safetensors_read(sp, &ti, &nt, &base) != 0) continue;
            FILE* f = fopen(sp, "rb");
            for (int i = 0; i < nt && f; i++) {
                uint64_t b = (ti[i].end > ti[i].begin) ? (ti[i].end - ti[i].begin) : 0; if (!b) continue; total += b;
                size_t win = b < 4096 ? (size_t)b : 4096;
                uint64_t offs[3]; int no = 0;
                if (b <= win) { offs[no++] = 0; } else { offs[no++] = 0; offs[no++] = b/2 - win/2; offs[no++] = b - win; }
                unsigned char* buf = (unsigned char*)malloc(win); if (!buf) continue;
                int z = 1;
                for (int w = 0; w < no && z; w++) {
                    if (fseek(f, (long)(base + ti[i].begin + offs[w]), SEEK_SET) != 0 || fread(buf, 1, win, f) != win) { z = 0; break; }
                    for (size_t kk = 0; kk < win; kk++) if (buf[kk]) { z = 0; break; }
                }
                free(buf); checked++;
                if (z) { allzero++; elim += b; int L2 = model_layer_of(ti[i].name); if (L2 >= 0 && L2 < 1024 && !seen[L2]) { seen[L2] = 1; layers++; }
                         if (print_cand && allzero <= 8) fprintf(LOUT, "  ALL-ZERO (candidato ELIMINA): %s (%llu byte)\n", ti[i].name, (unsigned long long)b); }
            }
            if (f) fclose(f); llmcodec_safetensors_info_free(ti);
        }
        closedir(dp);
    } else {
        FILE* f = fopen(path, "rb");
        for (uint64_t i = 0; i < fi->n_tensors && f; i++) {
            uint64_t numel = 1; for (int d = 0; d < fi->tensors[i].ndim; d++) numel *= fi->tensors[i].dims[d];
            uint64_t b = 0; if (llmcodec_ggml_type_bytes(fi->tensors[i].ggml_type, numel, &b) != 0 || b == 0) continue;
            total += b;
            size_t win = b < 4096 ? (size_t)b : 4096;
            uint64_t offs[3]; int no = 0;
            if (b <= win) { offs[no++] = 0; } else { offs[no++] = 0; offs[no++] = b/2 - win/2; offs[no++] = b - win; }
            unsigned char* buf = (unsigned char*)malloc(win); if (!buf) continue;
            int z = 1;
            for (int w = 0; w < no && z; w++) {
                if (fseek(f, (long)(fi->data_start + fi->tensors[i].offset + offs[w]), SEEK_SET) != 0 || fread(buf, 1, win, f) != win) { z = 0; break; }
                for (size_t kk = 0; kk < win; kk++) if (buf[kk]) { z = 0; break; }
            }
            /* BOUNDED (§4.8): non nullo ma max|w| < eps (solo F16/F32) = eliminabile con errore
             * limitato (‖W‖∞ < eps). Campionato come sopra. */
            float mabs = 0.0f;
            if (!z && eps > 0.0f && (fi->tensors[i].ggml_type == 0 || fi->tensors[i].ggml_type == 1)) {
                for (int w = 0; w < no; w++) {
                    if (fseek(f, (long)(fi->data_start + fi->tensors[i].offset + offs[w]), SEEK_SET) != 0 || fread(buf, 1, win, f) != win) break;
                    if (fi->tensors[i].ggml_type == 0) { const float* ff = (const float*)buf; for (size_t kk = 0; kk + 1 < win/4; kk++) { float a = fabsf(ff[kk]); if (a > mabs) mabs = a; } }
                    else { const uint16_t* hh = (const uint16_t*)buf; for (size_t kk = 0; kk + 1 < win/2; kk++) { float a = fabsf(llmcodec_f16_to_f32(hh[kk])); if (a > mabs) mabs = a; } }
                }
            }
            free(buf); checked++;
            if (z) { allzero++; elim += b; int L2 = model_layer_of(fi->tensors[i].name); if (L2 >= 0 && L2 < 1024 && !seen[L2]) { seen[L2] = 1; layers++; }
                     if (print_cand && allzero <= 8) fprintf(LOUT, "  ALL-ZERO (candidato ELIMINA): %s (%llu byte)\n", fi->tensors[i].name, (unsigned long long)b); }
            else if (eps > 0.0f && mabs < eps) { bounded++; boundb += b;
                     if (print_cand && bounded <= 8) fprintf(LOUT, "  BOUNDED (maxw=%.3e < %.1e, candidato ELIMINA): %s (%llu byte)\n", (double)mabs, (double)eps, fi->tensors[i].name, (unsigned long long)b); }
        }
        if (f) fclose(f);
    }
    if (out_bytes) *out_bytes = elim; if (out_checked) *out_checked = checked;
    if (out_total) *out_total = total; if (out_layers) *out_layers = layers;
    if (out_bounded) *out_bounded = bounded; if (out_bounded_bytes) *out_bounded_bytes = boundb;
    return allzero;
}
/* COMPILE (§4.9, ZGC): artefatto SPECIALIZZATO `.gmbllm` (JSON) = graph + memory plan +
 * exact capacity + kernel/precision per categoria. Report-only, serializzabile. */
static int llmmodelio_model_ir_compile(const char* path, const char* out_path) {
    if (!out_path || !*out_path) { fprintf(stderr, "LLM TENSOR COMPILE: atteso <path>, <out.gmbllm>\n"); return -1; }
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR COMPILE: solo GGUF (per ora)\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0; int F[1024]; memset(F, 0, sizeof F); int maxl = -1;
    /* kernel/precision per categoria (dall arch quant, fallback al tipo del tensore) */
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
        uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b;
        const char* r = llmmo_role_cat(fi.tensors[i].name);
        int L = model_layer_of(fi.tensors[i].name); if (L < 0) L = 0; if (L > maxl) maxl = L; if (L > 1023) continue;
        if (strncmp(r,"ATTN",4)==0) F[L]|=1; else if (strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0||strcmp(r,"ROUTER")==0||strstr(fi.tensors[i].name,"exps")) F[L]|=2;
        else if (strncmp(r,"FFN",3)==0) F[L]|=4; else if (strstr(r,"NORM")) F[L]|=8;
    }
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"attention.head_count_kv"))hkv=v; else if(strstr(k,"attention.head_count"))hd=v;
        else if(strstr(k,"attention.key_length"))kl=v; else if(strstr(k,"attention.value_length"))vl=v;
        else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    /* APPLICAZIONE CED (§4.8): se il kill-switch e' ON per QUESTO modello, l'artefatto esclude i
     * tensori ALL-ZERO dal peso (weights_bytes) e lo registra nel campo `ced`. */
    int ced_n = 0; unsigned long long ced_b = 0;
    if (s_ced_on && strcmp(s_ced_path, path) == 0) ced_n = llmmodelio_ced_scan(path, &fi, &ced_b, NULL, NULL, NULL, 0, 0.0f, NULL, NULL);
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    if (ced_b <= wb) wb -= ced_b;
    double kvpt=-1.0; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*(double)hkv*(double)(kd+vd)*2.0; }
    const double BUDGET = 90.0*1024.0*1024.0*1024.0, RESERVE = 2048.0*1024.0*1024.0; /* 90GB cap (RAM reale), 2GB riserva */
    long cap = (kvpt>0) ? (long)((BUDGET - RESERVE - (double)wb) / kvpt) : -1;
    const char* atn = g_arch_ok ? llmdesc_struct_type(&g_arch,"attention") : NULL;
    const char* moe = g_arch_ok ? llmdesc_struct_type(&g_arch,"moe") : NULL;
    FILE* o = fopen(out_path, "w"); if (!o) return -1;
    fprintf(o, "{\n  \"artifact\": \"gmbllm\",\n  \"version\": 1,\n  \"format\": \"GGUF\",\n  \"arch\": \"%s\",\n  \"layers\": %d,\n", g_arch_ok?g_arch.arch:"-", maxl+1);
    fprintf(o, "  \"attention\": \"%s\",\n  \"moe\": \"%s\",\n  \"modality\": \"%s\",\n  \"speculation\": \"%s\",\n",
            atn?atn:"-", moe?moe:"-", g_arch_ok?llmdesc_modality(&g_arch):"-", (g_arch_ok&&g_arch.mm_mtp)?(g_arch.mm_dspark?"DSPARK(MTP)":"MTP"):"NONE");
    fprintf(o, "  \"memory_plan\": { \"weights_bytes\": %llu, \"kv_per_token_bytes\": ", wb); if(kvpt>=0)fprintf(o,"%.0f",kvpt); else fprintf(o,"null"); fprintf(o, " },\n");
    fprintf(o, "  \"exact_capacity\": { \"rope\": 0, \"kv_bound\": %ld },\n", cap);
    fprintf(o, "  \"ced\": { \"enabled\": %s, \"eliminated\": %d, \"bytes\": %llu },\n",
            (s_ced_on && strcmp(s_ced_path, path) == 0) ? "true" : "false", ced_n, ced_b);
    fprintf(o, "  \"kernels\": { \"attention\": \"%s\", \"moe\": \"%s\", \"norm\": \"rmsnorm\", \"matmul\": \"affine_qmm\" },\n", atn?atn:"gqa", moe?moe:"moe_sorted_gather");
    fprintf(o, "  \"layer_ops\": [\n"); int first=1;
    for(int L=0;L<=maxl&&L<1024;L++){ if(!F[L])continue; fprintf(o,"%s    { \"layer\": %d, \"ops\": [",first?"":",\n",L);
        int f1=1; if(F[L]&1){fprintf(o,"%s\"attention\"",f1?"":" ,");f1=0;} if(F[L]&4){fprintf(o,"%s\"ffn\"",f1?"":" ,");f1=0;} if(F[L]&2){fprintf(o,"%s\"moe\"",f1?"":" ,");f1=0;} if(F[L]&8){fprintf(o,"%s\"norm\"",f1?"":" ,");f1=0;}
        fprintf(o,"] }"); first=0; }
    fprintf(o, "\n  ]\n}\n"); fclose(o);
    fprintf(LOUT, "LLM TENSOR COMPILE '%s' -> '%s' (arch=%s layers=%d capacity=%ld)\n", path, out_path, g_arch_ok?g_arch.arch:"-", maxl+1, cap);
    return 0;
}
/* GPU (§4.7 SIMT, report-only): divergenza/uniformita' del routing MoE + kernel specialization +
 * GPUCostModel. Analisi statica dal modello/arch (nessun GPU backend qui). */
static int llmmodelio_model_ir_gpu(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR GPU: solo GGUF (per ora)\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0; long nexp = -1, nact = -1, nl = -1;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    for (int i = 0; i < nkv; i++) { const char* k = kv[i].key; long v = (long)kv[i].u32;
        if (strstr(k, "expert_count")) nexp = v; else if (strstr(k, "expert_used_count")) nact = v; else if (strstr(k, "block_count")) nl = v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    const char* atn = g_arch_ok ? llmdesc_struct_type(&g_arch,"attention") : NULL;
    double div = (nexp > 0 && nact > 0) ? (double)nact / (double)nexp : -1.0;  /* activated/routed = "spread" */
    fprintf(LOUT, "# LLM TENSOR GPU '%s' arch=%s\n", path, g_arch_ok?g_arch.arch:"-");
    fprintf(LOUT, "  kernel_specialization: attention=%s moe=%s matmul=affine_qmm norm=rmsnorm\n",
            atn?atn:"gqa", (nexp>0)?"moe_sorted_gather":"-");
    if (div >= 0) fprintf(LOUT, "  routing: experts=%ld activated=%ld spread=%.3f -> %s\n", nexp, nact, div,
                          div < 0.1 ? "ALTA divergenza (batch MoE: sort/group->compact->GEMM->scatter)" : "bassa divergenza");
    else fprintf(LOUT, "  routing: n/d (expert_count/used_count assenti)\n");
    fprintf(LOUT, "  gpu_cost_model: layers=%ld weights=%llu bytes (%.2f GiB)\n", nl, wb, (double)wb/(1024.0*1024.0*1024.0));
    return 0;
}
/* PROFILE (I4) + RESIDENCY (I12) + STATE (I7) — report-only. */
/* ================= ASTRAZIONE formato-agnostica: GGUF *o* dir MLX/HF =================
 * Molti comandi `LLM TENSOR` lavoravano solo su .gguf. Qui un unico accesso: lista tensori
 * (nome/ndim/dims/byte) + metadati geometrici (layers/heads/hidden/inter/expert) + peso totale,
 * indifferentemente da un file GGUF o da una directory MLX/HF (config.json + *.safetensors).
 * Il ruolo dei tensori si ricava dall'arch risolta (`g_arch`) — che risolve ENTRAMBI. */
typedef struct { char name[256]; int ndim; uint64_t dims[8]; uint64_t bytes; } LlmModTensor;
typedef struct {
    LlmModTensor* tensors; uint64_t n_tensors; unsigned long long weights_bytes;
    int is_gguf; uint64_t data_start;                 /* GGUF: per lettura raw (CED) */
    long block_count, head_count, head_count_kv, key_length, value_length, embedding_length, inter_size, n_experts, top_k;
    int ok;
} LlmModInfo;
static int llmmod_is_gguf(const char* p) { size_t n = strlen(p); return n > 5 && strcasecmp(p + n - 5, ".gguf") == 0; }
static int llmmod_is_dir(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
/* GET intero da un JSON testuale: cerca "key" : <int> (primo match). Ritorna 0 e setta *out. */
static int llm_json_get_int(const char* json, const char* key, long* out) {
    if (!json || !key) return 0;
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(json, pat); if (!p) return 0;
    p += strlen(pat); while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\r' || *p == '\n')) p++;
    if (!(*p == '-' || (*p >= '0' && *p <= '9'))) return 0;
    *out = strtol(p, NULL, 10); return 1;
}
static long llm_json_get_int_any(const char* json, const char** keys, int nk, long dflt) {
    long v; for (int i = 0; i < nk; i++) if (llm_json_get_int(json, keys[i], &v)) return v; return dflt;
}
static char* llmmod_read_file(const char* path, long* out_n) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1); if (b && fread(b, 1, (size_t)n, f) == (size_t)n) b[n] = 0; else { free(b); b = NULL; }
    fclose(f); if (out_n) *out_n = n; return b;
}
/* Meta geometrica da <dir>/config.json (MLX/HF). */
static void llmmod_meta_cfg(const char* dir, LlmModInfo* m) {
    char p[2048]; snprintf(p, sizeof p, "%s/config.json", dir);
    char* j = llmmod_read_file(p, NULL); if (!j) return;
    const char* k_lay[] = {"num_hidden_layers","n_layer","num_layers"};
    const char* k_hd[]  = {"num_attention_heads","n_head","num_heads"};
    const char* k_hkv[] = {"num_key_value_heads","n_head_kv","num_kv_heads"};
    const char* k_en[]  = {"hidden_size","n_embd","d_model"};
    const char* k_in[]  = {"intermediate_size","n_inner","ffn_dim"};
    const char* k_ne[]  = {"n_routed_experts","num_experts","n_experts","num_local_experts"};
    const char* k_tk[]  = {"num_experts_per_tok","num_experts_per_token","top_k"};
    const char* k_kl[]  = {"head_dim","key_length","kv_channels"};
    m->block_count      = llm_json_get_int_any(j, k_lay, 3, -1);
    m->head_count      = llm_json_get_int_any(j, k_hd,  3, -1);
    m->head_count_kv   = llm_json_get_int_any(j, k_hkv, 3, -1);
    m->embedding_length= llm_json_get_int_any(j, k_en,  3, -1);
    m->inter_size      = llm_json_get_int_any(j, k_in,  3, -1);
    m->n_experts       = llm_json_get_int_any(j, k_ne,  4, -1);
    m->top_k           = llm_json_get_int_any(j, k_tk,  3, -1);
    m->key_length      = llm_json_get_int_any(j, k_kl,  3, -1);
    m->value_length    = m->key_length;
    free(j);
}
/* Carica tensori + meta da GGUF (is_gguf=1) o da una dir MLX/HF (is_gguf=0). */
static int llmmod_load(const char* path, LlmModInfo* m) {
    memset(m, 0, sizeof *m); m->block_count = m->head_count = m->head_count_kv = m->key_length =
        m->value_length = m->embedding_length = m->inter_size = m->n_experts = m->top_k = -1;
    if (llmmod_is_gguf(path)) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        m->is_gguf = 1; m->data_start = fi.data_start; m->n_tensors = fi.n_tensors;
        m->tensors = (LlmModTensor*)calloc(fi.n_tensors ? fi.n_tensors : 1, sizeof(LlmModTensor));
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            LlmModTensor* t = &m->tensors[i];
            snprintf(t->name, sizeof t->name, "%s", fi.tensors[i].name);
            t->ndim = fi.tensors[i].ndim; uint64_t numel = 1;
            for (int d = 0; d < t->ndim && d < 8; d++) { t->dims[d] = (uint64_t)fi.tensors[i].dims[d]; numel *= t->dims[d]; }
            uint64_t b = 0; (b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0); t->bytes = b; m->weights_bytes += b;
        }
        for (int i = 0; i < nkv; i++) { const char* k = kv[i].key; long v = (long)kv[i].u32;
            if (strstr(k,"attention.head_count_kv")) m->head_count_kv = v; else if (strstr(k,"attention.head_count")) m->head_count = v;
            else if (strstr(k,"attention.key_length")) m->key_length = v; else if (strstr(k,"attention.value_length")) m->value_length = v;
            else if (strstr(k,"block_count")) m->block_count = v; else if (strstr(k,"embedding_length")) m->embedding_length = v;
            else if (strstr(k,"expert_count")) m->n_experts = v; else if (strstr(k,"expert_used_count")) m->top_k = v; }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
        if (llmmod_is_dir(path)) llmmod_meta_cfg(path, m);
        m->ok = 1; return 0;
    }
    /* dir MLX/HF */
    if (!llmmod_is_dir(path)) return -1;
    if (llmmodelio_arch_resolve(path) != 0) g_arch_ok = 0;
    llmmod_meta_cfg(path, m);
    /* conta gli shard *.safetensors e somma i tensori dell'header */
    DIR* dp = opendir(path); if (!dp) return -1;
    uint64_t cap = 0; struct dirent* e;
    while ((e = readdir(dp)) != NULL) { size_t L = strlen(e->d_name); if (L > 12 && strcmp(e->d_name + L - 12, ".safetensors") == 0) cap += 4096; }
    closedir(dp);
    m->tensors = (LlmModTensor*)calloc(cap ? cap : 1, sizeof(LlmModTensor));
    uint64_t n = 0;
    dp = opendir(path); if (!dp) { free(m->tensors); m->tensors = NULL; return -1; }
    while ((e = readdir(dp)) != NULL) {
        size_t L = strlen(e->d_name); if (!(L > 12 && strcmp(e->d_name + L - 12, ".safetensors") == 0)) continue;
        char sp[2048]; snprintf(sp, sizeof sp, "%s/%s", path, e->d_name);
        LlmCodecStTensorInfo* ti = NULL; int nt = 0; uint64_t base = 0;
        if (llmcodec_safetensors_read(sp, &ti, &nt, &base) != 0) continue;
        for (int i = 0; i < nt && n < cap; i++) {
            LlmModTensor* t = &m->tensors[n++];
            snprintf(t->name, sizeof t->name, "%s", ti[i].name);
            t->ndim = ti[i].ndim; for (int d = 0; d < t->ndim && d < 8; d++) t->dims[d] = ti[i].shape[d];
            t->bytes = (ti[i].end > ti[i].begin) ? (ti[i].end - ti[i].begin) : 0; m->weights_bytes += t->bytes;
        }
        llmcodec_safetensors_info_free(ti);
    }
    closedir(dp);
    m->n_tensors = n; m->is_gguf = 0; m->ok = 1; return 0;
}
static void llmmod_free(LlmModInfo* m) { if (m->tensors) free(m->tensors); m->tensors = NULL; m->n_tensors = 0; }
/* Peso KV/token dai campi geometrici comuni. */
static double llmmod_kv_per_token(const LlmModInfo* m) {
    if (m->block_count > 0 && m->head_count_kv > 0) {
        long kd = m->key_length > 0 ? m->key_length : ((m->embedding_length > 0 && m->head_count > 0) ? m->embedding_length / m->head_count : -1);
        long vd = m->value_length > 0 ? m->value_length : kd;
        if (kd > 0 && vd > 0) return (double)m->block_count * (double)m->head_count_kv * (double)(kd + vd) * 2.0;
    }
    return -1.0;
}

static int llmmodelio_model_ir_profile(const char* path, int budget_gb) {
    LlmModInfo mi; if (llmmod_load(path, &mi) != 0) return -1;   /* errore = codice di ritorno, niente diagnostica in out/stderr */
    unsigned long long cat[8] = {0}; unsigned long long wb = mi.weights_bytes;
    for (uint64_t i = 0; i < mi.n_tensors; i++) {
        const char* r = llmmo_role_cat(mi.tensors[i].name);
        uint64_t b = mi.tensors[i].bytes;
        int c = (strcmp(r,"EMBEDDING")==0)?0:(strncmp(r,"ATTN",4)==0)?1:((strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0)?2:(strcmp(r,"ROUTER")==0)?3:(strncmp(r,"FFN",3)==0)?4:(strstr(r,"NORM")!=NULL)?5:(strcmp(r,"OUTPUT")==0)?6:7);
        cat[c] += b;
    }
    double kvpt = llmmod_kv_per_token(&mi);
    llmmod_free(&mi);
    double budget = (budget_gb > 0) ? (double)budget_gb*1024.0*1024.0*1024.0 : 90.0*1024.0*1024.0*1024.0;
    const char* cn[8]={"embedding","attention","experts","router","ffn_dense","norm","output","other"};
    fprintf(LOUT, "# LLM TENSOR PROFILE '%s' arch=%s weights=%llu (%.2f GiB)\n", path, g_arch_ok?g_arch.arch:"-", wb, (double)wb/(1024.0*1024.0*1024.0));
    for (int c=0;c<8;c++) if (cat[c]) fprintf(LOUT, "  %-10s %.2f GiB (%.1f%%)\n", cn[c], (double)cat[c]/(1024.0*1024.0*1024.0), 100.0*(double)cat[c]/(double)(wb?wb:1));
    if (kvpt >= 0) fprintf(LOUT, "  kv/token=%.0f bytes\n", kvpt);
    int fits = ((double)wb <= budget);
    fprintf(LOUT, "  RESIDENCY: budget=%.0f GiB -> %s%s\n", budget/(1024.0*1024.0*1024.0), fits ? "RESIDENT (tutto in RAM)" : "SWAP (streaming out-of-core)",
            (!fits && cat[2]) ? " — esperti candidate al demote/streaming" : "");
    return 0;
}
/* DIST (§4.6, tensor-parallel planner) + CED (§4.8, computation elimination) — report-only. */
static int llmmodelio_model_ir_dist(const char* path, int world) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR DIST: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    if (world <= 0) world = 1;
    unsigned long long expb = 0, attnb = 0, otherb = 0; long nexp = -1, nh = -1, nkvh = -1;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        const char* r = llmmo_role_cat(fi.tensors[i].name);
        uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
        uint64_t b = 0; (b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0);
        if (strcmp(r,"ROUTED_EXPERT")==0) expb += b; else if (strncmp(r,"ATTN",4)==0) attnb += b; else otherb += b;
    }
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"expert_count"))nexp=v; else if(strstr(k,"attention.head_count_kv"))nkvh=v; else if(strstr(k,"attention.head_count"))nh=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    fprintf(LOUT, "# LLM TENSOR DIST '%s' world_size=%d\n", path, world);
    fprintf(LOUT, "  expert-parallel (EP): experts=%ld -> %ld/rank (byte/rank=%.2f GiB)\n", nexp>0?nexp:0, nexp>0?nexp/world:0, (double)expb/(double)world/(1024.0*1024.0*1024.0));
    fprintf(LOUT, "  tensor-parallel (TP): attn heads=%ld kv=%ld -> %ld/%ld head/rank (byte/rank=%.2f GiB)\n", nh, nkvh, (nh>0?nh/world:0), (nkvh>0?nkvh/world:0), (double)attnb/(double)world/(1024.0*1024.0*1024.0));
    fprintf(LOUT, "  replicated (other): %.2f GiB\n  nota: EP=esperti, TP=teste/KV; all-reduce su residual (TP).\n", (double)otherb/(1024.0*1024.0*1024.0));
    return 0;
}
static int llmmodelio_model_ir_ced(const char* path, const char* gate) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR CED: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    /* eliminazione model-aware + GATE/kill-switch (§4.8): tensori ALL-ZERO (candidati ELIMINA).
     * Scan di TUTTI i tensori (anche i giganti), CAMPIONATO start/mid/end per non leggere 475GB.
     * `gate`: ON/1/ENABLE abilita la proposta (applicabile a compile/prune); default OFF = report-only. */
    int enabled = gate && (strcasecmp(gate,"ON")==0 || strcmp(gate,"1")==0 || strcasecmp(gate,"ENABLE")==0
                           || strncasecmp(gate,"ON:",3)==0 || strncasecmp(gate,"ENABLE:",7)==0);
    /* eps (§4.8): "ON:<eps>" (o "1:<eps>") = anche la prova BOUNDED (max|w| < eps) oltre all'esatta. */
    float eps = 0.0f; { const char* c = gate ? strchr(gate, ':') : NULL; if (c) eps = (float)atof(c+1); }
    int allzero = 0, checked = 0, elim_layers = 0, bounded = 0;
    unsigned long long total_bytes = 0, elim_bytes = 0, bound_bytes = 0;
    allzero = llmmodelio_ced_scan(path, &fi, &elim_bytes, &checked, &total_bytes, &elim_layers, 1, eps, &bounded, &bound_bytes);
    /* SET del kill-switch: COMPILE lo applica per QUESTO path (elimina gli all-zero). */
    if (enabled) { s_ced_on = 1; snprintf(s_ced_path, sizeof s_ced_path, "%s", path); }
    else { s_ced_on = 0; s_ced_path[0] = 0; }
    double pc = total_bytes ? (100.0 * (double)elim_bytes / (double)total_bytes) : 0.0;
    fprintf(LOUT, "# LLM TENSOR CED '%s' arch=%s: tensori=%d (scan campionato) all-zero=%d\n",
            path, g_arch_ok?g_arch.arch:"-", checked, allzero);
    fprintf(LOUT, "  elimination plan (ESATTA): %d tensori / %d layer, %llu byte su %llu = %.2f%% del modello\n",
            allzero, elim_layers, (unsigned long long)elim_bytes, (unsigned long long)total_bytes, pc);
    if (eps > 0.0f) fprintf(LOUT, "  elimination plan (BOUNDED, maxw<%.1e): %d tensori, %llu byte (prova: norma_inf < eps -> errore limitato)\n",
            (double)eps, bounded, (unsigned long long)bound_bytes);
    fprintf(LOUT, "  GATE/kill-switch: %s (default OFF = report-only; ON abilita l'applicazione a compile/prune)\n",
            enabled ? "ON — eliminazione ATTIVA (applica al .gmbllm/prune)" :
            (allzero > 0 ? "OFF — candidati presenti; richiede A/B + quality gate per abilitare" : "OFF — nessun candidato"));
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
/* SCHEDULE (§4.6, continuous batching plan) — report-only: max batch da budget KV. */
static int llmmodelio_model_ir_schedule(const char* path, int seqlen) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR SCHEDULE: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"attention.head_count_kv"))hkv=v; else if(strstr(k,"attention.head_count"))hd=v;
        else if(strstr(k,"attention.key_length"))kl=v; else if(strstr(k,"attention.value_length"))vl=v;
        else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1.0; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*(double)hkv*(double)(kd+vd)*2.0; }
    if (seqlen <= 0) seqlen = 2048;
    double budget = 90.0*1024.0*1024.0*1024.0 - 2048.0*1024.0*1024.0 - (double)wb;
    long maxbatch = (kvpt>0 && budget>0) ? (long)(budget / (kvpt*(double)seqlen)) : -1;
    fprintf(LOUT, "# LLM TENSOR SCHEDULE '%s' seqlen=%d weights=%.2f GiB\n", path, seqlen, (double)wb/(1024.0*1024.0*1024.0));
    if (kvpt>0) fprintf(LOUT, "  kv/seq=%d bytes (%.2f MiB)  max_batch(kv_bound)=%ld  -> CONTINUOUS BATCHING (paged KV, prefill/decode mixed)\n",
                        (int)(kvpt*seqlen), kvpt*(double)seqlen/(1024.0*1024.0), maxbatch);
    else fprintf(LOUT, "  max_batch: n/d (metadati KV assenti)\n");
    return 0;
}
/* LOADPLAN (§4.9): legge e VALIDA un artefatto .gmbllm (JSON) — artifact=gmbllm, version, arch,
 * layers. 0 ok, -1 invalido/assente. */
static const char* lj_v(const char* j, const char* key);
static void lj_s(const char* p, char* out, size_t cap);

static int llmmodelio_model_ir_loadplan(const char* path) {
    FILE* f = fopen(path, "rb"); if (f == NULL) return -1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (1 << 20)) { fclose(f); return -1; }
    char* j = (char*)malloc((size_t)n + 1); if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { free(j); fclose(f); return -1; } j[n] = 0; fclose(f);
    char art[32] = "", arch[96] = ""; int ver = -1;
    lj_s(lj_v(j, "artifact"), art, sizeof art); lj_s(lj_v(j, "arch"), arch, sizeof arch);
    { const char* v = lj_v(j, "version"); if (v) ver = atoi(v); }
    free(j);
    if (strcmp(art, "gmbllm") != 0) { fprintf(stderr, "LLM TENSOR LOADPLAN: non e' un artefatto gmbllm\n"); return -1; }
    if (ver != 1) { fprintf(stderr, "LLM TENSOR LOADPLAN: versione %d non supportata\n", ver); return -1; }
    fprintf(LOUT, "LLM TENSOR LOADPLAN '%s': artifact=gmbllm version=%d arch=%s -> VALIDO\n", path, ver, arch);
    return 0;
}
/* INSPECT (§4.9): mostra il CONTENUTO di un artefatto `.gmbllm` (a differenza di LOADPLAN che
 * si limita a validarlo). Report-only. Riusa i lettori JSON piatti (lj_v). */
static void lj_num(const char* p, char* out, size_t cap) {
    if (cap) out[0] = 0;
    if (!p) return; while (*p==' '||*p=='\t') p++;
    size_t i=0; while (*p && *p!=',' && *p!='}' && *p!='\n' && *p!=' ' && i<cap-1) out[i++]=*p++;
    out[i]=0;
}
static int llmmodelio_model_ir_inspect(const char* path) {
    FILE* f = fopen(path, "rb"); if (f == NULL) return -1;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if (n<=0 || n>(1<<20)) { fclose(f); return -1; }
    char* j=(char*)malloc((size_t)n+1); if(!j||fread(j,1,(size_t)n,f)!=(size_t)n){free(j);fclose(f);return -1;} j[n]=0; fclose(f);
    char art[32]="",arch[96]="",attn[64]="",moe[64]="",mod[64]="",spec[64]="",lay[32]="",wb[32]="",kvpt[32]="",kvb[32]="";
    lj_s(lj_v(j,"artifact"),art,sizeof art); lj_s(lj_v(j,"arch"),arch,sizeof arch);
    lj_s(lj_v(j,"attention"),attn,sizeof attn); lj_s(lj_v(j,"moe"),moe,sizeof moe);
    lj_s(lj_v(j,"modality"),mod,sizeof mod); lj_s(lj_v(j,"speculation"),spec,sizeof spec);
    lj_num(lj_v(j,"layers"),lay,sizeof lay); lj_num(lj_v(j,"weights_bytes"),wb,sizeof wb);
    lj_num(lj_v(j,"kv_per_token_bytes"),kvpt,sizeof kvpt); lj_num(lj_v(j,"kv_bound"),kvb,sizeof kvb);
    int nlop=0; { const char* p=j; while ((p=strstr(p,"\"layer\""))) { nlop++; p++; } }
    int nkern=0; { const char* p=j; while ((p=strstr(p,"\"matmul\""))) { nkern++; p++; } }
    free(j);
    if (strcmp(art,"gmbllm")!=0) { fprintf(stderr,"LLM TENSOR INSPECT: non e' un artefatto gmbllm\n"); return -1; }
    fprintf(LOUT, "# LLM TENSOR INSPECT '%s' (artefatto .gmbllm)\n", path);
    fprintf(LOUT, "  artifact=%s arch=%s layers=%s\n", art, arch[0]?arch:"-", lay[0]?lay:"-");
    fprintf(LOUT, "  attention=%s moe=%s modality=%s speculation=%s\n", attn[0]?attn:"-", moe[0]?moe:"-", mod[0]?mod:"-", spec[0]?spec:"-");
    fprintf(LOUT, "  memory_plan: weights_bytes=%s kv_per_token_bytes=%s\n", wb[0]?wb:"-", kvpt[0]?kvpt:"-");
    fprintf(LOUT, "  exact_capacity: kv_bound=%s\n", kvb[0]?kvb:"-");
    fprintf(LOUT, "  kernels: matmul=%s (+%d definizioni)\n", nkern?"affine_qmm":"-", nkern);
    fprintf(LOUT, "  layer_ops: %d layer con ops\n", nlop);
    return 0;
}
/* STATE (I7) + GRAPH EXPORT (§4.9) — report-only. */
static int llmmodelio_model_ir_state(const char* path) {
    size_t ln = strlen(path);
    if (1 /* read_meta gestisce anche le dir MLX */) { if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0; }
    else { if (llmmodelio_arch_resolve(path) != 0) return -1; }
    if (!g_arch_ok) return -1;
    int mtp = g_arch.mm_mtp || 0;
    fprintf(LOUT, "# LLM TENSOR STATE '%s' arch=%s\n  phase: prefill+decode (I7)\n  state=%s modality=%s speculation=%s kv.dtype=%s\n",
            path, g_arch.arch, g_arch.runtime_state[0]?g_arch.runtime_state:"(none)", llmdesc_modality(&g_arch),
            mtp?(g_arch.mm_dspark?"DSPARK(MTP)":"MTP"):"NONE", g_arch.runtime_kv_dtype[0]?g_arch.runtime_kv_dtype:"-");
    return 0;
}
static int llmmodelio_model_ir_gexport(const char* path, const char* out) {
    if (!out || !*out) { fprintf(stderr, "LLM TENSOR GEXPORT: atteso <path>, <out.dot>\n"); return -1; }
    int F[1024]; memset(F, 0, sizeof F); int maxl = -1; size_t ln = strlen(path);
    if (1 /* read_meta gestisce anche le dir MLX */) {
        if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
        LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
        if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) { const char* r = llmmo_role_cat(fi.tensors[i].name);
            int L = model_layer_of(fi.tensors[i].name); if (L < 0) L = 0; if (L > maxl) maxl = L; if (L > 1023) continue;
            if (strncmp(r,"ATTN",4)==0) F[L]|=1; else if (strstr(r,"EXPERT")||strcmp(r,"ROUTER")==0||strstr(fi.tensors[i].name,"exps")) F[L]|=2; else if (strncmp(r,"FFN",3)==0) F[L]|=4; else if (strstr(r,"NORM")) F[L]|=8; }
        llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    } else { if (llmmodelio_arch_resolve(path) != 0) return -1; }
    FILE* o = fopen(out, "w"); if (!o) return -1;
    fprintf(o, "digraph model { rankdir=LR; node [shape=box];\n");
    for (int L = 0; L <= maxl && L < 1024; L++) { if (!F[L]) continue;
        fprintf(o, "  L%d[shape=record,label=\"layer %d\"];\n", L, L);
        if (F[L]&1) fprintf(o, "  L%d -> A%d[label=\"attn\"]; A%d[label=\"attention\"];\n", L, L, L);
        if (F[L]&4) fprintf(o, "  L%d -> F%d[label=\"ffn\"]; F%d[label=\"ffn\"];\n", L, L, L);
        if (F[L]&2) fprintf(o, "  L%d -> M%d[label=\"moe\"]; M%d[label=\"moe\"];\n", L, L, L);
        if (L+1 <= maxl && F[L+1]) fprintf(o, "  L%d -> L%d;\n", L, L+1);
    }
    fprintf(o, "}\n"); fclose(o);
    fprintf(LOUT, "LLM TENSOR GEXPORT '%s' -> '%s' (DOT, layers=%d)\n", path, out, maxl+1);
    return 0;
}
/* MOBJ (M10, MEMORY OBJECT IR): oggetti di memoria per regione (weights/kv/activation) con
 * device, lifetime, transfer_cost. Report-only. */
static int llmmodelio_model_ir_mobj(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR MOBJ: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    long hkv = -1, kd = -1, vd = -1, en = -1, hd = -1, nl = -1;
    for (int i = 0; i < nkv; i++) { const char* k = kv[i].key; long v = (long)kv[i].u32;
        if (strstr(k,"head_count_kv")) hkv=v; else if (strstr(k,"key_length")) kd=v; else if (strstr(k,"value_length")) vd=v;
        else if (strstr(k,"embedding_length")) en=v; else if (strstr(k,"head_count")) hd=v; else if (strstr(k,"block_count")) nl=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1; if(nl>0&&hkv>0){ long kd2=kd>0?kd:((en>0&&hd>0)?en/hd:-1); long vd2=vd>0?vd:kd2; if(kd2>0&&vd2>0) kvpt=(double)nl*hkv*(kd2+vd2)*2.0; }
    fprintf(LOUT, "# LLM TENSOR MOBJ '%s' (M10 memory objects)\n", path);
    fprintf(LOUT, "  {region=weights,  bytes=%llu, device=gpu, lifetime=model, transfer_cost=one-time(mmap)}\n", wb);
    if (kvpt>=0) fprintf(LOUT, "  {region=kv,        bytes_per_token=%.0f, device=gpu, lifetime=session, transfer_cost=0}\n", kvpt);
    fprintf(LOUT, "  {region=activation, device=gpu, lifetime=step, transfer_cost=0}\n");
    fprintf(LOUT, "  {region=expert_workset, policy=residency, lifecycle=demote/evict LRU}\n");
    return 0;
}
/* EPLAN (§4.6): assegnazione esperti->rank (round-robin) + conteggio per rank. Report-only. */
static int llmmodelio_model_ir_eplan(const char* path, int world) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR EPLAN: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    long nexp = -1;
    for (int i = 0; i < nkv; i++) if (strstr(kv[i].key, "expert_count")) nexp = (long)kv[i].u32;
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    if (world <= 0) world = 1;
    if (nexp <= 0) { fprintf(LOUT, "# LLM TENSOR EPLAN '%s': n/d experts\n", path); return 0; }
    fprintf(LOUT, "# LLM TENSOR EPLAN '%s' experts=%ld world=%d\n", path, nexp, world);
    int* cnt = (int*)calloc((size_t)world, sizeof(int));
    for (long e = 0; e < nexp; e++) cnt[e % world]++;
    for (int r = 0; r < world; r++) fprintf(LOUT, "  rank %d: %d esperti\n", r, cnt[r]);
    free(cnt);
    return 0;
}
/* CONTEXT (§4.10): budget di contesto (max token da KV) + trim/GC. Report-only. */
/* CONTEXT GC (§4.10): piano di eviction del prefisso KV dato il contesto corrente e il target.
 * spec = "cur:target" (token) oppure vuoto (solo policy). */
/* CONTEXT MAP (§4.10): mappa del contesto (segmenti + budget token + KV) + strategia COMMIT. */
/* EXACT CAPACITY + STATIC MEMORY PLAN / arena (§4.9): capacità esatta a ctx + regioni arena. */
static int llmmodelio_model_ir_capacity(const char* path, const char* spec) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR CAPACITY: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0; long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    long ctx = (spec && *spec) ? atol(spec) : 8192; if (ctx <= 0) ctx = 8192;
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    double kvctx = kvpt>0 ? kvpt*(double)ctx : -1.0;
    double scratch = (en>0) ? (double)en*ctx*8.0 : 0.0;   /* attivazioni/attention scores stimate: en*ctx*8 B */
    double total = (double)wb + (kvctx>0?kvctx:0.0) + scratch;
    fprintf(LOUT, "# LLM TENSOR CAPACITY '%s' arch=%s ctx=%ld (§4.9 EXACT CAPACITY + STATIC MEMORY PLAN)\n", path, g_arch_ok?g_arch.arch:"-", ctx);
    fprintf(LOUT, "  arena[0] weights  = %.1f MiB (region=model, lifetime=process, align=page)\n", wb/(1024.0*1024.0));
    fprintf(LOUT, "  arena[1] kv       = %.1f MiB (region=session, lifetime=session, kv/token=%.0f B)\n", kvctx/(1024.0*1024.0), kvpt);
    fprintf(LOUT, "  arena[2] scratch  = %.1f MiB (region=step, lifetime=step, riuso; O(act+ctx))\n", scratch/(1024.0*1024.0));
    fprintf(LOUT, "  EXACT CAPACITY @ctx = %.1f MiB (weights + kv + scratch)\n", total/(1024.0*1024.0));
    fprintf(LOUT, "  fit: serve ~%.1f GiB di RAM (device=unified); vedi MPLAN per RESIDENT vs SWAP/SSD\n", total/(1024.0*1024.0*1024.0));
    return 0;
}
static int llmmodelio_model_ir_contextmap(const char* path, const char* spec) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR CONTEXTMAP: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    long ctx = (spec && *spec) ? atol(spec) : 8192; if (ctx <= 0) ctx = 8192;
    fprintf(LOUT, "# LLM TENSOR CONTEXTMAP '%s' arch=%s ctx=%ld (§4.10 context/semantic, lato modello)\n", path, g_arch_ok?g_arch.arch:"-", ctx);
    fprintf(LOUT, "# segmento\ttoken_budget\truolo\tKv\n");
    long system = ctx/16, tools = ctx/8, history = ctx - system - tools; if (history < 0) history = 0;
    fprintf(LOUT, "SYSTEM\t%ld\tistruzioni (immutabili, prefix-cache hit)\tKV_prefix\n", system);
    fprintf(LOUT, "TOOL\t%ld\trisultati tool (evictable, non-prefisso)\tKV_transient\n", tools);
    fprintf(LOUT, "HISTORY\t%ld\tturni utente/assistant (rolling window + GC)\tKV_rolling\n", history);
    fprintf(LOUT, "  kv/token=%.0f B  kv@ctx=%.1f MiB  kv@system=%.1f MiB\n", kvpt, kvpt>0?kvpt*ctx/(1024.0*1024.0):-1, kvpt>0?kvpt*system/(1024.0*1024.0):-1);
    fprintf(LOUT, "CONTEXT_COMMIT: commit del prefisso (SYSTEM+TOOL+parte HISTORY) a ogni turno oppure ogni %ld token;\n", ctx/8);
    fprintf(LOUT, "  prefix-cache SHARE su SYSTEM (hit -> salta PREFILL); TRIM/GC su HISTORY (CONTEXTGC); MAP<->KV pages (KVPLAN).\n");
    fprintf(LOUT, "CONTEXT_TRIM: mantieni SYSTEM + ultimi N turni; CONTEXT_GC: evict TOOL + HISTORY vecchia (vedi CONTEXTGC).\n");
    return 0;
}
static int llmmodelio_model_ir_contextgc(const char* path, const char* spec) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR CONTEXTGC: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    long cur=-1,tgt=-1; if (spec && *spec) { const char* c=strchr(spec,':'); if (c) { cur=atol(spec); tgt=atol(c+1); } else cur=atol(spec); }
    fprintf(LOUT, "# LLM TENSOR CONTEXTGC '%s' arch=%s kv/token=%.0f B\n", path, g_arch_ok?g_arch.arch:"-", kvpt);
    fprintf(LOUT, "# politiche: TRIM (drop prefisso piu' vecchio) | GC (evict prefix-cache) | PAGED (kv paginata) | PRESSURE (evict su soglia)\n");
    if (cur > 0 && tgt > 0 && cur > tgt && kvpt > 0) {
        long ev = cur - tgt; double freed = kvpt*(double)ev, keep = kvpt*(double)tgt;
        fprintf(LOUT, "  piano: cur=%ld tgt=%ld -> evict=%ld token (%.1f MiB), keep=%.1f MiB\n  impatto: prefisso %ld token perso (prefix-cache invalidata fino a %ld); ricalcolo al prossimo prompt lungo\n",
                cur, tgt, ev, freed/(1024.0*1024.0), keep/(1024.0*1024.0), ev, tgt);
        fprintf(LOUT, "  azione: GC(prefix) + TRIM -> mantieni coda %ld token (recenti, attenzione locale intatta)\n", tgt);
    } else if (cur > 0) {
        fprintf(LOUT, "  contesto corrente %ld token = %.1f MiB (nessun target -> nessuna eviction; usare <cur>:<target>)\n", cur, kvpt>0?kvpt*cur/(1024.0*1024.0):-1);
    }
    return 0;
}
static int llmmodelio_model_ir_context(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR CONTEXT: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    long maxtok = (kvpt>0) ? (long)((90.0*1024.0*1024.0*1024.0 - 2048.0*1024.0*1024.0 - (double)wb) / kvpt) : -1;
    fprintf(LOUT, "# LLM TENSOR CONTEXT '%s' arch=%s\n  kv/token=%.0f bytes  max_tokens(kv_bound)=%ld\n", path, g_arch_ok?g_arch.arch:"-", kvpt, maxtok);
    fprintf(LOUT, "  policy: TRIM (drop oldest prefix) | GC (evict prefix-cache) | PAGED (kv pages)\n");
    return 0;
}
/* QUALITY (I6, gate NLL/perplexity): carica il modello col motore MLX (dlopen, dipendenza
 * debole), tokenizza (llmtoken), punteggia token-per-token con logprob_of -> NLL/perplexity.
 * Nessun link statico MLX (una sola istanza; i backend C restano snelli). */
static int llm_quality_nll(const char* model_dir, const char* prompt, double* nll_mean, double* ppl, int* tokens);
/* Engine MLX robusto: prova le dylib note (`libgmb_ai_mlx`/`libllm_ai_mlx`) e i prefissi dei simboli
 * (`gmb_ai_mlx_`/`llm_ai_mlx_`), scegliendo quella che espone il set COMPLETO (incl.
 * `session_logprob_of` — la dylib `libgmb_ai_mlx` deployata ne e' priva). Ritorna l'handle o NULL;
 * `*out_prefix` = prefisso dei simboli della dylib scelta. */
static void* llm_mlx_engine_open(const char** out_prefix) {
    /* `@loader_path` = la dir della dylib che chiama (libgmb_ai_llm.dylib in Resources/libs/...) ->
     * trova l'engine deployato accanto senza dipendere dal loader path del processo. */
    static const char* const libs[] = {
        "@loader_path/libgmb_ai_mlx.dylib", "@loader_path/libllm_ai_mlx.dylib",
        "libgmb_ai_mlx.dylib", "libllm_ai_mlx.dylib" };
    static const char* const pfs[]  = { "gmb_ai_mlx_", "llm_ai_mlx_" };
    for (size_t li = 0; li < sizeof(libs) / sizeof(libs[0]); li++) {
        void* h = dlopen(libs[li], RTLD_NOW | RTLD_LOCAL);
        if (h == NULL) continue;
        for (int pi = 0; pi < 2; pi++) {
            char nm[64];
            snprintf(nm, sizeof nm, "%sload_model", pfs[pi]);        void* a = dlsym(h, nm);
            snprintf(nm, sizeof nm, "%screate_session", pfs[pi]);    void* b = dlsym(h, nm);
            snprintf(nm, sizeof nm, "%ssession_append", pfs[pi]);    void* c = dlsym(h, nm);
            snprintf(nm, sizeof nm, "%ssession_logprob_of", pfs[pi]); void* d = dlsym(h, nm);
            if (a != NULL && b != NULL && c != NULL && d != NULL) { if (out_prefix) *out_prefix = pfs[pi]; return h; }
        }
        dlclose(h);
    }
    return NULL;
}
static int llm_quality_nll(const char* model_dir, const char* prompt, double* out_nll_mean, double* out_ppl, int* out_tokens) {
    const char* PF = "llm_ai_mlx_";
    void* h = llm_mlx_engine_open(&PF);
    if (h == NULL) { fprintf(stderr, "LLM QUALITY: engine MLX non caricabile (libgmb_ai_mlx/libllm_ai_mlx)\n"); return -1; }
    typedef void* (*load_fn)(const char*); typedef void* (*sess_fn)(void*);
    typedef int (*app_fn)(void*, const int32_t*, int); typedef int (*lp_fn)(void*, int32_t, float*);
    typedef void (*free_fn)(void*); typedef void (*dsess_fn)(void*);
    char nm[64];
    snprintf(nm, sizeof nm, "%sload_model", PF);           load_fn L = (load_fn)dlsym(h, nm);
    snprintf(nm, sizeof nm, "%screate_session", PF);       sess_fn C = (sess_fn)dlsym(h, nm);
    snprintf(nm, sizeof nm, "%ssession_append", PF);       app_fn  A = (app_fn)dlsym(h, nm);
    snprintf(nm, sizeof nm, "%ssession_logprob_of", PF);   lp_fn   LP = (lp_fn)dlsym(h, nm);
    snprintf(nm, sizeof nm, "%sfree_model", PF);           free_fn FM = (free_fn)dlsym(h, nm);
    snprintf(nm, sizeof nm, "%sdestroy_session", PF);      dsess_fn DS = (dsess_fn)dlsym(h, nm);
    if (L == NULL || C == NULL || A == NULL || LP == NULL) { fprintf(stderr, "LLM QUALITY: simboli engine assenti\n"); return -1; }
    /* Tokenizer: dir MLX/HF -> <dir>/tokenizer.json; GGUF -> ricostruito dai token/merges
     * EMBEDDED nel GGUF (l'engine MLX lo espone come tokenizer.json; oppure il `tokenizer.gmb.json`
     * verbatim conservato dal converter). Cosi' l'engine Metal misura la PPL di un .gguf. */
    llmtoken_handle th = NULL;
    { size_t ml = strlen(model_dir);
      if (ml > 5 && strcasecmp(model_dir + ml - 5, ".gguf") == 0) {
          typedef char* (*tokfn)(const char*, size_t*);
          typedef void  (*freefn)(char*);
          char tjn[64]; snprintf(tjn, sizeof tjn, "%sgguf_tokenizer_json", PF);
          tokfn TJ = (tokfn)dlsym(h, tjn);
          if (TJ) {
              size_t jl = 0; char* js = TJ(model_dir, &jl);
              if (js != NULL) {
                  th = llmtoken_load_json(js, jl);
                  char fsn[64]; snprintf(fsn, sizeof fsn, "%sfree_str", PF);
                  freefn FS = (freefn)dlsym(h, fsn);
                  if (FS) FS(js); else free(js);
              }
          }
          if (th == NULL) { fprintf(stderr, "LLM QUALITY: tokenizer dal GGUF non costruibile\n"); return -1; }
      } else {
          th = llmmodelio_tokenizer_open(model_dir);
          if (th == NULL) { fprintf(stderr, "LLM QUALITY: tokenizer assente\n"); return -1; }
      }
    }
    int32_t ids[2048]; int n = llmtoken_encode(th, prompt, ids, 2048);
    if (n <= 1) { llmtoken_free(th); fprintf(stderr, "LLM QUALITY: prompt troppo corto\n"); return -1; }
    void* m = L(model_dir); if (m == NULL) { llmtoken_free(th); fprintf(stderr, "LLM QUALITY: load modello fallito\n"); return -1; }
    void* s = C(m); if (s == NULL) { if (FM) FM(m); llmtoken_free(th); return -1; }
    double nll = 0.0; int cnt = 0;
    for (int k = 0; k + 1 < n; k++) {
        if (A(s, &ids[k], 1) != 0) break;
        float lp = 0.0f; if (LP(s, ids[k + 1], &lp) != 0) break;
        nll += -(double)lp; cnt++;
    }
    double nll_mean = cnt > 0 ? nll / (double)cnt : -1.0;
    double ppl = nll_mean >= 0 ? exp(nll_mean) : -1.0;
    if (DS) DS(s); if (FM) FM(m); llmtoken_free(th);
    if (out_nll_mean) *out_nll_mean = nll_mean; if (out_ppl) *out_ppl = ppl; if (out_tokens) *out_tokens = n;
    return cnt > 0 ? 0 : -1;
}
/* ============================================================================
 * S6 — BENCHMARK D'USO (MixBench-style). Se la PPL di un corpus non premia, l'obiettivo giusto è un
 * mix di metriche d'USO (comprensione, istruzioni, codice, italiano). Formato del file benchmark,
 * una riga per item (TAB-separati):
 *     <dominio> \t <prompt> \t <completamento BUONO> \t <completamento CATTIVO>
 * Score per item = 1 se logp(buono|prompt) > logp(cattivo|prompt) (ranking), 0 altrimenti.
 * Accuratezza per dominio; composito = **MIN** fra i domini (il PEGGIORE, coerente con S4: robustezza
 * sui domini, non media). Usa lo stesso motore MLX della PPL (dlopen) + llmtoken. 0 ok / -1.
 * ============================================================================ */
static double llm_usage_logp(void* h, void* m, llmtoken_handle th, const char* prompt, const char* cont) {
    typedef void* (*sess_fn)(void*); typedef int (*app_fn)(void*, const int32_t*, int);
    typedef int (*lp_fn)(void*, int32_t, float*); typedef void (*dsess_fn)(void*);
    sess_fn C = (sess_fn)dlsym(h, "llm_ai_mlx_create_session");
    app_fn  A = (app_fn)dlsym(h, "llm_ai_mlx_session_append");
    lp_fn   LP = (lp_fn)dlsym(h, "llm_ai_mlx_session_logprob_of");
    dsess_fn DS = (dsess_fn)dlsym(h, "llm_ai_mlx_destroy_session");
    if (C == NULL || A == NULL || LP == NULL) return 0.0;
    int32_t pi[1024], ci[1024];
    int np = llmtoken_encode(th, prompt, pi, 1024);
    int nc = llmtoken_encode(th, cont, ci, 1024);
    if (np < 0 || nc <= 0) return 0.0;
    void* s = C(m); if (s == NULL) return 0.0;
    double lp = 0.0;
    for (int k = 0; k < np; k++) if (A(s, &pi[k], 1) != 0) { if (DS) DS(s); return 0.0; }
    for (int k = 0; k < nc; k++) { float v = 0.0f; if (LP(s, ci[k], &v) != 0) break; lp += v; if (A(s, &ci[k], 1) != 0) break; }
    if (DS) DS(s);
    return lp;
}

static int llm_usage_eval(const char* model_dir, const char* bench, char* out, size_t cap, double* out_worst) {
    if (out_worst) *out_worst = -1.0;
    { struct stat _st; if (stat(model_dir, &_st) != 0) return -1; }   /* pre-check: modello assente -> errore pulito senza spurie su stderr */
    void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) { fprintf(stderr, "LLM USAGEEVAL: engine MLX non caricabile\n"); return -1; }
    typedef void* (*load_fn)(const char*); typedef char* (*tokfn)(const char*, size_t*);
    typedef void (*free_fn)(void*); typedef void (*freefn)(char*);
    load_fn L = (load_fn)dlsym(h, "llm_ai_mlx_load_model");
    if (L == NULL) { fprintf(stderr, "LLM USAGEEVAL: load_model assente\n"); return -1; }
    llmtoken_handle th = NULL;
    { size_t ml = strlen(model_dir);
      if (ml > 5 && strcasecmp(model_dir + ml - 5, ".gguf") == 0) {
          tokfn TJ = (tokfn)dlsym(h, "llm_ai_mlx_gguf_tokenizer_json");
          if (TJ) { size_t jl = 0; char* js = TJ(model_dir, &jl);
              if (js) { th = llmtoken_load_json(js, jl); freefn FS = (freefn)dlsym(h, "llm_ai_mlx_free_str"); if (FS) FS(js); else free(js); } }
          if (th == NULL) { fprintf(stderr, "LLM USAGEEVAL: tokenizer dal GGUF non costruibile\n"); return -1; }
      } else {
          th = llmmodelio_tokenizer_open(model_dir);
          if (th == NULL) { fprintf(stderr, "LLM USAGEEVAL: tokenizer assente\n"); return -1; }
      } }
    FILE* bf = fopen(bench, "r");
    if (bf == NULL) { llmtoken_free(th); fprintf(stderr, "LLM USAGEEVAL: benchmark non leggibile: %s\n", bench); return -1; }
    void* m = L(model_dir);
    if (m == NULL) { fclose(bf); llmtoken_free(th); fprintf(stderr, "LLM USAGEEVAL: load modello fallito\n"); return -1; }
    struct { char dom[64]; int tot, ok; } D[16]; int nd = 0;
    char line[8192]; int items = 0;
    while (fgets(line, sizeof line, bf)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        char* dom = line; char* p1 = strchr(line, '\t'); if (!p1) continue; *p1++ = 0;
        char* good = strchr(p1, '\t'); if (!good) continue; *good++ = 0;
        char* bad = strchr(good, '\t'); if (!bad) continue; *bad++ = 0;
        double lg = llm_usage_logp(h, m, th, p1, good);
        double lb = llm_usage_logp(h, m, th, p1, bad);
        int di = -1; for (int i = 0; i < nd; i++) if (strcmp(D[i].dom, dom) == 0) { di = i; break; }
        if (di < 0 && nd < 16) { di = nd++; memset(&D[di], 0, sizeof D[di]); snprintf(D[di].dom, sizeof D[di].dom, "%s", dom); }
        if (di >= 0) { D[di].tot++; if (lg > lb) D[di].ok++; }
        items++;
    }
    fclose(bf);
    { free_fn FM = (free_fn)dlsym(h, "llm_ai_mlx_free_model"); if (FM) FM(m); }
    llmtoken_free(th);
    if (items == 0) { snprintf(out, cap, "USAGE items=0 (benchmark vuoto)"); return -1; }
    double worst = 1e30, mean = 0; size_t o = 0;
    o += (size_t)snprintf(out + o, cap - o, "USAGE items=%d domains=%d", items, nd);
    for (int i = 0; i < nd; i++) {
        double acc = D[i].tot > 0 ? (double)D[i].ok / D[i].tot : 0.0;
        o += (size_t)snprintf(out + o, cap - o, " %s=%.2f", D[i].dom, acc);
        if (acc < worst) worst = acc; mean += acc;
    }
    if (nd > 0) mean /= (double)nd;
    if (worst > 1e29) worst = 0;
    if (out_worst) *out_worst = worst;
    o += (size_t)snprintf(out + o, cap - o, " | mean=%.3f WORST=%.3f", mean, worst);
    return 0;
}

/* 6.1/S6 — USAGEGATE: AUTO vs ricetta baseline misurati sulla metrica d'USO (non la PPL). Verdetto
 * WORST(auto) > WORST(baseline) (l'auto NON deve peggiorare il dominio peggiore). 0/-1. */
static int llm_usage_gate(const char* model, const char* bench, double bpw) {
    if (model == NULL || !*model || bench == NULL || !*bench) return -1;
    struct stat _st; if (stat(model, &_st) != 0) return -1;
    const char* ta = "/tmp/gmb_usagate_auto.gguf";
    const char* tb = "/tmp/gmb_usagate_base.gguf";
    if (llmmodelio_model_quantize_auto(model, ta, bpw) != 0) return -1;
    if (llmmodelio_model_quantize(model, tb) != 0) { remove(ta); return -1; }
    char ra[512], rb[512]; double wa = -1, wb = -1;
    int okA = llm_usage_eval(ta, bench, ra, sizeof ra, &wa);
    int okB = llm_usage_eval(tb, bench, rb, sizeof rb, &wb);
    if (okA != 0 || okB != 0) { remove(ta); remove(tb); return -1; }
    fprintf(LOUT, "LLM TENSOR USAGEGATE '%s' target=%.3f bpw:\n  AUTO     %s\n  BASELINE %s\n",
            model, bpw, ra, rb);
    fprintf(LOUT, "  -> %s (WORST auto %s baseline)\n",
            (wa > wb) ? "OK: AUTO > baseline" : "NO: AUTO <= baseline",
            (wa > wb) ? ">" : "<=");
    remove(ta); remove(tb);
    return 0;
}

/* Wrapper: NLL del modello; se ref_model dato -> GATE (REJECT-FLAG se ppl peggiore > 2%). */
/* NLL/perplexity di un modello: se `prompt` è un FILE, è un DATASET (ogni riga non vuota è un
 * prompt) e si riporta la NLL/perplexity MEDIA; altrimenti è un prompt singolo. 0/-1.
 * §4: TRAIN EVAL <model>, <dataset>. */
static int llm_quality_nll_auto(const char* model_dir, const char* prompt, double* onll, double* oppl, int* otok) {
    struct stat st;
    if (prompt != NULL && stat(prompt, &st) == 0 && S_ISREG(st.st_mode)) {
        FILE* ds = fopen(prompt, "r");
        if (ds == NULL) return -1;
        char line[4096]; double snll = 0; long cnt = 0; int stok = 0;
        while (fgets(line, sizeof line, ds)) {
            size_t l = strlen(line);
            while (l && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
            if (l == 0) continue;
            double n2 = -1, p2 = -1; int t2 = 0;
            if (llm_quality_nll(model_dir, line, &n2, &p2, &t2) == 0) { snll += n2; stok += t2; cnt++; }
        }
        fclose(ds);
        if (cnt == 0) return -1;
        double mean = snll / (double)cnt;
        if (onll) *onll = mean; if (oppl) *oppl = mean >= 0 ? exp(mean) : -1.0; if (otok) *otok = stok;
        return 0;
    }
    return llm_quality_nll(model_dir, prompt, onll, oppl, otok);
}

int llmmodelio_model_quality(const char* model_dir, const char* prompt, const char* out_path, const char* ref_model) {
    if (model_dir == NULL || prompt == NULL || out_path == NULL) return -1;
    double nll = -1, ppl = -1; int tok = 0;
    if (llm_quality_nll_auto(model_dir, prompt, &nll, &ppl, &tok) != 0) return -1;
    FILE* o = fopen(out_path, "w");   /* non fatale: il valore NLL resta valido anche senza report */
    if (ref_model != NULL && *ref_model != '\0') {
        double rn = -1, rp = -1; int rt = 0;
        if (llm_quality_nll_auto(ref_model, prompt, &rn, &rp, &rt) != 0) { fclose(o); return -1; }
        const char* verdict = (rp <= 0) ? "ACCEPT" : (ppl <= rp * 1.02 ? "ACCEPT" : "REJECT-FLAG");
        if (o != NULL) fprintf(o, "model=%s tokens=%d nll_mean=%.6f ppl=%.6f\nref=%s ppl=%.6f\nGATE: %s (ppl %s rif..)\n",
                model_dir, tok, nll, ppl, ref_model, rp, verdict, ppl <= rp ? "<=" : ">");
        fprintf(LOUT, "LLM QUALITY '%s' ppl=%.4f vs ref '%s' ppl=%.4f -> GATE %s\n", model_dir, ppl, ref_model, rp, verdict);
    } else {
        if (o != NULL) fprintf(o, "model=%s tokens=%d nll_mean=%.6f ppl=%.6f\n", model_dir, tok, nll, ppl);
        fprintf(LOUT, "LLM QUALITY '%s': tokens=%d nll_mean=%.4f ppl=%.4f\n", model_dir, tok, nll, ppl);
    }
    if (o != NULL) fclose(o);
    return 0;
}
/* PPL "nuda" (senza report): wrapper di llm_quality_nll_auto per il registry (REGISTRY MEASURE). */
int llmmodelio_model_ppl(const char* model_dir, const char* prompt, double* out_ppl, int* out_tokens) {
    if (model_dir == NULL || prompt == NULL) return -1;
    double nll = -1, ppl = -1; int tok = 0;
    if (llm_quality_nll_auto(model_dir, prompt, &nll, &ppl, &tok) != 0) return -1;
    if (out_ppl) *out_ppl = ppl;
    if (out_tokens) *out_tokens = tok;
    return 0;
}
int llmmodelio_model_compile(const char* path, const char* out_path) { int rc = llmmodelio_model_ir_compile(path, out_path); fflush(LOUT); return rc; }

/* TRAIN LORA SAVE <ckpt>, <out>: converte l'adapter RAW del fine-tune (<ckpt>_best_{A,B}.raw +
 * <ckpt>_best.meta) in un .safetensors standard (nomi ...<mod>.lora_A.weight [rank,in] /
 * ...<mod>.lora_B.weight [out,rank] — stessa convenzione letta da llmmodelio_model_adapter).
 * L'A raw è [in,rank], B raw [rank,out]: qui si trasponе. 0/-1. */
int llmmodelio_model_adapter_save(const char* ckpt_prefix, const char* out_path) {
    if (ckpt_prefix == NULL || out_path == NULL) return -1;
    char mp[1300]; snprintf(mp, sizeof mp, "%s_best.meta", ckpt_prefix);
    int rank = 0, layer = -1; char mod[64] = "";
    FILE* m = fopen(mp, "r");
    if (m != NULL) { char line[256];
        while (fgets(line, sizeof line, m)) {
            if (sscanf(line, "rank=%d", &rank) == 1) continue;
            if (sscanf(line, "layer=%d", &layer) == 1) continue;
            if (strncmp(line, "mod=", 4) == 0) sscanf(line + 4, "%63s", mod);
        }
        fclose(m);
    }
    if (rank <= 0) return -1;
    char ap[1300], bp[1300];
    snprintf(ap, sizeof ap, "%s_best_A.raw", ckpt_prefix);
    snprintf(bp, sizeof bp, "%s_best_B.raw", ckpt_prefix);
    FILE* af = fopen(ap, "rb"); FILE* bf = fopen(bp, "rb");
    if (af == NULL || bf == NULL) { if (af) fclose(af); if (bf) fclose(bf); return -1; }
    fseek(af, 0, SEEK_END); long asz = ftell(af); fseek(af, 0, SEEK_SET);
    fseek(bf, 0, SEEK_END); long bsz = ftell(bf); fseek(bf, 0, SEEK_SET);
    if (asz <= 0 || bsz <= 0 || asz % (4L * rank) != 0 || bsz % (4L * rank) != 0) { fclose(af); fclose(bf); return -1; }
    int in = (int)(asz / (4L * rank)), out = (int)(bsz / (4L * rank));
    float* A = (float*)malloc((size_t)asz); float* B = (float*)malloc((size_t)bsz);
    float* At = (float*)malloc((size_t)asz); float* Bt = (float*)malloc((size_t)bsz);
    if (A == NULL || B == NULL || At == NULL || Bt == NULL
        || fread(A, 1, (size_t)asz, af) != (size_t)asz || fread(B, 1, (size_t)bsz, bf) != (size_t)bsz) {
        free(A); free(B); free(At); free(Bt); fclose(af); fclose(bf); return -1;
    }
    fclose(af); fclose(bf);
    for (int i = 0; i < in; i++) for (int r = 0; r < rank; r++) At[r * in + i] = A[i * rank + r];
    for (int r = 0; r < rank; r++) for (int o = 0; o < out; o++) Bt[o * rank + r] = B[r * out + o];
    char na[300], nb[300];
    snprintf(na, sizeof na, "model.layers.%d.%s.lora_A.weight", layer, mod[0] ? mod : "v_proj");
    snprintf(nb, sizeof nb, "model.layers.%d.%s.lora_B.weight", layer, mod[0] ? mod : "v_proj");
    int sha[2] = { rank, in }, shb[2] = { out, rank };
    LlmCodecStTensor ts[2];
    ts[0].name = na; ts[0].dtype = "F32"; ts[0].shape = sha; ts[0].ndim = 2; ts[0].data = At; ts[0]._offset = 0;
    ts[1].name = nb; ts[1].dtype = "F32"; ts[1].shape = shb; ts[1].ndim = 2; ts[1].data = Bt; ts[1]._offset = 0;
    int rc = llmcodec_safetensors_write(out_path, ts, 2);
    free(A); free(B); free(At); free(Bt);
    return rc;
}

/* TRAIN LORA LOAD <adapter>, <out>: legge un .safetensors adapter e scrive l'adapter RAW
 * <out>_best_{A,B}.raw + <out>_best.meta (layer/mod dedotti dal nome del tensore). 0/-1. */
int llmmodelio_model_adapter_load(const char* adapter_path, const char* out_prefix) {
    if (adapter_path == NULL || out_prefix == NULL) return -1;
    LlmCodecStTensorInfo* st = NULL; int n = 0; uint64_t dbase = 0;
    if (llmcodec_safetensors_read(adapter_path, &st, &n, &dbase) != 0) return -1;
    int ia = -1, ib = -1; char base[256] = "";
    for (int i = 0; i < n; i++) {
        size_t l = strlen(st[i].name), sl = strlen(".lora_A.weight");
        if (l > sl && strcmp(st[i].name + l - sl, ".lora_A.weight") == 0) { ia = i; memcpy(base, st[i].name, l - sl > 255 ? 255 : l - sl); base[l - sl > 255 ? 255 : l - sl] = 0; }
        else { size_t sb = strlen(".lora_B.weight"); if (l > sb && strcmp(st[i].name + l - sb, ".lora_B.weight") == 0) ib = i; }
    }
    if (ia < 0 || ib < 0 || st[ia].ndim != 2 || st[ib].ndim != 2) { llmcodec_safetensors_info_free(st); return -1; }
    FILE* fp = fopen(adapter_path, "rb");
    if (fp == NULL) { llmcodec_safetensors_info_free(st); return -1; }
    uint64_t An = 0, Bn = 0;
    float* As = llm_st_load_f32(fp, dbase, &st[ia], &An);   /* [rank, in] */
    float* Bs = llm_st_load_f32(fp, dbase, &st[ib], &Bn);   /* [out, rank] */
    fclose(fp);
    int rank = (int)st[ia].shape[0], in = (int)st[ia].shape[1], out = (int)st[ib].shape[0];
    if (As == NULL || Bs == NULL || (uint64_t)rank * in != An || (uint64_t)out * rank != Bn) {
        free(As); free(Bs); llmcodec_safetensors_info_free(st); return -1;
    }
    float* A = (float*)malloc((size_t)in * rank * 4); float* B = (float*)malloc((size_t)rank * out * 4);
    if (A == NULL || B == NULL) { free(As); free(Bs); free(A); free(B); llmcodec_safetensors_info_free(st); return -1; }
    for (int i = 0; i < in; i++) for (int r = 0; r < rank; r++) A[i * rank + r] = As[r * in + i];
    for (int r = 0; r < rank; r++) for (int o = 0; o < out; o++) B[r * out + o] = Bs[o * rank + r];
    free(As); free(Bs); llmcodec_safetensors_info_free(st);
    char ap[1300], bp[1300], mp[1300];
    snprintf(ap, sizeof ap, "%s_best_A.raw", out_prefix); FILE* af = fopen(ap, "wb");
    snprintf(bp, sizeof bp, "%s_best_B.raw", out_prefix); FILE* bf = fopen(bp, "wb");
    int ok = af != NULL && bf != NULL && fwrite(A, 4, (size_t)in * rank, af) == (size_t)in * rank
             && fwrite(B, 4, (size_t)rank * out, bf) == (size_t)rank * out;
    if (af) fclose(af); if (bf) fclose(bf);
    char mod[64] = ""; int layer = -1;
    { const char* lp = strstr(base, "layers."); if (lp) layer = atoi(lp + 7);
      const char* dp = strrchr(base, '.');   /* segmento dopo l'ULTIMO '.' = la proiezione */
      snprintf(mod, sizeof mod, "%s", dp ? dp + 1 : base); }
    snprintf(mp, sizeof mp, "%s_best.meta", out_prefix); FILE* mf = fopen(mp, "w");
    if (mf != NULL) { fprintf(mf, "layer=%d\nrank=%d\nmod=%s\nadapter=imported\n", layer, rank, mod[0] ? mod : "v_proj"); fclose(mf); }
    free(A); free(B);
    return ok ? 0 : -1;
}

/* TRAIN EVAL GATE (<ref>, <cand>[, <prompt>]): confronta la perplexity di cand vs ref
 * (tolleranza 2%) e SCRIVE il verdetto JSON in `out` (cap byte). Ritorna 0 (sempre, anche su
 * modelli mancanti: il verdetto è ERROR → comando deterministico). Riusa llm_quality_nll
 * (la stessa metrica di LLM QUALITY: NLL media su prompt di calibrazione). */
int llmmodelio_model_gate(const char* ref, const char* cand, const char* prompt, char* out, int cap) {
    if (out == NULL || cap <= 0) return -1;
    out[0] = 0;
    if (ref == NULL || *ref == '\0') { snprintf(out, (size_t)cap, "{\"verdict\":\"ERROR\",\"reason\":\"missing ref\"}"); return 0; }
    if (cand == NULL || *cand == '\0') { snprintf(out, (size_t)cap, "{\"verdict\":\"ERROR\",\"reason\":\"missing cand\"}"); return 0; }
    /* Pre-check esistenza modello (tokenizer.json): evita di chiamare llm_quality_nll (che logga
     * su stderr) quando i modelli non esistono — il verdetto è deterministico e senza rumore. */
    { struct stat st; char t[1300];
      if (stat(ref, &st) != 0 || (snprintf(t, sizeof t, "%s/tokenizer.json", ref), stat(t, &st) != 0))
          { snprintf(out, (size_t)cap, "{\"verdict\":\"ERROR\",\"reason\":\"ref model not loadable\"}"); return 0; }
      if (stat(cand, &st) != 0 || (snprintf(t, sizeof t, "%s/tokenizer.json", cand), stat(t, &st) != 0))
          { snprintf(out, (size_t)cap, "{\"verdict\":\"ERROR\",\"reason\":\"cand model not loadable\"}"); return 0; } }
    const char* pm = (prompt && *prompt) ? prompt : "The quick brown fox jumps over the lazy dog.";
    double rn = -1, rp = -1, cn = -1, cp = -1; int rt = 0, ct = 0;
    if (llm_quality_nll(ref, pm, &rn, &rp, &rt) != 0) { snprintf(out, (size_t)cap, "{\"verdict\":\"ERROR\",\"reason\":\"ref model not loadable\"}"); return 0; }
    if (llm_quality_nll(cand, pm, &cn, &cp, &ct) != 0) { snprintf(out, (size_t)cap, "{\"verdict\":\"ERROR\",\"reason\":\"cand model not loadable\"}"); return 0; }
    int accept = (cp <= rp * 1.02);
    snprintf(out, (size_t)cap, "{\"verdict\":\"%s\",\"metric\":\"ppl\",\"ref\":%.6f,\"cand\":%.6f,\"tolerance\":0.02}",
             accept ? "ACCEPT" : "REJECT-FLAG", rp, cp);
    return 0;
}

/* QUANTIR (I11) + MEMPLAN + KVPLAN + VIEW — report-only (§4.4/§4.5/§4.6/§4.9). */
static const char* llmo_role_of(const char* nm);
/* DECODE PROFILE (I3/GMB-AUTOTUNE): profilo decode (memory-bound) — param ATTIVI + KV@ctx,
 * banda -> tok/s bound + roofline. spec = "<ctx>" oppure "<ctx>:<bw_gbs>" (default 2048:450). */
static int llmmodelio_model_ir_decodeprofile(const char* path, const char* spec) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR DECODEPROFILE: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0, expert_b = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
        uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) != 0) continue;
        wb += b; const char* r = llmo_role_of(fi.tensors[i].name);
        if (strncmp(r, "experts", 7) == 0) expert_b += b;
    }
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1,nexp=-1,topk=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v;
        else if(strstr(k,"expert_count") && !strstr(k,"used") && !strstr(k,"group") && !strstr(k,"shared")) nexp=v;
        else if(strstr(k,"expert_used_count")) topk=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    long ctx=2048; double bw=450.0;   /* 450 GB/s = banda M5 Max (stima conservativa) */
    if (spec && *spec) { const char* c=strchr(spec,':'); if(c){ctx=atol(spec); bw=atof(c+1);} else ctx=atol(spec); }
    if (ctx<=0) ctx=2048; if (bw<=0) bw=450.0;
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    if (topk<=0) topk = (nexp>0)?1:0;   /* dense: 0; MoE: top-k */
    double per_exp = (nexp>0) ? (double)expert_b/(double)nexp : 0.0;
    double active_exp = per_exp*(double)topk;
    double dense = (double)wb - (double)expert_b;
    double kv_ctx = kvpt>0 ? kvpt*(double)ctx : 0.0;
    double per_tok = dense + active_exp + kv_ctx;   /* byte da LEGGERE a ogni token (memory-bound) */
    double tps = per_tok>0 ? bw*1e9/per_tok : -1.0;
    fprintf(LOUT, "# LLM TENSOR DECODEPROFILE '%s' arch=%s ctx=%ld (I3 roofline decode)\n", path, g_arch_ok?g_arch.arch:"-", ctx);
    fprintf(LOUT, "  weights=%.1f MiB (dense=%.1f, experts=%.1f, n_exp=%ld top_k=%ld)\n",
            wb/(1024.0*1024.0), dense/(1024.0*1024.0), expert_b/(1024.0*1024.0), nexp, topk);
    fprintf(LOUT, "  per-token read: dense=%.1f MiB + active_experts=%.1f MiB + KV@ctx=%.1f MiB = %.1f MiB\n",
            dense/(1024.0*1024.0), active_exp/(1024.0*1024.0), kv_ctx/(1024.0*1024.0), per_tok/(1024.0*1024.0));
    fprintf(LOUT, "  banda=%.0f GB/s  -> decode ~%.1f tok/s (bound MEMORY; roofline=memory-bound)\n", bw, tps);
    fprintf(LOUT, "  ottimizzazioni: kvquant + q8 KV, esperti RESIDENT vs stream (vedi EXPOLICY/MPLAN), speculative decode (SPEC)\n");
    return 0;
}
static int llmmodelio_model_ir_quantir(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR QUANTIR: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    fprintf(LOUT, "# LLM TENSOR QUANTIR '%s' arch=%s (I11: precisione per OP)\n# role\tweights\tactivation\taccum\toutput\n", path, g_arch_ok?g_arch.arch:"-");
    for (int i = 0; i < g_arch.n_quant; i++) {
        const char* role = g_arch.quant[i].role;
        /* I11: i 4 assi RISOLTI dalla descrizione (pesi + precisione del CALCOLO: activation/accum/output). */
        const char* w  = llmdesc_fmt(&g_arch, role);        if (!w) w = g_arch.quant[i].fmt;
        const char* ac = llmdesc_activation(&g_arch, role);
        const char* am = llmdesc_accum(&g_arch, role);
        const char* ou = llmdesc_output(&g_arch, role);
        fprintf(LOUT, "%s\tweights=%s\tactivation=%s\taccum=%s\toutput=%s\n", role, w, ac, am, ou);
    }
    /* QUANT POLICY (IR per-tensore MODIFICABILE): la precisione effettiva di OGNI tensore, risolta
     * da pol_decide (SETITEM > DSL > ruolo). Si modifica con `LLM POLICY SETITEM`/`DEFINE` e si
     * rilegge qui. Deterministico -> stessa risposta su tutti i backend. */
    fprintf(LOUT, "# per-tensor (IR modificabile: SETITEM > DSL > default); default -> policy\n");
    int qch = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        const char* role = llmmodelio_role_of(fi.tensors[i].name);
        const char* dflt = llmmodelio_proposed_of(role);
        int lay = -1; if (strncmp(fi.tensors[i].name, "blk.", 4) == 0) lay = atoi(fi.tensors[i].name + 4);
        /* I11: i 4 assi per-tensore dalla policy attiva (SETITEM > DSL > ruolo), poi i default dell'arch. */
        char w[16], ac[16], am[16], ou[16];
        pol_decide_axes(role, fi.tensors[i].name, lay, g_arch_ok?g_arch.arch:"", 1.0, w, sizeof w, ac, sizeof ac, am, sizeof am, ou, sizeof ou);
        if (!w[0])  snprintf(w,  sizeof w,  "%s", dflt);
        if (!ac[0]) snprintf(ac, sizeof ac, "%s", llmdesc_activation(&g_arch, role));
        if (!am[0]) snprintf(am, sizeof am, "%s", llmdesc_accum(&g_arch, role));
        if (!ou[0]) snprintf(ou, sizeof ou, "%s", llmdesc_output(&g_arch, role));
        int ch = (strcasecmp(w, dflt) != 0);
        if (ch) qch++;
        fprintf(LOUT, "%s\t%s\tw=%s\ta=%s\tacc=%s\to=%s%s\n", fi.tensors[i].name, role, w, ac, am, ou, ch ? " *" : "");
    }
    fprintf(LOUT, "  -> %d tensori con override di peso (IR modificabile via LLM POLICY SETITEM)\n", qch);
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
/* QUALITY BUDGET O0..O4 (§4.5 I6): mappa il livello a un budget bit/peso e verifica il piano per ruolo. */
static double llmo_bits(const char* f) {
    if (!f) return 0;
    if (strstr(f, "f32")) return 32; if (strstr(f, "f16") || strstr(f, "bf16")) return 16;
    if (strstr(f, "q8")) return 8.5; if (strstr(f, "q6")) return 6.6; if (strstr(f, "q5")) return 5.6;
    if (strstr(f, "mxfp4") || strstr(f, "q4") || strstr(f, "iq4")) return 4.5;
    if (strstr(f, "iq1")) return 1.56; if (strstr(f, "iq2")) return 2.06; if (strstr(f, "iq3")) return 3.44;
    if (strstr(f, "q3")) return 3.4; if (strstr(f, "q2")) return 2.6;
    return 4.5;
}
/* Riconosce i ruoli da nomi GGUF (blk.N.attn_q, ffn_gate_exps...) E HF/MLX (model.layers.N.self_attn.q_proj...). */
static const char* llmo_role_of(const char* nm) {
    if (!nm) return "other";
    /* HF/MLX (piu' specifici per primi) */
    if (strstr(nm, "switch_mlp")) {
        if (strstr(nm, "gate_proj")) return "experts.gate";
        if (strstr(nm, "up_proj"))   return "experts.up";
        if (strstr(nm, "down_proj")) return "experts.down";
        return "experts";
    }
    if (strstr(nm, "shared_experts")) {
        if (strstr(nm, "gate_proj")) return "shared.gate";
        if (strstr(nm, "up_proj"))   return "shared.up";
        if (strstr(nm, "down_proj")) return "shared.down";
        return "shared";
    }
    /* GGUF MoE: i tensori esperti raggruppati hanno suffisso `_exps` (ffn_gate_exps/up_exps/
     * down_exps). Vanno riconosciuti PRIMA dei densi (`ffn_gate`/`ffn_up`/`ffn_down`), altrimenti
     * MPLAN/DECODEPROFILE/EXPOLICY contano 0 byte esperti (bug). */
    if (strstr(nm, "ffn_gate_exps")) return "experts.gate";
    if (strstr(nm, "ffn_up_exps"))   return "experts.up";
    if (strstr(nm, "ffn_down_exps")) return "experts.down";
    if (strstr(nm, "_exps"))         return "experts";
    if (strstr(nm, "self_attn.q_proj") || strstr(nm, "attn_q")) return "attn.q";
    if (strstr(nm, "self_attn.k_proj") || strstr(nm, "attn_k")) return "attn.k";
    if (strstr(nm, "self_attn.v_proj") || strstr(nm, "attn_v")) return "attn.v";
    if (strstr(nm, "kv_a_proj") || strstr(nm, "kv_b_proj")) return "attn.kv";
    if (strstr(nm, "self_attn.o_proj") || strstr(nm, "attn_output") || strstr(nm, "attn_out")) return "attn.o";
    if (strstr(nm, "mlp.gate.weight") || strstr(nm, "ffn_gate_inp")) return "ffn.router";
    if (strstr(nm, "ffn_gate") || strstr(nm, "gate_proj")) return "ffn.gate";
    if (strstr(nm, "ffn_up") || strstr(nm, "up_proj")) return "ffn.up";
    if (strstr(nm, "ffn_down") || strstr(nm, "down_proj")) return "ffn.down";
    if (strstr(nm, "token_embd") || strstr(nm, "embed_tokens")) return "embed";
    if (strstr(nm, "lm_head") || strstr(nm, "output")) return "output";
    if (strstr(nm, "norm") || strstr(nm, "layernorm")) return "norm";
    return "other";
}
/* AUTOPOLICY (§4.1, policy automatica di selezione): dal modello + livello qualita' seleziona il
 * formato MIGLIORE per ogni ruolo ENTRO il budget (bit/peso) e stampa la DSL pronta per
 * `LLM POLICY DEFINE`. Il ciclo choose->apply->measure->reject e' poi `LLM POLICY APPLY` + `GATE`. */
static const char* llmo_pick_fmt(double b) {
    static const char* f[] = {"f32","f16","q8_0","q6_k","q5_k","q4_k","iq3_s","q3_k","iq3_xxs","q2_k","iq2_s","iq2_xs","iq2_xxs","iq1_m","iq1_s"};
    static const double  bits[] = {32.0,16.0,8.5,6.6,5.6,4.5,3.44,3.4,3.06,2.6,2.56,2.31,2.06,1.75,1.56};
    for (int i = 0; i < 15; i++) if (bits[i] <= b) return f[i];   /* max bits <= budget (f[] e' decrescente) */
    return f[14];   /* sotto 1.56 bit: iq1_s = massima compressione */
}
static int llmmodelio_model_ir_autopolicy(const char* path, const char* level) {
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    if (!g_arch_ok) { fprintf(stderr, "LLM TENSOR AUTOPOLICY: arch non risolta\n"); return -1; }
    int lv = 2; if (level && (level[0]=='O'||level[0]=='o')) lv = atoi(level + 1);
    if (lv < 0) lv = 0; if (lv > 6) lv = 6;   /* O5/O6: IQ1_M/IQ1_S (compressione estrema) */
    static const double tgt[7] = {16.0, 8.5, 4.5, 2.6, 2.1, 1.75, 1.56};
    static const char* nm[7] = {"O0", "O1", "O2", "O3", "O4", "O5", "O6"};
    double budget = tgt[lv];
    const char* def = llmo_pick_fmt(budget);
    fprintf(LOUT, "# LLM TENSOR AUTOPOLICY '%s' arch=%s livello=%s -> budget=%.1f bit/peso\n",
            path, g_arch.arch, nm[lv], budget);
    fprintf(LOUT, "# POLICY DSL (pronta):\nLLM POLICY DEFINE \"autopolicy_%s\", \"", nm[lv]);
    int nr = 0;
    for (int i = 0; i < g_arch.n_quant; i++) {
        const char* role = g_arch.quant[i].role;
        /* I ruoli non-quantizzabili (NORM/EMBEDDING) restano in f16/f32: non li si comprime. */
        const char* f = (strstr(role,"NORM") || strcmp(role,"EMBEDDING")==0) ? "f16" : llmo_pick_fmt(budget);
        fprintf(LOUT, "%sROLE=%s:%s", nr ? ";" : "", role, f); nr++;
    }
    fprintf(LOUT, "%sDEFAULT:%s\"\n", nr ? ";" : "", def);
    fprintf(LOUT, "# -> APPLY: `LLM POLICY APPLY \"autopolicy_%s\"`  |  misura/reject: `LLM POLICY GATE <modello>`\n", nm[lv]);
    return 0;
}
/* AUTOTUNE (§4.1, benchmark automatico): proietta la dimensione del modello per OGNI livello
 * O0..O4 (formato per-ruolo da AUTOPOLICY) e indica il livello raccomandato (max compressione che
 * entra nel budget RAM) + il fit. Report-only: nessuna quantizzazione (quella la fa `LLM QUANT`). */
static int llmmodelio_model_ir_autotune(const char* path) {
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    if (!g_arch_ok) { fprintf(stderr, "LLM TENSOR AUTOTUNE: arch non risolta\n"); return -1; }
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    static const double tgt[7] = {16.0, 8.5, 4.5, 2.6, 2.1, 1.75, 1.56};
    const double RAM = 90.0*1024.0*1024.0;   /* MiB (budget RAM, 2GB riserva) */
    fprintf(LOUT, "# LLM TENSOR AUTOTUNE '%s' arch=%s (benchmark proiettato O0..O6)\n", g_arch.arch);
    fprintf(LOUT, "# livello\tbudget(bit/peso)\tproiettato\tcompressione\tfit_RAM\n");
    int best = -1;
    for (int lv = 0; lv < 7; lv++) {
        double proj = 0, orig = 0;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            const char* role = llmdesc_role(&g_arch, fi.tensors[i].name);
            uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
            uint64_t ob = llmcodec_gguf_tensor_bytes(&fi.tensors[i]);   /* byte reali (MLX packed ok) */
            orig += (double)ob;
            /* proiezione: byte quantizzati ~ numel * bits/8 (per i ruoli quantizzabili) */
            const char* f = (strstr(role,"NORM") || strcmp(role,"EMBEDDING")==0) ? "f16" : llmo_pick_fmt(tgt[lv]);
            proj += (double)numel * llmo_bits(f) / 8.0;
        }
        double proj_mib = proj/(1024.0*1024.0);
        int fits = (proj_mib <= RAM);
        fprintf(LOUT, "O%d\t%.1f\t%.2f GiB\t%.1fx\t%s\n", lv, tgt[lv], proj_mib/1024.0,
                orig > 0 ? orig/proj : 0.0, fits ? "SI" : "NO");
        if (fits) best = lv;   /* il piu' compresso che entra (O crescente = piu' compresso) */
    }
    if (best >= 0) fprintf(LOUT, "# RACCOMANDATO: O%d (massima compressione entro il budget RAM). APPLY: `LLM POLICY APPLY \"autopolicy_O%d\"`\n", best, best);
    else fprintf(LOUT, "# RACCOMANDATO: nessun livello entra nel budget -> serve SWAP/streaming (EXPOLICY/MPLAN)\n");
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
static int llmmodelio_model_ir_obudget(const char* path, const char* level) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR OBUDGET: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    int lv = 2; if (level && (level[0] == 'O' || level[0] == 'o')) lv = atoi(level + 1);
    if (lv < 0) lv = 0; if (lv > 4) lv = 4;
    static const double tgt[5] = {16.0, 8.5, 4.5, 2.6, 2.1};
    static const char* nm[5] = {"O0 (f16, qualita' massima)", "O1 (q8)", "O2 (q4/mxfp4)", "O3 (q3/q2)", "O4 (iq2, massima compressione)"};
    double budget = tgt[lv];
    fprintf(LOUT, "# LLM TENSOR OBUDGET '%s' arch=%s qualita'=%s -> budget=%.1f bit/peso\n# role\tfmt\tbits\tverdetto\n",
            path, g_arch_ok?g_arch.arch:"-", nm[lv], budget);
    int over = 0, roles = 0, fallback = (g_arch.n_quant == 0);
    if (!fallback) {
        for (int i = 0; i < g_arch.n_quant; i++) {
            const char* f = g_arch.quant[i].fmt; double b = llmo_bits(f);
            const char* v = (b <= budget) ? "OK" : "OVER"; if (b > budget) over++;
            fprintf(LOUT, "%s\t%s\t%.1f\t%s\n", g_arch.quant[i].role, f, b, v); roles++;
        }
    } else {
        /* fallback: deriva i ruoli dai NOMI tensore + bits/peso dal tipo ggml reale. */
        const char* order[16] = {"embed","attn.q","attn.k","attn.v","attn.kv","attn.o","ffn.router","ffn.gate","ffn.up","ffn.down","experts.gate","experts.up","experts.down","shared.down","output","other"};
        double rb[16] = {0}; unsigned long long rbytes[16] = {0}, rnum[16] = {0}; int rt[16]; for (int i=0;i<16;i++) rt[i]=-1;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            const char* r = llmo_role_of(fi.tensors[i].name);
            int idx = 15; for (int j=0;j<15;j++) if (strcmp(order[j], r)==0) { idx=j; break; }
            uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
            uint64_t bb = 0; if (llmcodec_ggml_type_bytes(fi.tensors[i].ggml_type, numel, &bb) == 0) { rbytes[idx]+=bb; rnum[idx]+=numel; }
            if (rt[idx] < 0) rt[idx] = fi.tensors[i].ggml_type;
        }
        for (int j = 0; j < 15; j++) {
            if (rnum[j] == 0) continue;
            rb[j] = (double)(rbytes[j]*8)/(double)rnum[j];
            char tn[64]; if (llmcodec_ggml_type_name(rt[j], tn, sizeof tn) != 0) snprintf(tn, sizeof tn, "type%d", rt[j]);
            const char* v = (rb[j] <= budget) ? "OK" : "OVER"; if (rb[j] > budget) over++;
            fprintf(LOUT, "%s\t%s\t%.2f\t%s\n", order[j], tn, rb[j], v); roles++;
        }
    }
    fprintf(LOUT, "# TOTALE ruoli=%d over_budget=%d fonte=%s -> %s\n", roles, over, fallback ? "tensori(fallback)" : "arch",
            over == 0 ? "ACCEPT (piano entro il budget)" : "REJECT-FLAG (ruoli sopra il budget)");
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
static int llmmodelio_model_ir_memplan(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR MEMPLAN: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    unsigned long long wb = 0; for (uint64_t i = 0; i < fi.n_tensors; i++) { uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d]; uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    fprintf(LOUT, "# LLM TENSOR MEMPLAN '%s' (STATIC MEMORY PLAN / arena)\n", path);
    fprintf(LOUT, "  arena[0] weights  size=%llu B (region=model, lifetime=process, align=page)\n", wb);
    fprintf(LOUT, "  arena[1] kv       size=kv/token*ctx (region=session, lifetime=session)\n");
    fprintf(LOUT, "  arena[2] scratch  size=O(max_act) (region=step, lifetime=step, riuso)\n");
    return 0;
}
static int llmmodelio_model_ir_kvplan(const char* path, const char* spec) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR KVPLAN: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    long bc=-1,hkv=-1,hd=-1,kl=-1,vl=-1,en=-1;
    for (int i=0;i<nkv;i++){ const char* k=kv[i].key; long v=(long)kv[i].u32;
        if(strstr(k,"head_count_kv"))hkv=v; else if(strstr(k,"head_count"))hd=v; else if(strstr(k,"key_length"))kl=v;
        else if(strstr(k,"value_length"))vl=v; else if(strstr(k,"block_count"))bc=v; else if(strstr(k,"embedding_length"))en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double kvpt=-1; if(bc>0&&hkv>0){ long kd=kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd=vl>0?vl:kd; if(kd>0&&vd>0) kvpt=(double)bc*hkv*(kd+vd)*2.0; }
    const char* mode="PAGED"; long ctx=8192, page=256;
    if (spec && *spec) { char buf[64]; snprintf(buf,sizeof buf,"%s",spec);
        char* c1=strchr(buf,':'); char* c2=NULL; if(c1){*c1=0; c2=strchr(c1+1,':'); if(c2)*c2=0;}
        mode=buf; if(c1) ctx=atol(c1+1); if(c2) page=atol(c2+1); }
    if (page<=0) page=256; if (ctx<=0) ctx=8192;
    long npages=(long)((ctx+page-1)/page);
    double kvctx = kvpt>0 ? kvpt*(double)ctx : -1.0;
    fprintf(LOUT, "# LLM TENSOR KVPLAN '%s' mode=%s ctx=%ld page=%ld (§4.6 KV IR PAGE/MAP/UNMAP/PREFETCH/EVICT/SHARE)\n", path, mode, ctx, page);
    fprintf(LOUT, "  kv.dtype=%s  kv/token=%.0f B  kv@ctx=%.1f MiB  pages=%ld\n",
            (g_arch_ok && g_arch.runtime_kv_dtype[0]) ? g_arch.runtime_kv_dtype : "f16", kvpt, kvctx/(1024.0*1024.0), npages);
    fprintf(LOUT, "  PAGE     : %ld pagine da %ld token%s\n", npages, page, (strcasecmp(mode,"PAGED")==0)?" (block<->asse MAP/UNMAP)":"");
    fprintf(LOUT, "  PREFIX   : cache prefissi condivisi (SHARE); hit -> salta PREFILL (%s)\n", (strcasecmp(mode,"ROTATING")==0)?"no (rotating)":"si");
    fprintf(LOUT, "  PREFETCH : router -> KV (n_blocchi_ahead=1) su miss\n");
    fprintf(LOUT, "  EVICT    : LRU su pagine (WHEN_PRESSURE); %s\n", (strcasecmp(mode,"DISK_BACKED")==0)?"sorgente = KV su SSD (PREFETCH on-demand)":"in RAM");
    fprintf(LOUT, "  mode scelto=%s (FULL|PAGED|ROTATING|DISK_BACKED)\n", mode);
    return 0;
}
static int llmmodelio_model_ir_view(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR VIEW: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    /* VIEW/LAYOUT IR (dump COMPLETO): tutti i tensori con tipo GGML, byte e layout (§4.5/§4.9). */
    fprintf(LOUT, "# LLM TENSOR VIEW '%s' (VIEW/LAYOUT IR) — %llu tensori\n",
            path, (unsigned long long)fi.n_tensors);
    uint64_t total_bytes = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        LlmCodecGgufTensorInfo* t = &fi.tensors[i];
        char ds[96]; size_t o=0; ds[0]=0; uint64_t numel=1;
        for (int d=0; d<t->ndim; d++) { numel *= t->dims[d]; o+=(size_t)snprintf(ds+o,sizeof ds-o,"%s%llu",d?"x":"",(unsigned long long)t->dims[d]); }
        uint64_t nb=0; (nb = llmcodec_gguf_tensor_bytes(t), 0); total_bytes += nb;
        fprintf(LOUT, "  %s: type=%d bytes=%llu layout=contiguous dims=%s view=zero-copy(mmap)\n",
                t->name, t->ggml_type, (unsigned long long)nb, ds);
    }
    fprintf(LOUT, "# totale: %llu tensori, %llu byte (%.2f MB)\n",
            (unsigned long long)fi.n_tensors, (unsigned long long)total_bytes, (double)total_bytes/(1024.0*1024.0));
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
/* POLICYIR (§4.3 policy definibile) + IRLEVELS (§4.2 L0-L5) — report-only. */
static int llmmodelio_model_ir_policyir(const char* path) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR POLICYIR: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    if (!g_arch_ok) return -1;
    fprintf(LOUT, "# LLM TENSOR POLICYIR '%s' arch=%s (Policy definibile — regole dichiarative)\n", path, g_arch.arch);
    fprintf(LOUT, "  WHEN role=%s THEN fmt=%s (quant); recipe=%s boost_fmt=%s\n",
            "ROUTED_EXPERT", llmdesc_fmt(&g_arch, "ROUTED_EXPERT") ? llmdesc_fmt(&g_arch, "ROUTED_EXPERT") : "-", g_arch.recipe, g_arch.boost_fmt);
    for (int i = 0; i < g_arch.n_quant; i++)
        fprintf(LOUT, "  WHEN role=%s THEN fmt=%s\n", g_arch.quant[i].role, g_arch.quant[i].fmt);
    fprintf(LOUT, "  DEFAULT fmt=(dal modello)  PRIORITY=roles-first-match\n");
    return 0;
}
/* I7 — fasi PREFILL | PREFILL_KV | DECODE: geometria, KV, classe di op, roofline (§4.5). */
/* LOGITS (I11/logits report): top-k + entropia della distribuzione del prossimo token. */
/* Legge `add_bos_token` + `bos_token` da tokenizer_config.json (HF): ritorna 1 + la stringa BOS.
 * HF/mlx_lm prependono il BOS quando `add_bos_token` e' true — senza, la PRIMA predizione diverge
 * (verificato su LFM2-MoE: senza BOS -> " Italy", con BOS -> " Rome"). Gestisce sia la forma
 * stringa (`"bos_token": "<x>"`) sia l'oggetto HF (`"bos_token": {"content": "<x>"}`). */
static int llmmodelio_read_bos(const char* model_dir, char* out, size_t cap) {
    char p[1300]; snprintf(p, sizeof p, "%s/tokenizer_config.json", model_dir);
    FILE* f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1 << 20)) { fclose(f); return 0; }
    char* buf = (char*)malloc((size_t)sz + 1); if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = 0; fclose(f);
    int add = 0; char* ab = strstr(buf, "\"add_bos_token\"");
    if (ab) { char* c = strchr(ab, ':'); if (c && strstr(c, "true")) add = 1; }
    int has = 0;
    if (add) {
        char* b = strstr(buf, "\"bos_token\"");
        if (b) {
            char* c = strchr(b, ':');
            if (c) {
                char* q = strchr(c, '"');           /* forma stringa */
                char* cont = strstr(c, "\"content\"");  /* forma oggetto */
                if (cont && (!q || cont < q)) q = strchr(strchr(cont, ':'), '"');
                if (q) { char* q2 = strchr(q + 1, '"');
                    if (q2) { size_t n = (size_t)(q2 - q - 1); if (n >= cap) n = cap - 1; memcpy(out, q + 1, n); out[n] = 0; has = 1; } }
            }
        }
    }
    free(buf);
    return has;
}

static int llmmodelio_model_ir_logits(const char* model_dir, const char* prompt) {
    if (model_dir == NULL || prompt == NULL) return -1;
    /* fail-fast: verifica il tokenizer PRIMA di caricare l'engine (path invalido -> niente dlopen). */
    char ck[1300]; snprintf(ck,sizeof ck,"%s/tokenizer.json",model_dir);
    { FILE* tf=fopen(ck,"rb"); if(tf==NULL){ snprintf(ck,sizeof ck,"%s/tokenizer.model",model_dir); tf=fopen(ck,"rb"); if(tf==NULL) return -1; } fclose(tf); }   /* errore = codice di ritorno */
    void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) return -1;
    typedef void* (*load_fn)(const char*); typedef void* (*sess_fn)(void*);
    typedef int (*app_fn)(void*, const int32_t*, int); typedef void (*free_fn)(void*); typedef void (*dsess_fn)(void*);
    typedef int (*topk_fn)(void*, int, int32_t*, float*);
    typedef int (*ent_fn)(void*, float*);
    load_fn L=(load_fn)dlsym(h,"llm_ai_mlx_load_model"); sess_fn C=(sess_fn)dlsym(h,"llm_ai_mlx_create_session");
    app_fn A=(app_fn)dlsym(h,"llm_ai_mlx_session_append"); free_fn FM=(free_fn)dlsym(h,"llm_ai_mlx_free_model");
    dsess_fn DS=(dsess_fn)dlsym(h,"llm_ai_mlx_destroy_session"); topk_fn TK=(topk_fn)dlsym(h,"llm_ai_mlx_session_topk");
    ent_fn EN=(ent_fn)dlsym(h,"llm_ai_mlx_session_entropy");
    if (L==NULL||C==NULL||A==NULL||TK==NULL||EN==NULL) { fprintf(stderr,"LLM TENSOR LOGITS: simboli engine assenti\n"); return -1; }
    llmtoken_handle th = llmmodelio_tokenizer_open(model_dir); if (th==NULL){fprintf(stderr,"LLM TENSOR LOGITS: tokenizer assente\n");return -1;}
    int32_t ids[2048]; int n = llmtoken_encode(th, prompt, ids, 2048);
    /* BOS: HF/mlx_lm lo prependono quando `add_bos_token` e' true (parita' della prima predizione). */
    { char bos[128]; if (llmmodelio_read_bos(model_dir, bos, sizeof bos) && bos[0]) {
        int32_t b[4]; int nb = llmtoken_encode(th, bos, b, 4);
        if (nb == 1 && n >= 1 && n < 2047) { memmove(ids + 1, ids, (size_t)n * sizeof(int32_t)); ids[0] = b[0]; n++; }
    } }
    if (n<1){llmtoken_free(th);fprintf(stderr,"LLM TENSOR LOGITS: prompt vuoto\n");return -1;}
    void* m=L(model_dir); if(m==NULL){llmtoken_free(th);fprintf(stderr,"LLM TENSOR LOGITS: load modello fallito\n");return -1;}
    void* se=C(m); if(se==NULL){if(FM)FM(m);llmtoken_free(th);return -1;}
    int ra=A(se, ids, n); if(ra!=0){if(DS)DS(se);if(FM)FM(m);llmtoken_free(th);fprintf(stderr,"LLM TENSOR LOGITS: append fallito\n");return -1;}
    int32_t tid[10]; float tlp[10]; int k = TK(se, 10, tid, tlp);
    float ent = -1.0f; EN(se, &ent);
    if (k < 0) { if(DS)DS(se);if(FM)FM(m);llmtoken_free(th); fprintf(stderr,"LLM TENSOR LOGITS: topk fallito\n"); return -1; }
    fprintf(LOUT, "# LLM TENSOR LOGITS '%s' prompt_tokens=%d entropia=%.3f bit (top-%d del prossimo token)\n# rank\tlogprob\tprob\ttoken\n", model_dir, n, ent, k);
    for (int i = 0; i < k; i++) {
        char tb[96]; int tn2 = llmtoken_decode(th, &tid[i], 1, tb, (int)sizeof tb - 1);
        if (tn2 < 0) tn2 = 0; tb[tn2] = 0;
        for (int c = 0; c < tn2; ++c) if (tb[c] == '\n' || tb[c] == '\r' || tb[c] == '\t') tb[c] = ' ';
        fprintf(LOUT, "%d\t%.4f\t%.5f\t{%s}\n", i+1, tlp[i], exp((double)tlp[i]), tb);
    }
    if (DS)DS(se); if(FM)FM(m); llmtoken_free(th);
    return 0;
}
static int llmmodelio_model_ir_phase(const char* path, long seq) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR PHASE: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    long bc = -1, hkv = -1, hd = -1, kl = -1, vl = -1, en = -1;
    for (int i = 0; i < nkv; i++) { const char* k = kv[i].key; long v = (long)kv[i].u32;
        if (strstr(k,"attention.head_count_kv")) hkv=v; else if (strstr(k,"attention.head_count")) hd=v;
        else if (strstr(k,"attention.key_length")) kl=v; else if (strstr(k,"attention.value_length")) vl=v;
        else if (strstr(k,"block_count")) bc=v; else if (strstr(k,"embedding_length")) en=v; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    if (seq <= 0) seq = 512;
    long kd = kl>0?kl:((en>0&&hd>0)?en/hd:-1); long vd = vl>0?vl:kd;
    double kvpt = (bc>0&&hkv>0&&kd>0&&vd>0) ? (double)bc*(double)hkv*(double)(kd+vd)*2.0 : -1.0;
    fprintf(LOUT, "# LLM TENSOR PHASE '%s' arch=%s seq=%ld (I7: PREFILL | PREFILL_KV | DECODE)\n",
            path, g_arch_ok?g_arch.arch:"-", seq);
    fprintf(LOUT, "# fase\ttokens\tbatch\tkv_step(B)\tkv_total(B)\top\troofline\n");
    double kv_pre = kvpt>0 ? kvpt*(double)seq : -1.0;
    fprintf(LOUT, "PREFILL\t%ld\t%ld\t%.0f\t%.0f\t%s\tcompute-bound (GEMM, AI alto)\n",
            seq, seq, kvpt, kv_pre, "matmul(batched)");
    fprintf(LOUT, "PREFILL_KV\t%ld\t%ld\t%.0f\t%.0f\t%s\tKV scritta durante prefill, riusata in decode\n",
            0, 0, kvpt, kv_pre, "kv_write");
    fprintf(LOUT, "DECODE\t1\t1\t%.0f\t%.0f\t%s\tmemory-bound (GEMV, AI basso; KV riuso, no recompute)\n",
            kvpt, kvpt, "matvec+attn");
    fprintf(LOUT, "# note: prefill domina il compute, decode domina la banda (KV). kv/token=%.1f B, kv@seq=%.0f B\n", kvpt, kv_pre);
    return 0;
}
/* ARCHRECON (§4.2): valida la descrizione `.gmbarch` del modello — FONTE UNICA di architettura.
 * GMAD ELIMINATO (2026-10-04): l'arch del modello viene SOLO dal `.gmbarch` risolto, non più da
 * un blob embedded. GMBasic non conosce il GMAD; il suo uso termina con la creazione del `.gmbarch`. */
static int llmmodelio_model_ir_archrecon(const char* path) {
    if (llmo_is_dir(path)) { fprintf(LOUT, "LLM TENSOR ARCHRECON '%s': dir MLX (usa ARCHRECON su un GGUF, oppure IR/INFO/LAYERS sul dir)\n", path); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    const char* at  = g_arch_ok ? llmdesc_struct_type(&g_arch,"attention") : NULL;
    const char* moe = g_arch_ok ? llmdesc_struct_type(&g_arch,"moe") : NULL;
    fprintf(LOUT, "# LLM TENSOR ARCHRECON '%s' (.gmbarch)\n", path);
    fprintf(LOUT, "  .gmbarch : arch=%s attention=%s moe=%s\n",
            g_arch_ok?g_arch.arch:"-", at?at:"-", moe?moe:"-");
    fprintf(LOUT, "  modello  : tensori=%llu\n", (unsigned long long)fi.n_tensors);
    int ok = (g_arch_ok && g_arch.arch[0]) ? 1 : 0;
    fprintf(LOUT, "  esito    : %s\n", ok ? "RICONCILIATO (.gmbarch = fonte unica di architettura)"
                                          : "DIVERGENZA (.gmbarch non risolto per l'arch del modello)");
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return ok ? 0 : -1;
}

static int llmmodelio_model_ir_irlevels(const char* path) {
    size_t ln = strlen(path);
    if (llmo_is_dir(path)) { fprintf(LOUT, "LLM TENSOR IRLEVELS '%s': dir MLX (usa IR/INFO/LAYERS sul dir, oppure IRLEVELS su un GGUF)\n", path); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    /* gerarchia IR L0-L5 MODEL-AWARE: conteggi reali per livello dal modello. */
    int nattn = 0, nmoe = 0, nffn = 0, nnorm = 0, nother = 0, maxl = -1;
    uint64_t wb = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
        uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) == 0) wb += b;
        int L = model_layer_of(fi.tensors[i].name); if (L > maxl) maxl = L;
        const char* r = llmmo_role_cat(fi.tensors[i].name);
        if (strncmp(r,"ATTN",4)==0) nattn++;
        else if (strcmp(r,"ROUTED_EXPERT")==0||strcmp(r,"SHARED_EXPERT")==0||strcmp(r,"ROUTER")==0||strstr(fi.tensors[i].name,"exps")) nmoe++;
        else if (strncmp(r,"FFN",3)==0) nffn++;
        else if (strstr(r,"NORM")) nnorm++;
        else nother++;
    }
    int nrole = g_arch_ok ? g_arch.n_roles : 0;
    fprintf(LOUT, "# LLM TENSOR IRLEVELS '%s' arch=%s (gerarchia IR L0-L5, model-aware)\n", path, g_arch_ok?g_arch.arch:"-");
    fprintf(LOUT, "  L0 source   : HF/MLX/safetensors (pre-conversione)\n");
    fprintf(LOUT, "  L1 container: GGUF v%u, %llu tensori, %d KV, %.2f MB\n", fi.version, (unsigned long long)fi.n_tensors, nkv, (double)wb/(1024.0*1024.0));
    fprintf(LOUT, "  L2 model IR : arch=%s (ruoli=%d) layers=%d modality=%s speculation=%s\n",
            g_arch_ok?g_arch.arch:"-", nrole, maxl+1, g_arch_ok?llmdesc_modality(&g_arch):"-",
            (g_arch_ok&&g_arch.mm_mtp)?(g_arch.mm_dspark?"DSPARK(MTP)":"MTP"):"NONE");
    fprintf(LOUT, "  L3 primitive: attention=%d moe=%d ffn=%d norm=%d other=%d\n", nattn, nmoe, nffn, nnorm, nother);
    fprintf(LOUT, "  L4 execution: fasi PREFILL | PREFILL_KV | DECODE; layer_ops=%d\n", maxl+1);
    fprintf(LOUT, "  L5 lowering : MLX(metal) | C(c99) | VM(C)\n");
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    return 0;
}
/* M10 — MEMORY PLANNER compile-time completo: per-tensore MEMORY OBJECT + piano DATA-MOVEMENT.
 * budget_gb>0 -> fit analysis (RESIDENT vs COLD/stream). Report-only (M10 riceve un piano). */
static int llmmodelio_model_ir_mplan(const char* path, double budget_gb) {
    size_t ln = strlen(path);
    if (0 /* read_meta gestisce anche le dir MLX */) { fprintf(stderr, "LLM TENSOR MPLAN: solo GGUF\n"); return -1; }
    if (llmmodelio_arch_resolve_gguf(path) != 0) g_arch_ok = 0;
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(path, &kv, &nkv, &fi) != 0) return -1;
    double expert_b = 0, block_b = 0, glob_b = 0; int maxl = -1;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        uint64_t numel = 1; for (int d = 0; d < fi.tensors[i].ndim; d++) numel *= fi.tensors[i].dims[d];
        uint64_t b = 0; if ((b = llmcodec_gguf_tensor_bytes(&fi.tensors[i]), 0) != 0) continue;
        const char* r = llmo_role_of(fi.tensors[i].name);
        int L = model_layer_of(fi.tensors[i].name);
        if (L > maxl) maxl = L;
        if (strncmp(r, "experts", 7) == 0 || strncmp(r, "shared", 6) == 0) expert_b += (double)b;
        else if (L >= 0) block_b += (double)b; else glob_b += (double)b;
    }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    double total = expert_b + block_b + glob_b;
    fprintf(LOUT, "# LLM TENSOR MPLAN '%s' arch=%s layers=%d (M10 compile-time memory planner)\n",
            path, g_arch_ok?g_arch.arch:"-", maxl+1);
    fprintf(LOUT, "# MEMORY OBJECTS (region\tbytes\tdevice\tlifetime\tresidency\tprefetch\teviction\ttransfer_cost)\n");
    fprintf(LOUT, "embed/output\t%.0f\tgpu\tmodel\tRESIDENT\tNONE\tNEVER\tone-time(mmap)\n", glob_b);
    fprintf(LOUT, "per-layer weights\t%.0f\tgpu\tmodel\tRESIDENT\tNEXT_LAYER\tWHEN_PRESSURE\tone-time(mmap)\n", block_b);
    fprintf(LOUT, "experts\t%.0f\tgpu|ssd\tmodel/block\tRESIDENT|COLD\tROUTER->N_BLOCKS_AHEAD\tWHEN_PRESSURE\tstream_on_use\n", expert_b);
    fprintf(LOUT, "kv\tkv/token*session\tgpu\tsession\tRESIDENT\tNONE\tAFTER_BLOCK\t0\n");
    fprintf(LOUT, "activation\tscratch\tgpu\tstep\tTRANSIENT\tNONE\tAFTER_BLOCK\t0\n");
    fprintf(LOUT, "# totale pesi=%.1f MiB (embed/output=%.1f, per-layer=%.1f, experts=%.1f)\n",
            total/(1024.0*1024.0), glob_b/(1024.0*1024.0), block_b/(1024.0*1024.0), expert_b/(1024.0*1024.0));
    if (budget_gb > 0) {
        double bud = budget_gb*1024.0*1024.0*1024.0, non_expert = glob_b + block_b;
        double res_exp = bud - non_expert;
        fprintf(LOUT, "# FIT budget=%.1f GiB: non-expert=%.1f MiB -> residui per esperti=%.1f MiB\n",
                budget_gb, non_expert/(1024.0*1024.0), res_exp/(1024.0*1024.0));
        if (res_exp >= expert_b) {
            fprintf(LOUT, "#   tutti gli esperti RESIDENT (margine %.1f MiB)\n", (res_exp-expert_b)/(1024.0*1024.0));
        } else {
            double frac = res_exp > 0 ? res_exp/expert_b : 0.0;
            fprintf(LOUT, "#   esperti: %.1f%% RESIDENT, %.1f%% COLD/stream (SSD) -> policy=LRU/RECENCY + router->PREFETCH\n", frac*100.0, (1.0-frac)*100.0);
        }
    }
    fprintf(LOUT, "# DATA-MOVEMENT PLAN: PREFETCH expert-group BEFORE MOE_LAYER[N] (router) | RESIDENT KV FOR DECODE | WIRE weights-at-load | EVICT on_pressure | UNWIRE after_block\n");
    return 0;
}
/* LLAMA OPTIMIZER — EXPERT_RESIDENCY_POLICY model-aware + router->prefetch (report-only).
 * Inferisce n_esperti dal tensore esperti + top-k dal meta; deriva la policy per-layer dal budget. */
static int llmmodelio_model_ir_expolicy(const char* path, double budget_gb) {
    LlmModInfo mi; if (llmmod_load(path, &mi) != 0) return -1;
    long nexp = mi.n_experts, topk = mi.top_k; double expert_b = 0; int layers_exp = 0;
    for (uint64_t i = 0; i < mi.n_tensors; i++) {
        const char* r = llmo_role_of(mi.tensors[i].name);
        if (strncmp(r, "experts", 7) != 0 && strncmp(r, "shared", 6) != 0) continue;
        expert_b += (double)mi.tensors[i].bytes;
        /* n_esperti = prima dimensione del tensore esperti raggruppato (switch_mlp/ffn_*_exps) */
        if (nexp < 0 && strstr(mi.tensors[i].name, "gate_proj") && mi.tensors[i].ndim >= 1) nexp = (long)mi.tensors[i].dims[0];
        if (strstr(mi.tensors[i].name, "switch_mlp") || strstr(mi.tensors[i].name, "exps")) layers_exp++;
    }
    llmmod_free(&mi);
    if (topk <= 0) topk = 8;   /* fallback tipico */
    fprintf(LOUT, "# LLM TENSOR EXPOLICY '%s' experts=%ld top_k=%ld expert_bytes=%.1f MiB (LLAMA OPTIMIZER)\n",
            path, nexp, topk, expert_b/(1024.0*1024.0));
    if (budget_gb > 0) {
        double bud = budget_gb*1024.0*1024.0*1024.0;
        const char* pol = (expert_b <= bud) ? "STATIC (tutti resident)" : "LRU+RECENCY (working-set)";
        double frac = expert_b > 0 ? (bud < expert_b ? bud/expert_b : 1.0) : 1.0;
        fprintf(LOUT, "# budget=%.1f GiB -> policy=%s (residente %.1f%%)\n", budget_gb, pol, frac*100.0);
    } else {
        fprintf(LOUT, "# policy suggerite: STATIC (fit) | LRU | LFU | RECENCY | PREDICTIVE (router-led)\n");
    }
    fprintf(LOUT, "# router->PREFETCH: ad ogni MOE_LAYER[N], il router attiva top_k=%ld esperti -> PREFETCH di QUEI %ld\n", topk, topk);
    fprintf(LOUT, "  BEFORE MOE_LAYER[N]  (N_BLOCKS_AHEAD=1, dal router della layer N-1 o dalla stessa)\n");
    fprintf(LOUT, "  EVICTION=WHEN_PRESSURE, LIFETIME=BLOCK (working-set per blocco), transfer_cost=stream_on_miss\n");
    fprintf(LOUT, "  ENGRAM/IDX: i tensori condivisi (shared_experts) restano RESIDENT (lifetime=model)\n");
    return 0;
}
/* §9.A: emette il PIANO POLICY nel formato del runtime MLX (`llm_mlx_expolicy`), consumabile
 * via l'opzione registro `expolicy`. Model-aware: n_esperti/top_k dal modello; mode=OFFLOAD +
 * hotness (eviction per route) + prefetch_topn=top_k (router->prefetch). Dato PUL. */
static int llmmodelio_model_ir_expolicy_emit(const char* path, const char* out) {
    LlmModInfo mi; if (llmmod_load(path, &mi) != 0) return -1;
    long nexp = mi.n_experts, topk = mi.top_k; double expert_b = 0;
    for (uint64_t i = 0; i < mi.n_tensors; i++) {
        const char* r = llmo_role_of(mi.tensors[i].name);
        if (strncmp(r, "experts", 7) != 0 && strncmp(r, "shared", 6) != 0) continue;
        expert_b += (double)mi.tensors[i].bytes;
        if (nexp < 0 && strstr(mi.tensors[i].name, "gate_proj") && mi.tensors[i].ndim >= 1) nexp = (long)mi.tensors[i].dims[0];
    }
    llmmod_free(&mi);
    if (topk <= 0) topk = 8;
    FILE* o = fopen(out, "w"); if (o == NULL) return -1;
    fprintf(o, "{\n  \"source\": \"LLM TENSOR EXPOLICY\",\n  \"model\": \"%s\",\n  \"n_experts\": %ld,\n  \"top_k\": %ld,\n  \"expert_bytes\": %.0f,\n  \"mode\": \"OFFLOAD\",\n  \"hotness\": true,\n  \"prefetch_topn\": %ld\n}\n",
            path, nexp, topk, expert_b, topk);
    fclose(o);
    fprintf(LOUT, "LLM TENSOR EXPOLICY '%s' -> '%s' (mode=OFFLOAD hotness=1 prefetch_topn=%ld experts=%ld)\n", path, out, topk, nexp);
    return 0;
}
/* §9.A M10d: quote delle classi KV (RUNTIME vs PERSISTENT) dal manager engine
 * `llm_mlx_kvclasses` (LLMEnv, Apache) via dlopen (dipendenza debole, come LLM QUALITY).
 * 2o arg opzionale = file JSON di output. */
static int llmmodelio_model_ir_kvclasses(const char* out_path) {
    unsigned long long rt = 0, ps = 0; int nr = 0, np = 0;
    void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h != NULL) {
        typedef void (*st_fn)(unsigned long long*, unsigned long long*, int*, int*);
        st_fn fn = (st_fn)dlsym(h, "llm_mlx_kvclasses_stats");
        if (fn != NULL) fn(&rt, &ps, &nr, &np);
    }
    if (out_path != NULL && *out_path) {
        FILE* o = fopen(out_path, "w"); if (o == NULL) return -1;
        fprintf(o, "{\n  \"runtime_bytes\": %llu,\n  \"persistent_bytes\": %llu,\n  \"runtime_objects\": %d,\n  \"persistent_objects\": %d\n}\n",
                rt, ps, nr, np);
        fclose(o);
        fprintf(LOUT, "LLM TENSOR KVCLASSES -> '%s' (runtime=%llu persistent=%llu)\n", out_path, rt, ps);
    } else {
        fprintf(LOUT, "LLM TENSOR KVCLASSES: runtime_bytes=%llu persistent_bytes=%llu (objects r=%d p=%d)\n", rt, ps, nr, np);
    }
    return 0;
}
/* §9.A M10c: contratto GMB_KV_FORMAT (dtype di STORAGE del KV). Report + validazione: un dtype
 * di storage ammesso solo se nativo/supportato/quantizzabile; fp4-family solo se nativo del
 * modello (mai conversione silenziosa). Classificazione self-contained (niente dlopen). */
static const char* llmo_kvformat_class(const char* native, const char* req) {
    if (native && req && strcasecmp(native, req) == 0) return "NATIVE";
    if (!req) return "UNSUPPORTED";
    if (!strcasecmp(req,"f16")||!strcasecmp(req,"float16")||!strcasecmp(req,"bf16")||!strcasecmp(req,"bfloat16")||!strcasecmp(req,"f32")||!strcasecmp(req,"float32")) return "SUPPORTED";
    if (!strcasecmp(req,"q8_0")||!strcasecmp(req,"int8")||!strcasecmp(req,"i8")||!strcasecmp(req,"fp8")||!strcasecmp(req,"fp8_e4m3")||!strcasecmp(req,"e4m3")) return "QUANTIZABLE";
    if (!strcasecmp(req,"fp4")||!strcasecmp(req,"mxfp4")||!strcasecmp(req,"nvfp4")) return "RECONSTRUCTABLE";
    return "UNSUPPORTED";
}
static int llmmodelio_model_ir_kvformat(const char* path, const char* requested) {
    const char* native = "f16";   /* KV dtype nativo (compute) di default; addestrato se il modello lo dichiara. */
    if (requested && *requested) {
        const char* cls = llmo_kvformat_class(native, requested);
        int accept = (strcmp(cls, "NATIVE") == 0 || strcmp(cls, "SUPPORTED") == 0 || strcmp(cls, "QUANTIZABLE") == 0);
        fprintf(LOUT, "LLM TENSOR KVFORMAT '%s': native=%s requested=%s class=%s verdict=%s\n",
                path, native, requested, cls, accept ? "ACCEPT" : "REJECT");
    } else {
        fprintf(LOUT, "LLM TENSOR KVFORMAT '%s': native=%s (storage ammessi: f16/bf16/f32, q8_0/int8/fp8; fp4-family solo se nativo del modello)\n",
                path, native);
    }
    return 0;
}
/* §9.A M10a: reconstruction_cost + policy RECOMPUTE/PERSIST/KEEP. Decisione dal modello di costo
 * dell'engine (llm_mlx_kvcost) via dlopen. Arg 2 = "<n_tokens>,<kv_bytes>". */
typedef struct { int action; double cost_keep, cost_persist, cost_recompute; } KvCostRes;
static int llmmodelio_model_ir_kvdecide(const char* path, const char* args) {
    long long ntok = 0; double kvb = 0;
    if (args && *args) { double d = 0; if (sscanf(args, "%lld,%lf", &ntok, &d) >= 1) kvb = d; }
    int action = 2;   /* default RECOMPUTE se l'engine non è caricabile */ KvCostRes r; r.action = action; r.cost_keep = r.cost_persist = r.cost_recompute = 0;
    void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h != NULL) {
        typedef int (*df)(long long, double, double, double, double, KvCostRes*);
        df f = (df)dlsym(h, "llm_mlx_kvcost_decide");
        if (f != NULL) action = f(ntok, 1.0, kvb, 1e9, 1.0, &r);
    }
    const char* an = (action == 0) ? "KEEP" : (action == 1) ? "PERSIST" : "RECOMPUTE";
    fprintf(LOUT, "LLM TENSOR KVDECIDE '%s': n_tokens=%lld kv_bytes=%.0f -> %s (recompute=%.1f persist=%.1f keep=%.1f)\n",
            path, ntok, kvb, an, r.cost_recompute, r.cost_persist, r.cost_keep);
    return 0;
}
/* §9.A M10b: KV come DAG (base/derived/index) + espansione PREFETCH topologica. Via dlopen
 * all'engine (llm_mlx_kvdag_demo_csa2). Arg opzionale = file JSON. */
static int llmmodelio_model_ir_kvdag(const char* out_path) {
    char exp[1024]; exp[0] = 0; int n = 0;
    void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h != NULL) {
        typedef int (*df)(char*, size_t);
        df f = (df)dlsym(h, "llm_mlx_kvdag_demo_csa2");
        if (f != NULL) n = f(exp, sizeof exp);
    }
    if (out_path != NULL && *out_path) {
        FILE* o = fopen(out_path, "w"); if (o == NULL) return -1;
        fprintf(o, "{\n  \"nodes\": %d,\n  \"prefetch\": \"%s\"\n}\n", n, exp);
        fclose(o);
        fprintf(LOUT, "LLM TENSOR KVDAG -> '%s' (%d op)\n", out_path, n);
    } else {
        fprintf(LOUT, "LLM TENSOR KVDAG (CSA2): %d op -> %s\n", n, exp);
    }
    return 0;
}
/* §9.A M10: MEMORY OBJECT runtime manager unificato — stat live (categorie + totali). */
static int llmmodelio_model_ir_mobjlive(const char* out_path) {
    static const char* names[] = {"weights","expert_ram","expert_device","kv_runtime","kv_persistent","prefix","temp"};
    unsigned long long total = 0, cat[7]; int nobj = 0, cn[7]; for (int c = 0; c < 7; c++) { cat[c] = 0; cn[c] = 0; }
    void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
    if (h != NULL) {
        typedef void (*sf)(unsigned long long*, int*);
        typedef void (*cf)(int, unsigned long long*, int*);
        sf fs = (sf)dlsym(h, "llm_mlx_mobj_stats");
        cf fc = (cf)dlsym(h, "llm_mlx_mobj_stats_cat");
        if (fs) fs(&total, &nobj);
        if (fc) for (int c = 0; c < 7; c++) fc(c, &cat[c], &cn[c]);
    }
    if (out_path != NULL && *out_path) {
        FILE* o = fopen(out_path, "w"); if (o == NULL) return -1;
        fprintf(o, "{\n  \"total_bytes\": %llu,\n  \"objects\": %d,\n  \"categories\": [\n", total, nobj);
        for (int c = 0; c < 7; c++) fprintf(o, "%s    { \"cat\": \"%s\", \"bytes\": %llu, \"objects\": %d }", c ? ",\n" : "", names[c], cat[c], cn[c]);
        fprintf(o, "\n  ]\n}\n");
        fclose(o);
        fprintf(LOUT, "LLM TENSOR MOBJLIVE -> '%s' (total=%llu objects=%d)\n", out_path, total, nobj);
    } else {
        fprintf(LOUT, "LLM TENSOR MOBJLIVE: total_bytes=%llu objects=%d | kv_runtime=%llu/%d kv_persistent=%llu/%d weights=%llu/%d\n",
                total, nobj, cat[3], cn[3], cat[4], cn[4], cat[0], cn[0]);
    }
    return 0;
}
/* §6.1 S2 — SENSIBILITÀ EMPIRICA: ΔPPL di quantizzare UN SOLO ruolo (resto F16). Per ogni ruolo
 * costruisce la tabella AUTO (solo quel ruolo al candidato `bits`), quantizza e misura la PPL col
 * GATE reale. Il ΔPPL ordina i ruoli per sensibilità VERA (vs il proxy rel_error sui soli pesi).
 * `bits` da POLICY SEN_BITS (default 4). 0 ok / -1. */
static int llmmodelio_model_sensitivity(const char* model, const char* dataset, int bits)
{
    if (model == NULL || !*model || dataset == NULL || !*dataset) return -1;
    int cand_ty = LLMCODEC_GGML_T_Q4_K;   /* 4 bit */
    if (bits == 2) cand_ty = LLMCODEC_GGML_T_Q2_K;
    else if (bits == 3) cand_ty = LLMCODEC_GGML_T_Q3_K;
    else if (bits == 4) cand_ty = LLMCODEC_GGML_T_Q4_K;
    else if (bits == 5) cand_ty = LLMCODEC_GGML_T_Q5_K;
    else if (bits == 6) cand_ty = LLMCODEC_GGML_T_Q6_K;
    else if (bits == 8) cand_ty = LLMCODEC_GGML_T_Q8_0;
    { struct stat _st; if (stat(model, &_st) != 0) return -1; }   /* pre-check: modello assente -> errore pulito senza spurie su stderr */
    double bnll = 0, bppl = 0; int btok = 0;
    if (llm_quality_nll_auto(model, dataset, &bnll, &bppl, &btok) != 0) return -1;
    LlmCodecGgufFileInfo fi;
    if (llmcodec_gguf_read_info(model, &fi) != 0) return -1;
    if (fi.n_tensors == 0) { llmcodec_gguf_free_info(&fi); return -1; }
    fprintf(LOUT, "LLM TENSOR SENSITIVITY '%s' bits=%d: baseline ppl=%.4f (%d tok)\n", model, bits, bppl, btok);
    static const char* roles[] = { "ATTENTION", "FFN", "ROUTED_EXPERT", "EMBEDDING", "OUTPUT",
                                   "SHARED_EXPERT", "ROUTER", "NORM" };
    int nroles = (int)(sizeof roles / sizeof roles[0]);
    int* tab = (int*)malloc((size_t)fi.n_tensors * sizeof(int));
    if (tab == NULL) { llmcodec_gguf_free_info(&fi); return -1; }
    int n_ok = 0;
    for (int r = 0; r < nroles; r++) {
        int ncount = 0;
        for (uint64_t i = 0; i < fi.n_tensors; i++) {
            int isr = (strcmp(llmmodelio_role_of(fi.tensors[i].name), roles[r]) == 0);
            tab[i] = isr ? cand_ty : (int)LLMCODEC_GGML_T_F16;
            if (isr) ncount++;
        }
        if (ncount == 0) continue;   /* ruolo assente in questo modello */
        const char* tmp = "/tmp/gmb_sens_tmp.gguf";
        g_auto_type = tab;
        int qrc = llmmodelio_model_quantize(model, tmp);
        g_auto_type = NULL;
        if (qrc != 0) { fprintf(LOUT, "  %-14s quantize fallita\n", roles[r]); continue; }
        double pnll = 0, pppl = 0; int ptok = 0;
        if (llm_quality_nll_auto(tmp, dataset, &pnll, &pppl, &ptok) != 0) {
            fprintf(LOUT, "  %-14s PPL fallita\n", roles[r]); remove(tmp); continue;
        }
        fprintf(LOUT, "  %-14s tensori=%d ppl=%.4f dppl=%+.4f\n", roles[r], ncount, pppl, pppl - bppl);
        n_ok++;
        remove(tmp);
    }
    free(tab); llmcodec_gguf_free_info(&fi);
    return n_ok > 0 ? 0 : -1;
}

/* bit-per-peso effettivi di un artefatto GGUF = (byte*8)/parametri (somma dei dim dei tensori). */
static double gmb_gguf_bpw(const char* path) {
    struct stat st; if (path == NULL || stat(path, &st) != 0) return 0.0;
    LlmCodecGgufFileInfo fi; if (llmcodec_gguf_read_info(path, &fi) != 0) return 0.0;
    double params = 0.0;
    for (uint64_t i = 0; i < fi.n_tensors; i++) {
        double n = 1.0; for (int d = 0; d < fi.tensors[i].ndim; d++) n *= (double)fi.tensors[i].dims[d];
        params += n;
    }
    llmcodec_gguf_free_info(&fi);
    return params > 0.0 ? (double)st.st_size * 8.0 / params : 0.0;
}

/* §6.1 S3 — GATE DI ACCETTAZIONE: quantizza lo stesso modello con l'AUTO (`quantize_auto` a `bpw`) e
 * con la RICETTA baseline (default), poi confronta la PPL sul dataset a bpw effettivi dichiarati.
 * Verdetto `PPL(auto) < PPL(baseline)`. 0 ok / -1. */
static int llmmodelio_model_autogate(const char* model, const char* dataset, double bpw) {
    if (model == NULL || !*model || dataset == NULL || !*dataset) return -1;
    struct stat _st; if (stat(model, &_st) != 0) return -1;
    const char* ta = "/tmp/gmb_autogate_auto.gguf";
    const char* tb = "/tmp/gmb_autogate_base.gguf";
    if (llmmodelio_model_quantize_auto(model, ta, bpw) != 0) return -1;
    if (llmmodelio_model_quantize(model, tb) != 0) { remove(ta); return -1; }
    double an = 0, ap = 0, bn = 0, bp = 0; int at = 0, bt = 0;
    if (llm_quality_nll_auto(ta, dataset, &an, &ap, &at) != 0 ||
        llm_quality_nll_auto(tb, dataset, &bn, &bp, &bt) != 0) { remove(ta); remove(tb); return -1; }
    double abpw = gmb_gguf_bpw(ta), bbpw = gmb_gguf_bpw(tb);
    fprintf(LOUT, "LLM TENSOR AUTOGATE '%s' target=%.3f bpw:\n", model, bpw);
    fprintf(LOUT, "  AUTO     bpw=%.3f ppl=%.4f\n", abpw, ap);
    fprintf(LOUT, "  BASELINE bpw=%.3f ppl=%.4f\n", bbpw, bp);
    fprintf(LOUT, "  -> %s (PPL auto %s baseline)\n", ap < bp ? "OK: AUTO < BASELINE" : "NO: AUTO >= baseline",
            ap < bp ? "<" : ">=");
    remove(ta); remove(tb);
    return 0;
}

int llmmodelio_tensor_cmd(const char* sub, const char* a, const char* b, const char* c)
{
    const char* s = sub ? sub : "";
    const char* dir = a ? a : "";
    const char* tn = b ? b : "";
    const char* extra = c ? c : "";
    /* B2 — tokenizer/vocab: dominio separato (llmmodelio_tokenizer.c). Se riconosce il
     * sotto-verbo ritorna 0/-1 e ha già stampato su LOUT; altrimenti prosegue. */
    { int tok_handled = 0; int tok_rc = llmmodelio_tokenizer_cmd(s, dir, tn, extra, LOUT, &tok_handled);
      if (tok_handled) return tok_rc; }
    /* B3/B4/B5 — sotto-verbi con dominio separato (guardati per SUB: le funzioni gestiscono solo
     * il proprio verbo, così non intercettano gli altri). */
    if (strcasecmp(s, "REGISTRY") == 0)   return llmmodelio_registry_cmd(dir, tn, extra, LOUT, NULL);
    if (strcasecmp(s, "QUANTERROR") == 0) return llmmodelio_quant_error(dir, tn, LOUT, NULL);
    if (strcasecmp(s, "COMPARE") == 0)    return llmmodelio_compare(dir, tn, extra, LOUT, NULL);
    if (strcasecmp(s, "DERIVE") == 0)     return llmmodelio_derive(dir, tn, LOUT, NULL);
    if (strcasecmp(s, "MAKEPATCH") == 0)  return llmmodelio_makepatch(dir, tn, extra, LOUT, NULL);
    if (strcasecmp(s, "APPLYPATCH") == 0) {
        if (!*dir || !*tn || !*extra) { fprintf(stderr, "LLM APPLYPATCH: attesi <base>, <patch>, <out>\n"); return -1; }
        if (llmmodelio_model_add(dir, tn, extra) != 0) return -1;
        fprintf(LOUT, "LLM APPLYPATCH: base=%s patch=%s out=%s\n", dir, tn, extra);
        return 0;
    }
    if (strcasecmp(s, "AUTOGATE") == 0 || strcasecmp(s, "AUTOG") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR AUTOGATE: attesi <model>, <dataset>[, <bpw>]\n"); return -1; }
        /* 3° argomento esplicito vince; altrimenti POLICY AUTO_BPW (registro opzioni); default 4.5. */
        const char* bs = *extra ? extra : llmmodelio_model_policy_get("AUTO_BPW");
        return llmmodelio_model_autogate(dir, tn, bs ? atof(bs) : 4.5);
    }
    if (strcasecmp(s, "USAGEEVAL") == 0 || strcasecmp(s, "USAGE") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR USAGEEVAL: attesi <model>, <bench>\n"); return -1; }
        char rpt[512]; rpt[0] = 0; int rc = llm_usage_eval(dir, tn, rpt, sizeof rpt, NULL);
        if (rc == 0) fprintf(LOUT, "LLM TENSOR USAGEEVAL '%s': %s\n", dir, rpt);
        return rc;
    }
    if (strcasecmp(s, "USAGEGATE") == 0 || strcasecmp(s, "USAGEG") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR USAGEGATE: attesi <model>, <bench>\n"); return -1; }
        const char* bs = llmmodelio_model_policy_get("AUTO_BPW");
        return llm_usage_gate(dir, tn, bs ? atof(bs) : 4.5);
    }
    if (strcasecmp(s, "SENSITIVITY") == 0 || strcasecmp(s, "SENS") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR SENSITIVITY: attesi <model>, <dataset>\n"); return -1; }
        const char* bs = llmmodelio_model_policy_get("SEN_BITS");
        return llmmodelio_model_sensitivity(dir, tn, bs ? atoi(bs) : 4);
    }
    if (strcasecmp(s, "ROLE") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR ROLE: attesi <dir>, <tensor>\n"); return -1; }
        if (!g_arch_ok && llmmodelio_arch_resolve(dir) != 0) return -1;
        const char* role = llmdesc_role(&g_arch, tn);
        const char* fmt = llmdesc_fmt(&g_arch, role);
        fprintf(LOUT, "LLM TENSOR '%s': %s -> ruolo=%s formato=%s\n", tn, g_arch.arch, role, fmt ? fmt : "-");
        return 0;
    }
    if (strcasecmp(s, "SCHEDULE") == 0 || strcasecmp(s, "BATCH") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR SCHEDULE: atteso <path>[, <seqlen>]\n"); return -1; }
        return llmmodelio_model_ir_schedule(dir, *tn ? atoi(tn) : 0);
    }
    if (strcasecmp(s, "DIST") == 0 || strcasecmp(s, "DISTRIBUTION") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR DIST: atteso <path>[, <world>]\n"); return -1; }
        return llmmodelio_model_ir_dist(dir, *tn ? atoi(tn) : 1);
    }
    if (strcasecmp(s, "PRUNEHEADS") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR PRUNEHEADS: attesi <in>, <out>\n"); return -1; }
        const char* pk = llmmodelio_model_policy_get("PRUNE_HEADS");
        int nk = pk ? atoi(pk) : 0;
        if (nk <= 0) { fprintf(stderr, "LLM TENSOR PRUNEHEADS: imposta PRUNE_HEADS (n teste da tenere)\n"); return -1; }
        return llmmodelio_model_prune_heads(dir, tn, nk);
    }
    if (strcasecmp(s, "MOEDENSE") == 0 || strcasecmp(s, "MOE2DENSE") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR MOEDENSE: attesi <in>, <out>\n"); return -1; }
        return llmmodelio_model_moe_to_dense(dir, tn);
    }
    if (strcasecmp(s, "DENSE") == 0 || strcasecmp(s, "DENSEEXPERT") == 0 || strcasecmp(s, "SPLITMOE") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR DENSE: attesi <in>, <out>[, <spec-esperti>]\n"); return -1; }
        int rc = llmmodelio_model_dense_expert(dir, tn, extra);
        if (rc == 0) fprintf(LOUT, "LLM TENSOR DENSE: in=%s out=%s spec=%s\n", dir, tn, *extra ? extra : "0");
        return rc;
    }
    if (strcasecmp(s, "SPLITEXPERTS") == 0 || strcasecmp(s, "SPLITEXPS") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR SPLITEXPERTS: attesi <in>, <out>\n"); return -1; }
        return llmmodelio_model_split_experts(dir, tn);
    }
    if (strcasecmp(s, "DEQUANT") == 0 || strcasecmp(s, "F16PLAIN") == 0 || strcasecmp(s, "TOF16") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR DEQUANT: attesi <in_dir>, <out_dir>\n"); return -1; }
        return llmmodelio_mlx_dequant(dir, tn);
    }
    if (strcasecmp(s, "PRUNEFFN") == 0 || strcasecmp(s, "PRUNEHIDDEN") == 0) {
        if (!*dir || !*tn) { fprintf(stderr, "LLM TENSOR PRUNEFFN: attesi <in>, <out>\n"); return -1; }
        const char* fk = llmmodelio_model_policy_get("FFN_KEEP");
        int k = fk ? atoi(fk) : 0;
        if (k <= 0) { fprintf(stderr, "LLM TENSOR PRUNEFFN: imposta FFN_KEEP (n unita intermedie da tenere)\n"); return -1; }
        return llmmodelio_model_prune_ffn(dir, tn, k);
    }
    if (strcasecmp(s, "CED") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR CED: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_ced(dir, tn);
    }
    if (strcasecmp(s, "PROFILE") == 0 || strcasecmp(s, "RESIDENCY") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR PROFILE: atteso <path>[, <budget_gb>]\n"); return -1; }
        return llmmodelio_model_ir_profile(dir, *tn ? atoi(tn) : 0);
    }
    if (strcasecmp(s, "GPU") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR GPU: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_gpu(dir);
    }
    if (strcasecmp(s, "POLICYIR") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR POLICYIR: atteso <path>\n");return -1;} return llmmodelio_model_ir_policyir(dir); }
    if (strcasecmp(s, "LOGITS") == 0 || strcasecmp(s, "TOPK") == 0) { if(!*dir||!*tn){fprintf(stderr,"LLM TENSOR LOGITS: attesi <model_dir>, <prompt>\n");return -1;} return llmmodelio_model_ir_logits(dir, tn); }
    if (strcasecmp(s, "PHASE") == 0 || strcasecmp(s, "PREFILL") == 0 || strcasecmp(s, "PREFILL_KV") == 0 || strcasecmp(s, "DECODE") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR PHASE: atteso <path>[, <seqlen>]\n");return -1;} return llmmodelio_model_ir_phase(dir, *tn ? atol(tn) : 0); }
    if (strcasecmp(s, "IRLEVELS") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR IRLEVELS: atteso <path>\n");return -1;} return llmmodelio_model_ir_irlevels(dir); }
    if (strcasecmp(s, "ARCHRECON") == 0 || strcasecmp(s, "RECONCILE") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR ARCHRECON: atteso <path>\n");return -1;} return llmmodelio_model_ir_archrecon(dir); }
    if (strcasecmp(s, "DECODEPROFILE") == 0 || strcasecmp(s, "DECODE_PROFILE") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR DECODEPROFILE: atteso <path>[, <ctx[:bw_gbs]>]\n");return -1;} return llmmodelio_model_ir_decodeprofile(dir, tn); }
    if (strcasecmp(s, "QUANTIR") == 0 || strcasecmp(s, "QUANT_IR") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR QUANTIR: atteso <path>\n");return -1;} return llmmodelio_model_ir_quantir(dir); }
    if (strcasecmp(s, "OBUDGET") == 0 || strcasecmp(s, "QUALITYLEVEL") == 0 || strcasecmp(s, "OBUDGETIR") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR OBUDGET: atteso <path>[, <O0..O4>]\n");return -1;} return llmmodelio_model_ir_obudget(dir, tn); }
    if (strcasecmp(s, "AUTOPOLICY") == 0 || strcasecmp(s, "AUTOPOL") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR AUTOPOLICY: atteso <path>[, <O0..O4>]\n");return -1;} return llmmodelio_model_ir_autopolicy(dir, tn); }
    if (strcasecmp(s, "AUTOTUNE") == 0 || strcasecmp(s, "AUTOBENCH") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR AUTOTUNE: atteso <path>\n");return -1;} return llmmodelio_model_ir_autotune(dir); }
    if (strcasecmp(s, "EXPOLICY") == 0 || strcasecmp(s, "EXPERTPOLICY") == 0) {
        if(!*dir){fprintf(stderr,"LLM TENSOR EXPOLICY: atteso <path>[, <budget_gb>|<out.json>]\n");return -1;}
        /* 2o arg .json -> EMETTE il piano policy (formato runtime, opzione `expolicy`); altrimenti report. */
        if (*tn && strstr(tn, ".json")) return llmmodelio_model_ir_expolicy_emit(dir, tn);
        return llmmodelio_model_ir_expolicy(dir, *tn ? atof(tn) : 0.0);
    }
    if (strcasecmp(s, "KVCLASSES") == 0 || strcasecmp(s, "KV_MEMORY") == 0) {
        return llmmodelio_model_ir_kvclasses(*tn ? tn : NULL);
    }
    if (strcasecmp(s, "KVFORMAT") == 0 || strcasecmp(s, "KV_FORMAT") == 0) {
        return llmmodelio_model_ir_kvformat(dir, *tn ? tn : NULL);
    }
    if (strcasecmp(s, "KVDECIDE") == 0 || strcasecmp(s, "KVRECOMPUTE") == 0) {
        return llmmodelio_model_ir_kvdecide(dir, tn);
    }
    if (strcasecmp(s, "KVDAG") == 0 || strcasecmp(s, "KV_DAG") == 0) {
        return llmmodelio_model_ir_kvdag(*tn ? tn : NULL);
    }
    if (strcasecmp(s, "MOBJLIVE") == 0 || strcasecmp(s, "MOBJ_RUNTIME") == 0) {
        return llmmodelio_model_ir_mobjlive(*tn ? tn : NULL);
    }
    if (strcasecmp(s, "MPLAN") == 0 || strcasecmp(s, "MEMORYPLAN") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR MPLAN: atteso <path>[, <budget_gb>]\n");return -1;} return llmmodelio_model_ir_mplan(dir, *tn ? atof(tn) : 0.0); }
    if (strcasecmp(s, "MEMPLAN") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR MEMPLAN: atteso <path>\n");return -1;} return llmmodelio_model_ir_memplan(dir); }
    if (strcasecmp(s, "KVPLAN") == 0 || strcasecmp(s, "KVPAGED") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR KVPLAN: atteso <path>[, <mode:ctx:page>]\n");return -1;} return llmmodelio_model_ir_kvplan(dir, tn); }
    if (strcasecmp(s, "VIEW") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR VIEW: atteso <path>\n");return -1;} return llmmodelio_model_ir_view(dir); }
    if (strcasecmp(s, "CAPACITY") == 0 || strcasecmp(s, "EXACT_CAPACITY") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR CAPACITY: atteso <path>[, <ctx>]\n");return -1;} return llmmodelio_model_ir_capacity(dir, tn); }
    if (strcasecmp(s, "CONTEXTMAP") == 0 || strcasecmp(s, "CONTEXT_MAP") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR CONTEXTMAP: atteso <path>[, <ctx>]\n");return -1;} return llmmodelio_model_ir_contextmap(dir, tn); }
    if (strcasecmp(s, "CONTEXTGC") == 0 || strcasecmp(s, "CONTEXT_GC") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR CONTEXTGC: atteso <path>[, <cur:target>]\n");return -1;} return llmmodelio_model_ir_contextgc(dir, tn); }
    if (strcasecmp(s, "CONTEXT") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR CONTEXT: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_context(dir);
    }
    if (strcasecmp(s, "EPLAN") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR EPLAN: atteso <path>[, <world>]\n"); return -1; }
        return llmmodelio_model_ir_eplan(dir, *tn ? atoi(tn) : 1);
    }
    if (strcasecmp(s, "MOBJ") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR MOBJ: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_mobj(dir);
    }
    if (strcasecmp(s, "STATE") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR STATE: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_state(dir);
    }
    if (strcasecmp(s, "GEXPORT") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR GEXPORT: atteso <path>, <out.dot>\n"); return -1; }
        return llmmodelio_model_ir_gexport(dir, tn);
    }
    if (strcasecmp(s, "LOADPLAN") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR LOADPLAN: atteso <file.gmbllm>\n"); return -1; }
        return llmmodelio_model_ir_loadplan(dir);
    }
    if (strcasecmp(s, "INSPECT") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR INSPECT: atteso <file.gmbllm>\n"); return -1; }
        return llmmodelio_model_ir_inspect(dir);
    }
    if (strcasecmp(s, "COMPILE") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR COMPILE: atteso <path>, <out.gmbllm>\n"); return -1; }
        return llmmodelio_model_ir_compile(dir, tn);
    }
    if (strcasecmp(s, "EXECPLAN") == 0 || strcasecmp(s, "PLAN") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR EXECPLAN: atteso <path>, <out>\n"); return -1; }
        return llmmodelio_model_ir_execplan(dir, tn);
    }
    if (strcasecmp(s, "KV") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR KV: atteso <path>[, <mode>]\n"); return -1; }
        return llmmodelio_model_ir_kv(dir, tn);
    }
    if (strcasecmp(s, "SPEC") == 0 || strcasecmp(s, "SPECULATION") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR SPEC: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_spec(dir);
    }
    if (strcasecmp(s, "COST") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR COST: atteso <path>[, <ctx>]\n"); return -1; }
        return llmmodelio_model_ir_cost(dir, *tn ? atoi(tn) : 0);
    }
    if (strcasecmp(s, "MEMORY") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR MEMORY: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_memory(dir);
    }
    if (strcasecmp(s, "OPRECORD") == 0 || strcasecmp(s, "OPTIMIZATION_RECORD") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR OPRECORD: atteso <path>[, <out>]\n");return -1;} return llmmodelio_model_ir_oprecord(dir, tn); }
    if (strcasecmp(s, "NUMGRAPH") == 0 || strcasecmp(s, "NUMERICGRAPH") == 0) { if(!*dir){fprintf(stderr,"LLM TENSOR NUMGRAPH: atteso <path>\n");return -1;} return llmmodelio_model_ir_numgraph(dir); }
    if (strcasecmp(s, "GRAPH") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR GRAPH: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_graph(dir);
    }
    if (strcasecmp(s, "INFO") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR INFO: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_info(dir);
    }
    if (strcasecmp(s, "LAYERS") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR LAYERS: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_layers(dir);
    }
    if (strcasecmp(s, "CHECK") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR CHECK: atteso <path>\n"); return -1; }
        return llmmodelio_model_ir_check(dir);
    }
    if (strcasecmp(s, "IR") == 0 || strcasecmp(s, "MAP") == 0) {
        if (!*dir) { fprintf(stderr, "LLM TENSOR IR: atteso <path mod?>\n"); return -1; }
        return llmmodelio_model_ir_print(dir);
    }
    if (strcasecmp(s, "LIST") == 0 || *s == 0) {
        if (!g_arch_ok && (!*dir || llmmodelio_arch_resolve(dir) != 0)) { fprintf(LOUT, "LLM TENSOR: nessuna descrizione risolta\n"); return -1; }
        fprintf(LOUT, "LLM TENSOR '%s': %d ruoli\n", g_arch.arch, g_arch.n_roles);
        for (int i = 0; i < g_arch.n_roles; i++) {
            const char* f = llmdesc_fmt(&g_arch, g_arch.roles[i].role);
            fprintf(LOUT, "  %-24s -> %-16s %s\n", g_arch.roles[i].match, g_arch.roles[i].role, f ? f : "-");
        }
        return 0;
    }
    fprintf(stderr, "LLM TENSOR: sotto-verbo '%s' non supportato (ROLE|LIST)\n", s);
    return -1;
}

/* KERNEL: sub=SHOW a=<dir> b=<kind> -> pipeline della struttura (primitive/step);
 * sub=CODEC a=<dir> b=<fmt> -> plugin che implementa il formato; sub=PRIM a=<dir> b=<kind> ->
 * plugin che implementa la primitiva. Nessun plugin = il runtime usa il built-in (mai silenzioso). */
int llmmodelio_kernel_cmd(const char* sub, const char* a, const char* b)
{
    const char* s = sub ? sub : "";
    const char* dir = a ? a : "";
    const char* k = b ? b : "";
    if (strcasecmp(s, "CODEC") == 0) {
        if (!*dir || !*k) { fprintf(stderr, "LLM KERNEL CODEC: attesi <dir>, <fmt>\n"); return -1; }
        LlmCodec c; char pp[1024];
        if (llmmodelio_codec_resolve(dir, k, &c, pp, sizeof pp) == 0)
            fprintf(LOUT, "LLM KERNEL CODEC '%s': plugin '%s'%s%s%s\n", k, c.name ? c.name : "?",
                    pp[0] ? " (" : "", pp[0] ? pp : "", pp[0] ? ")" : "");
        else
            fprintf(LOUT, "LLM KERNEL CODEC '%s': nessun plugin dichiarato (il runtime usa il built-in)\n", k);
        return 0;
    }
    if (strcasecmp(s, "PRIM") == 0) {
        if (!*dir || !*k) { fprintf(stderr, "LLM KERNEL PRIM: attesi <dir>, <kind>\n"); return -1; }
        LlmEnginePluginPrim p; char pp[1024];
        if (llmmodelio_prim_resolve(dir, k, &p, pp, sizeof pp) == 0)
            fprintf(LOUT, "LLM KERNEL PRIM '%s': plugin '%s'%s%s%s\n", k, p.name ? p.name : "?",
                    pp[0] ? " (" : "", pp[0] ? pp : "", pp[0] ? ")" : "");
        else
            fprintf(LOUT, "LLM KERNEL PRIM '%s': nessun plugin dichiarato (il runtime usa il built-in)\n", k);
        return 0;
    }
    /* SHOW / PIPELINE / default: se `b` e' una struttura nota, mostra la sua pipeline. */
    if (!g_arch_ok && (!*dir || llmmodelio_arch_resolve(dir) != 0)) { fprintf(LOUT, "LLM KERNEL: nessuna descrizione risolta\n"); return -1; }
    if (*k) {
        char buf[2048]; buf[0] = 0;
        if (llmmodelio_arch_pipeline(*dir ? dir : llmmodelio_arch_path(), k, buf, sizeof buf) >= 0) {
            fprintf(LOUT, "LLM KERNEL '%s': %s\n", k, buf);
            return 0;
        }
        fprintf(LOUT, "LLM KERNEL '%s': struttura senza pipeline dichiarata\n", k);
        return 0;
    }
    fprintf(LOUT, "LLM KERNEL '%s': %d strutture\n", g_arch.arch, g_arch.n_struct);
    for (int i = 0; i < g_arch.n_struct; i++) {
        const LlmDescStruct* S = &g_arch.structures[i];
        fprintf(LOUT, "  %-12s type=%-16s prim=%s", S->key, S->type[0] ? S->type : "-", S->prim[0] ? S->prim : "-");
        for (int j = 0; j < S->n_steps; j++) fprintf(LOUT, " %s->%s", S->steps[j].op, S->steps[j].prim);
        fprintf(LOUT, "\n");
    }
    return 0;
}

/* MODULE: sub=SHOW/SUPPORT a=<dir> -> strutture (moduli) dichiarate e se il converter le sa
 * mappare (llmmodelio_arch_support); sub=REGISTER -> riservato (richiede permesso esplicito: le
 * librerie/moduli sono infrastruttura). Mai no-op silenzioso. */
int llmmodelio_module_cmd(const char* sub, const char* a, const char* b)
{
    const char* s = sub ? sub : "";
    const char* dir = a ? a : "";
    (void)b;
    if (strcasecmp(s, "REGISTER") == 0 || strcasecmp(s, "PRIORITY") == 0) {
        fprintf(stderr, "LLM MODULE %s: riservato (registrazione moduli/librerie richiede permesso esplicito)\n", s);
        return -1;
    }
    if (strcasecmp(s, "MODALITY") == 0) {
        if (!*dir) { fprintf(stderr, "LLM MODULE MODALITY: atteso <path>\n"); return -1; }
        char mod[32], mm[512], ar[96];
        if (llmmodelio_model_modality_of(dir, mod, sizeof mod, mm, sizeof mm, ar, sizeof ar) != 0) return -1;
        fprintf(LOUT, "LLM MODULE MODALITY '%s': arch=%s modality=%s mmproj=%s\n", dir, ar, mod, mm[0] ? mm : "-");
        return 0;
    }
    if (strcasecmp(s, "SHOW") == 0 || strcasecmp(s, "SUPPORT") == 0 || *s == 0) {
        if (!*dir) { fprintf(stderr, "LLM MODULE: atteso <dir>\n"); return -1; }
        char buf[4096];
        int nbad = llmmodelio_arch_support(dir, buf, sizeof buf);
        if (nbad < 0) return -1;
        fprintf(LOUT, "%s", buf);
        return 0;
    }
    fprintf(stderr, "LLM MODULE: sotto-verbo '%s' non supportato (SHOW|SUPPORT)\n", s);
    return -1;
}

/* KV: sub=SHOW a=<dir> -> metadati RUNTIME (engine) dichiarati dalla .llmarch: state, dtype del
 * KV e dell'attivazione. NON sono codec di checkpoint (Knowledge/Encoder-Decoder-Modelli.md §19).
 * sub=SET -> non supportato qui (la KV è configurazione del motore, non del compilatore): errore
 * esplicito, mai silenzioso. */
int llmmodelio_kv_cmd(const char* sub, const char* a, const char* b)
{
    const char* s = sub ? sub : "";
    const char* dir = a ? a : "";
    (void)b;
    if (strcasecmp(s, "SET") == 0) {
        fprintf(stderr, "LLM KV SET: la KV è configurazione del MOTORE (non del compilatore): non supportato qui\n");
        return -1;
    }
    if (strcasecmp(s, "SHOW") == 0 || *s == 0) {
        if (!g_arch_ok && (!*dir || llmmodelio_arch_resolve(dir) != 0)) { fprintf(LOUT, "LLM KV: nessuna descrizione risolta\n"); return -1; }
        fprintf(LOUT, "LLM KV '%s': state=%s kv.dtype=%s activation.dtype=%s\n", g_arch.arch,
                g_arch.runtime_state[0] ? g_arch.runtime_state : "-",
                g_arch.runtime_kv_dtype[0] ? g_arch.runtime_kv_dtype : "-",
                g_arch.runtime_activation_dtype[0] ? g_arch.runtime_activation_dtype : "-");
        return 0;
    }
    fprintf(stderr, "LLM KV: sotto-verbo '%s' non supportato (SHOW)\n", s);
    return -1;
}

/* Wrapper `_to`: come llmmodelio_arch_cmd_to — cattura il testo in `out` per i backend gestiti. */
#define LLMMODELIO_CMD_TO(name, fn)                                                       \
    int name(const char* sub, const char* a, const char* b, char* out, size_t cap)     \
    {                                                                                  \
        if (!out || cap == 0) return fn(sub, a, b);                                    \
        out[0] = 0;                                                                    \
        FILE* f = fmemopen(out, cap - 1, "w");                                          \
        if (!f) return fn(sub, a, b);                                                  \
        g_llm_out = f;                                                                 \
        int rc = fn(sub, a, b);                                                        \
        fflush(f); fclose(f); g_llm_out = NULL;                                        \
        out[cap - 1] = 0;                                                              \
        return rc;                                                                     \
    }
/* Variante `_to` dedicata (4 argomenti: sub,a,b,c) — TENSOR ha un argomento opzionale in piu'
 * (es. il bpw di AUTOGATE); gli altri verbi (KERNEL/MODULE/KV) restano a 3 via macro. */
int llmmodelio_tensor_cmd_to(const char* sub, const char* a, const char* b, const char* c, char* out, size_t cap)
{
    if (!out || cap == 0) return llmmodelio_tensor_cmd(sub, a, b, c);
    out[0] = 0;
    FILE* f = fmemopen(out, cap - 1, "w");
    if (!f) return llmmodelio_tensor_cmd(sub, a, b, c);
    g_llm_out = f;
    int rc = llmmodelio_tensor_cmd(sub, a, b, c);
    fflush(f); fclose(f); g_llm_out = NULL;
    out[cap - 1] = 0;
    return rc;
}
LLMMODELIO_CMD_TO(llmmodelio_kernel_cmd_to, llmmodelio_kernel_cmd)
LLMMODELIO_CMD_TO(llmmodelio_module_cmd_to, llmmodelio_module_cmd)
LLMMODELIO_CMD_TO(llmmodelio_kv_cmd_to,     llmmodelio_kv_cmd)

/* LLM MODEL CONVERT <src>, <dst>[, <fmt>] — conversione di formato del modello.
 * fmt: GGUF | MLX (il formato GGUF NON esiste piu').
 *   - GGUF  : MLX/HF (safetensors) -> GGUF, guidato dalla .llmarch risolta dal sorgente.
 *   - MLX   : GGUF -> MLX (safetensors), idem.
 * I converter sono quelli del progetto CODEC (llmcodec_conv_*_arch): una sola sorgente per tutti i
 * backend (IL/PCodeVM/C99/VM-C/C99Native), nessuna ricopiatura. */
/* Sorgente .gguf: llmmodelio_arch_resolve si aspetta una dir HF (config.json). Per un file
 * GGUF si ricava `general.architecture` e si risolve <slug>.gmbarch nella cartella ABI di LLMEnv
 * (llmdesc_abi_dir) — stessa logica di llmdesc_resolve, senza config.json. */
static int llmmodelio_arch_resolve_gguf(const char* gguf_path) {
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(gguf_path, &kv, &nkv, &fi) != 0) return -1;
    char arch[96] = "";
    for (int i = 0; i < nkv; i++)
        if (strcmp(kv[i].key, "general.architecture") == 0 && kv[i].str) { snprintf(arch, sizeof arch, "%s", kv[i].str); break; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    if (!arch[0]) return -1;
    char slug[96]; { size_t k = 0; for (const char* p = arch; *p && k < sizeof slug - 1; ++p) { char c = *p; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); slug[k++] = c; } slug[k] = 0; }
    char path[4096];
    /* UNICA sede dei descrittori = cartella ABI di LLMEnv (llmdesc_abi_dir: impostazione paths.abiDir, localizzazione alla
     * prima esecuzione). Niente piu' ricerca di ABI/ o arch/ risalendo dalla cwd. Senza ABI il resolver registrato dal
     * runtime ha gia' segnalato l'errore: si ferma qui invece di ripiegare sul descrittore generico. */
    { char abi[2048];
      if (!llmdesc_abi_dir(abi, sizeof abi)) return -1;
      static const char* exts[] = { "llmarch", "gmbarch" };
      for (int x = 0; x < 2; x++) {
          snprintf(path, sizeof path, "%s/%s.%s", abi, slug, exts[x]);
          if (llmdesc_load(path, &g_arch) == 0) { snprintf(g_arch_path, sizeof g_arch_path, "%s", path); g_arch_ok = 1; return 0; }
      }
    }
    /* Fallback 2026-10-03: l'arch GGUF può OMETTERE gli underscore del nome file ABI (es. gguf
     * "qwen3moe" vs ABI "qwen3_moe.gmbarch"). Cerca nella dir ABI un file il cui nome, senza '_',
     * coincide col slug — così l'emitter MLX risolve anche i modelli con nomi "fusi". */
    { char abi[2048];
      if (llmdesc_abi_dir(abi, sizeof abi)) {
        DIR* dp = opendir(abi);
        if (dp != NULL) {
            struct dirent* de;
            while ((de = readdir(dp)) != NULL) {
                size_t ln = strlen(de->d_name);
                int isarch = (ln > 8 && (strcmp(de->d_name + ln - 8, ".llmarch") == 0 || strcmp(de->d_name + ln - 8, ".gmbarch") == 0));
                if (!isarch) continue;
                char base[256]; size_t bl = ln - 8; if (bl > sizeof base - 1) bl = sizeof base - 1;
                memcpy(base, de->d_name, bl); base[bl] = 0;
                char norm[256]; size_t k = 0;
                for (const char* q = base; *q && k < sizeof norm - 1; ++q) if (*q != '_') norm[k++] = *q;
                norm[k] = 0;
                if (strcasecmp(norm, slug) == 0) {
                    snprintf(path, sizeof path, "%s/%s", abi, de->d_name);
                    if (llmdesc_load(path, &g_arch) == 0) { snprintf(g_arch_path, sizeof g_arch_path, "%s", path); g_arch_ok = 1; closedir(dp); return 0; }
                }
            }
            closedir(dp);
        }
      }
    }
    /* FALLBACK GENERICO (2026-10-03): nessun `.gmbarch` -> arch GENERICO dedotto dalla
     * convenzione HF/GGUF (g_arch_path vuoto -> llmcodec_rolemap_load cade sul generico). Cosi'
     * un modello a decoder standard si converte GGUF->MLX senza file arch. */
    g_arch_ok = 1; g_arch_path[0] = 0; return 0;
}
/* ── MODALITY: emit automatico del .gmmod da modello+mmproj reali ──
 * Risolve l'arch DALL'ABI confrontando il basename del mmproj con `source.mmproj` dichiarato in
 * ogni .llmarch/.gmbarch (fallback: arch del base/ dell mmproj). Deriva la modality dichiarata,
 * valida che il mmproj porti tensori di quella famiglia, e scrive il descriptor `out_path`. */
static int lj_mod_role(const LlmDesc* a, const char* name, const char* modality);
int llmmodelio_model_modality(const char* base_path, const char* mmproj_path, const char* out_path) {
    if (base_path == NULL || mmproj_path == NULL || out_path == NULL) return -1;
    const char* sl = strrchr(mmproj_path, '/'); const char* mmb = sl ? sl + 1 : mmproj_path;
    int found = 0; char arch[96] = "";
    char abi[2048];
    if (llmdesc_abi_dir(abi, sizeof abi)) {
        DIR* dp = opendir(abi);
        if (dp != NULL) {
            struct dirent* de;
            while ((de = readdir(dp)) != NULL && !found) {
                size_t ln = strlen(de->d_name);
                int isarch = (ln > 8 && (strcmp(de->d_name + ln - 8, ".llmarch") == 0 || strcmp(de->d_name + ln - 8, ".gmbarch") == 0));
                if (!isarch) continue;
                char path[4096]; snprintf(path, sizeof path, "%s/%s", abi, de->d_name);
                LlmDesc d;
                if (llmdesc_load(path, &d) != 0) continue;
                if (d.source_mmproj[0]) {
                    const char* sb = strrchr(d.source_mmproj, '/'); const char* sbn = sb ? sb + 1 : d.source_mmproj;
                    if (strcmp(sbn, mmb) == 0) { g_arch = d; snprintf(g_arch_path, sizeof g_arch_path, "%s", path); g_arch_ok = 1; found = 1; snprintf(arch, sizeof arch, "%s", d.arch); }
                }
            }
            closedir(dp);
        }
    }
    if (!found) {
        if (llmmodelio_arch_resolve_gguf(base_path) != 0 && llmmodelio_arch_resolve_gguf(mmproj_path) != 0) return -1;
        if (g_arch_ok) snprintf(arch, sizeof arch, "%s", g_arch.arch);
    }
    if (!g_arch_ok) return -1;
    const char* mod = llmdesc_modality(&g_arch);
    if (strcmp(mod, "text") == 0) return -1;                  /* nessuna modality dichiarata -> errore */
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv = NULL; int nkv = 0;
    if (llmcodec_gguf_read_meta(mmproj_path, &kv, &nkv, &fi) != 0) return -1;
    int carry = 0;
    for (uint64_t i = 0; i < fi.n_tensors; i++)
        if (lj_mod_role(&g_arch, fi.tensors[i].name, mod) || lj_mod_role(&g_arch, fi.tensors[i].name, "vision")
            || lj_mod_role(&g_arch, fi.tensors[i].name, "audio") || lj_mod_role(&g_arch, fi.tensors[i].name, "mtp")) { carry = 1; break; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    if (!carry) return -1;                                    /* mmproj incoerente con la modality */
    FILE* o = fopen(out_path, "w"); if (o == NULL) return -1;
    fprintf(o, "{\n  \"module\": \"%s\",\n  \"arch\": \"%s\",\n  \"modality\": \"%s\",\n  \"base\": \"%s\",\n  \"mmproj\": \"%s\",\n  \"source\": { \"format\": \"gguf\", \"mmproj\": \"%s\" }\n}\n",
            mmb, arch, mod, base_path, mmproj_path, mmb);
    fclose(o);
    return 0;
}
/* ── AGGIUNGERE modulo dichiarato (.gmmod): dichiarata nell'ABI + risolta + eseguita ── */
static char* lj_slurp(const char* p) {
    FILE* f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char* b=(char*)malloc((size_t)n+1); if(!b){fclose(f);return NULL;}
    if (fread(b,1,(size_t)n,f)!=(size_t)n){free(b);fclose(f);return NULL;} b[n]=0; fclose(f); return b;
}
static const char* lj_v(const char* j, const char* key) {
    if (!j) return NULL;
    char pat[96]; snprintf(pat,sizeof pat,"\"%s\"",key);
    const char* p=j;
    while ((p=strstr(p,pat))) { const char* c=p+strlen(pat); while(*c==' '||*c=='\t')c++; if(*c==':'){c++;while(*c==' '||*c=='\t')c++;return c;} p++; }
    return NULL;
}
static void lj_s(const char* p, char* out, size_t cap) {
    if (cap) out[0]=0;
    if (!p) return;
    while (*p==' '||*p=='\t') p++;
    if (*p!='"') return; p++;
    size_t i=0; while (*p && *p!='"') { if (i<cap-1) out[i++]=*p; p++; } out[i]=0;
}
/* il tensore `name` appartiene alla famiglia della modalita' dichiarata? */
/* L'arch DICHIARA la modalita' `mod`? (per-famiglia: MTP/VISION/AUDIO/OCR coesistono). */
static int llm_desc_has_modality(const LlmDesc* a, const char* mod) {
    if (a == NULL || mod == NULL || !*mod) return 0;
    if (strcasecmp(mod, "text") == 0 || strcasecmp(mod, "adapter") == 0 || strcasecmp(mod, "module") == 0) return 1;
    if (strcasecmp(mod, "mtp") == 0) {
        if (a->mm_mtp) return 1;
        for (int i = 0; i < a->n_roles; i++) if (strcmp(a->roles[i].role, "MTP") == 0) return 1;
        return 0;
    }
    if (strcasecmp(mod, "audio") == 0) {
        for (int i = 0; i < a->n_roles; i++) if (strncmp(a->roles[i].role, "AUDIO", 5) == 0) return 1;
        return llmdesc_struct(a, "audio") != NULL;
    }
    if (strcasecmp(mod, "vision") == 0 || strcasecmp(mod, "ocr") == 0) {
        for (int i = 0; i < a->n_roles; i++) { const char* r = a->roles[i].role; if (strcmp(r, "VISION") == 0 || strcmp(r, "VISION_ALIGNER") == 0) return 1; }
        return llmdesc_struct(a, "vision") != NULL;
    }
    if (strcasecmp(mod, "omni") == 0) return llm_desc_has_modality(a, "vision") && llm_desc_has_modality(a, "audio");
    return 1;
}
static int lj_mod_role(const LlmDesc* a, const char* name, const char* modality) {
    const char* role = llmdesc_role(a, name);
    int vis = (strcmp(role,"VISION")==0 || strcmp(role,"VISION_ALIGNER")==0 || strstr(name,"vision") || strstr(name,"mmproj") || strstr(name,"image_"));
    int aud = (strncmp(role,"AUDIO",5)==0 || strstr(name,"audio") || strstr(name,"whisper"));
    int mtp = (strcmp(role,"MTP")==0 || strncmp(name,"mtp.",4)==0);
    if (strcasecmp(modality,"vision")==0 || strcasecmp(modality,"ocr")==0) return vis;
    if (strcasecmp(modality,"audio")==0) return aud;
    if (strcasecmp(modality,"mtp")==0) return mtp;
    if (strcasecmp(modality,"omni")==0) return vis || aud;
    return 1;   /* adapter|text|module: nessun vincolo di ruolo */
}
int llmmodelio_model_module(const char* base_path, const char* gmmod_path, const char* module_path, const char* out_path) {
    if (base_path==NULL || gmmod_path==NULL || module_path==NULL || out_path==NULL) return -1;
    char* j = lj_slurp(gmmod_path); if (j == NULL) return -1;
    char modality[32]; lj_s(lj_v(j,"modality"), modality, sizeof modality);
    char arch[96]; lj_s(lj_v(j,"arch"), arch, sizeof arch);
    char mmproj[256]; lj_s(lj_v(j,"mmproj"), mmproj, sizeof mmproj);
    free(j);
    if (!modality[0]) return -1;                 /* la modalita' DEVE essere dichiarata nel .gmmod */
    /* 1) RISOLVI l'ABI dell'arch (dal .gmmod o dal modulo gguf). */
    int ok = 0;
    if (arch[0]) {
        char abi[2048];
        if (llmdesc_abi_dir(abi, sizeof abi)) {
            char path[4096];
            snprintf(path,sizeof path,"%s/%s.llmarch", abi, arch);
            if (llmdesc_load(path,&g_arch)==0){ snprintf(g_arch_path,sizeof g_arch_path,"%s",path); g_arch_ok=1; ok=1; }
            if (!ok){ snprintf(path,sizeof path,"%s/%s.gmbarch", abi, arch); if (llmdesc_load(path,&g_arch)==0){ snprintf(g_arch_path,sizeof g_arch_path,"%s",path); g_arch_ok=1; ok=1; } }
        }
    }
    if (!ok) { if (llmmodelio_arch_resolve_gguf(module_path)!=0) return -1; }
    if (!g_arch_ok) return -1;
    /* 2) VALIDA: l'ABI dichiara la modalita'? (dichiarata nell'ABI) */
    if (!llm_desc_has_modality(&g_arch, modality)) return -1;   /* la modality dev'essere DICHIARATA dall'ABI */
    (void)llmdesc_modality;
    /* 3) VALIDA: il modulo porta almeno un tensore della famiglia dichiarata (risolta via ruoli ABI). */
    LlmCodecGgufFileInfo fi; LlmCodecGgufMetaKVOwned* kv=NULL; int nkv=0;
    if (llmcodec_gguf_read_meta(module_path, &kv, &nkv, &fi) != 0) return -1;
    int carry = 0;
    for (uint64_t i=0;i<fi.n_tensors;i++) if (lj_mod_role(&g_arch, fi.tensors[i].name, modality)) { carry=1; break; }
    llmcodec_gguf_free_info(&fi); llmcodec_gguf_meta_free(kv, nkv);
    if (!carry) return -1;
    (void)mmproj;
    /* 4) ESEGUI: fondi il modulo nella base. */
    return llmmodelio_model_add(base_path, module_path, out_path);
}
/* ── MODALITY OF: query della modality dichiarata (per consumatori: loader/scanner) ──
 * .gmmod -> legge {modality,arch,mmproj}; dir HF -> config.json + .llmarch; .gguf -> arch resolve. */
int llmmodelio_model_modality_of(const char* path, char* modality, size_t mcap, char* mmproj, size_t mmcap, char* arch, size_t acap) {
    if (path == NULL) return -1;
    if (modality && mcap) modality[0] = 0;
    if (mmproj && mmcap) mmproj[0] = 0;
    if (arch && acap) arch[0] = 0;
    size_t ln = strlen(path);
    if (ln > 6 && strcasecmp(path + ln - 6, ".gmmod") == 0) {
        char* j = lj_slurp(path); if (j == NULL) return -1;
        lj_s(lj_v(j,"modality"), modality, mcap);
        lj_s(lj_v(j,"arch"), arch, acap);
        lj_s(lj_v(j,"mmproj"), mmproj, mmcap);
        free(j);
        return (modality && modality[0]) ? 0 : -1;
    }
    int ok = 0;
    if (1 /* read_meta gestisce anche le dir MLX */) { if (llmmodelio_arch_resolve_gguf(path) == 0) ok = 1; }
    else { if (llmmodelio_arch_resolve(path) == 0) ok = 1; }
    if (!ok || !g_arch_ok) return -1;
    if (modality && mcap) snprintf(modality, mcap, "%s", llmdesc_modality(&g_arch));
    if (mmproj && mmcap) snprintf(mmproj, mmcap, "%s", llmdesc_source_mmproj(&g_arch));
    if (arch && acap) snprintf(arch, acap, "%s", g_arch.arch);
    return 0;
}
/* Ritorna la modality dichiarata come stringa STATICA (buffer interno) — comodo per P/Invoke.
 * "" se non risolta. Non liberare: e' un buffer statico interno. */
const char* llmmodelio_model_modality_str(const char* path) {
    static char buf[32];
    char m[32];
    if (llmmodelio_model_modality_of(path, m, sizeof m, NULL, 0, NULL, 0) != 0) return "";
    snprintf(buf, sizeof buf, "%s", m);
    return buf;
}
int llmmodelio_model_convert(const char* src, const char* dst, const char* fmt)
{
    const char* s = src ? src : "";
    const char* d = dst ? dst : "";
    const char* f = fmt ? fmt : "";
    if (!*s || !*d) return -1;
    /* FORMATO = proprietà del COMANDO, non del modello di partenza: `CONVERT <src>,<dst>,"<fmt>"`
     * decide l'OUTPUT (`fmt` = GGUF/F16 -> famiglia GGUF; MLX -> MLX) qualunque sia l'INPUT
     * (dir/.mlx = MLX; *.gguf = GGUF). Le 4 combinazioni sono coperte; MLX->MLX passa da un GGUF
     * INTERMEDIO (il surgery è GGUF-only) e ri-emette MLX. Una sola sorgente C -> parità 4 backend. */
    /* Nessun diagnostico su stderr (l'errore viaggia col codice di ritorno: parità cross-backend,
     * verificato 2026-09-24). */
    size_t ls = strlen(s);
    int in_gguf = (ls > 5 && strcasecmp(s + ls - 5, ".gguf") == 0);
    int in_mlx  = !in_gguf;
    size_t ld = strlen(d);
    int dst_mlx_ext = (ld >= 4 && strcasecmp(d + ld - 4, ".mlx") == 0) || (ld >= 1 && d[ld-1] == '/');
    { struct stat _st; if (!dst_mlx_ext && ld > 0 && stat(d, &_st) == 0 && S_ISDIR(_st.st_mode)) dst_mlx_ext = 1; }
    int out_mlx = (strcasecmp(f, "MLX") == 0) || (f[0] == 0 && dst_mlx_ext);
    /* Risolvi l'arch dall'input (ricetta _M): GGUF dal file, MLX dalla dir/.gmbarch. */
    if (in_gguf) { if (llmmodelio_arch_resolve_gguf(s) != 0) return -1; }
    else if (llmmodelio_arch_resolve(s) != 0) return -1;
    const char* ap = llmmodelio_arch_path();
    /* group/bits di OUTPUT per un artefatto MLX: dalla CONFIG DEL MODELLO (quantization.*), così
     * il round-trip conserva la quantizzazione e i loader MLX (mlx_lm) accettano l'output. Se la
     * config non li dichiara, 0 -> il codec ricade sul rolemap (.gmbarch) / default. */
    uint32_t out_group = 0, out_bits = 0;
    if (in_mlx) {
        char cjp[1300]; snprintf(cjp, sizeof cjp, "%s/config.json", s);
        LlmCodecQuantCfg qc; if (llmcodec_quantcfg_load(cjp, &qc) == 0) {
            if (qc.group > 0) out_group = (uint32_t)qc.group;
            if (qc.bits  > 0) out_bits  = (uint32_t)qc.bits;
        }
    }
    /* Target GGUF: "F16"=tensori F16 (sorgente quant); altro ("GGUF"/"MLX")=MXFP4 (default).
     * MLX->MLX usa MXFP4 per l'INTERMEDIO: il giro non è lossless, è la stessa ri-quantizzazione
     * della conversione a GGUF, solo con il container di ARRIVO scelto dal comando. */
    int target = (strcasecmp(f, "F16") == 0) ? LLMCODEC_GGML_T_F16 : LLMCODEC_GGML_T_MXFP4;
    int grp = (target == LLMCODEC_GGML_T_F16) ? 0 : 32;
    int bts = (target == LLMCODEC_GGML_T_F16) ? 0 : 4;

    if (in_mlx) {
        /* Tokenizer: unica sorgente = llmtoken; il codec riceve i dati digeriti (non ri-parsa). */
        LlmCodecTokenizerMeta tm; memset(&tm, 0, sizeof tm);
        llmtoken_handle th = llmmodelio_tokenizer_open(s);
        if (th) {
            size_t ntok = 0, nmg = 0, ntt = 0;
            tm.tokens = llmtoken_export_tokens(th, &ntok); tm.n_tokens = (int)ntok;
            tm.merges = llmtoken_export_merges(th, &nmg); tm.n_merges = (int)nmg;
            tm.token_types = llmtoken_export_token_types(th, &ntt); tm.n_token_types = (int)ntt;
            tm.model  = llmtoken_gguf_model(th);
        }
        if (!out_mlx) {
            int rc = llmcodec_conv_mlx_to_gguf_arch_ex(s, d, ap, target, grp, bts, th ? &tm : NULL);
            if (th) llmtoken_free(th);
            return rc;
        }
        /* MLX -> MLX: GGUF intermedio (veicolo del surgery) poi emissione MLX. */
        char tmp[1600]; snprintf(tmp, sizeof tmp, "%s.__conv__.gguf", d);
        int rc = llmcodec_conv_mlx_to_gguf_arch_ex(s, tmp, ap, target, grp, bts, th ? &tm : NULL);
        if (th) llmtoken_free(th);
        /* group/bits = 0 -> la funzione li prende dal rolemap (.gmbarch): così l'MLX di uscita
         * conserva la quantizzazione del modello (l'hardcode 32/4 produce scales di forma
         * inattesa e i loader MLX rifiutano l'artefatto). */
        if (rc == 0) rc = llmcodec_conv_gguf_to_mlx_arch(tmp, d, ap, out_group, out_bits);
        remove(tmp);
        return rc;
    }
    /* INPUT GGUF: -> MLX (emissione) o -> GGUF (ri-quantizzazione/ricetta). */
    if (out_mlx) return llmcodec_conv_gguf_to_mlx_arch(s, d, ap, out_group, out_bits);
    return llmmodelio_model_quantize(s, d);
}
