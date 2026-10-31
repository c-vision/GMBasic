/* llmmodelio_patch.c — B5: delta binario applicabile (`LLM MAKEPATCH` / `LLM APPLYPATCH`).
 *
 * Un `.gmbpatch` è un **GGUF** che contiene SOLO i tensori di <target> che DIFFERISCONO (byte) o
 * sono NUOVI rispetto a <base>; applicarlo sovrascrive/aggiunge by-name → ricostruisce <target>
 * senza copiare la parte invariata. Utile per cambi SPARSI (prune esperti, adapter, distill);
 * dopo una REQUANT il delta ≈ il modello intero (nessun risparmio, ma corretto).
 *
 *   MAKEPATCH <base>, <target>, <patch>   crea il delta .gmbpatch
 *   APPLYPATCH <base>, <patch>, <out>     base + delta -> out (= llmmodelio_model_add)
 *
 * NB: non copre la RIMOZIONE di tensori (es. `REMOVE` di un layer) — per quello usare il salvataggio
 * completo. Assume base/target/patch resi disponibili dai chiamanti (path). */
#include "llmmodelio_model.h"
#include "llmcodec_gguf_read.h"
#include "llmcodec_gguf_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t pt_numel(const uint64_t* d, int nd) { uint64_t n = 1; for (int i = 0; i < nd; i++) n *= d[i]; return n; }
static LlmCodecGgufTensorInfo* pt_find(LlmCodecGgufFileInfo* fi, const char* nm) {
    for (uint64_t i = 0; i < fi->n_tensors; i++) if (strcmp(fi->tensors[i].name, nm) == 0) return &fi->tensors[i];
    return NULL;
}

int llmmodelio_makepatch(const char* base, const char* target, const char* patch, FILE* out, int* handled)
{
    if (handled) *handled = 0;
    if (base == NULL || !*base || target == NULL || !*target || patch == NULL || !*patch) return -1;
    FILE* fo = out ? out : stdout;
    if (handled) *handled = 1;

    LlmCodecGgufFileInfo fb, ft;
    LlmCodecGgufMetaKVOwned *kvb = NULL, *kvt = NULL; int nkvb = 0, nkvt = 0;
    if (llmcodec_gguf_read_meta(base, &kvb, &nkvb, &fb) != 0) { fprintf(stderr, "LLM MAKEPATCH: read base fallito: %s\n", base); return -1; }
    if (llmcodec_gguf_read_meta(target, &kvt, &nkvt, &ft) != 0) {
        llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_free_info(&fb); return -1;
    }
    FILE* bfp = fopen(base, "rb"); FILE* tfp = fopen(target, "rb");
    if (bfp == NULL || tfp == NULL) { if (bfp) fclose(bfp); if (tfp) fclose(tfp);
        llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_meta_free(kvt, nkvt);
        llmcodec_gguf_free_info(&fb); llmcodec_gguf_free_info(&ft); return -1; }

    LlmCodecGgufTensor* ts = (LlmCodecGgufTensor*)calloc((size_t)(ft.n_tensors ? ft.n_tensors : 1), sizeof(*ts));
    uint64_t* dims = (uint64_t*)calloc((size_t)(ft.n_tensors ? ft.n_tensors : 1) * 8, sizeof(uint64_t));
    LlmCodecGgufMetaKV* wkv = (LlmCodecGgufMetaKV*)calloc((size_t)(nkvt + 1), sizeof(*wkv));
    int rc = 0; uint64_t nout = 0;
    for (uint64_t j = 0; j < ft.n_tensors && rc == 0; j++) {
        LlmCodecGgufTensorInfo* tt = &ft.tensors[j];
        uint64_t b = 0, numel = pt_numel(tt->dims, tt->ndim);
        if ((b = llmcodec_gguf_tensor_bytes(tt), 0) != 0 || b == 0) { rc = -1; break; }
        void* buf = malloc((size_t)b); if (buf == NULL) { rc = -1; break; }
        if (fseek(tfp, (long)(ft.data_start + tt->offset), SEEK_SET) != 0 || fread(buf, 1, (size_t)b, tfp) != (size_t)b) { free(buf); rc = -1; break; }
        /* include se NON in base, o byte diversi */
        int include = 1;
        LlmCodecGgufTensorInfo* tb = pt_find(&fb, tt->name);
        if (tb != NULL && tb->ggml_type == tt->ggml_type && tb->ndim == tt->ndim) {
            int dims_ok = 1; for (int d = 0; d < tt->ndim; d++) if (tb->dims[d] != tt->dims[d]) dims_ok = 0;
            if (dims_ok && llmcodec_gguf_tensor_bytes(tb) == b) {
                void* bb = malloc((size_t)b);
                if (bb != NULL && fseek(bfp, (long)(fb.data_start + tb->offset), SEEK_SET) == 0 && fread(bb, 1, (size_t)b, bfp) == (size_t)b)
                    include = (memcmp(bb, buf, (size_t)b) != 0);
                free(bb);
            }
        }
        if (!include) { free(buf); continue; }
        ts[nout].name = tt->name; ts[nout].ggml_type = tt->ggml_type; ts[nout].ndim = tt->ndim;
        ts[nout].dims = dims + nout * 8; for (int d = 0; d < tt->ndim; d++) ((uint64_t*)ts[nout].dims)[d] = tt->dims[d];
        ts[nout].data = buf; nout++;
    }
    /* Tensori presenti nella BASE ma ASSENTI nel TARGET = RIMOSSI (es. `LLM MODEL REMOVE`):
     * il delta li registra nel KV `gmbasic.patch.removed` così APPLYPATCH li elimina dalla base. */
    char* removed = NULL; size_t rem_len = 0, rem_cap = 0; uint64_t nrem = 0;
    for (uint64_t i = 0; i < fb.n_tensors; i++) {
        if (pt_find(&ft, fb.tensors[i].name) != NULL) continue;   /* presente anche nel target */
        size_t nl = strlen(fb.tensors[i].name);
        if (rem_len + nl + 2 > rem_cap) { rem_cap = (rem_len + nl + 2) * 2 + 64; removed = (char*)realloc(removed, rem_cap); }
        if (rem_len) removed[rem_len++] = ',';
        memcpy(removed + rem_len, fb.tensors[i].name, nl); rem_len += nl; removed[rem_len] = 0; nrem++;
    }

    if (rc == 0 && nout == 0) {
        fclose(bfp); fclose(tfp); free(ts); free(dims); free(wkv);
        llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_meta_free(kvt, nkvt);
        llmcodec_gguf_free_info(&fb); llmcodec_gguf_free_info(&ft);
        fprintf(fo, "LLM MAKEPATCH: base=%s target=%s patch=%s tensori_delta=0 (nessuna differenza)\n", base, target, patch);
        return 0;
    }
    if (rc == 0) {
        for (int i = 0; i < nkvt; i++) llmcodec_gguf_meta_to_writer(&kvt[i], &wkv[i]);
        wkv[nkvt].key = "gmbasic.patch.removed";
        wkv[nkvt].type = LLMCODEC_GGUF_MT_STRING;   /* enum da llmcodec_gguf_read.h (=8) */
        wkv[nkvt].str = removed ? removed : "";
        rc = llmcodec_gguf_write(patch, wkv, nkvt + 1, ts, (int)nout, 32);
    }
    if (rc != 0) fprintf(stderr, "LLM MAKEPATCH: scrittura fallita (%s, nout=%llu)\n", patch, (unsigned long long)nout);
    for (uint64_t i = 0; i < nout; i++) free((void*)ts[i].data);
    fclose(bfp); fclose(tfp); free(ts); free(dims); free(wkv); free(removed);
    llmcodec_gguf_meta_free(kvb, nkvb); llmcodec_gguf_meta_free(kvt, nkvt);
    llmcodec_gguf_free_info(&fb); llmcodec_gguf_free_info(&ft);
    if (rc != 0) return -1;
    fprintf(fo, "LLM MAKEPATCH: base=%s target=%s patch=%s tensori_delta=%llu/%llu rimossi=%llu\n",
            base, target, patch, (unsigned long long)nout, (unsigned long long)ft.n_tensors, (unsigned long long)nrem);
    return 0;
}
