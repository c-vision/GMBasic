/* llmmodelio_quanterror.c — B4: errore di quantizzazione numerico (`LLM TENSOR QUANTERROR`).
 *
 * Confronta un modello di RIFERIMENTO (es. F16) con un CANDIDATO (es. quantizzato) e misura
 * l'errore di ricostruzione sui tensori **F32/F16 comuni** (dequant-free: valori grezzi):
 *   rel_l2 = sqrt( Σ(cand-ref)² / Σref² )  aggregato su tutti i tensori numerici condivisi;
 *   max_abs = max |cand-ref|.
 * I tensori quantizzati (bit) e quelli solo in un modello NON entrano nel calcolo numerico ma
 * sono contati. ref == cand → rel_l2 = 0. Nessuna dipendenza esterna (codec GGUF condiviso). */
#include "llmmodelio_model.h"
#include "llmcodec_gguf_read.h"
#include "llmcodec_ggml_quants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static float qe_h2f(uint16_t h)
{
    uint32_t sign = (uint32_t)(h >> 15) << 31, exp = (h >> 10) & 0x1f, man = h & 0x3ff, f;
    if (exp == 0) {
        if (man == 0) f = sign;
        else { int e = -1; do { e++; man <<= 1; } while (!(man & 0x400)); man &= 0x3ff;
               f = sign | ((uint32_t)(127 - 15 - e) << 23) | (man << 13); }
    } else if (exp == 31) f = sign | 0x7f800000u | (man << 13);
    else f = sign | ((uint32_t)(exp - 15 + 127) << 23) | (man << 13);
    float o; memcpy(&o, &f, 4); return o;
}

int llmmodelio_quant_error(const char* ref, const char* cand, FILE* out, int* handled)
{
    if (handled) *handled = 0;
    if (ref == NULL || !*ref || cand == NULL || !*cand) return -1;
    FILE* fo = out ? out : stdout;
    if (handled) *handled = 1;

    LlmCodecGgufFileInfo fa, fb;
    LlmCodecGgufMetaKVOwned *kva = NULL, *kvb = NULL; int nkva = 0, nkvb = 0;
    if (llmcodec_gguf_read_meta(ref, &kva, &nkva, &fa) != 0) return -1;
    if (llmcodec_gguf_read_meta(cand, &kvb, &nkvb, &fb) != 0) {
        llmcodec_gguf_meta_free(kva, nkva); llmcodec_gguf_free_info(&fa); return -1;
    }
    FILE* afp = fopen(ref, "rb"); FILE* bfp = fopen(cand, "rb");
    if (afp == NULL || bfp == NULL) {
        if (afp) fclose(afp); if (bfp) fclose(bfp);
        llmcodec_gguf_meta_free(kva, nkva); llmcodec_gguf_meta_free(kvb, nkvb);
        llmcodec_gguf_free_info(&fa); llmcodec_gguf_free_info(&fb); return -1;
    }
    double sum_err = 0.0, sum_ref = 0.0; double max_abs = 0.0;
    uint64_t numeric = 0, quant = 0, only = 0;
    for (uint64_t i = 0; i < fa.n_tensors; i++) {
        LlmCodecGgufTensorInfo* ta = &fa.tensors[i]; LlmCodecGgufTensorInfo* tb = NULL;
        for (uint64_t j = 0; j < fb.n_tensors; j++)
            if (strcmp(ta->name, fb.tensors[j].name) == 0) { tb = &fb.tensors[j]; break; }
        if (tb == NULL) { only++; continue; }
        int isf32 = (ta->ggml_type == LLMCODEC_GGML_T_F32 && tb->ggml_type == LLMCODEC_GGML_T_F32);
        int isf16 = (ta->ggml_type == LLMCODEC_GGML_T_F16 && tb->ggml_type == LLMCODEC_GGML_T_F16);
        if (!isf32 && !isf16) { quant++; continue; }
        int dims_ok = (ta->ndim == tb->ndim); for (int d = 0; d < ta->ndim && dims_ok; d++) if (ta->dims[d] != tb->dims[d]) dims_ok = 0;
        if (!dims_ok) { quant++; continue; }
        uint64_t numel = 1; for (int d = 0; d < ta->ndim; d++) numel *= ta->dims[d];
        uint64_t b = llmcodec_gguf_tensor_bytes(ta);
        if (b == 0 || numel == 0) { quant++; continue; }
        unsigned char* buf = (unsigned char*)malloc((size_t)b);
        unsigned char* buf2 = (unsigned char*)malloc((size_t)b);
        if (buf == NULL || buf2 == NULL) { free(buf); free(buf2); quant++; continue; }
        int ok = (fseek(afp, (long)(fa.data_start + ta->offset), SEEK_SET) == 0) && (fread(buf, 1, (size_t)b, afp) == (size_t)b)
              && (fseek(bfp, (long)(fb.data_start + tb->offset), SEEK_SET) == 0) && (fread(buf2, 1, (size_t)b, bfp) == (size_t)b);
        if (!ok) { free(buf); free(buf2); quant++; continue; }
        for (uint64_t k = 0; k < numel; k++) {
            double va, vb;
            if (isf32) { float x, y; memcpy(&x, buf + 4*k, 4); memcpy(&y, buf2 + 4*k, 4); va = x; vb = y; }
            else { uint16_t x, y; memcpy(&x, buf + 2*k, 2); memcpy(&y, buf2 + 2*k, 2); va = qe_h2f(x); vb = qe_h2f(y); }
            double d = vb - va; sum_err += d*d; sum_ref += va*va; if (fabs(d) > max_abs) max_abs = fabs(d);
        }
        numeric++;
        free(buf); free(buf2);
    }
    fclose(afp); fclose(bfp);
    llmcodec_gguf_meta_free(kva, nkva); llmcodec_gguf_meta_free(kvb, nkvb);
    llmcodec_gguf_free_info(&fa); llmcodec_gguf_free_info(&fb);
    double rel_l2 = sum_ref > 0.0 ? sqrt(sum_err / sum_ref) : 0.0;
    fprintf(fo, "LLM TENSOR QUANTERROR '%s' vs '%s': numeric=%llu quant=%llu only=%llu rel_l2=%.6g max_abs=%.6g\n",
            ref, cand, (unsigned long long)numeric, (unsigned long long)quant,
            (unsigned long long)only, rel_l2, max_abs);
    return 0;
}
